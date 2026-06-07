/**
 * @file hostile_observation_fleet_data_hooks.cc
 * @brief Sidecar-local hostile observation from FleetDataSystem events.
 */
#include "patches/hostile_observation_fleet_data_hooks.h"

#include "config.h"
#include "patches/hook_registry.h"
#include "patches/hostile_observation_sidecar_sync.h"
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
#include "patches/live_debug_event_dispatcher.h"
#endif
#include "patches/live_debug_fleet_serializers.h"
#include "patches/system_view_session_tracker.h"

#include "prime/FleetDeployedData.h"
#include "prime/HullSpec.h"
#include "prime/IList.h"
#include "prime/NodeAddress.h"
#include "prime/UserProfile.h"

#include "str_utils.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>

namespace
{
using json = nlohmann::json;

constexpr size_t kMaxSeenSignatures = 4096;

constexpr HookDescriptor kOnFleetsAddedEventHook = {
    "FleetDataSystem.OnFleetsAddedEvent",
    "observe newly loaded hostile fleet data for sidecar-local cataloging",
    {"Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem", "OnFleetsAddedEvent"},
    "passive hostile sightings will be missing on initial system load",
};

constexpr HookDescriptor kOnFleetsUpdatedEventHook = {
    "FleetDataSystem.OnFleetsUpdatedEvent",
    "observe hostile fleet updates for sidecar-local cataloging",
    {"Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem", "OnFleetsUpdatedEvent"},
    "passive hostile sightings may not refresh when loaded fleets change",
};

constexpr HookDescriptor kOnFleetsEnterSystemEventHook = {
    "FleetDataSystem.OnFleetsEnterSystemEvent",
    "observe hostiles when the game reports fleets entering the visible system",
    {"Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem", "OnFleetsEnterSystemEvent"},
    "passive hostile sightings may not appear during system-entry transitions",
};

constexpr HookDescriptor kAddFleetHook = {
    "FleetDataSystem.AddFleet",
    "observe hostiles at the lower-level fleet insertion path",
    {"Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem", "AddFleet"},
    "passive hostile sightings may be missed if higher-level events are bypassed",
};

constexpr HookDescriptor kUpdateFleetHook = {
    "FleetDataSystem.UpdateFleet",
    "observe hostiles at the lower-level fleet update path",
    {"Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem", "UpdateFleet"},
    "passive hostile sightings may be stale if higher-level events are bypassed",
};

std::mutex                      g_signatures_mutex;
std::unordered_set<std::string> g_seen_signatures;
std::atomic_uint64_t            g_callback_count = 0;
std::atomic_uint64_t            g_hostile_count  = 0;
std::atomic_uint64_t            g_emitted_count  = 0;

bool hostile_observation_diagnostic_logging_enabled()
{ return AdvancedDiagnosticsSettings().logging; }

const char* fleet_type_name(const DeployedFleetType type)
{
  switch (type) {
    case DeployedFleetType::Nonexistent:
      return "Nonexistent";
    case DeployedFleetType::Player:
      return "Player";
    case DeployedFleetType::Marauder:
      return "Marauder";
    case DeployedFleetType::NpcInstantiated:
      return "NpcInstantiated";
    case DeployedFleetType::Sentinel:
      return "Sentinel";
    case DeployedFleetType::Alliance:
      return "Alliance";
    case DeployedFleetType::Challenge:
      return "Challenge";
  }

  return "Unexpected";
}

const char* hull_type_name(const HullType type)
{
  switch (type) {
    case HullType::Any:
      return "Any";
    case HullType::Destroyer:
      return "Destroyer";
    case HullType::Survey:
      return "Survey";
    case HullType::Explorer:
      return "Explorer";
    case HullType::Battleship:
      return "Battleship";
    case HullType::Defense:
      return "Defense";
    case HullType::ArmadaTarget:
      return "ArmadaTarget";
  }

  return "Unexpected";
}

bool hostile_fleet_type(const DeployedFleetType type)
{
  switch (type) {
    case DeployedFleetType::Marauder:
    case DeployedFleetType::NpcInstantiated:
    case DeployedFleetType::Sentinel:
    case DeployedFleetType::Challenge:
      return true;
    case DeployedFleetType::Nonexistent:
    case DeployedFleetType::Player:
    case DeployedFleetType::Alliance:
      return false;
  }

  return false;
}

std::string node_address_key(NodeAddress* address)
{
  if (!address) {
    return "0:0:0:0";
  }

  return std::to_string(address->Galaxy) + ":" + std::to_string(address->System) + ":" + std::to_string(address->Planet)
         + ":" + std::to_string(address->Instance);
}

bool remember_signature(std::string signature)
{
  if (signature.empty()) {
    return false;
  }

  std::scoped_lock lock(g_signatures_mutex);
  if (g_seen_signatures.size() >= kMaxSeenSignatures) {
    g_seen_signatures.clear();
    spdlog::warn("[HostileObservation] fleet-data signature cache cleared after {} entries", kMaxSeenSignatures);
  }

  return g_seen_signatures.emplace(std::move(signature)).second;
}

void append_address_fields(json& details, NodeAddress* address)
{
  if (!address) {
    return;
  }

  details["galaxyId"]   = address->Galaxy;
  details["systemId"]   = address->System;
  details["planetId"]   = address->Planet;
  details["instanceId"] = address->Instance;
}

std::string extract_user_id(FleetDeployedData* fleet, UserProfile* user)
{
  if (fleet && fleet->UserId) {
    return to_string(fleet->UserId);
  }

  if (user && user->UserId) {
    return to_string(user->UserId);
  }

  return {};
}

json build_fleet_sighting(const char* source_event, FleetDeployedData* fleet, NodeAddress* event_address)
{
  if (!fleet || !hostile_fleet_type(fleet->FleetType)) {
    return nullptr;
  }

  auto*      hull             = fleet->Hull;
  auto*      user             = fleet->User;
  auto*      address          = fleet->Address ? fleet->Address : event_address;
  const auto runtime_fleet_id = std::to_string(static_cast<int64_t>(fleet->ID));
  const auto hull_id          = hull ? static_cast<int64_t>(hull->Id) : 0;
  const auto user_id          = extract_user_id(fleet, user);
  const auto hull_name        = (hull && hull->Name) ? to_string(hull->Name) : std::string{};
  const auto user_name        = (user && user->Name) ? to_string(user->Name) : std::string{};
  const auto signature        = "fleet-data:" + runtime_fleet_id + ":" + std::to_string(hull_id) + ":" + user_id + ":"
                                + node_address_key(address) + ":" + std::to_string(fleet->CurrentState) + ":"
                                + (fleet->IsDestroyed ? "1" : "0");

  json details = {{"signature", signature},
                  {"sourceSurface", "fleet_data_system"},
                  {"sourceEvent", source_event},
                  {"confidence", "strong"},
                  {"runtimeFleetId", runtime_fleet_id},
                  {"fleetTypeValue", static_cast<int>(fleet->FleetType)},
                  {"fleetTypeName", fleet_type_name(fleet->FleetType)},
                  {"hullId", hull_id},
                  {"hullName", hull_name},
                  {"hullTypeValue", hull ? static_cast<int>(hull->Type) : -1},
                  {"hullTypeName", hull ? hull_type_name(hull->Type) : ""},
                  {"hullFactionValue", hull ? hull->Faction : -1},
                  {"hullGrade", hull ? hull->Grade : -1},
                  {"currentStateValue", fleet->CurrentState},
                  {"currentStateName", fleet_state_name_from_value(fleet->CurrentState)},
                  {"previousStateValue", fleet->PreviousState},
                  {"previousStateName", fleet_state_name_from_value(fleet->PreviousState)},
                  {"currentlyBattling", fleet->CurrentlyBattling},
                  {"isDestroyed", fleet->IsDestroyed},
                  {"needsHostileHighlight", fleet->NeedsHostileHighlight},
                  {"userId", user_id},
                  {"userName", user_name},
                  {"userLocaId", user ? static_cast<int64_t>(user->LocaId) : 0},
                  {"userLevel", user ? user->Level : -1}};

  append_address_fields(details, address);
  return details;
}

void emit_observed_hostile(const std::string& signature, json details)
{
  system_view_session_note_passive_observation(details);

  if (!remember_signature(signature)) {
    return;
  }

#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  live_debug_events::RecordEvent("observed-hostile", details);
#endif

  ++g_emitted_count;
  if (hostile_observation_diagnostic_logging_enabled()) {
    spdlog::info("[HostileObservation] source={} event={} confidence={} runtimeFleetId={} userId={} hullId={} "
                 "hullName='{}' systemId={} signature={}",
                 details.value("sourceSurface", std::string{}), details.value("sourceEvent", std::string{}),
                 details.value("confidence", std::string{}), details.value("runtimeFleetId", std::string{}),
                 details.value("userId", std::string{}), details.value("hullId", 0LL),
                 details.value("hullName", std::string{}), details.value("systemId", 0LL), signature);
  }

  hostile_observation_sidecar_emit(details);
}

void observe_fleet_list(const char* source_event, IList* fleets, NodeAddress* event_address = nullptr)
{
  ++g_callback_count;

  if (!fleets) {
    return;
  }

  const auto count = fleets->Count < 0 ? 0 : fleets->Count;
  for (int index = 0; index < count; ++index) {
    auto* fleet    = reinterpret_cast<FleetDeployedData*>(fleets->Get(index));
    auto  sighting = build_fleet_sighting(source_event, fleet, event_address);
    if (sighting.is_null()) {
      continue;
    }

    ++g_hostile_count;
    const auto signature = sighting.value("signature", std::string{});
    emit_observed_hostile(signature, std::move(sighting));
  }
}

void FleetDataSystem_OnFleetsAddedEvent_Hook(auto original, void* self, IList* fleets)
{
  original(self, fleets);
  observe_fleet_list("OnFleetsAddedEvent", fleets);
}

void FleetDataSystem_OnFleetsUpdatedEvent_Hook(auto original, void* self, IList* fleets)
{
  original(self, fleets);
  observe_fleet_list("OnFleetsUpdatedEvent", fleets);
}

void FleetDataSystem_OnFleetsEnterSystemEvent_Hook(auto original, void* self, NodeAddress* address, IList* fleets)
{
  original(self, address, fleets);
  observe_fleet_list("OnFleetsEnterSystemEvent", fleets, address);
}

void FleetDataSystem_AddFleet_Hook(auto original, void* self, FleetDeployedData* fleet)
{
  original(self, fleet);
  ++g_callback_count;
  auto sighting = build_fleet_sighting("AddFleet", fleet, nullptr);
  if (sighting.is_null()) {
    return;
  }

  ++g_hostile_count;
  const auto signature = sighting.value("signature", std::string{});
  emit_observed_hostile(signature, std::move(sighting));
}

void FleetDataSystem_UpdateFleet_Hook(auto original, void* self, FleetDeployedData* fleet)
{
  original(self, fleet);
  ++g_callback_count;
  auto sighting = build_fleet_sighting("UpdateFleet", fleet, nullptr);
  if (sighting.is_null()) {
    return;
  }

  ++g_hostile_count;
  const auto signature = sighting.value("signature", std::string{});
  emit_observed_hostile(signature, std::move(sighting));
}
} // namespace

