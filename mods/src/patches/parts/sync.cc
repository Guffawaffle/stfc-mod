/**
 * @file sync.cc
 * @brief Data synchronization engine — external HTTP sync of game state.
 *
 * Intercepts the game's protobuf and JSON entity-group processing to extract
 * player data (inventories, officers, research, ships, battle logs, etc.) and
 * forward it to user-configured external sync targets over HTTP.
 *
 * Architecture:
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │ Game hooks (DataContainer::ParseBinaryObject, etc.)         │
 *  │   → HandleEntityGroup() dispatches by EntityGroup::Type     │
 *  │       → process_* functions parse protobuf/JSON             │
 *  │           → queue_data() enqueues to sync_data_queue        │
 *  │               → ship_sync_data() thread dequeues & sends    │
 *  │                   → http::send_data() → per-target workers  │
 *  └─────────────────────────────────────────────────────────────┘
 *
 * Battle logs follow a separate pipeline:
 *  process_battle_headers() → combat_log_data_queue
 *    → ship_combat_log_data() thread fetches full journal from Scopely
 *    → resolves player/alliance names via cache or API
 *    → sends enriched battle data via http::send_data()
 *
 * Threading model:
 *  - ship_sync_data:       long-lived consumer for the main sync queue
 *  - ship_combat_log_data: long-lived consumer for combat log enrichment
 *  - target_worker_thread: one per sync target (created lazily), owns its
 *                          own cpr::Session and request queue
 *  - process_* functions:  each invoked on a detached std::thread from
 *                          HandleEntityGroup's submit_async lambda
 *
 * Each process_* function maintains its own static state map (with mutex)
 * and only emits data when values actually change (delta-based sync).
 *
 * Config keys (sync_targets[name]):
 *  - url, token, proxy, verify_ssl: per-target connection settings
 *  - enabled types: battlelogs, resources, ships, buildings, inventory, etc.
 * Config keys (sync_options):
 *  - proxy, verify_ssl: Scopely API proxy for combat log enrichment
 *  - sync_resolver_cache_ttl: TTL for player/alliance name caches
 *  - sync_logging, sync_debug: log verbosity toggles
 */

#include "errormsg.h"
#include "patches/hook_registry.h"
#include "patches/sync_battle_logs.h"
#include "patches/sync_capability_snapshot.h"
#include "patches/sync_payload_builders.h"
#include "patches/sync_scheduler.h"
#include "patches/sync_transport.h"
#include "str_utils.h"

#include <il2cpp-api-types.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/RealtimeDataPayload.h>
#include <prime/ServiceResponse.h>
#include <spud/detour.h>

#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

// Sync payload builders and entity-group dispatch live in sync_payload_builders.cc.

