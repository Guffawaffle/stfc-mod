/**
 * @file live_debug_ui_change_events.cc
 * @brief UI observation change event helpers for live-debug.
 */
#include "patches/live_debug_ui_change_events.h"

#include "patches/live_debug_event_dispatcher.h"
#include "patches/live_debug_ui_serializers.h"

namespace {
const char* classify_top_canvas_change_kind(const TopCanvasObservation& previous,
                                            const TopCanvasObservation& current)
{
  if (!previous.found && current.found) {
    return "top-canvas-visible";
  }
  if (previous.found && !current.found) {
    return "top-canvas-hidden";
  }
  return "top-canvas-changed";
}

const char* classify_station_warning_change_kind(const StationWarningObservation& previous,
                                                 const StationWarningObservation& current)
{
  if (!previous.tracked && current.tracked) {
    return "station-warning-visible";
  }
  if (previous.tracked && !current.tracked) {
    return "station-warning-hidden";
  }
  if (!previous.hasContext && current.hasContext) {
    return "station-warning-context-bound";
  }
  if (previous.hasContext && !current.hasContext) {
    return "station-warning-context-cleared";
  }
  return "station-warning-changed";
}

bool has_navigation_interaction_context(const NavigationInteractionObservation& observation)
{
  for (const auto& entry : observation.entries) {
    if (entry.hasContext) {
      return true;
    }
  }

  return false;
}

const char* classify_navigation_interaction_change_kind(const NavigationInteractionObservation& previous,
                                                        const NavigationInteractionObservation& current)
{
  const bool previous_has_context = has_navigation_interaction_context(previous);
  const bool current_has_context = has_navigation_interaction_context(current);

  if (!previous.tracked && current.tracked) {
    return "navigation-interaction-visible";
  }
  if (previous.tracked && !current.tracked) {
    return "navigation-interaction-hidden";
  }
  if (!previous_has_context && current_has_context) {
    return "navigation-interaction-context-bound";
  }
  if (previous_has_context && !current_has_context) {
    return "navigation-interaction-context-cleared";
  }
  return "navigation-interaction-changed";
}

void append_navigation_follow_up(const LiveDebugUiChangeEventHooks& hooks,
                                 const NavigationInteractionObservation& previous,
                                 const NavigationInteractionObservation& current)
{
  if (hooks.append_navigation_hook_actionable_follow_up) {
    hooks.append_navigation_hook_actionable_follow_up(previous, current);
  }
  if (hooks.append_navigation_poll_actionable) {
    hooks.append_navigation_poll_actionable(previous, current);
  }
}
}

bool is_navigation_interaction_top_canvas(const TopCanvasObservation& observation)
{
  return observation.found && observation.name == "NavigationInteractionUI";
}

void append_top_canvas_change_events(const TopCanvasObservation& previous,
                                     const TopCanvasObservation& current)
{
  live_debug_events::RecordEvent(classify_top_canvas_change_kind(previous, current),
                                 nlohmann::json{{"from", top_canvas_observation_to_json(previous)},
                                                {"to", top_canvas_observation_to_json(current)}});
}

void append_station_warning_change_events(const StationWarningObservation& previous,
                                          const StationWarningObservation& current)
{
  live_debug_events::RecordEvent(classify_station_warning_change_kind(previous, current),
                                 nlohmann::json{{"from", station_warning_observation_to_json(previous)},
                                                {"to", station_warning_observation_to_json(current)}});
}

void append_navigation_interaction_change_events(
    const NavigationInteractionObservation& previous,
    const NavigationInteractionObservation& current,
    const LiveDebugUiChangeEventHooks& hooks)
{
  live_debug_events::RecordEvent(classify_navigation_interaction_change_kind(previous, current),
                                 nlohmann::json{{"from", navigation_interaction_observation_to_json(previous)},
                                                {"to", navigation_interaction_observation_to_json(current)}});
  append_navigation_follow_up(hooks, previous, current);
}