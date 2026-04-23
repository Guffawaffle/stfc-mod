/**
 * @file live_debug.cc
 * @brief Main-thread file-backed live query channel.
 *
 * V1 keeps the surface deliberately small and read-only: AX writes a JSON
 * request file, the game consumes it from ScreenManager::Update, and writes a
 * JSON response file with basic runtime state.
 */
#include "patches/live_debug.h"

#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "patches/object_tracker_state.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetDeployedData.h"
#include "prime/FleetsManager.h"
#include "prime/FleetPlayerData.h"
#include "prime/IList.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/PreScanStationTargetWidget.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"
#include "prime/StarNodeObjectViewerWidget.h"
#include "str_utils.h"
#include "version.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {
using json = nlohmann::json;

constexpr std::string_view kRequestFile   = "community_patch_debug.cmd";
constexpr std::string_view kResponseFile  = "community_patch_debug.out";
constexpr std::string_view kTempSuffix    = ".tmp";
constexpr int              kFleetIndexMax = 10;
constexpr size_t           kRecentEventLimit = 256;
constexpr int64_t          kMineTimerBucketSeconds = 30;

struct TopCanvasObservation {
  bool        found = false;
  std::string pointer;
  std::string className;
  std::string classNamespace;
};

struct FleetObservation {
  bool        tracked = false;
  std::string pointer;
  int         selectedIndex = -1;
  bool        hasController = false;
  bool        hasFleet = false;
  uint64_t    fleetId = 0;
  int         currentState = -1;
  int         previousState = -1;
  int         cargoFillPercent = -1;
  int         cargoFillBasisPoints = -1;
  std::string hullName;
};

struct FleetSlotObservation {
  int         slotIndex = -1;
  bool        selected = false;
  bool        present = false;
  uint64_t    fleetId = 0;
  int         currentState = -1;
  int         previousState = -1;
  int         cargoFillPercent = -1;
  int         cargoFillBasisPoints = -1;
  std::string hullName;
};

struct MineViewerObservation {
  bool        miningViewerTracked = false;
  std::string miningPointer;
  bool        enabled = false;
  bool        isActiveAndEnabled = false;
  bool        isInfoShown = false;
  bool        hasParent = false;
  bool        parentIsShowing = false;
  int         occupiedState = -1;
  bool        hasScanEngageButtons = false;
  bool        hasTimer = false;
  int         timerState = -1;
  int         timerType = -1;
  int64_t     timerRemainingSeconds = -1;
  int64_t     timerRemainingBucket = -1;
  bool        starNodeViewerTracked = false;
  std::string starNodePointer;
  bool        starNodeEnabled = false;
  bool        starNodeActiveAndEnabled = false;
};

struct TargetViewerObservation {
  bool        preScanTargetTracked = false;
  std::string preScanTargetPointer;
  bool        preScanStationTargetTracked = false;
  std::string preScanStationTargetPointer;
  bool        celestialViewerTracked = false;
  std::string celestialViewerPointer;
};

std::deque<json> g_recent_events;
uint64_t         g_recent_event_sequence = 0;
bool             g_recent_observations_initialized = false;
TopCanvasObservation g_last_top_canvas;
FleetObservation     g_last_fleet;
std::array<FleetSlotObservation, kFleetIndexMax> g_last_fleet_slots;
MineViewerObservation g_last_mine_viewer;
TargetViewerObservation g_last_target_viewer;

std::filesystem::path get_live_debug_path(std::string_view filename)
{
  return std::filesystem::path(std::string(File::MakePath(filename)));
}

std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string get_request_id(const json& request)
{
  if (const auto id = request.find("id"); id != request.end() && id->is_string()) {
    return id->get<std::string>();
  }

  return "";
}

