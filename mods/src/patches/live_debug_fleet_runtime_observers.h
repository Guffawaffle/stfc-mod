/**
 * @file live_debug_fleet_runtime_observers.h
 * @brief Runtime fleet-bar observation helpers for live-debug.
 */
#pragma once

#include "patches/live_debug_fleet_serializers.h"

#include <array>

struct FleetBarViewController;

int get_selected_fleet_index(FleetBarViewController* fleet_bar);
FleetObservation observe_fleetbar();
std::array<FleetSlotObservation, kFleetIndexMax> observe_fleet_slots();