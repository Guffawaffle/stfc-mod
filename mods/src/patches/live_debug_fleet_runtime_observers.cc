/**
 * @file live_debug_fleet_runtime_observers.cc
 * @brief Runtime fleet-bar observation helpers for live-debug.
 */
#include "patches/live_debug_fleet_runtime_observers.h"

#include "errormsg.h"
#include "patches/object_tracker_state.h"

#include "prime/FleetBarViewController.h"
#include "prime/FleetsManager.h"

#include "str_utils.h"

#include <sstream>
#include <string>

namespace {
std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
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

  if (auto hull = fleet->Hull; hull) {
    observation.hullSpecId = hull->Id;
    if (hull->Name) {
      observation.hullName = to_string(hull->Name);
    }
  }

  if (auto ship = fleet->Ship; ship) {
    observation.shipIdentityProbeId = std::to_string(ship->ID);
  }

  return observation;
}

FleetObservation observe_fleetbar(FleetBarViewController* fleet_bar)
{
  FleetObservation observation;
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

    if (auto hull = fleet->Hull; hull) {
      observation.hullSpecId = hull->Id;
      if (hull->Name) {
        observation.hullName = to_string(hull->Name);
      }
    }

    if (auto ship = fleet->Ship; ship) {
      observation.shipIdentityProbeId = std::to_string(ship->ID);
    }
  }

  return observation;
}

std::array<FleetSlotObservation, kFleetIndexMax> observe_fleet_slots(FleetBarViewController* fleet_bar)
{
  std::array<FleetSlotObservation, kFleetIndexMax> observations{};
  for (int slot_index = 0; slot_index < kFleetIndexMax; ++slot_index) {
    // Slot state comes from FleetsManager even when the fleet bar is unavailable.
    // Only selection metadata remains UI-dependent on FleetBarViewController.
    observations[static_cast<size_t>(slot_index)] = observe_fleet_slot(slot_index, fleet_bar);
  }

  return observations;
}
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

FleetObservation observe_fleetbar()
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
  return observe_fleetbar(fleet_bar.get());
}

std::array<FleetSlotObservation, kFleetIndexMax> observe_fleet_slots()
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
  return observe_fleet_slots(fleet_bar.get());
}

FleetRuntimeSnapshot observe_fleet_runtime_snapshot()
{
  FleetRuntimeSnapshot snapshot;
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
  snapshot.fleet = observe_fleetbar(fleet_bar.get());
  snapshot.slots = observe_fleet_slots(fleet_bar.get());
  return snapshot;
}