namespace
{
constexpr HookDescriptor SyncHook(std::string_view name, std::string_view purpose, std::string_view namespc,
                                  std::string_view class_name, std::string_view method_name,
                                  std::string_view likely_symptom)
{
  return {name,
          purpose,
          {"Digit.Client.PrimeLib.Runtime", namespc, class_name, method_name},
          likely_symptom,
          HookSupportTier::Production};
}

constexpr auto kModelRegistryProcessResult =
    SyncHook("model-registry-process-result", "capture service response entity groups", "Digit.PrimeServer.Core",
             "GameServerModelRegistry", "ProcessResultInternal", "HTTP-backed sync categories stop updating");
constexpr auto kModelRegistryParseBinary =
    SyncHook("model-registry-parse-binary", "capture bulk binary entity groups", "Digit.PrimeServer.Core",
             "GameServerModelRegistry", "ParseBinaryObjectsHelper", "binary sync categories stop updating");
constexpr auto kBuffContainerParse =
    SyncHook("buff-container-parse", "capture buff entity groups", "Digit.PrimeServer.Services", "BuffDataContainer",
             "ParseBinaryObject", "buff sync stops updating");
constexpr auto kBuffServiceParse =
    SyncHook("buff-service-parse", "capture buff service entity groups", "Digit.PrimeServer.Services", "BuffService",
             "ParseBinaryObject", "buff sync may miss service responses");
constexpr auto kInventoryContainerParse =
    SyncHook("inventory-container-parse", "capture inventory entity groups", "Digit.PrimeServer.Services",
             "InventoryDataContainer", "ParseBinaryObject", "inventory sync stops updating");
constexpr auto kJobServiceParse =
    SyncHook("job-service-parse", "capture job entity groups", "Digit.PrimeServer.Services", "JobService",
             "ParseBinaryObject", "job sync stops updating");
constexpr auto kJobContainerParse =
    SyncHook("job-container-parse", "capture job container entity groups", "Digit.PrimeServer.Services",
             "JobServiceDataContainer", "ParseBinaryObject", "job sync misses container updates");
constexpr auto kMissionsContainerParse =
    SyncHook("missions-container-parse", "capture mission entity groups", "Digit.PrimeServer.Models",
             "MissionsDataContainer", "ParseBinaryObject", "mission sync stops updating");
constexpr auto kResearchContainerParse =
    SyncHook("research-container-parse", "capture research entity groups", "Digit.PrimeServer.Services",
             "ResearchDataContainer", "ParseBinaryObject", "research sync stops updating");
constexpr auto kResearchServiceParse =
    SyncHook("research-service-parse", "capture research service entity groups", "Digit.PrimeServer.Services",
             "ResearchService", "ParseBinaryObject", "research sync misses service updates");
constexpr auto kSlotContainerParse =
    SyncHook("slot-container-parse", "capture full slot entity groups", "Digit.PrimeServer.Services",
             "SlotDataContainer", "ParseBinaryObject", "slot sync stops receiving full snapshots");
constexpr auto kSlotContainerEntitySlots =
    SyncHook("slot-container-entity-slots", "capture parsed slot entity groups", "Digit.PrimeServer.Services",
             "SlotDataContainer", "ParseEntitySlotsData", "slot sync misses parsed snapshots");
constexpr auto kSlotAssignRtc =
    SyncHook("slot-assign-rtc", "capture realtime slot assignments", "Digit.PrimeServer.Parsers", "SlotAssignRtcParser",
             "ParseFinalPayload", "CT/FT equip assignments do not refresh sync consumers");
constexpr auto kSlotClearRtc =
    SyncHook("slot-clear-rtc", "capture realtime slot clears", "Digit.PrimeServer.Parsers", "SlotClearRtcParser",
             "ParseFinalPayload", "CT/FT equip clears do not refresh sync consumers");
constexpr HookDescriptor kPrimeAppInit{"prime-app-init",
                                       "capture server URL and session identity",
                                       {"Assembly-CSharp", "Digit.Client.Core", "PrimeApp", "InitPrimeServer"},
                                       "authenticated Scopely sync requests fail",
                                       HookSupportTier::Production};
constexpr auto           kGameServerInitialise =
    SyncHook("game-server-initialise", "capture the current game version", "Digit.PrimeServer.Core", "GameServer",
             "Initialise", "sync requests use a stale game version header");
constexpr auto kGameServerInstance =
    SyncHook("game-server-instance", "capture the current instance id", "Digit.PrimeServer.Core", "GameServer",
             "SetInstanceIdHeader", "sync requests use a stale instance header");
} // namespace

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

/**
 * @brief Hook: DataContainer::ParseBinaryObject
 *
 * Intercepts binary entity group parsing to extract data before the game processes it.
 * Original method: deserializes a protobuf entity group into the data container.
 * Our modification: calls HandleEntityGroup() first, then the original.
 */
#if __APPLE__
void DataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group)
{
  HandleEntityGroup(group);
  return original(_this, group);
}
#else
void DataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
}
#endif

void DataContainer_ParseEntitySlotsData(auto original, void* _this, EntityGroup* group)
{
  HandleEntityGroup(group);
  return original(_this, group);
}

// IL2CPP value type. Passing this by value preserves the following arguments on
// both x64 and macOS ARM64; a void* declaration shifts them on AAPCS64.
struct CentrifugoInfo {
  Il2CppString* Channel;
  int32_t       Offset;
};

static_assert(sizeof(CentrifugoInfo) == 16);

void* RtcParser_ParseFinalPayload(auto original, void* _this, CentrifugoInfo centrifugo_info, RealtimeDataPayload* data,
                                  void* final_payload)
{
  HandleRealtimeDataPayload(data);
  return original(_this, centrifugo_info, data, final_payload);
}

/**
 * @brief Hook: GameServerModelRegistry::ProcessResultInternal
 *
 * Intercepts HTTP response processing to extract entity groups from the response.
 * Original method: processes the service response and invokes callbacks.
 * Our modification: iterates entity groups and calls HandleEntityGroup() before original.
 */
