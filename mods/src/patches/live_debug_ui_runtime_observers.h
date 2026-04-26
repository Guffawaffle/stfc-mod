/**
 * @file live_debug_ui_runtime_observers.h
 * @brief Runtime UI observation helpers for live-debug station and navigation state.
 */
#pragma once

#include "patches/live_debug_ui_observations.h"

struct LiveDebugUiObserverTraceHooks {
  void (*trace_step)(const char* step,
                     const char* phase,
                     const void* controller,
                     const void* sender,
                     const void* callback_context) = nullptr;
  void (*mark_current_poll_visible)() = nullptr;
};

StationWarningObservation observe_station_warning(const LiveDebugUiObserverTraceHooks& trace_hooks = {});
NavigationInteractionObservation observe_navigation_interaction(const LiveDebugUiObserverTraceHooks& trace_hooks = {});