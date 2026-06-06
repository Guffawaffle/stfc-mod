/**
 * @file hostile_observation_probe.cc
 * @brief Opt-in sidecar-local hostile observation probe.
 */
#include "patches/hostile_observation_probe.h"

#include "config.h"
#include "patches/live_debug_event_dispatcher.h"
#include "patches/object_tracker_state.h"

#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"
#include "prime/VisibilityController.h"

#include "str_utils.h"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace
{
using Clock = std::chrono::steady_clock;

constexpr auto   kProbeInterval     = std::chrono::milliseconds(250);
constexpr size_t kMaxSeenSignatures = 4096;

Clock::time_point               g_last_probe_at{};
bool                            g_probe_started = false;
std::unordered_set<std::string> g_seen_signatures;

std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}

bool visible_or_showing(VisibilityController* controller)
{
  return controller && (controller->_state == VisibilityState::Visible || controller->_state == VisibilityState::Show);
}

const char* visibility_state_name(VisibilityState state)
{
  switch (state) {
    case VisibilityState::Unknown:
      return "Unknown";
    case VisibilityState::Show:
      return "Show";
    case VisibilityState::Hide:
      return "Hide";
    case VisibilityState::Hidden:
      return "Hidden";
    case VisibilityState::Visible:
      return "Visible";
  }

  return "Unexpected";
}

const char* fleet_type_name(DeployedFleetType type)
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
  }

  return "Unexpected";
}

const char* hull_type_name(HullType type)
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

bool hostile_fleet_type(DeployedFleetType type)
{
  switch (type) {
    case DeployedFleetType::Marauder:
    case DeployedFleetType::NpcInstantiated:
    case DeployedFleetType::Sentinel:
      return true;
    case DeployedFleetType::Nonexistent:
    case DeployedFleetType::Player:
    case DeployedFleetType::Alliance:
      return false;
  }

  return false;
}

bool hostile_navigation_candidate(NavigationInteractionUIContext* context)
{
  if (!context) {
    return false;
  }

  if (context->IsMarauder) {
    return true;
  }

  return context->ValidNavigationInput && context->ShowSetCourseArm && context->ThreatLevel >= 0;
}

bool should_probe_now()
{
  const auto now = Clock::now();
  if (!g_probe_started) {
    g_probe_started = true;
    g_last_probe_at = now;
    return true;
  }

  if ((now - g_last_probe_at) < kProbeInterval) {
    return false;
  }

  g_last_probe_at = now;
  return true;
}

bool remember_signature(std::string signature)
{
  if (signature.empty()) {
    return false;
  }

  if (g_seen_signatures.size() >= kMaxSeenSignatures) {
    g_seen_signatures.clear();
    spdlog::warn("[HostileObservation] signature cache cleared after {} entries", kMaxSeenSignatures);
  }

  return g_seen_signatures.emplace(std::move(signature)).second;
}

void emit_observed_hostile(const std::string& signature, nlohmann::json details)
{
  if (!remember_signature(signature)) {
    return;
  }

  live_debug_events::RecordEvent("observed-hostile", details);

  const auto source_surface          = details.value("sourceSurface", std::string{});
  const auto confidence              = details.value("confidence", std::string{});
  const auto runtime_fleet_id        = details.value("runtimeFleetId", std::string{});
  const auto hull_id                 = details.value("hullId", 0LL);
  const auto hull_name               = details.value("hullName", std::string{});
  const auto threat_level            = details.value("threatLevel", -1);
  const auto location_translation_id = details.value("locationTranslationId", 0LL);

  spdlog::info("[HostileObservation] source={} confidence={} runtimeFleetId={} hullId={} hullName='{}' threatLevel={} "
               "locationTranslationId={} signature={}",
               source_surface, confidence, runtime_fleet_id, hull_id, hull_name, threat_level, location_translation_id,
               signature);
}