bool try_read_text_file(const std::filesystem::path& path, std::string& text)
{
  std::ifstream input(path, std::ios::in | std::ios::binary);
  if (!input.good()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

void remove_file_if_exists(const std::filesystem::path& path)
{
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

bool try_write_text_file_atomic(const std::filesystem::path& path, std::string_view text)
{
  const auto temp_path = std::filesystem::path(path.string() + std::string(kTempSuffix));
  remove_file_if_exists(temp_path);

  {
    std::ofstream output(temp_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.good()) {
      return false;
    }

    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    if (!output.good()) {
      return false;
    }
  }

  remove_file_if_exists(path);

  std::error_code rename_ec;
  std::filesystem::rename(temp_path, path, rename_ec);
  if (!rename_ec) {
    return true;
  }

  std::error_code copy_ec;
  std::filesystem::copy_file(temp_path, path, std::filesystem::copy_options::overwrite_existing, copy_ec);
  remove_file_if_exists(temp_path);
  return !copy_ec;
}

json make_error_response(const std::string& request_id, std::string_view error)
{
  return json{{"id", request_id}, {"ok", false}, {"error", error}};
}

json make_ok_response(const std::string& request_id, const json& result)
{
  return json{{"id", request_id}, {"ok", true}, {"result", result}};
}

void append_recent_event(std::string_view kind, json details)
{
  g_recent_events.push_back(
      json{{"seq", ++g_recent_event_sequence},
           {"timestampMsUtc", current_time_millis_utc()},
           {"kind", kind},
           {"details", std::move(details)}});

  while (g_recent_events.size() > kRecentEventLimit) {
    g_recent_events.pop_front();
  }
}

void reset_recent_events()
{
  g_recent_events.clear();
  g_recent_event_sequence = 0;
  g_recent_observations_initialized = false;
  g_last_top_canvas = {};
  g_last_fleet = {};
  g_last_fleet_slots = {};
  g_last_mine_viewer = {};
  g_last_target_viewer = {};
}

bool same_top_canvas_observation(const TopCanvasObservation& left, const TopCanvasObservation& right)
{
  return left.found == right.found && left.className == right.className &&
      left.classNamespace == right.classNamespace;
}

bool same_fleet_observation(const FleetObservation& left, const FleetObservation& right)
{
  return left.tracked == right.tracked && left.selectedIndex == right.selectedIndex &&
      left.hasController == right.hasController && left.hasFleet == right.hasFleet &&
      left.fleetId == right.fleetId && left.currentState == right.currentState &&
  left.previousState == right.previousState && left.cargoFillBasisPoints == right.cargoFillBasisPoints &&
      left.hullName == right.hullName;
}

bool same_fleet_slot_observation(const FleetSlotObservation& left, const FleetSlotObservation& right)
{
  return left.slotIndex == right.slotIndex && left.present == right.present &&
      left.fleetId == right.fleetId && left.currentState == right.currentState &&
      left.previousState == right.previousState && left.cargoFillBasisPoints == right.cargoFillBasisPoints &&
      left.hullName == right.hullName;
}

bool same_mine_viewer_observation(const MineViewerObservation& left, const MineViewerObservation& right)
{
  return left.miningViewerTracked == right.miningViewerTracked && left.enabled == right.enabled &&
      left.isActiveAndEnabled == right.isActiveAndEnabled && left.isInfoShown == right.isInfoShown &&
      left.hasParent == right.hasParent && left.parentIsShowing == right.parentIsShowing &&
      left.occupiedState == right.occupiedState && left.hasScanEngageButtons == right.hasScanEngageButtons &&
      left.hasTimer == right.hasTimer && left.timerState == right.timerState &&
      left.timerType == right.timerType && left.timerRemainingBucket == right.timerRemainingBucket &&
      left.starNodeViewerTracked == right.starNodeViewerTracked &&
      left.starNodeEnabled == right.starNodeEnabled &&
      left.starNodeActiveAndEnabled == right.starNodeActiveAndEnabled;
}

bool same_target_viewer_observation(const TargetViewerObservation& left, const TargetViewerObservation& right)
{
  return left.preScanTargetTracked == right.preScanTargetTracked &&
      left.preScanTargetPointer == right.preScanTargetPointer &&
      left.preScanStationTargetTracked == right.preScanStationTargetTracked &&
      left.preScanStationTargetPointer == right.preScanStationTargetPointer &&
      left.celestialViewerTracked == right.celestialViewerTracked &&
      left.celestialViewerPointer == right.celestialViewerPointer;
}

bool is_meaningful_mine_viewer_observation(const MineViewerObservation& observation)
{
  return (observation.miningViewerTracked &&
          (observation.isActiveAndEnabled || observation.parentIsShowing || observation.hasParent)) ||
      (observation.starNodeViewerTracked && observation.starNodeActiveAndEnabled);
}

TopCanvasObservation observe_top_canvas();
FleetObservation observe_fleetbar();
void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current);
void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current);

json top_canvas_observation_to_json(const TopCanvasObservation& observation)
{
  json result = {{"found", observation.found}};

  if (!observation.found) {
    return result;
  }

  result["pointer"] = observation.pointer;
  result["className"] = observation.className;
  result["classNamespace"] = observation.classNamespace;
  result["visibleOnlyHint"] = true;
  return result;
}

const char* fleet_state_name(FleetState state)
{
  switch (state) {
    case FleetState::Unknown:
      return "Unknown";
    case FleetState::IdleInSpace:
      return "IdleInSpace";
    case FleetState::Docked:
      return "Docked";
    case FleetState::Mining:
      return "Mining";
    case FleetState::Destroyed:
      return "Destroyed";
    case FleetState::TieringUp:
      return "TieringUp";
    case FleetState::Repairing:
      return "Repairing";
    case FleetState::CannotLaunch:
      return "CannotLaunch";
    case FleetState::Battling:
      return "Battling";
    case FleetState::WarpCharging:
      return "WarpCharging";
    case FleetState::Warping:
      return "Warping";
    case FleetState::CanRemove:
      return "CanRemove";
    case FleetState::CannotMove:
      return "CannotMove";
    case FleetState::Impulsing:
      return "Impulsing";
    case FleetState::CanManage:
      return "CanManage";
    case FleetState::Capturing:
      return "Capturing";
    case FleetState::CanRecall:
      return "CanRecall";
    case FleetState::CanEngage:
      return "CanEngage";
    case FleetState::Deployed:
      return "Deployed";
    case FleetState::CanLocate:
      return "CanLocate";
    default:
      return "Unmapped";
  }
}

const char* fleet_state_name_from_value(int state)
{
  if (state < 0) {
    return "None";
  }

  return fleet_state_name(static_cast<FleetState>(state));
}

json fleet_to_json(FleetPlayerData* fleet)
{
  json result = {{"present", fleet != nullptr}};

  if (!fleet) {
    return result;
  }

  result["id"]            = fleet->Id;
  result["currentState"]  = static_cast<int>(fleet->CurrentState);
  result["currentStateName"] = fleet_state_name(fleet->CurrentState);
  result["previousState"] = static_cast<int>(fleet->PreviousState);
  result["previousStateName"] = fleet_state_name(fleet->PreviousState);
  result["cargoFill"]     = fleet->CargoResourceFillLevel;

  if (auto hull = fleet->Hull; hull) {
    result["hull"] = {
        {"id", hull->Id},
        {"name", hull->Name ? to_string(hull->Name) : ""},
        {"type", static_cast<int>(hull->Type)},
    };
  } else {
    result["hull"] = nullptr;
  }

  return result;
}

json fleet_observation_to_json(const FleetObservation& observation)
{
  json result = {{"tracked", observation.tracked}};

  if (!observation.tracked) {
    return result;
  }

  result["pointer"] = observation.pointer;
  result["selectedIndex"] = observation.selectedIndex;
  result["hasController"] = observation.hasController;
  result["fleet"] = {{"present", observation.hasFleet}};

  if (observation.hasFleet) {
    result["fleet"]["id"] = observation.fleetId;
    result["fleet"]["currentState"] = observation.currentState;
    result["fleet"]["currentStateName"] = fleet_state_name(static_cast<FleetState>(observation.currentState));
    result["fleet"]["previousState"] = observation.previousState;
    result["fleet"]["previousStateName"] = fleet_state_name(static_cast<FleetState>(observation.previousState));
    result["fleet"]["cargoFillPercent"] = observation.cargoFillPercent;
    result["fleet"]["cargoFillBasisPoints"] = observation.cargoFillBasisPoints;
    result["fleet"]["hullName"] = observation.hullName;
  }

  return result;
}

FleetSlotObservation observe_fleet_slot(int slot_index, FleetBarViewController* fleet_bar)
{
  FleetSlotObservation observation;
  observation.slotIndex = slot_index;
  observation.selected = fleet_bar ? fleet_bar->IsIndexSelected(slot_index) : false;

  auto fleets_manager = FleetsManager::Instance();
  if (!fleets_manager) {
    return observation;
  }

  auto fleet = fleets_manager->GetFleetPlayerData(slot_index);
  if (!fleet) {
    return observation;
  }

  observation.present = true;
  observation.fleetId = fleet->Id;
  observation.currentState = static_cast<int>(fleet->CurrentState);
  observation.previousState = static_cast<int>(fleet->PreviousState);
  observation.cargoFillPercent = static_cast<int>(fleet->CargoResourceFillLevel * 100.0f);
  observation.cargoFillBasisPoints = static_cast<int>(fleet->CargoResourceFillLevel * 10000.0f);

  if (auto hull = fleet->Hull; hull && hull->Name) {
    observation.hullName = to_string(hull->Name);
  }

  return observation;
}

json fleet_slot_observation_to_json(const FleetSlotObservation& observation)
{
  json result = {{"slotIndex", observation.slotIndex},
                 {"selected", observation.selected},
                 {"present", observation.present}};

  if (!observation.present) {
    return result;
  }

  result["fleetId"] = observation.fleetId;
  result["currentState"] = observation.currentState;
  result["currentStateName"] = fleet_state_name_from_value(observation.currentState);
  result["previousState"] = observation.previousState;
  result["previousStateName"] = fleet_state_name_from_value(observation.previousState);
  result["cargoFillPercent"] = observation.cargoFillPercent;
  result["cargoFillBasisPoints"] = observation.cargoFillBasisPoints;
  result["hullName"] = observation.hullName;
  return result;
}

json fleet_slots_to_json(const std::array<FleetSlotObservation, kFleetIndexMax>& observations)
{
  json result = json::array();
  for (const auto& observation : observations) {
    result.push_back(fleet_slot_observation_to_json(observation));
  }
  return result;
}

std::array<FleetSlotObservation, kFleetIndexMax> observe_fleet_slots()
{
  std::array<FleetSlotObservation, kFleetIndexMax> observations{};
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  for (int slot_index = 0; slot_index < kFleetIndexMax; ++slot_index) {
    auto& observation = observations[static_cast<size_t>(slot_index)];
    observation.slotIndex = slot_index;

    if (!fleet_bar) {
      continue;
    }

    observation = observe_fleet_slot(slot_index, fleet_bar);
  }

  return observations;
}

int count_list_items(IList* list)
{
  if (!list) {
    return 0;
  }

  return list->Count < 0 ? 0 : list->Count;
}

json deployed_fleet_to_json(FleetDeployedData* fleet)
{
  json result = {{"present", fleet != nullptr}};
  if (!fleet) {
    return result;
  }

  result["id"] = fleet->ID;
  result["fleetType"] = static_cast<int>(fleet->FleetType);

  if (auto hull = fleet->Hull; hull && hull->Name) {
    result["hullName"] = to_string(hull->Name);
  }

  return result;
}

json deployed_fleet_list_to_json(IList* fleets)
{
  json result = json::array();
  if (!fleets) {
    return result;
  }

  const auto count = count_list_items(fleets);
  for (int index = 0; index < count; ++index) {
    result.push_back(deployed_fleet_to_json(reinterpret_cast<FleetDeployedData*>(fleets->Get(index))));
  }

  return result;
}

void append_event_if_live_debug_enabled(std::string_view kind, json details)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  append_recent_event(kind, std::move(details));
}

