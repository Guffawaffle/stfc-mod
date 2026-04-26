#include "patches/live_debug_fleet_serializers.h"

using nlohmann::json;

namespace {
const char* fleet_state_name(int state)
{
  switch (state) {
    case 0:
      return "Unknown";
    case 1:
      return "IdleInSpace";
    case 2:
      return "Docked";
    case 4:
      return "Mining";
    case 8:
      return "Destroyed";
    case 16:
      return "TieringUp";
    case 32:
      return "Repairing";
    case 56:
      return "CannotLaunch";
    case 64:
      return "Battling";
    case 128:
      return "WarpCharging";
    case 256:
      return "Warping";
    case 384:
      return "CanRemove";
    case 504:
      return "CannotMove";
    case 512:
      return "Impulsing";
    case 899:
      return "CanManage";
    case 1024:
      return "Capturing";
    case 1541:
      return "CanRecall";
    case 1543:
      return "CanEngage";
    case 1989:
      return "Deployed";
    case 1991:
      return "CanLocate";
    default:
      return "Unmapped";
  }
}
}

const char* fleet_state_name_from_value(int state)
{
  if (state < 0) {
    return "None";
  }

  return fleet_state_name(state);
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
    result["fleet"]["currentStateName"] = fleet_state_name_from_value(observation.currentState);
    result["fleet"]["previousState"] = observation.previousState;
    result["fleet"]["previousStateName"] = fleet_state_name_from_value(observation.previousState);
    result["fleet"]["cargoFillPercent"] = observation.cargoFillPercent;
    result["fleet"]["cargoFillBasisPoints"] = observation.cargoFillBasisPoints;
    result["fleet"]["hullName"] = observation.hullName;
  }

  return result;
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