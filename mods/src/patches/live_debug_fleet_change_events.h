/**
 * @file live_debug_fleet_change_events.h
 * @brief Runtime fleet change event emission helpers for live-debug.
 */
#pragma once

#include "patches/live_debug_fleet_serializers.h"

void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current);
void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current);