void initialize_recent_model_observations(std::string_view source)
{
  if (g_recent_observations_initialized) {
    return;
  }

  const auto fleet = observe_fleetbar();
  const auto fleet_slots = observe_fleet_slots();

  g_last_fleet = fleet;
  g_last_fleet_slots = fleet_slots;
  g_recent_observations_initialized = true;

  append_recent_event("observer-ready",
                      json{{"source", source},
                           {"fleet", fleet_observation_to_json(fleet)},
                           {"fleetSlots", fleet_slots_to_json(fleet_slots)}});
}

void capture_recent_model_events(std::string_view source)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  if (!g_recent_observations_initialized) {
    initialize_recent_model_observations(source);
    return;
  }

  const auto fleet = observe_fleetbar();
  const auto fleet_slots = observe_fleet_slots();

  if (!same_fleet_observation(fleet, g_last_fleet)) {
    append_fleet_change_events(g_last_fleet, fleet);
  }
  g_last_fleet = fleet;

  for (size_t slot_index = 0; slot_index < fleet_slots.size(); ++slot_index) {
    if (!same_fleet_slot_observation(fleet_slots[slot_index], g_last_fleet_slots[slot_index])) {
      append_fleet_slot_change_events(g_last_fleet_slots[slot_index], fleet_slots[slot_index]);
    }
  }
  g_last_fleet_slots = fleet_slots;
}

const char* classify_fleet_transition_kind(const FleetObservation& previous, const FleetObservation& current)
{
  const auto from = previous.currentState;
  const auto to   = current.currentState;

  if (!previous.hasFleet && current.hasFleet) {
    return "selected-fleet-visible";
  }
  if (previous.hasFleet && !current.hasFleet) {
    return "selected-fleet-cleared";
  }
  if (from == static_cast<int>(FleetState::Docked) && to == static_cast<int>(FleetState::Repairing)) {
    return "fleet-repair-started";
  }
  if (from == static_cast<int>(FleetState::Repairing) && to == static_cast<int>(FleetState::Docked)) {
    return "fleet-repair-completed";
  }
  if (to == static_cast<int>(FleetState::Battling) && from != static_cast<int>(FleetState::Battling)) {
    return "fleet-combat-started";
  }
  if (from == static_cast<int>(FleetState::Battling) && to != static_cast<int>(FleetState::Battling)) {
    return "fleet-combat-ended";
  }
  if (to == static_cast<int>(FleetState::WarpCharging)) {
    return "fleet-warp-started";
  }
  if (to == static_cast<int>(FleetState::Warping)) {
    return "fleet-warp-engaged";
  }
  if (from == static_cast<int>(FleetState::Warping) && to == static_cast<int>(FleetState::Impulsing)) {
    return "fleet-arrived-in-system";
  }
  if (to == static_cast<int>(FleetState::Docked) && from != static_cast<int>(FleetState::Repairing)) {
    return "fleet-docked";
  }
  if (to == static_cast<int>(FleetState::Mining) && from != static_cast<int>(FleetState::Mining)) {
    return "fleet-mining-started";
  }
  if (from == static_cast<int>(FleetState::Mining) && to != static_cast<int>(FleetState::Mining)) {
    return "fleet-mining-stopped";
  }

  return "fleet-state-changed";
}

