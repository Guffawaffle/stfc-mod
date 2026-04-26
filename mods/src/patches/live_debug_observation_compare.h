/**
 * @file live_debug_observation_compare.h
 * @brief Observation comparison helpers for live-debug state tracking.
 */
#pragma once

#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_ui_observations.h"
#include "patches/live_debug_viewer_serializers.h"

bool same_top_canvas_observation(const TopCanvasObservation& left, const TopCanvasObservation& right);
bool same_fleet_observation(const FleetObservation& left, const FleetObservation& right);
bool same_fleet_slot_observation(const FleetSlotObservation& left, const FleetSlotObservation& right);
bool same_mine_viewer_observation(const MineViewerObservation& left, const MineViewerObservation& right);
bool same_target_viewer_observation(const TargetViewerObservation& left, const TargetViewerObservation& right);
bool same_station_warning_observation(const StationWarningObservation& left,
                                      const StationWarningObservation& right);
bool same_navigation_interaction_observation(const NavigationInteractionObservation& left,
                                             const NavigationInteractionObservation& right);
bool is_meaningful_mine_viewer_observation(const MineViewerObservation& observation);