void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, void* parsing_context,
                                                   ServiceResponse* service_response, MethodInfo* method)
{
  HandleServiceResponseEntityGroups(service_response);

  return original(_this, parsing_context, service_response, method);
}

/**
 * @brief Hook: GameServerModelRegistry::ParseBinaryObjectsHelper
 *
 * Intercepts bulk binary object handling to extract entity groups.
 * Same pattern as ProcessResultInternal but for binary-only responses.
 */
void GameServerModelRegistry_ParseBinaryObjectsHelper(auto original, void* _this, void* parsing_context,
                                                      ServiceResponse* service_response, void* parsedEntityTypes,
                                                      MethodInfo* method)
{
  HandleServiceResponseEntityGroups(service_response);

  return original(_this, parsing_context, service_response, parsedEntityTypes, method);
}

/**
 * @brief Hook: PrimeApp::InitPrimeServer
 *
 * Captures the game server URL and session ID so we can make authenticated
 * requests to the Scopely API for combat log enrichment.
 */
void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl, Il2CppString* gatewayServerUrl,
                              Il2CppString* sessionId, Il2CppString* serverRegion)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId, serverRegion);
  http::headers::SetPrimeServerHeaders(to_string(to_wstring(gameServerUrl)), to_string(to_wstring(sessionId)));
}

/** @brief Hook: GameServer::Initialise — captures the game version string for HTTP headers. */
void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                           bool encryptRequests, Il2CppString* serverRegion)
{
  original(_this, sessionId, gameVersion, encryptRequests, serverRegion);
  http::headers::SetPrimeVersion(to_string(to_wstring(gameVersion)));
}

/** @brief Hook: GameServer::SetInstanceIdHeader — captures the instance ID for HTTP headers. */
void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::SetInstanceId(instanceId);
}

// ─── Hook Installation ──────────────────────────────────────────────────────

/**
 * @brief Installs all sync-related hooks and starts background threads.
 *
 * Hooks multiple DataContainer::ParseBinaryObject variants to intercept
 * entity group data, hooks PrimeApp/GameServer for session credentials,
 * and spawns the two long-lived consumer threads:
 *  - ship_sync_data (main sync queue consumer)
 *  - ship_combat_log_data (combat log enrichment consumer)
 */
void InstallSyncPatches()
{
  HookModuleHealth hooks("SyncHooks");
  load_previously_sent_logs();

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
    hooks.record_missing_helper(kModelRegistryProcessResult);
    hooks.record_missing_helper(kModelRegistryParseBinary);
  } else {
    auto* ptr = game_server_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInternal");
      hooks.record_missing_method(kModelRegistryProcessResult);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kModelRegistryProcessResult, ptr,
                                       GameServerModelRegistry_ProcessResultInternal);
    }

    ptr = game_server_model_registry.GetMethod("ParseBinaryObjectsHelper");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ParseBinaryObjectsHelper");
      hooks.record_missing_method(kModelRegistryParseBinary);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kModelRegistryParseBinary, ptr,
                                       GameServerModelRegistry_ParseBinaryObjectsHelper);
    }
  }