json fleet_transition_to_json(const FleetObservation& previous, const FleetObservation& current)
{
  return json{{"selectedIndex", current.selectedIndex},
              {"fleetId", current.fleetId},
              {"hullName", current.hullName},
              {"fromState", previous.currentState},
              {"fromStateName", fleet_state_name_from_value(previous.currentState)},
              {"toState", current.currentState},
              {"toStateName", fleet_state_name_from_value(current.currentState)},
              {"modelPreviousState", current.previousState},
              {"modelPreviousStateName", fleet_state_name_from_value(current.previousState)},
              {"cargoFillBasisPoints", current.cargoFillBasisPoints}};
}

void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current)
{
  const bool selection_changed =
      previous.selectedIndex != current.selectedIndex || previous.fleetId != current.fleetId;

  if (selection_changed) {
    append_recent_event(
        "selected-fleet-changed",
        json{{"from", fleet_observation_to_json(previous)}, {"to", fleet_observation_to_json(current)}});
  }
}

const char* classify_fleet_slot_transition_kind(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  const auto from = previous.currentState;
  const auto to = current.currentState;

  if (!previous.present && current.present) {
    return "fleet-slot-visible";
  }
  if (previous.present && !current.present) {
    return "fleet-slot-cleared";
  }
  if (from == static_cast<int>(FleetState::Docked) && to == static_cast<int>(FleetState::Repairing)) {
    return "fleet-slot-repair-started";
  }
  if (from == static_cast<int>(FleetState::Repairing) && to == static_cast<int>(FleetState::Docked)) {
    return "fleet-slot-repair-completed";
  }
  if (to == static_cast<int>(FleetState::Battling) && from != static_cast<int>(FleetState::Battling)) {
    return "fleet-slot-combat-started";
  }
  if (from == static_cast<int>(FleetState::Battling) && to != static_cast<int>(FleetState::Battling)) {
    return "fleet-slot-combat-ended";
  }
  if (to == static_cast<int>(FleetState::WarpCharging)) {
    return "fleet-slot-warp-started";
  }
  if (to == static_cast<int>(FleetState::Warping)) {
    return "fleet-slot-warp-engaged";
  }
  if (from == static_cast<int>(FleetState::Warping) && to == static_cast<int>(FleetState::Impulsing)) {
    return "fleet-slot-arrived-in-system";
  }
  if (to == static_cast<int>(FleetState::Docked) && from != static_cast<int>(FleetState::Repairing)) {
    return "fleet-slot-docked";
  }
  if (to == static_cast<int>(FleetState::Mining) && from != static_cast<int>(FleetState::Mining)) {
    return "fleet-slot-mining-started";
  }
  if (from == static_cast<int>(FleetState::Mining) && to != static_cast<int>(FleetState::Mining)) {
    return "fleet-slot-mining-stopped";
  }

  return "fleet-slot-state-changed";
}

json fleet_slot_transition_to_json(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  return json{{"slotIndex", current.slotIndex},
              {"selected", current.selected},
              {"fleetId", current.fleetId},
              {"hullName", current.hullName},
              {"fromState", previous.currentState},
              {"fromStateName", fleet_state_name_from_value(previous.currentState)},
              {"toState", current.currentState},
              {"toStateName", fleet_state_name_from_value(current.currentState)},
              {"modelPreviousState", current.previousState},
              {"modelPreviousStateName", fleet_state_name_from_value(current.previousState)},
              {"cargoFillBasisPoints", current.cargoFillBasisPoints}};
}

void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  bool emitted = false;
  const bool same_fleet = previous.present && current.present && previous.fleetId == current.fleetId;
  const bool fleet_changed = previous.present && current.present && previous.fleetId != current.fleetId;

  if (fleet_changed) {
    append_recent_event("fleet-slot-fleet-changed",
                        json{{"slotIndex", current.slotIndex},
                             {"from", fleet_slot_observation_to_json(previous)},
                             {"to", fleet_slot_observation_to_json(current)}});
    emitted = true;
  }

  if ((previous.present != current.present) || (same_fleet && previous.currentState != current.currentState)) {
    append_recent_event(classify_fleet_slot_transition_kind(previous, current),
                        fleet_slot_transition_to_json(previous, current));
    emitted = true;
  }

  if (same_fleet && previous.hullName != current.hullName) {
    append_recent_event("fleet-slot-hull-changed",
                        json{{"slotIndex", current.slotIndex},
                             {"selected", current.selected},
                             {"fleetId", current.fleetId},
                             {"fromHullName", previous.hullName},
                             {"toHullName", current.hullName},
                             {"state", current.currentState},
                             {"stateName", fleet_state_name_from_value(current.currentState)}});
    emitted = true;
  }

  if (same_fleet && current.cargoFillBasisPoints > previous.cargoFillBasisPoints) {
    append_recent_event("fleet-slot-cargo-gained",
                        json{{"slotIndex", current.slotIndex},
                             {"selected", current.selected},
                             {"fleetId", current.fleetId},
                             {"fromCargoFillBasisPoints", previous.cargoFillBasisPoints},
                             {"toCargoFillBasisPoints", current.cargoFillBasisPoints},
                             {"deltaCargoFillBasisPoints", current.cargoFillBasisPoints - previous.cargoFillBasisPoints},
                             {"state", current.currentState},
                             {"stateName", fleet_state_name_from_value(current.currentState)}});
    emitted = true;
  }

  if (!emitted) {
    append_recent_event("fleet-slot-changed", fleet_slot_observation_to_json(current));
  }
}

json handle_ping(const std::string& request_id)
{
  return make_ok_response(request_id, json{{"pong", true}, {"version", VER_PRODUCT_VERSION_STR}});
}

json handle_tracker_list(const std::string& request_id)
{
  const auto summaries = GetTrackedObjectSummary();

  json   classes = json::array();
  size_t total_objects = 0;
  for (const auto& summary : summaries) {
    total_objects += summary.count;
    const auto full_name = summary.classNamespace.empty()
        ? summary.className
        : summary.classNamespace + "." + summary.className;

    classes.push_back(json{{"classPointer", summary.classPointer},
                 {"classNamespace", summary.classNamespace},
                           {"className", summary.className},
                           {"fullName", full_name},
                           {"count", summary.count}});
  }

  return make_ok_response(request_id, json{{"trackedClassCount", summaries.size()},
                                           {"trackedObjectCount", total_objects},
                                           {"classes", std::move(classes)}});
}

