/**
 * @file live_debug.cc
 * @brief Main-thread file-backed live query channel.
 *
 * V1 keeps the surface deliberately small and read-only: AX writes a JSON
 * request file, the game consumes it from ScreenManager::Update, and writes a
 * JSON response file with basic runtime state.
 */
#include "patches/live_debug.h"
#include "patches/live_debug_connector.h"
#include "patches/live_debug_event_dispatcher.h"
#include "patches/live_debug_fleet_change_events.h"
#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_fleet_runtime_observers.h"
#include "patches/live_debug_fleet_runtime_serializers.h"
#include "patches/live_debug_observation_compare.h"
#include "patches/live_debug_recent_event_requests.h"
#include "patches/live_debug_request_dispatch.h"
#include "patches/live_debug_state_results.h"
#include "patches/live_debug_ui_change_events.h"
#include "patches/live_debug_ui_runtime_observers.h"
#include "patches/live_debug_ui_serializers.h"
#include "patches/live_debug_viewer_runtime.h"
#include "patches/live_debug_viewer_serializers.h"

#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "patches/object_tracker_state.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FleetPlayerData.h"
#include "prime/IList.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanStationTargetWidget.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"
#include "prime/StationWarningViewController.h"
#include "prime/StarNodeObjectViewerWidget.h"
#include "str_utils.h"
#include "version.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using json = nlohmann::json;

constexpr std::string_view kNavigationHookTraceFile = "community_patch_navhook_trace.log";

bool             g_recent_observations_initialized = false;
TopCanvasObservation g_last_top_canvas;
FleetObservation     g_last_fleet;
std::array<FleetSlotObservation, kFleetIndexMax> g_last_fleet_slots;
MineViewerObservation g_last_mine_viewer;
TargetViewerObservation g_last_target_viewer;
StationWarningObservation g_last_station_warning;
NavigationInteractionObservation g_last_navigation_interaction;

struct PendingNavigationHookNote {
  bool        pending = false;
  const char* phase = nullptr;
  const void* controller = nullptr;
  const void* sender = nullptr;
  const void* callbackContext = nullptr;
  bool        prePollCaptured = false;
  TopCanvasObservation prePollTopCanvas;
  NavigationInteractionObservation prePollNavigationInteraction;
};

struct RecentNavigationHookFollowUp {
  bool        active = false;
  const char* phase = nullptr;
  const void* controller = nullptr;
  const void* sender = nullptr;
  const void* callbackContext = nullptr;
};

PendingNavigationHookNote g_pending_navigation_hook_note;
RecentNavigationHookFollowUp g_recent_navigation_hook_follow_up;
std::string g_last_navigation_poll_actionable_pointer;
bool g_logged_navigation_hook_tick_enter = false;
bool g_logged_navigation_hook_tick_after_ui_poll = false;

constexpr bool kEnableLiveDebugUiPollingFromTick = false;
constexpr bool kEnableLiveDebugTopCanvasPolling = true;
constexpr bool kEnableLiveDebugStationWarningPolling = false;
constexpr bool kEnableLiveDebugNavigationInteractionPolling = false;
constexpr bool kEnableLiveDebugObserverStepTrace = false;
bool g_ui_observer_trace_current_poll = false;
int g_ui_observer_trace_budget = 4000;
void append_navigation_hook_actionable_follow_up_event(const NavigationInteractionObservation& previous,
                                                       const NavigationInteractionObservation& current);
void append_navigation_poll_actionable_event(const NavigationInteractionObservation& previous,
                                             const NavigationInteractionObservation& current);

std::filesystem::path get_live_debug_path(std::string_view filename)
{
  return std::filesystem::path(File::MakePathString(filename));
}

std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

void append_navigation_hook_trace_step(const char* step,
                                       const char* phase,
                                       const void* controller = nullptr,
                                       const void* sender = nullptr,
                                       const void* callback_context = nullptr)
{
  const auto trace_path = get_live_debug_path(kNavigationHookTraceFile);
  const auto path_text = trace_path.string();
  auto* trace_file = std::fopen(path_text.c_str(), "ab");
  if (!trace_file) {
    return;
  }

  std::fprintf(trace_file,
               "[%lld] step=%s phase='%s' controller=%p sender=%p callbackContext=%p\n",
               static_cast<long long>(current_time_millis_utc()),
               step ? step : "",
               phase ? phase : "",
               controller,
               sender,
               callback_context);
  std::fflush(trace_file);
  std::fclose(trace_file);
}

void append_ui_observer_trace_step(const char* step,
                                   const char* phase,
                                   const void* controller = nullptr,
                                   const void* sender = nullptr,
                                   const void* callback_context = nullptr)
{
  if (!kEnableLiveDebugObserverStepTrace || g_ui_observer_trace_budget <= 0) {
    return;
  }
  if (!g_ui_observer_trace_current_poll) {
    return;
  }

  --g_ui_observer_trace_budget;
  append_navigation_hook_trace_step(step, phase, controller, sender, callback_context);
}

void append_ui_observer_forced_trace_step(const char* step,
                                          const char* phase,
                                          const void* controller = nullptr,
                                          const void* sender = nullptr,
                                          const void* callback_context = nullptr)
{
  if (!kEnableLiveDebugObserverStepTrace || g_ui_observer_trace_budget <= 0) {
    return;
  }

  --g_ui_observer_trace_budget;
  append_navigation_hook_trace_step(step, phase, controller, sender, callback_context);
}