#if 0
  // Classification: temporary exception, parked for `audit/legacy-sync-platform-registry-hook`.
  // This disabled legacy hook must not be re-enabled without seam-owner review and provenance metadata.
  if (auto platform_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
      !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (auto *const ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInternal");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }
#endif

  if (auto buff_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffDataContainer");
      !buff_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffDataContainer");
    hooks.record_missing_helper(kBuffContainerParse);
  } else {
    if (const auto ptr = buff_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kBuffContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kBuffContainerParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

#if __APPLE__
  // 1.000.49105: BuffService.ParseBinaryObject is a 0x18-byte body immediately before HandleResponseData.
  // Spud's ARM64 absolute jump is larger than that, so detouring it overwrites the next function entry.
  spdlog::info("Skipping BuffService hook lookup on macOS");
  hooks.record_skipped(kBuffServiceParse, "macOS method body is too small for the ARM64 absolute jump");
#else
  if (auto buff_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
      !buff_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
    hooks.record_missing_helper(kBuffServiceParse);
  } else {
    if (const auto ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ParseBinaryObject");
      hooks.record_missing_method(kBuffServiceParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kBuffServiceParse, ptr, DataContainer_ParseBinaryObject);
    }
  }
#endif

  if (auto inventory_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                              "Digit.PrimeServer.Services", "InventoryDataContainer");
      !inventory_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
    hooks.record_missing_helper(kInventoryContainerParse);
  } else {
    if (const auto ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kInventoryContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kInventoryContainerParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
      !job_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobService");
    hooks.record_missing_helper(kJobServiceParse);
  } else {
    if (const auto ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobService", "ParseBinaryObject");
      hooks.record_missing_method(kJobServiceParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kJobServiceParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service_data_container = il2cpp_get_class_helper(
          "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobServiceDataContainer");
      !job_service_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobServiceDataContainer");
    hooks.record_missing_helper(kJobContainerParse);
  } else {
    if (const auto ptr = job_service_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobServiceDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kJobContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kJobContainerParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto missions_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
      !missions_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
    hooks.record_missing_helper(kMissionsContainerParse);
  } else {
    if (const auto ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kMissionsContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kMissionsContainerParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                             "Digit.PrimeServer.Services", "ResearchDataContainer");
      !research_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
    hooks.record_missing_helper(kResearchContainerParse);
  } else {
    if (const auto ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kResearchContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kResearchContainerParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
      !research_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
    hooks.record_missing_helper(kResearchServiceParse);
  } else {
    if (const auto ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
      hooks.record_missing_method(kResearchServiceParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kResearchServiceParse, ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto slot_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SlotDataContainer");
      !slot_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "SlotDataContainer");
    hooks.record_missing_helper(kSlotContainerParse);
    hooks.record_missing_helper(kSlotContainerEntitySlots);
  } else {
    if (const auto ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
      hooks.record_missing_method(kSlotContainerParse);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotContainerParse, ptr, DataContainer_ParseBinaryObject);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseEntitySlotsData"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseEntitySlotsData");
      hooks.record_missing_method(kSlotContainerEntitySlots);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotContainerEntitySlots, ptr, DataContainer_ParseEntitySlotsData);
    }
  }

  if (auto slot_assign_parser =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotAssignRtcParser");
      !slot_assign_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotAssignRtcParser");
    hooks.record_missing_helper(kSlotAssignRtc);
  } else if (const auto ptr = slot_assign_parser.GetMethod("ParseFinalPayload"); ptr == nullptr) {
    ErrorMsg::MissingMethod("SlotAssignRtcParser", "ParseFinalPayload");
    hooks.record_missing_method(kSlotAssignRtc);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotAssignRtc, ptr, RtcParser_ParseFinalPayload);
  }

  if (auto slot_clear_parser =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotClearRtcParser");
      !slot_clear_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotClearRtcParser");
    hooks.record_missing_helper(kSlotClearRtc);
  } else if (const auto ptr = slot_clear_parser.GetMethod("ParseFinalPayload"); ptr == nullptr) {
    ErrorMsg::MissingMethod("SlotClearRtcParser", "ParseFinalPayload");
    hooks.record_missing_method(kSlotClearRtc);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotClearRtc, ptr, RtcParser_ParseFinalPayload);
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
    hooks.record_missing_helper(kPrimeAppInit);
  } else {
    if (const auto ptr = prime_app.GetMethod("InitPrimeServer"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
      hooks.record_missing_method(kPrimeAppInit);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kPrimeAppInit, ptr, PrimeApp_InitPrimeServer);
    }
  }

  if (auto game_server =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
      !game_server.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
    if (Config::Get().installGameVersionHook) {
      hooks.record_missing_helper(kGameServerInitialise);
    } else {
      hooks.record_skipped(kGameServerInitialise, "disabled by patches.game_version");
    }
    hooks.record_missing_helper(kGameServerInstance);
  } else {
    if (!Config::Get().installGameVersionHook) {
      spdlog::info("Sync: skipping GameServer::Initialise hook (patches.game_version = false)");
      hooks.record_skipped(kGameServerInitialise, "disabled by patches.game_version");
    } else if (const auto ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
      hooks.record_missing_method(kGameServerInitialise);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kGameServerInitialise, ptr, GameServer_Initialise);
    }

    if (const auto ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
      hooks.record_missing_method(kGameServerInstance);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kGameServerInstance, ptr, GameServer_SetInstanceIdHeader);
    }
  }

  hooks.log_summary();

  StartSyncSchedulerWorker();
  queue_mod_capability_snapshot();
  StartCombatLogWorker();
}
