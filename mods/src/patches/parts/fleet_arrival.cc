/**
 * @file fleet_arrival.cc
 * @brief Fleet arrival detection driven by the bottom fleet bar state widgets.
 *
 * Hooks FleetStateWidget::SetWidgetData and watches FleetPlayerData state
 * transitions. The most useful arrival signal is Warping -> Impulsing, which
 * means the ship has dropped out of warp and entered the destination system.
 */
#include "config.h"
#include "errormsg.h"

#include <hook/hook.h>
#include <il2cpp/il2cpp_helper.h>

#include <patches/notification_service.h>
#include <prime/FleetPlayerData.h>
#include <prime/SpecManager.h>

#include <algorithm>
#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <cmath>
#include <sstream>
#include <string_view>
#include <unordered_map>

static std::unordered_map<uint64_t, FleetState> s_fleet_bar_states;
static std::unordered_map<uint64_t, std::string> s_fleet_bar_ship_names;
static std::unordered_map<uint64_t, std::string> s_fleet_bar_resource_names;
static std::unordered_map<uint64_t, float> s_fleet_bar_cargo_fill_levels;
static std::unordered_map<uint64_t, std::string> s_fleet_bar_resource_probe_signatures;

static std::string fleet_bar_ship_name(FleetPlayerData* fleet)
{
  auto* hull = fleet ? fleet->Hull : nullptr;
  auto  name = (hull && hull->Name) ? to_string(hull->Name) : std::string{"?"};

  constexpr std::string_view live_suffix = "_LIVE";
  if (name.size() >= live_suffix.size() &&
      name.compare(name.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
    name.erase(name.size() - live_suffix.size());
  }

  for (auto& ch : name) {
    if (ch == '_') {
      ch = ' ';
    }
  }

  return name;
}

static FleetPlayerData* fleet_bar_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  auto helper      = IL2CppClassHelper{((Il2CppObject*)self)->klass};
  auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

static std::string fleet_bar_cached_ship_name(uint64_t fleetId)
{
  auto it = s_fleet_bar_ship_names.find(fleetId);
  if (it == s_fleet_bar_ship_names.end()) {
    return {};
  }

  return it->second;
}

static std::string fleet_bar_cached_resource_name(uint64_t fleetId)
{
  auto it = s_fleet_bar_resource_names.find(fleetId);
  if (it == s_fleet_bar_resource_names.end()) {
    return {};
  }

  return it->second;
}

static float fleet_bar_cached_cargo_fill_level(uint64_t fleetId)
{
  auto it = s_fleet_bar_cargo_fill_levels.find(fleetId);
  if (it == s_fleet_bar_cargo_fill_levels.end()) {
    return -1.0f;
  }

  return it->second;
}

static std::string normalize_resource_name(const std::string& name)
{
  if (name.empty()) {
    return {};
  }

  auto normalized = name;
  constexpr std::string_view live_suffix = "_LIVE";
  if (normalized.size() >= live_suffix.size()
      && normalized.compare(normalized.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
    normalized.erase(normalized.size() - live_suffix.size());
  }

  for (auto& ch : normalized) {
    if (ch == '_') {
      ch = ' ';
    }
  }

  constexpr std::string_view resource_prefix = "Resource ";
  if (normalized.size() >= resource_prefix.size()
      && normalized.compare(0, resource_prefix.size(), resource_prefix) == 0) {
    normalized.erase(0, resource_prefix.size());
  }

  return normalized;
}

static void maybe_log_fleet_resource_probe(uint64_t fleetId, const char* phase, int64_t resourceId,
                                           int64_t pointResourceId, bool hasSpecManager, bool hasResourceSpec,
                                           bool hasRawName, const std::string& rawName,
                                           const std::string& normalizedName)
{
  std::ostringstream signature;
  signature << phase << "|resourceId=" << resourceId << "|pointResourceId=" << pointResourceId
            << "|hasSpecManager=" << hasSpecManager << "|hasResourceSpec=" << hasResourceSpec
            << "|hasRawName=" << hasRawName << "|rawNameLen=" << rawName.size()
            << "|normalizedLen=" << normalizedName.size() << "|rawName=" << rawName
            << "|normalizedName=" << normalizedName;

  auto newSignature = signature.str();
  auto existing     = s_fleet_bar_resource_probe_signatures.find(fleetId);
  if (existing != s_fleet_bar_resource_probe_signatures.end() && existing->second == newSignature) {
    return;
  }

  s_fleet_bar_resource_probe_signatures[fleetId] = newSignature;

  spdlog::info(
      "[FleetBar] RESOURCE_PROBE phase={} id={} resource_id={} point_resource_id={} spec_manager={} resource_spec={} raw_name_present={} raw_name_len={} normalized_len={} raw_name='{}' normalized='{}'",
      phase, fleetId, resourceId, pointResourceId, hasSpecManager, hasResourceSpec, hasRawName, rawName.size(),
      normalizedName.size(), rawName, normalizedName);
}

static std::string fleet_bar_resource_name(FleetPlayerData* fleet)
{
  auto fleetId = fleet ? fleet->Id : 0;
  auto* miningData = fleet ? fleet->MiningData : nullptr;
  if (!miningData) {
    return {};
  }

  const auto resourceId = miningData->ResourceId;
  auto* pointData       = miningData->PointData;
  const auto pointResourceId = pointData ? pointData->ResourceID : 0;
  auto* specManager = SpecManager::Instance();
  if (!specManager) {
    maybe_log_fleet_resource_probe(fleetId, "lookup", resourceId, pointResourceId, false, false, false, {}, {});
    return {};
  }

  auto* resourceSpec = specManager->GetResourceSpec(resourceId);
  auto* rawName      = resourceSpec ? resourceSpec->Name : nullptr;
  auto  rawNameText  = rawName ? to_string(rawName) : std::string{};
  auto  normalizedName = normalize_resource_name(rawNameText);

  maybe_log_fleet_resource_probe(fleetId, "lookup", resourceId, pointResourceId, true, resourceSpec != nullptr,
                                 rawName != nullptr, rawNameText, normalizedName);

  return normalizedName;
}

static std::string fleet_cargo_fill_text(float fillLevel)
{
  if (!std::isfinite(fillLevel) || fillLevel < 0.0f) {
    return {};
  }

  auto percent = static_cast<int>(std::lround(std::clamp(fillLevel, 0.0f, 1.0f) * 100.0f));
  return std::to_string(percent) + "% cargo";
}

static void maybe_notify_fleet_bar_transition(uint64_t fleetId, const std::string& shipName,
                                              FleetState oldState, FleetState newState,
                                              const std::string& resourceName,
                                              const std::string& cargoText)
{
  if (oldState == FleetState::Warping && newState == FleetState::Impulsing) {
    if (!Config::Get().notifications.fleet_arrived_in_system) {
      return;
    }

    auto body = "Your " + shipName + " has arrived in-system";
    spdlog::debug("[FleetBar] ARRIVED_IN_SYSTEM id={} ship='{}'", fleetId, shipName);
    notification_show("Fleet Arrived", body.c_str());
    return;
  }

  if (oldState != FleetState::Mining && newState == FleetState::Mining) {
    if (!Config::Get().notifications.fleet_started_mining) {
      return;
    }

    auto body = shipName + " mining";
    if (!resourceName.empty()) {
      body += " " + resourceName;
    }

    if (!cargoText.empty()) {
      body += ", " + cargoText;
    }

    spdlog::info("[FleetBar] STARTED_MINING id={} ship='{}' resource_len={} resource='{}' cargo='{}' body='{}'",
                 fleetId, shipName, resourceName.size(), resourceName, cargoText, body);
    notification_show("Mining Started", body.c_str());
    return;
  }

  if (oldState != FleetState::Docked && newState == FleetState::Docked) {
    if (!Config::Get().notifications.fleet_docked) {
      return;
    }

    auto body = "Your " + shipName + " docked";
    spdlog::debug("[FleetBar] DOCKED id={} ship='{}'", fleetId, shipName);
    notification_show("Fleet Docked", body.c_str());
    return;
  }
}

static void maybe_notify_fleet_node_depleted(int64_t fleetId)
{
  if (!Config::Get().notifications.fleet_node_depleted) {
    return;
  }

  auto shipName = fleet_bar_cached_ship_name(static_cast<uint64_t>(fleetId));
  auto resourceName = fleet_bar_cached_resource_name(static_cast<uint64_t>(fleetId));
  auto cargoText = fleet_cargo_fill_text(fleet_bar_cached_cargo_fill_level(static_cast<uint64_t>(fleetId)));

  spdlog::info("[Fleet] NODE_DEPLETED id={} ship='{}' cached_resource_len={} cached_resource='{}' cargo='{}'",
               fleetId, shipName.empty() ? "?" : shipName, resourceName.size(), resourceName, cargoText);

  // Prefer the explicit fleet observer hook over any generic toast path here.
  // The observer gives us a stable fleet id for enrichment, while the generic
  // standard-toast route is kept only as a fallback exploration path if this
  // game callback ever disappears in a future client build.
  if (!shipName.empty() && shipName != "?") {
    auto body = shipName + " depleted";
    if (!resourceName.empty()) {
      body += " " + resourceName;
    } else {
      body += " node";
    }
    if (!cargoText.empty()) {
      body += ", " + cargoText;
    }
    notification_show("Node Depleted", body.c_str());
    return;
  }

  auto body = std::string{"Fleet depleted"};
  if (!resourceName.empty()) {
    body += " " + resourceName;
  } else {
    body += " node";
  }
  if (!cargoText.empty()) {
    body += ", " + cargoText;
  }
  notification_show("Node Depleted", body.c_str());
}

typedef void (*FleetStateWidget_SetWidgetData_fn)(void*);
static FleetStateWidget_SetWidgetData_fn FleetStateWidget_SetWidgetData_original = nullptr;

typedef void (*ToastFleetObserver_HandleMiningDepleted_fn)(void*, int64_t);
static ToastFleetObserver_HandleMiningDepleted_fn ToastFleetObserver_HandleMiningDepleted_original = nullptr;

static void FleetStateWidget_SetWidgetData_Hook(void* self)
{
  auto* fleet = fleet_bar_widget_context(self);
  if (fleet) {
    auto fleetId      = fleet->Id;
    auto currentState = fleet->CurrentState;
    auto shipName     = fleet_bar_ship_name(fleet);
    auto resourceName = fleet_bar_resource_name(fleet);
    auto cargoFillLevel = fleet->CargoResourceFillLevel;
    auto cargoText = fleet_cargo_fill_text(cargoFillLevel);

    auto it = s_fleet_bar_states.find(fleetId);
    auto previousState = FleetState::Unknown;
    auto hadPreviousState = false;
    if (it != s_fleet_bar_states.end()) {
      previousState = it->second;
      hadPreviousState = true;
    }

    s_fleet_bar_states[fleetId] = currentState;
    s_fleet_bar_ship_names[fleetId] = shipName;
    if (!resourceName.empty()) {
      s_fleet_bar_resource_names[fleetId] = resourceName;
    }
    s_fleet_bar_cargo_fill_levels[fleetId] = cargoFillLevel;

    if (hadPreviousState && previousState != currentState) {
      maybe_notify_fleet_bar_transition(fleetId, shipName, previousState, currentState, resourceName, cargoText);
    }
  }

  FleetStateWidget_SetWidgetData_original(self);
}

static void ToastFleetObserver_HandleMiningDepleted_Hook(void* self, int64_t fleetId)
{
  ToastFleetObserver_HandleMiningDepleted_original(self, fleetId);
  maybe_notify_fleet_node_depleted(fleetId);
}

void InstallFleetArrivalHooks()
{
  notification_init();

  auto fleet_state_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
  if (!fleet_state_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.HUD", "FleetStateWidget");
    return;
  }

  auto set_widget_data = fleet_state_widget.GetMethod("SetWidgetData");
  if (set_widget_data == nullptr) {
    ErrorMsg::MissingMethod("FleetStateWidget", "SetWidgetData");
    return;
  }

  mh_install(set_widget_data, (void*)FleetStateWidget_SetWidgetData_Hook, (void**)&FleetStateWidget_SetWidgetData_original);

  auto toast_fleet_observer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver");
  if (!toast_fleet_observer.isValidHelper()) {
    spdlog::warn("[Fleet] ToastFleetObserver helper not found; node depleted notifications disabled");
    return;
  }

  auto handle_mining_depleted = toast_fleet_observer.GetMethod("HandleMiningDepleted");
  if (handle_mining_depleted == nullptr) {
    spdlog::warn("[Fleet] ToastFleetObserver.HandleMiningDepleted not found; node depleted notifications disabled");
    return;
  }

  mh_install(handle_mining_depleted, (void*)ToastFleetObserver_HandleMiningDepleted_Hook,
             (void**)&ToastFleetObserver_HandleMiningDepleted_original);
}