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
#include "str_utils.h"
#include "patches/sync_battle_logs.h"
#include "patches/sync_payload_builders.h"
#include "patches/sync_scheduler.h"
#include "patches/sync_transport.h"

#include <il2cpp-api-types.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/ServiceResponse.h>
#include <prime/RealtimeDataPayload.h>
#include <spud/detour.h>

#include <spdlog/spdlog.h>

#include <string>
#include <thread>

// Sync payload builders and entity-group dispatch live in sync_payload_builders.cc.

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

/**
 * @brief Hook: DataContainer::ParseBinaryObject
 *
 * Intercepts binary entity group parsing to extract data before the game processes it.
 * Original method: deserializes a protobuf entity group into the data container.
 * Our modification: calls HandleEntityGroup() first, then the original.
 */
void DataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
}

/**
 * @brief Hook: SlotDataContainer::ParseSlotUpdatedJson / ParseSlotRemovedJson
 *
 * Intercepts real-time slot update/removal notifications (RTC channel).
 * Original method: parses a JSON payload for slot changes.
 * Our modification: spawns process_entity_slots_rtc() on a detached thread.
 */
void DataContainer_ParseRtcPayload(auto original, void* _this, bool incrementalJsonParsing, RealtimeDataPayload* data)
{
  original(_this, incrementalJsonParsing, data);
  HandleRealtimeDataPayload(data);
}

/**
 * @brief Hook: GameServerModelRegistry::ProcessResultInternal
 *
 * Intercepts HTTP response processing to extract entity groups from the response.
 * Original method: processes the service response and invokes callbacks.
 * Our modification: iterates entity groups and calls HandleEntityGroup() before original.
 */
void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, HttpResponse* http_response,
                                                   ServiceResponse* service_response, void* callback,
                                                   void* callback_error)
{
  HandleServiceResponseEntityGroups(service_response);

  return original(_this, http_response, service_response, callback, callback_error);
}

/**
 * @brief Hook: GameServerModelRegistry::HandleBinaryObjects
 *
 * Intercepts bulk binary object handling to extract entity groups.
 * Same pattern as ProcessResultInternal but for binary-only responses.
 */
void GameServerModelRegistry_HandleBinaryObjects(auto original, void* _this, ServiceResponse* service_response)
{
  HandleServiceResponseEntityGroups(service_response);

  return original(_this, service_response);
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
  http::headers::instanceSessionId = to_string(to_wstring(sessionId));
  http::headers::gameServerUrl     = to_string(to_wstring(gameServerUrl));
}

/** @brief Hook: GameServer::Initialise — captures the game version string for HTTP headers. */
void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                           bool encryptRequests, Il2CppString* serverRegion)
{
  original(_this, sessionId, gameVersion, encryptRequests, serverRegion);
  http::headers::primeVersion = to_string(to_wstring(gameVersion));
}

/** @brief Hook: GameServer::SetInstanceIdHeader — captures the instance ID for HTTP headers. */
void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::instanceId = instanceId;
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
  load_previously_sent_logs();

  void* process_result_internal_target = nullptr;

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
  } else {
    auto ptr = game_server_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
      process_result_internal_target = ptr;
    }

    ptr = game_server_model_registry.GetMethod("HandleBinaryObjects");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegsitry", "HandleBinaryObjects");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_HandleBinaryObjects);
    }
  }

  if (auto platform_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
      !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (const auto ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInterval");
    } else if (ptr == process_result_internal_target) {
      spdlog::info("PlatformModelRegistry::ProcessResultInternal shares address with GameServerModelRegistry — already hooked");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }

  if (auto buff_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffDataContainer");
      !buff_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffDataContainer");
  } else {
    if (const auto ptr = buff_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto buff_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
      !buff_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    if (const auto ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto inventory_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                              "Digit.PrimeServer.Services", "InventoryDataContainer");
      !inventory_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
  } else {
    if (const auto ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
      !job_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobService");
  } else {
    if (const auto ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service_data_container = il2cpp_get_class_helper(
          "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobServiceDataContainer");
      !job_service_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobServiceDataContainer");
  } else {
    if (const auto ptr = job_service_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobServiceDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto missions_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
      !missions_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
  } else {
    if (const auto ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                             "Digit.PrimeServer.Services", "ResearchDataContainer");
      !research_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
  } else {
    if (const auto ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
      !research_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
  } else {
    if (const auto ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto slot_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SlotDataContainer");
      !slot_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "SlotDataContainer");
  } else {
    if (const auto ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotUpdatedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotUpdatedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotRemovedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotRemovedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    if (const auto ptr = prime_app.GetMethod("InitPrimeServer"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
    } else {
      SPUD_STATIC_DETOUR(ptr, PrimeApp_InitPrimeServer);
    }
  }

  if (auto game_server =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
      !game_server.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
  } else {
    if (const auto ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_Initialise);
    }

    if (const auto ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_SetInstanceIdHeader);
    }
  }

  std::thread(ship_sync_data).detach();
  std::thread(ship_combat_log_data).detach();
}
