#include "patches/live_debug_fleet_runtime_serializers.h"

#include "patches/live_debug_fleet_serializers.h"
#include "prime/FleetDeployedData.h"
#include "prime/FleetPlayerData.h"
#include "prime/IList.h"
#include "str_utils.h"

using nlohmann::json;

const char* deployed_fleet_type_name(int fleet_type)
{
  switch (fleet_type) {
    case 1:
      return "Player";
    case 2:
      return "Marauder";
    case 3:
      return "NpcInstantiated";
    case 4:
      return "Sentinel";
    case 5:
      return "Alliance";
    case 6:
      return "Challenge";
    case 0:
      return "None";
    default:
      return "Unknown";
  }
}

json fleet_to_json(FleetPlayerData* fleet)
{
  json result = {{"present", fleet != nullptr}};

  if (!fleet) {
    return result;
  }

  result["id"] = fleet->Id;
  result["currentState"] = static_cast<int>(fleet->CurrentState);
  result["currentStateName"] = fleet_state_name_from_value(static_cast<int>(fleet->CurrentState));
  result["previousState"] = static_cast<int>(fleet->PreviousState);
  result["previousStateName"] = fleet_state_name_from_value(static_cast<int>(fleet->PreviousState));
  result["cargoFill"] = fleet->CargoResourceFillLevel;

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

  const auto count = fleets->Count < 0 ? 0 : fleets->Count;
  for (int index = 0; index < count; ++index) {
    result.push_back(deployed_fleet_to_json(reinterpret_cast<FleetDeployedData*>(fleets->Get(index))));
  }

  return result;
}