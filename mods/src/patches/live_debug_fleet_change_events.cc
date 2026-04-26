/**
 * @file live_debug_fleet_change_events.cc
 * @brief Runtime fleet change event emission helpers for live-debug.
 */
#include "patches/live_debug_fleet_change_events.h"

#include "patches/live_debug_event_dispatcher.h"

#include "prime/FleetPlayerData.h"

#include <nlohmann/json.hpp>

namespace {
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

nlohmann::json fleet_slot_transition_to_json(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  return nlohmann::json{{"slotIndex", current.slotIndex},
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
}

void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current)
{
  const bool selection_changed =
      previous.selectedIndex != current.selectedIndex || previous.fleetId != current.fleetId;

  if (selection_changed) {
    live_debug_events::RecordEvent(
        "selected-fleet-changed",
        nlohmann::json{{"from", fleet_observation_to_json(previous)}, {"to", fleet_observation_to_json(current)}});
  }
}

void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  bool emitted = false;
  const bool same_fleet = previous.present && current.present && previous.fleetId == current.fleetId;
  const bool fleet_changed = previous.present && current.present && previous.fleetId != current.fleetId;

  if (fleet_changed) {
    live_debug_events::RecordEvent("fleet-slot-fleet-changed",
                                   nlohmann::json{{"slotIndex", current.slotIndex},
                                                  {"from", fleet_slot_observation_to_json(previous)},
                                                  {"to", fleet_slot_observation_to_json(current)}});
    emitted = true;
  }

  if ((previous.present != current.present) || (same_fleet && previous.currentState != current.currentState)) {
    live_debug_events::RecordEvent(classify_fleet_slot_transition_kind(previous, current),
                                   fleet_slot_transition_to_json(previous, current));
    emitted = true;
  }

  if (same_fleet && previous.hullName != current.hullName) {
    live_debug_events::RecordEvent("fleet-slot-hull-changed",
                                   nlohmann::json{{"slotIndex", current.slotIndex},
                                                  {"selected", current.selected},
                                                  {"fleetId", current.fleetId},
                                                  {"fromHullName", previous.hullName},
                                                  {"toHullName", current.hullName},
                                                  {"state", current.currentState},
                                                  {"stateName", fleet_state_name_from_value(current.currentState)}});
    emitted = true;
  }

  if (same_fleet && current.cargoFillBasisPoints > previous.cargoFillBasisPoints) {
    live_debug_events::RecordEvent("fleet-slot-cargo-gained",
                                   nlohmann::json{{"slotIndex", current.slotIndex},
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
    live_debug_events::RecordEvent("fleet-slot-changed", fleet_slot_observation_to_json(current));
  }
}