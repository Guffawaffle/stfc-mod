#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

constexpr int kFleetIndexMax = 10;

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

const char* fleet_state_name_from_value(int state);
nlohmann::json fleet_observation_to_json(const FleetObservation& observation);
nlohmann::json fleet_slot_observation_to_json(const FleetSlotObservation& observation);
nlohmann::json fleet_slots_to_json(const std::array<FleetSlotObservation, kFleetIndexMax>& observations);