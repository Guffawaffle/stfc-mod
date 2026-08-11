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
 *  │ Central service-response hook plus session and RTC hooks    │
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

#include <array>
#include <initializer_list>
#include <sstream>
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

constexpr std::array kReplacedBinaryHooks{kModelRegistryParseBinary, kBuffContainerParse,     kBuffServiceParse,
                                          kInventoryContainerParse,  kJobServiceParse,        kJobContainerParse,
                                          kMissionsContainerParse,   kResearchContainerParse, kResearchServiceParse,
                                          kSlotContainerParse};

std::string il2cpp_type_name(const Il2CppType* type)
{
  if (type == nullptr) {
    return "<null>";
  }

  char* const raw_name = il2cpp_type_get_name(type);
  if (raw_name == nullptr) {
    return "<unknown>";
  }

  std::string name(raw_name);
  il2cpp_free(raw_name);
  return name;
}

std::string method_signature(const MethodInfo* method)
{
  if (method == nullptr) {
    return "<missing>";
  }

  std::ostringstream out;
  out << il2cpp_type_name(il2cpp_method_get_return_type(method)) << " (";
  const auto count = il2cpp_method_get_param_count(method);
  for (uint32_t index = 0; index < count; ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << il2cpp_type_name(il2cpp_method_get_param(method, index));
  }
  out << ')';
  return out.str();
}

const MethodInfo* find_exact_sync_method(IL2CppClassHelper& helper, const HookDescriptor& descriptor,
                                         const std::string_view                        expected_return,
                                         const std::initializer_list<std::string_view> expected_parameters,
                                         HookModuleHealth&                             health)
{
  const auto* method =
      helper.GetMethodInfo(descriptor.target.method_name.data(), static_cast<int>(expected_parameters.size()));
  if (method == nullptr) {
    health.record_missing_method(descriptor);
    return nullptr;
  }

  bool matches = il2cpp_type_name(il2cpp_method_get_return_type(method)) == expected_return;
  if (il2cpp_method_get_param_count(method) != expected_parameters.size()) {
    matches = false;
  } else {
    uint32_t index = 0;
    for (const auto expected : expected_parameters) {
      if (il2cpp_type_name(il2cpp_method_get_param(method, index++)) != expected) {
        matches = false;
      }
    }
  }

  if (!matches) {
    health.record_signature_mismatch(descriptor, "expected " + std::string(expected_return) + " with "
                                                     + std::to_string(expected_parameters.size())
                                                     + " parameter(s); actual " + method_signature(method));
    return nullptr;
  }

  if (method->methodPointer == nullptr) {
    health.record_signature_mismatch(descriptor, "validated metadata has a null native method pointer");
    return nullptr;
  }

  return method;
}
} // namespace

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

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
 * Hooks the central service-response seam to intercept entity-group data,
 * hooks PrimeApp/GameServer for session credentials,
 * and spawns the two long-lived consumer threads:
 *  - ship_sync_data (main sync queue consumer)
 *  - ship_combat_log_data (combat log enrichment consumer)
 */
void InstallSyncPatches()
{
  HookModuleHealth hooks("SyncHooks");
  load_previously_sent_logs();

  for (const auto& descriptor : kReplacedBinaryHooks) {
    hooks.record_replaced(descriptor,
                          "client 253 removed the binary-container seam; central ProcessResultInternal owner");
  }
  hooks.record_replaced(
      kSlotContainerEntitySlots,
      "client 253 parameter is EntitySlotsData, not EntityGroup; central ProcessResultInternal owner");

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
    hooks.record_missing_helper(kModelRegistryProcessResult);
  } else if (const auto* method = find_exact_sync_method(
                 game_server_model_registry, kModelRegistryProcessResult, "System.Void",
                 {"Digit.Networking.Core.Parsing.IParsingContext", "Digit.PrimeServer.Models.ServiceResponse"},
                 hooks)) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kModelRegistryProcessResult, method->methodPointer,
                                     GameServerModelRegistry_ProcessResultInternal);
  }

  if (auto slot_assign_parser =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotAssignRtcParser");
      !slot_assign_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotAssignRtcParser");
    hooks.record_missing_helper(kSlotAssignRtc);
  } else if (const auto* method = find_exact_sync_method(
                 slot_assign_parser, kSlotAssignRtc, "Digit.Networking.Core.GSRTCModel",
                 {"Digit.Networking.Core.CentrifugoInfo", "Digit.Networking.RTC.RealtimeDataPayload", "IParsable"},
                 hooks)) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotAssignRtc, method->methodPointer, RtcParser_ParseFinalPayload);
  }

  if (auto slot_clear_parser =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotClearRtcParser");
      !slot_clear_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotClearRtcParser");
    hooks.record_missing_helper(kSlotClearRtc);
  } else if (const auto* method = find_exact_sync_method(
                 slot_clear_parser, kSlotClearRtc, "Digit.Networking.Core.GSRTCModel",
                 {"Digit.Networking.Core.CentrifugoInfo", "Digit.Networking.RTC.RealtimeDataPayload", "IParsable"},
                 hooks)) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSlotClearRtc, method->methodPointer, RtcParser_ParseFinalPayload);
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
    hooks.record_missing_helper(kPrimeAppInit);
  } else if (const auto* method =
                 find_exact_sync_method(prime_app, kPrimeAppInit, "System.Void",
                                        {"System.String", "System.String", "System.String", "System.String"}, hooks)) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kPrimeAppInit, method->methodPointer, PrimeApp_InitPrimeServer);
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
    } else if (const auto* method = find_exact_sync_method(
                   game_server, kGameServerInitialise, "System.Void",
                   {"System.String", "System.String", "System.Boolean", "System.String"}, hooks)) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kGameServerInitialise, method->methodPointer, GameServer_Initialise);
    }

    if (const auto* method =
            find_exact_sync_method(game_server, kGameServerInstance, "System.Void", {"System.Int32"}, hooks)) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kGameServerInstance, method->methodPointer,
                                       GameServer_SetInstanceIdHeader);
    }
  }

  hooks.log_summary();

  StartSyncSchedulerWorker();
  queue_mod_capability_snapshot();
  StartCombatLogWorker();
}