json handle_top_canvas(const std::string& request_id)
{
  return make_ok_response(request_id, top_canvas_observation_to_json(observe_top_canvas()));
}

int get_selected_fleet_index(FleetBarViewController* fleet_bar)
{
  if (!fleet_bar) {
    return -1;
  }

  for (int index = 0; index < kFleetIndexMax; ++index) {
    if (fleet_bar->IsIndexSelected(index)) {
      return index;
    }
  }

  return -1;
}

json handle_fleetbar_state(const std::string& request_id)
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  json result = {{"tracked", fleet_bar != nullptr}};

  if (!fleet_bar) {
    return make_ok_response(request_id, result);
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;

  result["pointer"]       = pointer_to_string(fleet_bar);
  result["selectedIndex"] = get_selected_fleet_index(fleet_bar);
  result["hasController"] = fleet_controller != nullptr;
  result["fleet"]         = fleet_to_json(fleet);

  return make_ok_response(request_id, result);
}

json handle_fleet_slots_state(const std::string& request_id)
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  const auto slot_observations = observe_fleet_slots();
  size_t present_count = 0;
  for (const auto& slot : slot_observations) {
    if (slot.present) {
      ++present_count;
    }
  }

  return make_ok_response(request_id,
                          json{{"fleetBarTracked", fleet_bar != nullptr},
                               {"slotCount", kFleetIndexMax},
                               {"presentSlotCount", present_count},
                               {"slots", fleet_slots_to_json(slot_observations)}});
}

FleetObservation observe_fleetbar()
{
  FleetObservation observation;
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
  observation.tracked = fleet_bar != nullptr;

  if (!fleet_bar) {
    return observation;
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet = fleet_controller ? fleet_controller->fleet : nullptr;

  observation.pointer = pointer_to_string(fleet_bar);
  observation.selectedIndex = get_selected_fleet_index(fleet_bar);
  observation.hasController = fleet_controller != nullptr;
  observation.hasFleet = fleet != nullptr;

  if (fleet) {
    observation.fleetId = fleet->Id;
    observation.currentState = static_cast<int>(fleet->CurrentState);
    observation.previousState = static_cast<int>(fleet->PreviousState);
    observation.cargoFillPercent = static_cast<int>(fleet->CargoResourceFillLevel * 100.0f);
    observation.cargoFillBasisPoints = static_cast<int>(fleet->CargoResourceFillLevel * 10000.0f);

    if (auto hull = fleet->Hull; hull && hull->Name) {
      observation.hullName = to_string(hull->Name);
    }
  }

  return observation;
}

const char* occupied_state_name(OccupiedState state)
{
  switch (state) {
    case NotOccupied:
      return "NotOccupied";
    case LocalPlayerOccupied:
      return "LocalPlayerOccupied";
    case OtherPlayerOccupied:
      return "OtherPlayerOccupied";
    default:
      return "Unknown";
  }
}

json timer_context_to_json(TimerDataContext* timer)
{
  if (!timer) {
    return nullptr;
  }

  return json{{"remainingTicks", timer->RemainingTime.Ticks},
              {"remainingSeconds", timer->RemainingTime.TotalSeconds()},
              {"timerType", timer->TimerTypeValue},
              {"timerState", timer->TimerStateValue},
              {"showTimerLabel", timer->ShowTimerLabel}};
}

int64_t get_timer_remaining_bucket(TimerDataContext* timer)
{
  if (!timer) {
    return -1;
  }

  return static_cast<int64_t>(timer->RemainingTime.TotalSeconds()) / kMineTimerBucketSeconds;
}

json mining_viewer_to_json(MiningObjectViewerWidget* mining_viewer)
{
  if (!mining_viewer) {
    return nullptr;
  }

  auto parent = mining_viewer->Parent;

  return json{{"pointer", pointer_to_string(mining_viewer)},
              {"enabled", mining_viewer->enabled},
              {"isActiveAndEnabled", mining_viewer->isActiveAndEnabled},
              {"isInfoShown", mining_viewer->IsInfoShown},
              {"hasParent", parent != nullptr},
              {"parentIsShowing", parent ? parent->IsShowing : false},
              {"occupiedState", static_cast<int>(mining_viewer->_occupiedState)},
              {"occupiedStateName", occupied_state_name(mining_viewer->_occupiedState)},
              {"hasScanEngageButtons", mining_viewer->_scanEngageButtonsWidget != nullptr},
              {"timer", timer_context_to_json(mining_viewer->_miningTimerWidgetContext)}};
}

json star_node_viewer_to_json(StarNodeObjectViewerWidget* star_node_viewer)
{
  if (!star_node_viewer) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(star_node_viewer)},
              {"enabled", star_node_viewer->enabled},
              {"isActiveAndEnabled", star_node_viewer->isActiveAndEnabled}};
}

json prescan_target_to_json(PreScanTargetWidget* prescan_target)
{
  if (!prescan_target) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(prescan_target)}};
}

json celestial_viewer_to_json(CelestialObjectViewerWidget* celestial_viewer)
{
  if (!celestial_viewer) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(celestial_viewer)}};
}

TargetViewerObservation observe_target_viewer()
{
  TargetViewerObservation observation;
  auto prescan_target = GetLatestTrackedObject<PreScanTargetWidget>();
  auto prescan_station_target = GetLatestTrackedObject<PreScanStationTargetWidget>();
  auto celestial_viewer = GetLatestTrackedObject<CelestialObjectViewerWidget>();

  observation.preScanTargetTracked = prescan_target != nullptr;
  observation.preScanStationTargetTracked = prescan_station_target != nullptr;
  observation.celestialViewerTracked = celestial_viewer != nullptr;

  if (prescan_target) {
    observation.preScanTargetPointer = pointer_to_string(prescan_target);
  }
  if (prescan_station_target) {
    observation.preScanStationTargetPointer = pointer_to_string(prescan_station_target);
  }
  if (celestial_viewer) {
    observation.celestialViewerPointer = pointer_to_string(celestial_viewer);
  }

  return observation;
}

