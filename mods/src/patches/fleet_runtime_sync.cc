/**
 * @file fleet_runtime_sync.cc
 * @brief Change-driven fleet runtime sync snapshots.
 */
#include "patches/fleet_runtime_sync.h"

#include "patches/live_debug_fleet_runtime_observers.h"
#include "patches/sync_scheduler.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>

namespace
{
using json = nlohmann::json;

struct FleetSlotStateKey {
  bool        selected = false;
  bool        present = false;
  uint64_t    fleet_id = 0;
  int         current_state = -1;
  int         previous_state = -1;
  int         cargo_fill_percent = -1;
  std::string hull_name;

  auto operator<=>(const FleetSlotStateKey&) const = default;
};

struct FleetStateKey {
  bool tracked = false;
  int  selected_index = -1;

  bool        fleet_present = false;
  uint64_t    fleet_id = 0;
  int         current_state = -1;
  int         previous_state = -1;
  int         cargo_fill_percent = -1;
  std::string hull_name;

  std::array<FleetSlotStateKey, kFleetIndexMax> slots{};

  auto operator<=>(const FleetStateKey&) const = default;
};

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

int cargo_fill_percent_bucket(int basis_points, int percent)
{
  if (percent >= 0) {
    return percent;
  }
  if (basis_points >= 0) {
    return basis_points / 100;
  }
  return -1;
}

FleetStateKey make_state_key(const FleetObservation& fleet,
                             const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  FleetStateKey key;
  key.tracked = fleet.tracked;
  key.selected_index = fleet.selectedIndex;
  key.fleet_present = fleet.hasFleet;
  key.fleet_id = fleet.fleetId;
  key.current_state = fleet.currentState;
  key.previous_state = fleet.previousState;
  key.cargo_fill_percent = cargo_fill_percent_bucket(fleet.cargoFillBasisPoints, fleet.cargoFillPercent);
  key.hull_name = fleet.hullName;

  for (size_t index = 0; index < slots.size(); ++index) {
    const auto& slot = slots[index];
    auto&       state = key.slots[index];
    state.selected = slot.selected;
    state.present = slot.present;
    state.fleet_id = slot.fleetId;
    state.current_state = slot.currentState;
    state.previous_state = slot.previousState;
    state.cargo_fill_percent = cargo_fill_percent_bucket(slot.cargoFillBasisPoints, slot.cargoFillPercent);
    state.hull_name = slot.hullName;
  }

  return key;
}

bool is_meaningful_state(const FleetStateKey& state)
{
  return state.tracked || state.fleet_present
      || std::ranges::any_of(state.slots, [](const auto& slot) { return slot.present; });
}

json fleet_to_json(const FleetObservation& fleet)
{
  json result = {{"present", fleet.hasFleet}};

  if (!fleet.hasFleet) {
    return result;
  }

  result["id"] = fleet.fleetId;
  result["currentState"] = fleet.currentState;
  result["currentStateName"] = fleet_state_name_from_value(fleet.currentState);
  result["previousState"] = fleet.previousState;
  result["previousStateName"] = fleet_state_name_from_value(fleet.previousState);
  result["cargoFillPercent"] = fleet.cargoFillPercent;
  result["cargoFillBasisPoints"] = fleet.cargoFillBasisPoints;
  result["hullName"] = fleet.hullName;
  return result;
}

json fleet_slot_to_json(const FleetSlotObservation& slot)
{
  json result = {
      {"slotIndex", slot.slotIndex},
      {"selected", slot.selected},
      {"present", slot.present},
  };

  if (!slot.present) {
    return result;
  }

  result["fleetId"] = slot.fleetId;
  result["currentState"] = slot.currentState;
  result["currentStateName"] = fleet_state_name_from_value(slot.currentState);
  result["previousState"] = slot.previousState;
  result["previousStateName"] = fleet_state_name_from_value(slot.previousState);
  result["cargoFillPercent"] = slot.cargoFillPercent;
  result["cargoFillBasisPoints"] = slot.cargoFillBasisPoints;
  result["hullName"] = slot.hullName;
  return result;
}

json slots_to_json(const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  auto result = json::array();
  for (const auto& slot : slots) {
    result.push_back(fleet_slot_to_json(slot));
  }
  return result;
}

json build_snapshot_payload(std::string_view source, const FleetObservation& fleet,
                            const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  return json{
      {"type", "fleet.runtime"},
      {"schemaVersion", "stfc.fleet.runtime_snapshot.v1"},
      {"source", source},
      {"observedAtMs", current_time_millis_utc()},
      {"fleetBarTracked", fleet.tracked},
      {"selectedIndex", fleet.selectedIndex},
      {"fleet", fleet_to_json(fleet)},
      {"slots", slots_to_json(slots)},
  };
}
} // namespace

void fleet_runtime_sync_capture(std::string_view source)
{
  static std::optional<FleetStateKey> last_state;

  const auto snapshot = observe_fleet_runtime_snapshot();
  const auto state = make_state_key(snapshot.fleet, snapshot.slots);

  if (!is_meaningful_state(state) && !last_state.has_value()) {
    return;
  }

  if (last_state.has_value() && *last_state == state) {
    return;
  }

  last_state = state;
  queue_data(SyncConfig::Type::FleetRuntime, build_snapshot_payload(source, snapshot.fleet, snapshot.slots));
}
