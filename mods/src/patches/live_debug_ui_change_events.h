/**
 * @file live_debug_ui_change_events.h
 * @brief UI observation change event helpers for live-debug.
 */
#pragma once

#include "patches/live_debug_ui_observations.h"

struct LiveDebugUiChangeEventHooks {
  void (*append_navigation_hook_actionable_follow_up)(const NavigationInteractionObservation& previous,
                                                      const NavigationInteractionObservation& current) = nullptr;
  void (*append_navigation_poll_actionable)(const NavigationInteractionObservation& previous,
                                            const NavigationInteractionObservation& current) = nullptr;
};

bool is_navigation_interaction_top_canvas(const TopCanvasObservation& observation);
void append_top_canvas_change_events(const TopCanvasObservation& previous,
                                     const TopCanvasObservation& current);
void append_station_warning_change_events(const StationWarningObservation& previous,
                                          const StationWarningObservation& current);
void append_navigation_interaction_change_events(
    const NavigationInteractionObservation& previous,
    const NavigationInteractionObservation& current,
    const LiveDebugUiChangeEventHooks& hooks = {});