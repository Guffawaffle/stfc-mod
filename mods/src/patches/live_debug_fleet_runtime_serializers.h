#pragma once

#include <nlohmann/json.hpp>

struct FleetPlayerData;
struct FleetDeployedData;
struct IList;

const char* deployed_fleet_type_name(int fleet_type);
nlohmann::json fleet_to_json(FleetPlayerData* fleet);
nlohmann::json deployed_fleet_to_json(FleetDeployedData* fleet);
nlohmann::json deployed_fleet_list_to_json(IList* fleets);