json target_viewer_observation_to_json(const TargetViewerObservation& observation)
{
  return json{{"preScanTargetTracked", observation.preScanTargetTracked},
              {"preScanTarget", observation.preScanTargetTracked
                                     ? json{{"pointer", observation.preScanTargetPointer}}
                                     : json(nullptr)},
              {"preScanStationTargetTracked", observation.preScanStationTargetTracked},
              {"preScanStationTarget", observation.preScanStationTargetTracked
                                            ? json{{"pointer", observation.preScanStationTargetPointer}}
                                            : json(nullptr)},
              {"celestialViewerTracked", observation.celestialViewerTracked},
              {"celestialViewer", observation.celestialViewerTracked
                                      ? json{{"pointer", observation.celestialViewerPointer}}
                                      : json(nullptr)}};
}

MineViewerObservation observe_mine_viewer()
{
  MineViewerObservation observation;
  auto mining_viewer = GetLatestTrackedObject<MiningObjectViewerWidget>();
  auto star_node_viewer = GetLatestTrackedObject<StarNodeObjectViewerWidget>();

  observation.miningViewerTracked = mining_viewer != nullptr;
  observation.starNodeViewerTracked = star_node_viewer != nullptr;

  if (mining_viewer) {
    auto timer = mining_viewer->_miningTimerWidgetContext;
    auto parent = mining_viewer->Parent;

    observation.miningPointer = pointer_to_string(mining_viewer);
    observation.enabled = mining_viewer->enabled;
    observation.isActiveAndEnabled = mining_viewer->isActiveAndEnabled;
    observation.isInfoShown = mining_viewer->IsInfoShown;
    observation.hasParent = parent != nullptr;
    observation.parentIsShowing = parent ? parent->IsShowing : false;
    observation.occupiedState = static_cast<int>(mining_viewer->_occupiedState);
    observation.hasScanEngageButtons = mining_viewer->_scanEngageButtonsWidget != nullptr;
    observation.hasTimer = timer != nullptr;

    if (timer) {
      observation.timerState = timer->TimerStateValue;
      observation.timerType = timer->TimerTypeValue;
      observation.timerRemainingSeconds = static_cast<int64_t>(timer->RemainingTime.TotalSeconds());
      observation.timerRemainingBucket = get_timer_remaining_bucket(timer);
    }
  }

  if (star_node_viewer) {
    observation.starNodePointer = pointer_to_string(star_node_viewer);
    observation.starNodeEnabled = star_node_viewer->enabled;
    observation.starNodeActiveAndEnabled = star_node_viewer->isActiveAndEnabled;
  }

  return observation;
}

json mine_viewer_observation_to_json(const MineViewerObservation& observation)
{
  json result = {{"miningViewerTracked", observation.miningViewerTracked},
                 {"starNodeViewerTracked", observation.starNodeViewerTracked}};

  if (observation.miningViewerTracked) {
    result["miningViewer"] = {{"pointer", observation.miningPointer},
                               {"enabled", observation.enabled},
                               {"isActiveAndEnabled", observation.isActiveAndEnabled},
                               {"isInfoShown", observation.isInfoShown},
                               {"hasParent", observation.hasParent},
                               {"parentIsShowing", observation.parentIsShowing},
                               {"occupiedState", observation.occupiedState},
                               {"occupiedStateName", occupied_state_name(static_cast<OccupiedState>(observation.occupiedState))},
                               {"hasScanEngageButtons", observation.hasScanEngageButtons},
                               {"timer", observation.hasTimer
                                             ? json{{"remainingSeconds", observation.timerRemainingSeconds},
                                                    {"remainingSecondsBucket", observation.timerRemainingBucket},
                                                    {"timerState", observation.timerState},
                                                    {"timerType", observation.timerType}}
                                             : json(nullptr)}};
  } else {
    result["miningViewer"] = nullptr;
  }

  if (observation.starNodeViewerTracked) {
    result["starNodeViewer"] = {{"pointer", observation.starNodePointer},
                                 {"enabled", observation.starNodeEnabled},
                                 {"isActiveAndEnabled", observation.starNodeActiveAndEnabled}};
  } else {
    result["starNodeViewer"] = nullptr;
  }

  return result;
}

TopCanvasObservation observe_top_canvas()
{
  TopCanvasObservation observation;
  auto top_canvas = ScreenManager::GetTopCanvas(true);
  observation.found = top_canvas != nullptr;

  if (!top_canvas) {
    return observation;
  }

  const auto canvas_object = reinterpret_cast<Il2CppObject*>(top_canvas);
  const auto canvas_class = canvas_object ? canvas_object->klass : nullptr;

  observation.pointer = pointer_to_string(top_canvas);
  observation.className = (canvas_class && canvas_class->name) ? canvas_class->name : "";
  observation.classNamespace = (canvas_class && canvas_class->namespaze) ? canvas_class->namespaze : "";
  return observation;
}

void DeploymentEvents_TriggerFleetStateChangeEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  append_event_if_live_debug_enabled(
      "deployment-fleet-state-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-fleet-state-event");
}

void DeploymentEvents_TriggerPlayerFleetsUpdatedEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  capture_recent_model_events("deployment-player-fleets-updated-event");
}

void DeploymentEvents_TriggerCoursePlannedEvent_Hook(auto original, IList* courses)
{
  original(courses);
  append_event_if_live_debug_enabled("deployment-course-planned-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-planned-event");
}

void DeploymentEvents_TriggerCourseStartEvent_Hook(auto original, IList* courses)
{
  original(courses);
  append_event_if_live_debug_enabled("deployment-course-start-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-start-event");
}

void DeploymentEvents_TriggerCourseChangeEvent_Hook(auto original, IList* old_courses, IList* new_courses)
{
  original(old_courses, new_courses);
  append_event_if_live_debug_enabled("deployment-course-change-event",
                                     json{{"oldCourseCount", count_list_items(old_courses)},
                                          {"newCourseCount", count_list_items(new_courses)}});
  capture_recent_model_events("deployment-course-change-event");
}

void DeploymentEvents_TriggerCourseEndEvent_Hook(auto original, IList* courses)
{
  original(courses);
  append_event_if_live_debug_enabled("deployment-course-end-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-end-event");
}

void DeploymentEvents_TriggerSetCourseResponseEvent_Hook(auto original, long fleet_id, bool success,
                                                         bool is_recall_course, void* planned_course_data)
{
  original(fleet_id, success, is_recall_course, planned_course_data);
  append_event_if_live_debug_enabled("deployment-set-course-response-event",
                                     json{{"fleetId", fleet_id},
                                          {"success", success},
                                          {"isRecallCourse", is_recall_course},
                                          {"hasCourseData", planned_course_data != nullptr}});
  capture_recent_model_events("deployment-set-course-response-event");
}