void mark_ui_observer_current_poll_visible()
{
  g_ui_observer_trace_current_poll = true;
}

const LiveDebugUiObserverTraceHooks& ui_observer_trace_hooks()
{
  static const LiveDebugUiObserverTraceHooks hooks = {
      append_ui_observer_trace_step,
      mark_ui_observer_current_poll_visible,
  };
  return hooks;
}

const LiveDebugUiChangeEventHooks& ui_change_event_hooks()
{
  static const LiveDebugUiChangeEventHooks hooks = {
      append_navigation_hook_actionable_follow_up_event,
      append_navigation_poll_actionable_event,
  };
  return hooks;
}

std::string get_request_id(const json& request)
{
  if (const auto id = request.find("id"); id != request.end() && id->is_string()) {
    return id->get<std::string>();
  }

  return "";
}

json make_error_response(const std::string& request_id, std::string_view error)
{
  return json{{"id", request_id}, {"ok", false}, {"error", error}};
}

json make_ok_response(const std::string& request_id, const json& result)
{
  return json{{"id", request_id}, {"ok", true}, {"result", result}};
}

const NavigationInteractionObservation::Entry* find_navigation_interaction_entry(
    const NavigationInteractionObservation& observation, const void* controller)
{
  if (!controller) {
    return nullptr;
  }

  const auto controller_pointer = pointer_to_string(controller);
  for (const auto& entry : observation.entries) {
    if (entry.pointer == controller_pointer) {
      return &entry;
    }
  }

  return nullptr;
}

const NavigationInteractionObservation::Entry* find_navigation_interaction_entry(
    const NavigationInteractionObservation& observation, const std::string& pointer)
{
  for (const auto& entry : observation.entries) {
    if (entry.pointer == pointer) {
      return &entry;
    }
  }

  return nullptr;
}

bool is_navigation_interaction_entry_actionable(const NavigationInteractionObservation::Entry& entry)
{
  return entry.hasContext && (entry.validNavigationInput || entry.showSetCourseArm);
}

void append_navigation_hook_actionable_follow_up_event(const NavigationInteractionObservation& previous,
                                                       const NavigationInteractionObservation& current)
{
  if (!g_recent_navigation_hook_follow_up.active || !g_recent_navigation_hook_follow_up.controller) {
    return;
  }

  append_navigation_hook_trace_step("followup/enter",
                                    g_recent_navigation_hook_follow_up.phase,
                                    g_recent_navigation_hook_follow_up.controller,
                                    g_recent_navigation_hook_follow_up.sender,
                                    g_recent_navigation_hook_follow_up.callbackContext);

  const bool sender_matches_top_canvas =
      g_recent_navigation_hook_follow_up.sender && g_last_top_canvas.found &&
      pointer_to_string(g_recent_navigation_hook_follow_up.sender) == g_last_top_canvas.pointer;
  if (!sender_matches_top_canvas) {
    append_navigation_hook_trace_step("followup/clear-top-canvas-miss",
                                      g_recent_navigation_hook_follow_up.phase,
                                      g_recent_navigation_hook_follow_up.controller,
                                      g_recent_navigation_hook_follow_up.sender,
                                      g_recent_navigation_hook_follow_up.callbackContext);
    g_recent_navigation_hook_follow_up = {};
    return;
  }

  const auto* current_entry =
      find_navigation_interaction_entry(current, g_recent_navigation_hook_follow_up.controller);
  if (!current_entry) {
    append_navigation_hook_trace_step("followup/no-current-entry",
                                      g_recent_navigation_hook_follow_up.phase,
                                      g_recent_navigation_hook_follow_up.controller,
                                      g_recent_navigation_hook_follow_up.sender,
                                      g_recent_navigation_hook_follow_up.callbackContext);
    return;
  }

  const auto* previous_entry =
      find_navigation_interaction_entry(previous, g_recent_navigation_hook_follow_up.controller);
  if (previous_entry && is_navigation_interaction_entry_actionable(*previous_entry)) {
    append_navigation_hook_trace_step("followup/already-actionable",
                                      g_recent_navigation_hook_follow_up.phase,
                                      g_recent_navigation_hook_follow_up.controller,
                                      g_recent_navigation_hook_follow_up.sender,
                                      g_recent_navigation_hook_follow_up.callbackContext);
    return;
  }
  if (!is_navigation_interaction_entry_actionable(*current_entry)) {
    append_navigation_hook_trace_step("followup/not-yet-actionable",
                                      g_recent_navigation_hook_follow_up.phase,
                                      g_recent_navigation_hook_follow_up.controller,
                                      g_recent_navigation_hook_follow_up.sender,
                                      g_recent_navigation_hook_follow_up.callbackContext);
    return;
  }

  json details{{"phase", g_recent_navigation_hook_follow_up.phase ? g_recent_navigation_hook_follow_up.phase : ""},
               {"pointer", pointer_to_string(g_recent_navigation_hook_follow_up.controller)},
               {"senderMatchesTopCanvas", true},
               {"topCanvas", top_canvas_observation_to_json(g_last_top_canvas)},
               {"navigationInteractionTrackedCount", current.trackedCount},
               {"matchedController", navigation_interaction_entry_to_json(*current_entry)}};

  if (g_recent_navigation_hook_follow_up.sender) {
    details["senderPointer"] = pointer_to_string(g_recent_navigation_hook_follow_up.sender);
  }
  if (g_recent_navigation_hook_follow_up.callbackContext) {
    details["callbackContextPointer"] = pointer_to_string(g_recent_navigation_hook_follow_up.callbackContext);
  }
  if (previous_entry) {
    details["previousMatchedController"] = navigation_interaction_entry_to_json(*previous_entry);
  }

  append_navigation_hook_trace_step("followup/before-append",
                                    g_recent_navigation_hook_follow_up.phase,
                                    g_recent_navigation_hook_follow_up.controller,
                                    g_recent_navigation_hook_follow_up.sender,
                                    g_recent_navigation_hook_follow_up.callbackContext);
  live_debug_events::RecordEvent("navigation-interaction-hook-became-actionable", std::move(details));
  append_navigation_hook_trace_step("followup/after-append",
                                    g_recent_navigation_hook_follow_up.phase,
                                    g_recent_navigation_hook_follow_up.controller,
                                    g_recent_navigation_hook_follow_up.sender,
                                    g_recent_navigation_hook_follow_up.callbackContext);
  g_recent_navigation_hook_follow_up = {};
}