bool hostile_observation_fleet_data_enabled()
{ return SidecarProbesSettings().hostile_observation; }

nlohmann::json hostile_observation_fleet_data_state()
{
  size_t signature_count = 0;
  {
    std::scoped_lock lock(g_signatures_mutex);
    signature_count = g_seen_signatures.size();
  }

  return json{{"enabled", hostile_observation_fleet_data_enabled()},
              {"sidecarTransportReady", hostile_observation_sidecar_delivery_enabled()},
              {"callbackCount", g_callback_count.load()},
              {"hostileCandidateCount", g_hostile_count.load()},
              {"emittedCount", g_emitted_count.load()},
              {"signatureCount", signature_count}};
}

void InstallHostileObservationFleetDataHooks()
{
  HookModuleHealth hooks("HostileObservationFleetDataHooks");
  if (!hostile_observation_fleet_data_enabled()) {
    hooks.record_skipped(kOnFleetsAddedEventHook, "sidecar hostile observation disabled");
    hooks.record_skipped(kOnFleetsUpdatedEventHook, "sidecar hostile observation disabled");
    hooks.record_skipped(kOnFleetsEnterSystemEventHook, "sidecar hostile observation disabled");
    hooks.record_skipped(kAddFleetHook, "sidecar hostile observation disabled");
    hooks.record_skipped(kUpdateFleetHook, "sidecar hostile observation disabled");
    hooks.log_summary();
    return;
  }

  auto fleet_data_system_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", "FleetDataSystem");
  if (!fleet_data_system_helper.isValidHelper()) {
    hooks.record_missing_helper(kOnFleetsAddedEventHook);
    hooks.record_missing_helper(kOnFleetsUpdatedEventHook);
    hooks.record_missing_helper(kOnFleetsEnterSystemEventHook);
    hooks.record_missing_helper(kAddFleetHook);
    hooks.record_missing_helper(kUpdateFleetHook);
    hooks.log_summary();
    return;
  }

  auto on_fleets_added_event = fleet_data_system_helper.GetMethod("OnFleetsAddedEvent");
  if (on_fleets_added_event == nullptr) {
    hooks.record_missing_method(kOnFleetsAddedEventHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kOnFleetsAddedEventHook, on_fleets_added_event,
                                     FleetDataSystem_OnFleetsAddedEvent_Hook);
  }

  auto on_fleets_updated_event = fleet_data_system_helper.GetMethod("OnFleetsUpdatedEvent");
  if (on_fleets_updated_event == nullptr) {
    hooks.record_missing_method(kOnFleetsUpdatedEventHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kOnFleetsUpdatedEventHook, on_fleets_updated_event,
                                     FleetDataSystem_OnFleetsUpdatedEvent_Hook);
  }

  auto on_fleets_enter_system_event = fleet_data_system_helper.GetMethod("OnFleetsEnterSystemEvent");
  if (on_fleets_enter_system_event == nullptr) {
    hooks.record_missing_method(kOnFleetsEnterSystemEventHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kOnFleetsEnterSystemEventHook, on_fleets_enter_system_event,
                                     FleetDataSystem_OnFleetsEnterSystemEvent_Hook);
  }

  auto add_fleet = fleet_data_system_helper.GetMethod("AddFleet");
  if (add_fleet == nullptr) {
    hooks.record_missing_method(kAddFleetHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kAddFleetHook, add_fleet, FleetDataSystem_AddFleet_Hook);
  }

  auto update_fleet = fleet_data_system_helper.GetMethod("UpdateFleet");
  if (update_fleet == nullptr) {
    hooks.record_missing_method(kUpdateFleetHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kUpdateFleetHook, update_fleet, FleetDataSystem_UpdateFleet_Hook);
  }

  hooks.log_summary();
}