void DeploymentEvents_TriggerBattleStartEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  append_event_if_live_debug_enabled(
      "deployment-battle-start-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-battle-start-event");
}

void DeploymentEvents_TriggerBattleEndEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  append_event_if_live_debug_enabled(
      "deployment-battle-end-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-battle-end-event");
}

void DeploymentEvents_TriggerStaleFleetDataDetected_Hook(auto original)
{
  original();
  append_event_if_live_debug_enabled("deployment-stale-fleet-data-detected-event", json::object());
  capture_recent_model_events("deployment-stale-fleet-data-detected-event");
}

json handle_mine_viewer_state(const std::string& request_id)
{
  auto mining_viewer    = GetLatestTrackedObject<MiningObjectViewerWidget>();
  auto star_node_viewer = GetLatestTrackedObject<StarNodeObjectViewerWidget>();

  json result = {{"miningViewerTracked", mining_viewer != nullptr},
                 {"starNodeViewerTracked", star_node_viewer != nullptr}};

  result["miningViewer"]   = mining_viewer_to_json(mining_viewer);
  result["starNodeViewer"] = star_node_viewer_to_json(star_node_viewer);

  return make_ok_response(request_id, result);
}

json handle_target_viewer_state(const std::string& request_id)
{
  auto prescan_target = GetLatestTrackedObject<PreScanTargetWidget>();
  auto prescan_station_target = GetLatestTrackedObject<PreScanStationTargetWidget>();
  auto celestial_viewer = GetLatestTrackedObject<CelestialObjectViewerWidget>();

  return make_ok_response(
      request_id,
      json{{"preScanTargetTracked", prescan_target != nullptr},
           {"preScanStationTargetTracked", prescan_station_target != nullptr},
           {"celestialViewerTracked", celestial_viewer != nullptr},
           {"preScanTarget", prescan_target_to_json(prescan_target)},
           {"preScanStationTarget", prescan_target_to_json(prescan_station_target)},
           {"celestialViewer", celestial_viewer_to_json(celestial_viewer)}});
}

json handle_recent_events(const std::string& request_id)
{
  json events = json::array();
  json kind_counts = json::object();

  for (const auto& event : g_recent_events) {
    events.push_back(event);

    const auto kind_it = event.find("kind");
    if (kind_it == event.end() || !kind_it->is_string()) {
      continue;
    }

    const auto kind = kind_it->get<std::string>();
    const auto existing = kind_counts.find(kind);
    if (existing == kind_counts.end()) {
      kind_counts[kind] = 1;
    } else {
      *existing = existing->get<int>() + 1;
    }
  }

  return make_ok_response(request_id,
                          json{{"count", g_recent_events.size()},
                               {"capacity", kRecentEventLimit},
                               {"kindCounts", std::move(kind_counts)},
                               {"events", std::move(events)}});
}

json handle_clear_recent_events(const std::string& request_id)
{
  const auto cleared = g_recent_events.size();
  reset_recent_events();
  return make_ok_response(request_id, json{{"cleared", cleared}});
}

json execute_live_debug_command(const json& request)
{
  const auto request_id = get_request_id(request);
  const auto cmd_it     = request.find("cmd");
  if (cmd_it == request.end() || !cmd_it->is_string()) {
    return make_error_response(request_id, "request must contain string field 'cmd'");
  }

  const auto cmd = cmd_it->get<std::string>();

  try {
    if (cmd == "ping") {
      return handle_ping(request_id);
    }
    if (cmd == "tracker-list") {
      return handle_tracker_list(request_id);
    }
    if (cmd == "top-canvas") {
      return handle_top_canvas(request_id);
    }
    if (cmd == "fleetbar-state") {
      return handle_fleetbar_state(request_id);
    }
    if (cmd == "fleet-slots-state") {
      return handle_fleet_slots_state(request_id);
    }
    if (cmd == "mine-viewer-state") {
      return handle_mine_viewer_state(request_id);
    }
    if (cmd == "target-viewer-state") {
      return handle_target_viewer_state(request_id);
    }
    if (cmd == "recent-events") {
      return handle_recent_events(request_id);
    }
    if (cmd == "clear-recent-events") {
      return handle_clear_recent_events(request_id);
    }

    return make_error_response(request_id, "unknown command");
  } catch (const std::exception& ex) {
    spdlog::warn("live_debug command '{}' failed: {}", cmd, ex.what());
    return make_error_response(request_id, ex.what());
  } catch (...) {
    spdlog::warn("live_debug command '{}' failed with unknown exception", cmd);
    return make_error_response(request_id, "command failed");
  }
}
} // namespace