void append_navigation_poll_actionable_event(const NavigationInteractionObservation& previous,
                                             const NavigationInteractionObservation& current)
{
  if (!is_navigation_interaction_top_canvas(g_last_top_canvas)) {
    return;
  }

  for (const auto& entry : current.entries) {
    if (!is_navigation_interaction_entry_actionable(entry)) {
      continue;
    }

    const auto* previous_entry = find_navigation_interaction_entry(previous, entry.pointer);
    if (previous_entry && is_navigation_interaction_entry_actionable(*previous_entry) &&
        g_last_navigation_poll_actionable_pointer == entry.pointer) {
      continue;
    }

    json details{{"topCanvas", top_canvas_observation_to_json(g_last_top_canvas)},
                 {"navigationInteractionTrackedCount", current.trackedCount},
                 {"matchedController", navigation_interaction_entry_to_json(entry)}};
    if (previous_entry) {
      details["previousMatchedController"] = navigation_interaction_entry_to_json(*previous_entry);
    }

    live_debug_events::RecordEvent("navigation-interaction-poll-became-actionable", std::move(details));
    g_last_navigation_poll_actionable_pointer = entry.pointer;
    return;
  }
}

void flush_pending_navigation_hook_note()
{
  if (!g_pending_navigation_hook_note.pending) {
    return;
  }

  append_navigation_hook_trace_step("flush/enter",
                                    g_pending_navigation_hook_note.phase,
                                    g_pending_navigation_hook_note.controller,
                                    g_pending_navigation_hook_note.sender,
                                    g_pending_navigation_hook_note.callbackContext);

  auto note = g_pending_navigation_hook_note;
  g_pending_navigation_hook_note = {};
  append_navigation_hook_trace_step("flush/copied",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  if (!kEnableLiveDebugUiPollingFromTick) {
    json details{{"phase", note.phase ? note.phase : ""},
                 {"pointer", pointer_to_string(note.controller)}};
    if (note.sender) {
      details["senderPointer"] = pointer_to_string(note.sender);
    }
    if (note.callbackContext) {
      details["callbackContextPointer"] = pointer_to_string(note.callbackContext);
    }

    live_debug_events::RecordEvent("navigation-interaction-hook-note", std::move(details));
    append_navigation_hook_trace_step("flush/minimal-after-append",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
    return;
  }

  json details{{"phase", note.phase ? note.phase : ""},
               {"pointer", pointer_to_string(note.controller)}};
  append_navigation_hook_trace_step("flush/base-details", note.phase, note.controller, note.sender, note.callbackContext);

  if (note.sender) {
    append_navigation_hook_trace_step("flush/before-add-sender",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
    details["senderPointer"] = pointer_to_string(note.sender);
    append_navigation_hook_trace_step("flush/after-add-sender",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
  }

  if (note.callbackContext) {
    append_navigation_hook_trace_step("flush/before-add-callback-context",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
    details["callbackContextPointer"] = pointer_to_string(note.callbackContext);
    append_navigation_hook_trace_step("flush/after-add-callback-context",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
  }

  append_navigation_hook_trace_step("flush/before-pre-poll-details",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  if (note.prePollCaptured) {
    details["prePollTopCanvas"] = top_canvas_observation_to_json(note.prePollTopCanvas);
    details["prePollNavigationInteractionTrackedCount"] = note.prePollNavigationInteraction.trackedCount;
  }
  append_navigation_hook_trace_step("flush/after-pre-poll-details",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  append_navigation_hook_trace_step("flush/before-top-canvas",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  details["topCanvas"] = top_canvas_observation_to_json(g_last_top_canvas);
  append_navigation_hook_trace_step("flush/after-top-canvas",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  append_navigation_hook_trace_step("flush/before-navigation-count",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  details["navigationInteractionTrackedCount"] = g_last_navigation_interaction.trackedCount;
  append_navigation_hook_trace_step("flush/after-navigation-count",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  append_navigation_hook_trace_step("flush/before-controller-pointer-read",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  const auto controller_pointer = details["pointer"].get<std::string>();
  append_navigation_hook_trace_step("flush/after-controller-pointer-read",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  if (note.prePollCaptured) {
    append_navigation_hook_trace_step("flush/before-pre-poll-match-loop",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
    for (const auto& entry : note.prePollNavigationInteraction.entries) {
      if (entry.pointer == controller_pointer) {
        details["prePollMatchedController"] = navigation_interaction_entry_to_json(entry);
        break;
      }
    }

    if (note.sender && note.prePollTopCanvas.found) {
      details["prePollSenderMatchesTopCanvas"] =
          pointer_to_string(note.sender) == note.prePollTopCanvas.pointer;
    }
    append_navigation_hook_trace_step("flush/after-pre-poll-match-loop",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
  }

  append_navigation_hook_trace_step("flush/before-match-loop",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  for (const auto& entry : g_last_navigation_interaction.entries) {
    if (entry.pointer == controller_pointer) {
      append_navigation_hook_trace_step("flush/match-found",
                                        note.phase,
                                        note.controller,
                                        note.sender,
                                        note.callbackContext);
      details["matchedController"] = navigation_interaction_entry_to_json(entry);
      append_navigation_hook_trace_step("flush/after-match-json",
                                        note.phase,
                                        note.controller,
                                        note.sender,
                                        note.callbackContext);
      break;
    }
  }

  append_navigation_hook_trace_step("flush/after-match-loop",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);

  if (note.sender && g_last_top_canvas.found) {
    append_navigation_hook_trace_step("flush/before-sender-compare",
                                      note.phase,
                                      note.controller,
                                      note.sender,
                                      note.callbackContext);
    details["senderMatchesTopCanvas"] =
        pointer_to_string(note.sender) == g_last_top_canvas.pointer;
    append_navigation_hook_trace_step(
        details["senderMatchesTopCanvas"].get<bool>() ? "flush/after-sender-compare/match"
                                                       : "flush/after-sender-compare/miss",
        note.phase,
        note.controller,
        note.sender,
        note.callbackContext);
  }

  append_navigation_hook_trace_step("flush/before-append",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
  live_debug_events::RecordEvent("navigation-interaction-hook-note", std::move(details));
  g_recent_navigation_hook_follow_up = RecentNavigationHookFollowUp{
      note.controller != nullptr, note.phase, note.controller, note.sender, note.callbackContext};
  append_navigation_hook_trace_step("flush/after-append",
                                    note.phase,
                                    note.controller,
                                    note.sender,
                                    note.callbackContext);
}

void reset_recent_observations()
{
  g_recent_observations_initialized = false;
  g_last_top_canvas = {};
  g_last_fleet = {};
  g_last_fleet_slots = {};
  g_last_mine_viewer = {};
  g_last_target_viewer = {};
  g_last_station_warning = {};
  g_last_navigation_interaction = {};
}

int count_list_items(IList* list)
{
  if (!list) {
    return 0;
  }

  return list->Count < 0 ? 0 : list->Count;
}

void initialize_recent_model_observations(std::string_view source)
{
  if (g_recent_observations_initialized) {
    return;
  }

    const auto top_canvas = kEnableLiveDebugTopCanvasPolling ? observe_top_canvas() : TopCanvasObservation{};
  const auto fleet = observe_fleetbar();
  const auto fleet_slots = observe_fleet_slots();
    const auto station_warning =
      kEnableLiveDebugStationWarningPolling ? observe_station_warning(ui_observer_trace_hooks()) : StationWarningObservation{};
    const auto navigation_interaction = kEnableLiveDebugNavigationInteractionPolling
      ? observe_navigation_interaction(ui_observer_trace_hooks())
      : NavigationInteractionObservation{};

  g_last_top_canvas = top_canvas;
  g_last_fleet = fleet;
  g_last_fleet_slots = fleet_slots;
  g_last_station_warning = station_warning;
  g_last_navigation_interaction = navigation_interaction;
  g_recent_observations_initialized = true;

  live_debug_events::RecordEvent(
      "observer-ready",
      json{{"source", source},
           {"topCanvas", top_canvas_observation_to_json(top_canvas)},
           {"fleet", fleet_observation_to_json(fleet)},
           {"fleetSlots", fleet_slots_to_json(fleet_slots)},
           {"stationWarning", station_warning_observation_to_json(station_warning)},
           {"navigationInteraction",
            navigation_interaction_observation_to_json(navigation_interaction)}});
}

void poll_recent_ui_events()
{
  g_ui_observer_trace_current_poll = false;
  append_ui_observer_trace_step("poll/enter", "poll_recent_ui_events");
  append_ui_observer_trace_step("poll/before-top-canvas", "poll_recent_ui_events");
  const auto top_canvas = observe_top_canvas();
  g_ui_observer_trace_current_poll = !same_top_canvas_observation(top_canvas, g_last_top_canvas) ||
                                     is_navigation_interaction_top_canvas(top_canvas) ||
                                     is_navigation_interaction_top_canvas(g_last_top_canvas);
  append_ui_observer_trace_step("poll/after-top-canvas", "poll_recent_ui_events");
  append_ui_observer_trace_step("poll/before-station-warning", "poll_recent_ui_events");
  const auto station_warning =
      kEnableLiveDebugStationWarningPolling ? observe_station_warning(ui_observer_trace_hooks()) : g_last_station_warning;
  append_ui_observer_trace_step("poll/after-station-warning", "poll_recent_ui_events");
  const bool trace_navigation_poll =
      kEnableLiveDebugNavigationInteractionPolling &&
      (is_navigation_interaction_top_canvas(top_canvas) ||
       is_navigation_interaction_top_canvas(g_last_top_canvas));
  if (trace_navigation_poll) {
    append_navigation_hook_trace_step("poll/before-observe-navigation-interaction", "poll_recent_ui_events");
  }
  const auto navigation_interaction = kEnableLiveDebugNavigationInteractionPolling
      ? observe_navigation_interaction(ui_observer_trace_hooks())
      : g_last_navigation_interaction;
  append_ui_observer_trace_step("poll/after-navigation-interaction", "poll_recent_ui_events");
  if (trace_navigation_poll) {
    append_navigation_hook_trace_step("poll/after-observe-navigation-interaction", "poll_recent_ui_events");
  }

  if (kEnableLiveDebugTopCanvasPolling && !same_top_canvas_observation(top_canvas, g_last_top_canvas)) {
    if (trace_navigation_poll) {
      append_navigation_hook_trace_step("poll/before-top-canvas-change", "poll_recent_ui_events");
    }
    append_top_canvas_change_events(g_last_top_canvas, top_canvas);
    if (trace_navigation_poll) {
      append_navigation_hook_trace_step("poll/after-top-canvas-change", "poll_recent_ui_events");
    }
  }
  if (kEnableLiveDebugTopCanvasPolling) {
    g_last_top_canvas = top_canvas;
  }

  if (kEnableLiveDebugStationWarningPolling && !same_station_warning_observation(station_warning, g_last_station_warning)) {
    append_station_warning_change_events(g_last_station_warning, station_warning);
  }
  if (kEnableLiveDebugStationWarningPolling) {
    g_last_station_warning = station_warning;
  }

  if (kEnableLiveDebugNavigationInteractionPolling &&
      !same_navigation_interaction_observation(navigation_interaction, g_last_navigation_interaction)) {
    if (trace_navigation_poll) {
      append_navigation_hook_trace_step("poll/before-navigation-interaction-change", "poll_recent_ui_events");
    }
    append_navigation_interaction_change_events(g_last_navigation_interaction,
                                                navigation_interaction,
                                                ui_change_event_hooks());
    if (trace_navigation_poll) {
      append_navigation_hook_trace_step("poll/after-navigation-interaction-change", "poll_recent_ui_events");
    }
  }
  if (kEnableLiveDebugNavigationInteractionPolling) {
    g_last_navigation_interaction = navigation_interaction;
  }
  g_ui_observer_trace_current_poll = false;
}

void capture_recent_model_events(std::string_view source)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  if (!g_recent_observations_initialized) {
    initialize_recent_model_observations(source);
    return;
  }

  poll_recent_ui_events();

  const auto fleet = observe_fleetbar();
  const auto fleet_slots = observe_fleet_slots();

  if (!same_fleet_observation(fleet, g_last_fleet)) {
    append_fleet_change_events(g_last_fleet, fleet);
  }
  g_last_fleet = fleet;

  for (size_t slot_index = 0; slot_index < fleet_slots.size(); ++slot_index) {
    if (!same_fleet_slot_observation(fleet_slots[slot_index], g_last_fleet_slots[slot_index])) {
      append_fleet_slot_change_events(g_last_fleet_slots[slot_index], fleet_slots[slot_index]);
    }
  }
  g_last_fleet_slots = fleet_slots;
}

void DeploymentEvents_TriggerFleetStateChangeEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  live_debug_events::RecordEvent(
      "deployment-fleet-state-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-fleet-state-event");
}

void DeploymentEvents_TriggerPlayerFleetsUpdatedEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  capture_recent_model_events("deployment-player-fleets-updated-event");
}

void DeploymentEvents_TriggerCoursePlannedEvent_Hook(auto original, IList* courses)
{
  original(courses);
  live_debug_events::RecordEvent("deployment-course-planned-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-planned-event");
}

void DeploymentEvents_TriggerCourseStartEvent_Hook(auto original, IList* courses)
{
  original(courses);
  live_debug_events::RecordEvent("deployment-course-start-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-start-event");
}

void DeploymentEvents_TriggerCourseChangeEvent_Hook(auto original, IList* old_courses, IList* new_courses)
{
  original(old_courses, new_courses);
  live_debug_events::RecordEvent("deployment-course-change-event",
                                     json{{"oldCourseCount", count_list_items(old_courses)},
                                          {"newCourseCount", count_list_items(new_courses)}});
  capture_recent_model_events("deployment-course-change-event");
}

void DeploymentEvents_TriggerCourseEndEvent_Hook(auto original, IList* courses)
{
  original(courses);
  live_debug_events::RecordEvent("deployment-course-end-event",
                                     json{{"courseCount", count_list_items(courses)}});
  capture_recent_model_events("deployment-course-end-event");
}

void DeploymentEvents_TriggerSetCourseResponseEvent_Hook(auto original, long fleet_id, bool success,
                                                         bool is_recall_course, void* planned_course_data)
{
  original(fleet_id, success, is_recall_course, planned_course_data);
  live_debug_events::RecordEvent("deployment-set-course-response-event",
                                     json{{"fleetId", fleet_id},
                                          {"success", success},
                                          {"isRecallCourse", is_recall_course},
                                          {"hasCourseData", planned_course_data != nullptr}});
  capture_recent_model_events("deployment-set-course-response-event");
}

void DeploymentEvents_TriggerBattleStartEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  live_debug_events::RecordEvent(
      "deployment-battle-start-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-battle-start-event");
}

void DeploymentEvents_TriggerBattleEndEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  live_debug_events::RecordEvent(
      "deployment-battle-end-event",
      json{{"fleetCount", count_list_items(fleets)}, {"fleets", deployed_fleet_list_to_json(fleets)}});
  capture_recent_model_events("deployment-battle-end-event");
}

void DeploymentEvents_TriggerStaleFleetDataDetected_Hook(auto original)
{
  original();
  live_debug_events::RecordEvent("deployment-stale-fleet-data-detected-event", json::object());
  capture_recent_model_events("deployment-stale-fleet-data-detected-event");
}

json handle_recent_events(const std::string& request_id, const json& request)
{
  auto snapshot = live_debug_events::Snapshot(live_debug_recent_events_query_from_request(request));
  return make_ok_response(request_id, live_debug_recent_events_result(std::move(snapshot)));
}

json handle_clear_recent_events(const std::string& request_id)
{
  const auto cleared = live_debug_events::Clear();
  reset_recent_observations();
  return make_ok_response(request_id, json{{"cleared", cleared}});
}

json execute_live_debug_command(const json& request)
{
  const auto request_id = get_request_id(request);
  const auto cmd_it     = request.find("cmd");
  if (cmd_it == request.end() || !cmd_it->is_string()) {
    return make_error_response(request_id, "request must contain string field 'cmd'");
  }

  const auto cmd = cmd_it->get<std::string>();

  try {
    if (cmd == "ping") {
      return make_ok_response(request_id, live_debug_state_results::Ping());
    }
    if (cmd == "tracker-list") {
      return make_ok_response(request_id, live_debug_state_results::TrackerList());
    }
    if (cmd == "top-canvas") {
      return make_ok_response(request_id, live_debug_state_results::TopCanvas());
    }
    if (cmd == "fleetbar-state") {
      return make_ok_response(request_id, live_debug_state_results::FleetbarState());
    }
    if (cmd == "fleet-slots-state") {
      return make_ok_response(request_id, live_debug_state_results::FleetSlotsState());
    }
    if (cmd == "mine-viewer-state") {
      return make_ok_response(request_id, live_debug_state_results::MineViewerState());
    }
    if (cmd == "target-viewer-state") {
      return make_ok_response(request_id, live_debug_state_results::TargetViewerState());
    }
    if (cmd == "recent-events") {
      return handle_recent_events(request_id, request);
    }
    if (cmd == "clear-recent-events") {
      return handle_clear_recent_events(request_id);
    }

    return make_error_response(request_id, "unknown command");
  } catch (const std::exception& ex) {
    spdlog::warn("live_debug command '{}' failed: {}", cmd, ex.what());
    return make_error_response(request_id, ex.what());
  } catch (...) {
    spdlog::warn("live_debug command '{}' failed with unknown exception", cmd);
    return make_error_response(request_id, "command failed");
  }
}
} // namespace

std::string live_debug_handle_request_text(std::string_view request_text)
{
  nlohmann::json response;

  try {
    auto request = nlohmann::json::parse(request_text);
    if (!request.is_object()) {
      response = make_error_response("", "request must be a JSON object");
    } else {
      response = execute_live_debug_command(request);
    }
  } catch (const nlohmann::json::exception& ex) {
    spdlog::warn("live_debug rejected malformed JSON: {}", ex.what());
    response = make_error_response("", std::string("invalid JSON: ") + ex.what());
  } catch (const std::exception& ex) {
    spdlog::warn("live_debug failed to process request: {}", ex.what());
    response = make_error_response("", ex.what());
  } catch (...) {
    spdlog::warn("live_debug failed to process request: unknown exception");
    response = make_error_response("", "request handling failed");
  }

  return response.dump();
}

void InstallLiveDebugHooks()
{
  auto deployment_events_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Events", "DeploymentEvents");
  if (!deployment_events_helper.isValidHelper()) {
    deployment_events_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.PrimeServer.Events", "DeploymentEvents");
  }
  if (!deployment_events_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.PrimeServer.Events", "DeploymentEvents");
    return;
  }

  auto trigger_fleet_state_change_event = deployment_events_helper.GetMethod("TriggerFleetStateChangeEvent");
  if (trigger_fleet_state_change_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerFleetStateChangeEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_fleet_state_change_event, DeploymentEvents_TriggerFleetStateChangeEvent_Hook);
  }

  auto trigger_player_fleets_updated_event = deployment_events_helper.GetMethod("TriggerPlayerFleetsUpdatedEvent");
  if (trigger_player_fleets_updated_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerPlayerFleetsUpdatedEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_player_fleets_updated_event, DeploymentEvents_TriggerPlayerFleetsUpdatedEvent_Hook);
  }

  auto trigger_course_planned_event = deployment_events_helper.GetMethod("TriggerCoursePlannedEvent");
  if (trigger_course_planned_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCoursePlannedEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_planned_event, DeploymentEvents_TriggerCoursePlannedEvent_Hook);
  }

  auto trigger_course_start_event = deployment_events_helper.GetMethod("TriggerCourseStartEvent");
  if (trigger_course_start_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseStartEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_start_event, DeploymentEvents_TriggerCourseStartEvent_Hook);
  }

  auto trigger_course_change_event = deployment_events_helper.GetMethod("TriggerCourseChangeEvent");
  if (trigger_course_change_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseChangeEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_change_event, DeploymentEvents_TriggerCourseChangeEvent_Hook);
  }

  auto trigger_course_end_event = deployment_events_helper.GetMethod("TriggerCourseEndEvent");
  if (trigger_course_end_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerCourseEndEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_course_end_event, DeploymentEvents_TriggerCourseEndEvent_Hook);
  }

  auto trigger_set_course_response_event = deployment_events_helper.GetMethod("TriggerSetCourseResponseEvent");
  if (trigger_set_course_response_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerSetCourseResponseEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_set_course_response_event, DeploymentEvents_TriggerSetCourseResponseEvent_Hook);
  }

  auto trigger_battle_start_event = deployment_events_helper.GetMethod("TriggerBattleStartEvent");
  if (trigger_battle_start_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerBattleStartEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_battle_start_event, DeploymentEvents_TriggerBattleStartEvent_Hook);
  }

  auto trigger_battle_end_event = deployment_events_helper.GetMethod("TriggerBattleEndEvent");
  if (trigger_battle_end_event == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerBattleEndEvent");
  } else {
    SPUD_STATIC_DETOUR(trigger_battle_end_event, DeploymentEvents_TriggerBattleEndEvent_Hook);
  }

  auto trigger_stale_fleet_data_detected = deployment_events_helper.GetMethod("TriggerStateFleetDataDetected");
  if (trigger_stale_fleet_data_detected == nullptr) {
    ErrorMsg::MissingMethod("DeploymentEvents", "TriggerStateFleetDataDetected");
  } else {
    SPUD_STATIC_DETOUR(trigger_stale_fleet_data_detected, DeploymentEvents_TriggerStaleFleetDataDetected_Hook);
  }

}

void live_debug_tick(ScreenManager*)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  if (!g_logged_navigation_hook_tick_enter) {
    append_navigation_hook_trace_step("tick/first-enter", "live_debug_tick");
    g_logged_navigation_hook_tick_enter = true;
  }

  const bool had_pending_navigation_hook_note =
      kEnableLiveDebugUiPollingFromTick && g_pending_navigation_hook_note.pending;
  const auto pending_note = g_pending_navigation_hook_note;
  if (had_pending_navigation_hook_note) {
    append_navigation_hook_trace_step(
        "tick/enter-with-pending", pending_note.phase, pending_note.controller, pending_note.sender, pending_note.callbackContext);
    g_pending_navigation_hook_note.prePollCaptured = true;
    g_pending_navigation_hook_note.prePollTopCanvas = g_last_top_canvas;
    g_pending_navigation_hook_note.prePollNavigationInteraction = g_last_navigation_interaction;
  }

  if (kEnableLiveDebugUiPollingFromTick) {
    if (!g_recent_observations_initialized) {
      initialize_recent_model_observations("screen-manager-update");
    } else {
      poll_recent_ui_events();
    }
  }

  if (!g_logged_navigation_hook_tick_after_ui_poll) {
    append_navigation_hook_trace_step("tick/first-after-ui-poll", "live_debug_tick");
    g_logged_navigation_hook_tick_after_ui_poll = true;
  }

  if (had_pending_navigation_hook_note) {
    append_navigation_hook_trace_step(
        "tick/after-ui-poll", pending_note.phase, pending_note.controller, pending_note.sender, pending_note.callbackContext);
  }

  flush_pending_navigation_hook_note();

  if (had_pending_navigation_hook_note) {
    append_navigation_hook_trace_step(
        "tick/after-flush", pending_note.phase, pending_note.controller, pending_note.sender, pending_note.callbackContext);
  }

  live_debug_process_request_cycle();
}

void live_debug_record_space_action_warp_cancel(FleetBarViewController* fleet_bar, FleetPlayerData* fleet,
                                                bool has_primary, bool has_secondary, bool has_queue,
                                                bool has_queue_clear, bool has_recall, bool has_repair,
                                                bool has_recall_cancel, bool force_space_action,
                                                int visible_pre_scan_target_count, bool mining_viewer_visible,
                                                bool star_node_viewer_visible,
                                                bool navigation_interaction_visible)
{
  live_debug_events::RecordEvent(
      "space-action-cancel-warp",
      json{{"selectedIndex", fleet_bar ? get_selected_fleet_index(fleet_bar) : -1},
           {"fleetPresent", fleet != nullptr},
           {"fleetId", fleet ? fleet->Id : 0},
           {"currentState", fleet ? static_cast<int>(fleet->CurrentState) : -1},
         {"currentStateName", fleet ? fleet_state_name_from_value(static_cast<int>(fleet->CurrentState)) : "None"},
           {"previousState", fleet ? static_cast<int>(fleet->PreviousState) : -1},
         {"previousStateName", fleet ? fleet_state_name_from_value(static_cast<int>(fleet->PreviousState)) : "None"},
           {"inputs",
            {"primary", has_primary},
            {"secondary", has_secondary},
            {"queue", has_queue},
            {"queueClear", has_queue_clear},
            {"recall", has_recall},
            {"repair", has_repair},
            {"recallCancel", has_recall_cancel},
            {"forceSpaceAction", force_space_action}},
           {"visiblePreScanTargetCount", visible_pre_scan_target_count},
           {"miningViewerVisible", mining_viewer_visible},
           {"starNodeViewerVisible", star_node_viewer_visible},
           {"navigationInteractionVisible", navigation_interaction_visible}});
}

void live_debug_record_space_action_warp_cancel_suppressed(FleetBarViewController* fleet_bar,
                                                           FleetPlayerData* fleet, bool has_primary,
                                                           bool has_secondary, bool has_queue,
                                                           bool has_queue_clear, bool has_recall,
                                                           bool has_repair, bool has_recall_cancel,
                                                           bool force_space_action,
                                                           int visible_pre_scan_target_count,
                                                           bool mining_viewer_visible,
                                                           bool star_node_viewer_visible,
                                                           bool navigation_interaction_visible)
{
  live_debug_events::RecordEvent(
      "space-action-cancel-warp-suppressed",
      json{{"selectedIndex", fleet_bar ? get_selected_fleet_index(fleet_bar) : -1},
           {"fleetPresent", fleet != nullptr},
           {"fleetId", fleet ? fleet->Id : 0},
           {"currentState", fleet ? static_cast<int>(fleet->CurrentState) : -1},
         {"currentStateName", fleet ? fleet_state_name_from_value(static_cast<int>(fleet->CurrentState)) : "None"},
           {"previousState", fleet ? static_cast<int>(fleet->PreviousState) : -1},
         {"previousStateName", fleet ? fleet_state_name_from_value(static_cast<int>(fleet->PreviousState)) : "None"},
           {"inputs",
            {"primary", has_primary},
            {"secondary", has_secondary},
            {"queue", has_queue},
            {"queueClear", has_queue_clear},
            {"recall", has_recall},
            {"repair", has_repair},
            {"recallCancel", has_recall_cancel},
            {"forceSpaceAction", force_space_action}},
           {"suppressedReason", "mouse-primary-context"},
           {"visiblePreScanTargetCount", visible_pre_scan_target_count},
           {"miningViewerVisible", mining_viewer_visible},
           {"starNodeViewerVisible", star_node_viewer_visible},
           {"navigationInteractionVisible", navigation_interaction_visible}});
}

void live_debug_record_incoming_fleet_materialized(std::string_view phase, int target_type,
                                                   uint64_t target_fleet_id,
                                                   int quick_scan_fleet_type,
                                                   uint64_t quick_scan_target_fleet_id,
                                                   std::string_view quick_scan_target_id)
{
  live_debug_events::RecordEvent(
      "incoming-fleet-materialized",
      json{{"phase", phase},
           {"targetType", target_type},
           {"targetTypeName", incoming_attack_target_type_name(target_type)},
           {"targetFleetId", target_fleet_id},
           {"quickScanFleetType", quick_scan_fleet_type},
           {"quickScanFleetTypeName", deployed_fleet_type_name(quick_scan_fleet_type)},
           {"quickScanTargetFleetId", quick_scan_target_fleet_id},
           {"quickScanTargetId", quick_scan_target_id}});
}

void live_debug_record_toast_notification(std::string_view source,
                                          const void* toast,
                                          int state,
                                          std::string_view title)
{
  live_debug_events::RecordEvent(
      "toast-notification-observed",
      json{{"source", source},
           {"toastPointer", pointer_to_string(toast)},
           {"state", state},
           {"title", title}});
}

void live_debug_record_incoming_attack_notification_context(std::string_view source,
                                                           std::string_view body,
                                                           int candidate_count,
                                                           uint64_t selected_fleet_id,
                                                           std::string_view selected_ship_name,
                                                           int selected_state,
                                                           int attacker_fleet_type)
{
  live_debug_events::RecordEvent(
      "incoming-attack-notification-context",
      json{{"source", source},
           {"body", body},
           {"candidateCount", candidate_count},
           {"selectedFleetId", selected_fleet_id},
           {"selectedShipName", selected_ship_name},
           {"selectedState", selected_state},
           {"selectedStateName", fleet_state_name_from_value(selected_state)},
           {"attackerFleetType", attacker_fleet_type},
           {"attackerFleetTypeName", deployed_fleet_type_name(attacker_fleet_type)}});
}

void live_debug_record_navigation_interaction(std::string_view phase,
                                              std::string_view controller_pointer,
                                              bool has_context,
                                              int context_data_state,
                                              int input_interaction_type,
                                              std::string_view user_id,
                                              bool is_marauder,
                                              int threat_level,
                                              bool valid_navigation_input,
                                              bool show_set_course_arm,
                                              int64_t location_translation_id,
                                              std::string_view poi_pointer,
                                              std::string_view sender_pointer,
                                              std::string_view sender_class_namespace,
                                              std::string_view sender_class_name,
                                              std::string_view callback_context_pointer,
                                              std::string_view callback_context_class_namespace,
                                              std::string_view callback_context_class_name)
{
  json details{{"source", "NavigationInteractionUIViewController"},
               {"phase", phase},
               {"pointer", controller_pointer},
               {"hasContext", has_context}};

  if (!sender_pointer.empty()) {
    details["senderPointer"] = sender_pointer;
    details["senderClassNamespace"] = sender_class_namespace;
    details["senderClassName"] = sender_class_name;
  }

  if (!callback_context_pointer.empty()) {
    details["callbackContextPointer"] = callback_context_pointer;
    details["callbackContextClassNamespace"] = callback_context_class_namespace;
    details["callbackContextClassName"] = callback_context_class_name;
  }

  if (has_context) {
    details["contextDataState"] = context_data_state;
    details["contextDataStateName"] = navigation_context_data_state_name(context_data_state);
    details["inputInteractionType"] = input_interaction_type;
    details["inputInteractionTypeName"] = input_interaction_type_name(input_interaction_type);
    details["userId"] = user_id;
    details["isMarauder"] = is_marauder;
    details["threatLevel"] = threat_level;
    details["threatLevelName"] = navigation_threat_level_name(threat_level);
    details["validNavigationInput"] = valid_navigation_input;
    details["showSetCourseArm"] = show_set_course_arm;
    details["locationTranslationId"] = location_translation_id;
    details["poiPointer"] = poi_pointer;
  }

  live_debug_events::RecordEvent("navigation-interaction-lifecycle", std::move(details));
}