void observe_visible_prescan_targets()
{
  const auto widgets = GetTrackedObjects<PreScanTargetWidget>();
  for (auto* widget : widgets) {
    if (!widget) {
      continue;
    }

    auto* visibility = widget->_visibilityController;
    if (!visible_or_showing(visibility)) {
      continue;
    }

    auto* battle_target = widget->_battleTargetData;
    auto* deployed      = battle_target ? battle_target->TargetFleetDeployedData : nullptr;
    if (!deployed || !hostile_fleet_type(deployed->FleetType)) {
      continue;
    }

    auto*      hull             = deployed->Hull;
    const auto runtime_fleet_id = std::to_string(static_cast<int64_t>(deployed->ID));
    const auto hull_id          = hull ? static_cast<int64_t>(hull->Id) : 0;
    const auto hull_name        = (hull && hull->Name) ? to_string(hull->Name) : std::string{};
    const auto signature =
        "prescan:" + runtime_fleet_id + ":" + std::to_string(hull_id) + ":" + std::to_string((int)deployed->FleetType);

    emit_observed_hostile(
        signature,
        nlohmann::json{{"sourceSurface", "prescan_target_widget"},
                       {"confidence", "strong"},
                       {"widgetPointer", pointer_to_string(widget)},
                       {"runtimeFleetId", runtime_fleet_id},
                       {"fleetTypeValue", static_cast<int>(deployed->FleetType)},
                       {"fleetTypeName", fleet_type_name(deployed->FleetType)},
                       {"hullId", hull_id},
                       {"hullName", hull_name},
                       {"hullTypeValue", hull ? static_cast<int>(hull->Type) : -1},
                       {"hullTypeName", hull ? hull_type_name(hull->Type) : ""},
                       {"hasAddToQueueButton", widget->_addToQueueButtonWidget != nullptr},
                       {"hasScanEngageButtons", widget->_scanEngageButtonsWidget != nullptr},
                       {"hasRewardsButton", widget->_rewardsButtonWidget != nullptr},
                       {"visibilityStateValue", visibility ? static_cast<int>(visibility->_state) : -1},
                       {"visibilityStateName", visibility ? visibility_state_name(visibility->_state) : ""}});
  }
}

void observe_navigation_candidates()
{
  const auto controllers = GetTrackedObjects<NavigationInteractionUIViewController>();
  for (auto* controller : controllers) {
    if (!controller) {
      continue;
    }

    auto* context = controller->CanvasContext;
    if (!hostile_navigation_candidate(context)) {
      continue;
    }

    const auto user_id     = context->UserId ? to_string(context->UserId) : std::string{};
    const auto poi_pointer = context->Poi ? pointer_to_string(context->Poi) : std::string{};
    const auto signature   = "nav:" + user_id + ":" + std::to_string(context->ThreatLevel) + ":"
                             + std::to_string(context->LocationTranslationId) + ":" + (context->IsMarauder ? "1" : "0")
                             + ":" + poi_pointer;

    emit_observed_hostile(signature,
                          nlohmann::json{{"sourceSurface", "navigation_interaction"},
                                         {"confidence", context->IsMarauder ? "candidate-high" : "candidate"},
                                         {"controllerPointer", pointer_to_string(controller)},
                                         {"contextDataState", context->ContextDataState},
                                         {"inputInteractionType", context->InputInteractionType},
                                         {"userId", user_id},
                                         {"isMarauder", context->IsMarauder},
                                         {"threatLevel", context->ThreatLevel},
                                         {"validNavigationInput", context->ValidNavigationInput},
                                         {"showSetCourseArm", context->ShowSetCourseArm},
                                         {"locationTranslationId", context->LocationTranslationId},
                                         {"poiPointer", poi_pointer}});
  }
}
} // namespace

bool hostile_observation_frame_subscriber_enabled()
{ return AdvancedDiagnosticsSettings().hostile_observation && Config::Get().installObjectTracker; }

void hostile_observation_tick(ScreenManager* screen_manager)
{
  (void)screen_manager;

  if (!hostile_observation_frame_subscriber_enabled() || !should_probe_now()) {
    return;
  }

  observe_visible_prescan_targets();
  observe_navigation_candidates();
}