void InstallLiveDebugHooks()
{
  auto deployment_events_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Events", "DeploymentEvents");
  if (!deployment_events_helper.isValidHelper()) {
    deployment_events_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.PrimeServer.Events", "DeploymentEvents");
  }
  if (!deployment_events_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.PrimeServer.Events", "DeploymentEvents");
    return;
  }

  auto trigger_fleet_state_change_event = deployment_events_helper.GetMethod("TriggerFleetStateChangeEvent");
  if (trigger_fleet_state_change_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerFleetStateChangeEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_fleet_state_change_event, DeploymentEvents_TriggerFleetStateChangeEvent_Hook);
  }

  auto trigger_player_fleets_updated_event = deployment_events_helper.GetMethod("TriggerPlayerFleetsUpdatedEvent");
  if (trigger_player_fleets_updated_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerPlayerFleetsUpdatedEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_player_fleets_updated_event, DeploymentEvents_TriggerPlayerFleetsUpdatedEvent_Hook);
  }

  auto trigger_course_planned_event = deployment_events_helper.GetMethod("TriggerCoursePlannedEvent");
  if (trigger_course_planned_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCoursePlannedEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_planned_event, DeploymentEvents_TriggerCoursePlannedEvent_Hook);
  }

  auto trigger_course_start_event = deployment_events_helper.GetMethod("TriggerCourseStartEvent");
  if (trigger_course_start_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseStartEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_start_event, DeploymentEvents_TriggerCourseStartEvent_Hook);
  }

  auto trigger_course_change_event = deployment_events_helper.GetMethod("TriggerCourseChangeEvent");
  if (trigger_course_change_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseChangeEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_change_event, DeploymentEvents_TriggerCourseChangeEvent_Hook);
  }

  auto trigger_course_end_event = deployment_events_helper.GetMethod("TriggerCourseEndEvent");
  if (trigger_course_end_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseEndEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_end_event, DeploymentEvents_TriggerCourseEndEvent_Hook);
  }

  auto trigger_set_course_response_event = deployment_events_helper.GetMethod("TriggerSetCourseResponseEvent");
  if (trigger_set_course_response_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerSetCourseResponseEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_set_course_response_event, DeploymentEvents_TriggerSetCourseResponseEvent_Hook);
  }

  auto trigger_battle_start_event = deployment_events_helper.GetMethod("TriggerBattleStartEvent");
  if (trigger_battle_start_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerBattleStartEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_battle_start_event, DeploymentEvents_TriggerBattleStartEvent_Hook);
  }

  auto trigger_battle_end_event = deployment_events_helper.GetMethod("TriggerBattleEndEvent");
  if (trigger_battle_end_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerBattleEndEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_battle_end_event, DeploymentEvents_TriggerBattleEndEvent_Hook);
  }

  auto trigger_stale_fleet_data_detected = deployment_events_helper.GetMethod("TriggerStateFleetDataDetected");
  if (trigger_stale_fleet_data_detected == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerStateFleetDataDetected");
  } else {
    SPUD_STATIC_DETOUR(trigger_stale_fleet_data_detected, DeploymentEvents_TriggerStaleFleetDataDetected_Hook);
  }
}

void live_debug_tick(ScreenManager*)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  const auto request_path = get_live_debug_path(kRequestFile);
  if (!std::filesystem::exists(request_path)) {
    return;
  }

  std::string request_text;
  if (!try_read_text_file(request_path, request_text)) {
    return;
  }

  remove_file_if_exists(request_path);

  json response;
  try {
    auto request = json::parse(request_text);
    if (!request.is_object()) {
      response = make_error_response("", "request must be a JSON object");
    } else {
      response = execute_live_debug_command(request);
    }
  } catch (const json::exception& ex) {
    spdlog::warn("live_debug rejected malformed JSON: {}", ex.what());
    response = make_error_response("", std::string("invalid JSON: ") + ex.what());
  } catch (const std::exception& ex) {
    spdlog::warn("live_debug failed to process request: {}", ex.what());
    response = make_error_response("", ex.what());
  } catch (...) {
    spdlog::warn("live_debug failed to process request: unknown exception");
    response = make_error_response("", "request handling failed");
  }

  const auto response_path = get_live_debug_path(kResponseFile);
  if (!try_write_text_file_atomic(response_path, response.dump())) {
    spdlog::warn("live_debug failed to write response file {}", response_path.string());
  }
}

void live_debug_record_space_action_warp_cancel(FleetBarViewController* fleet_bar, FleetPlayerData* fleet,
                                                bool has_primary, bool has_secondary, bool has_queue,
                                                bool has_queue_clear, bool has_recall, bool has_repair,
                                                bool has_recall_cancel, bool force_space_action,
                                                int visible_pre_scan_target_count, bool mining_viewer_visible,
                                                bool star_node_viewer_visible,
                                                bool navigation_interaction_visible)
{
  append_event_if_live_debug_enabled(
      "space-action-cancel-warp",
      json{{"selectedIndex", fleet_bar ? get_selected_fleet_index(fleet_bar) : -1},
           {"fleetPresent", fleet != nullptr},
           {"fleetId", fleet ? fleet->Id : 0},
           {"currentState", fleet ? static_cast<int>(fleet->CurrentState) : -1},
           {"currentStateName", fleet ? fleet_state_name(fleet->CurrentState) : "None"},
           {"previousState", fleet ? static_cast<int>(fleet->PreviousState) : -1},
           {"previousStateName", fleet ? fleet_state_name(fleet->PreviousState) : "None"},
           {"inputs",
            {"primary", has_primary},
            {"secondary", has_secondary},
            {"queue", has_queue},
            {"queueClear", has_queue_clear},
            {"recall", has_recall},
            {"repair", has_repair},
            {"recallCancel", has_recall_cancel},
            {"forceSpaceAction", force_space_action}},
           {"visiblePreScanTargetCount", visible_pre_scan_target_count},
           {"miningViewerVisible", mining_viewer_visible},
           {"starNodeViewerVisible", star_node_viewer_visible},
           {"navigationInteractionVisible", navigation_interaction_visible}});
}

void live_debug_record_space_action_warp_cancel_suppressed(FleetBarViewController* fleet_bar,
                                                           FleetPlayerData* fleet, bool has_primary,
                                                           bool has_secondary, bool has_queue,
                                                           bool has_queue_clear, bool has_recall,
                                                           bool has_repair, bool has_recall_cancel,
                                                           bool force_space_action,
                                                           int visible_pre_scan_target_count,
                                                           bool mining_viewer_visible,
                                                           bool star_node_viewer_visible,
                                                           bool navigation_interaction_visible)
{
  append_event_if_live_debug_enabled(
      "space-action-cancel-warp-suppressed",
      json{{"selectedIndex", fleet_bar ? get_selected_fleet_index(fleet_bar) : -1},
           {"fleetPresent", fleet != nullptr},
           {"fleetId", fleet ? fleet->Id : 0},
           {"currentState", fleet ? static_cast<int>(fleet->CurrentState) : -1},
           {"currentStateName", fleet ? fleet_state_name(fleet->CurrentState) : "None"},
           {"previousState", fleet ? static_cast<int>(fleet->PreviousState) : -1},
           {"previousStateName", fleet ? fleet_state_name(fleet->PreviousState) : "None"},
           {"inputs",
            {"primary", has_primary},
            {"secondary", has_secondary},
            {"queue", has_queue},
            {"queueClear", has_queue_clear},
            {"recall", has_recall},
            {"repair", has_repair},
            {"recallCancel", has_recall_cancel},
            {"forceSpaceAction", force_space_action}},
           {"suppressedReason", "mouse-primary-context"},
           {"visiblePreScanTargetCount", visible_pre_scan_target_count},
           {"miningViewerVisible", mining_viewer_visible},
           {"starNodeViewerVisible", star_node_viewer_visible},
           {"navigationInteractionVisible", navigation_interaction_visible}});
}