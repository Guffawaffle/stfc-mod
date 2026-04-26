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
#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_fleet_runtime_serializers.h"
#include "patches/live_debug_request_dispatch.h"
#include "patches/live_debug_ui_serializers.h"
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
#include "prime/CanvasController.h"
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
constexpr int64_t          kMineTimerBucketSeconds = 30;
constexpr int              kTopCanvasChildNameLimit = 24;


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
bool is_navigation_interaction_top_canvas(const TopCanvasObservation& observation);

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

bool same_top_canvas_observation(const TopCanvasObservation& left, const TopCanvasObservation& right)
{
  return left.found == right.found && left.pointer == right.pointer &&
  left.className == right.className && left.classNamespace == right.classNamespace &&
  left.name == right.name && left.visible == right.visible && left.enabled == right.enabled &&
  left.internalVisible == right.internalVisible && left.activeChildNames == right.activeChildNames;
}

bool same_fleet_observation(const FleetObservation& left, const FleetObservation& right)
{
  return left.tracked == right.tracked && left.selectedIndex == right.selectedIndex &&
      left.hasController == right.hasController && left.hasFleet == right.hasFleet &&
      left.fleetId == right.fleetId && left.currentState == right.currentState &&
  left.previousState == right.previousState && left.cargoFillBasisPoints == right.cargoFillBasisPoints &&
      left.hullName == right.hullName;
}

bool same_fleet_slot_observation(const FleetSlotObservation& left, const FleetSlotObservation& right)
{
  return left.slotIndex == right.slotIndex && left.present == right.present &&
      left.fleetId == right.fleetId && left.currentState == right.currentState &&
      left.previousState == right.previousState && left.cargoFillBasisPoints == right.cargoFillBasisPoints &&
      left.hullName == right.hullName;
}

bool same_mine_viewer_observation(const MineViewerObservation& left, const MineViewerObservation& right)
{
  return left.miningViewerTracked == right.miningViewerTracked && left.enabled == right.enabled &&
      left.isActiveAndEnabled == right.isActiveAndEnabled && left.isInfoShown == right.isInfoShown &&
      left.hasParent == right.hasParent && left.parentIsShowing == right.parentIsShowing &&
      left.occupiedState == right.occupiedState && left.hasScanEngageButtons == right.hasScanEngageButtons &&
      left.hasTimer == right.hasTimer && left.timerState == right.timerState &&
      left.timerType == right.timerType && left.timerRemainingBucket == right.timerRemainingBucket &&
      left.starNodeViewerTracked == right.starNodeViewerTracked &&
      left.starNodeEnabled == right.starNodeEnabled &&
      left.starNodeActiveAndEnabled == right.starNodeActiveAndEnabled;
}

bool same_target_viewer_observation(const TargetViewerObservation& left, const TargetViewerObservation& right)
{
  return left.preScanTargetTracked == right.preScanTargetTracked &&
      left.preScanTargetPointer == right.preScanTargetPointer &&
      left.preScanStationTargetTracked == right.preScanStationTargetTracked &&
      left.preScanStationTargetPointer == right.preScanStationTargetPointer &&
      left.celestialViewerTracked == right.celestialViewerTracked &&
      left.celestialViewerPointer == right.celestialViewerPointer;
}

bool same_station_warning_observation(const StationWarningObservation& left,
                                      const StationWarningObservation& right)
{
  return left.tracked == right.tracked && left.pointer == right.pointer &&
      left.hasContext == right.hasContext && left.targetType == right.targetType &&
      left.targetFleetId == right.targetFleetId && left.targetUserId == right.targetUserId &&
      left.quickScanTargetFleetId == right.quickScanTargetFleetId &&
      left.quickScanTargetId == right.quickScanTargetId;
}

    bool same_navigation_interaction_observation(const NavigationInteractionObservation& left,
                     const NavigationInteractionObservation& right)
    {
      if (left.tracked != right.tracked || left.trackedCount != right.trackedCount ||
          left.entries.size() != right.entries.size()) {
        return false;
      }

      for (size_t index = 0; index < left.entries.size(); ++index) {
        const auto& left_entry = left.entries[index];
        const auto& right_entry = right.entries[index];
        if (left_entry.pointer != right_entry.pointer ||
            left_entry.hasContext != right_entry.hasContext ||
            left_entry.contextDataState != right_entry.contextDataState ||
            left_entry.inputInteractionType != right_entry.inputInteractionType ||
            left_entry.userId != right_entry.userId ||
            left_entry.isMarauder != right_entry.isMarauder ||
            left_entry.threatLevel != right_entry.threatLevel ||
            left_entry.validNavigationInput != right_entry.validNavigationInput ||
            left_entry.showSetCourseArm != right_entry.showSetCourseArm ||
            left_entry.locationTranslationId != right_entry.locationTranslationId ||
            left_entry.poiPointer != right_entry.poiPointer) {
          return false;
        }
      }

      return true;
    }

bool is_meaningful_mine_viewer_observation(const MineViewerObservation& observation)
{
  return (observation.miningViewerTracked &&
          (observation.isActiveAndEnabled || observation.parentIsShowing || observation.hasParent)) ||
      (observation.starNodeViewerTracked && observation.starNodeActiveAndEnabled);
}

TopCanvasObservation observe_top_canvas();
FleetObservation observe_fleetbar();
void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current);
void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current);

std::vector<std::string> collect_active_child_names(Transform* parent)
{
  std::vector<std::string> names;
  if (!parent) {
    return names;
  }

  const auto child_count = parent->childCount;
  if (child_count <= 0) {
    return names;
  }

  names.reserve(std::min(child_count, kTopCanvasChildNameLimit));

  for (int index = 0; index < child_count && static_cast<int>(names.size()) < kTopCanvasChildNameLimit; ++index) {
    auto child_transform = parent->GetChild(index);
    if (!child_transform) {
      continue;
    }

    auto child_object = child_transform->gameObject;
    if (!child_object || !child_object->activeInHierarchy) {
      continue;
    }

    if (auto child_name = child_object->name; child_name) {
      names.push_back(to_string(child_name));
    } else {
      names.emplace_back("");
    }
  }

  return names;
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

  if (auto hull = fleet->Hull; hull && hull->Name) {
    observation.hullName = to_string(hull->Name);
  }

  return observation;
}

std::array<FleetSlotObservation, kFleetIndexMax> observe_fleet_slots()
{
  std::array<FleetSlotObservation, kFleetIndexMax> observations{};
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  for (int slot_index = 0; slot_index < kFleetIndexMax; ++slot_index) {
    auto& observation = observations[static_cast<size_t>(slot_index)];
    observation.slotIndex = slot_index;

    if (!fleet_bar) {
      continue;
    }

    observation = observe_fleet_slot(slot_index, fleet_bar);
  }

  return observations;
}

StationWarningObservation observe_station_warning()
{
  append_ui_observer_trace_step("station/enter", "observe_station_warning");
  StationWarningObservation observation;
  auto controller = GetLatestTrackedObject<StationWarningViewController>();
  if (controller) {
    g_ui_observer_trace_current_poll = true;
  }
  append_ui_observer_trace_step("station/after-get-controller", "observe_station_warning", controller);
  observation.tracked = controller != nullptr;

  if (!controller) {
    append_ui_observer_trace_step("station/no-controller", "observe_station_warning");
    return observation;
  }

  observation.pointer = pointer_to_string(controller);

  append_ui_observer_trace_step("station/before-canvas-context", "observe_station_warning", controller);
  auto context = controller->CanvasContext;
  append_ui_observer_trace_step("station/after-canvas-context", "observe_station_warning", controller, context);
  observation.hasContext = context != nullptr;
  if (!context) {
    append_ui_observer_trace_step("station/no-context", "observe_station_warning", controller);
    return observation;
  }

  append_ui_observer_trace_step("station/before-target-fields", "observe_station_warning", controller, context);
  observation.targetType = static_cast<int>(context->TargetType);
  observation.targetFleetId = static_cast<uint64_t>(context->TargetFleetId);
  append_ui_observer_trace_step("station/after-target-fields", "observe_station_warning", controller, context);

  if (auto target_user_id = context->TargetUserId; target_user_id) {
    append_ui_observer_trace_step("station/before-target-user-id", "observe_station_warning", controller, context, target_user_id);
    observation.targetUserId = to_string(target_user_id);
    append_ui_observer_trace_step("station/after-target-user-id", "observe_station_warning", controller, context, target_user_id);
  }

  append_ui_observer_trace_step("station/before-quick-scan-result", "observe_station_warning", controller, context);
  if (auto quick_scan_result = context->QuickScanResult; quick_scan_result) {
    append_ui_observer_trace_step("station/after-quick-scan-result", "observe_station_warning", controller, context, quick_scan_result);
    observation.quickScanTargetFleetId = static_cast<uint64_t>(quick_scan_result->TargetFleetId);
    if (auto quick_scan_target_id = quick_scan_result->TargetId; quick_scan_target_id) {
      append_ui_observer_trace_step("station/before-quick-scan-target-id", "observe_station_warning", controller, quick_scan_result, quick_scan_target_id);
      observation.quickScanTargetId = to_string(quick_scan_target_id);
      append_ui_observer_trace_step("station/after-quick-scan-target-id", "observe_station_warning", controller, quick_scan_result, quick_scan_target_id);
    }
  } else {
    append_ui_observer_trace_step("station/no-quick-scan-result", "observe_station_warning", controller, context);
  }

  append_ui_observer_trace_step("station/return", "observe_station_warning", controller, context);
  return observation;
}

NavigationInteractionObservation observe_navigation_interaction()
{
  append_ui_observer_trace_step("nav/enter", "observe_navigation_interaction");
  NavigationInteractionObservation observation;
  const auto controllers = GetTrackedObjects<NavigationInteractionUIViewController>();
  if (!controllers.empty()) {
    g_ui_observer_trace_current_poll = true;
  }
  append_ui_observer_trace_step("nav/after-get-controllers", "observe_navigation_interaction");
  observation.tracked = !controllers.empty();
  observation.trackedCount = controllers.size();

  if (controllers.empty()) {
    append_ui_observer_trace_step("nav/no-controllers", "observe_navigation_interaction");
    return observation;
  }

  observation.entries.reserve(controllers.size());
  for (auto* controller : controllers) {
    append_ui_observer_trace_step("nav/before-entry", "observe_navigation_interaction", controller);
    NavigationInteractionObservation::Entry entry;
    entry.pointer = pointer_to_string(controller);

    append_ui_observer_trace_step("nav/before-canvas-context", "observe_navigation_interaction", controller);
    auto context = controller->CanvasContext;
    append_ui_observer_trace_step("nav/after-canvas-context", "observe_navigation_interaction", controller, context);
    entry.hasContext = context != nullptr;
    if (context) {
      append_ui_observer_trace_step("nav/before-context-fields", "observe_navigation_interaction", controller, context);
      entry.contextDataState = context->ContextDataState;
      entry.inputInteractionType = context->InputInteractionType;
      append_ui_observer_trace_step("nav/after-basic-context-fields", "observe_navigation_interaction", controller, context);
      if (auto user_id = context->UserId; user_id) {
        append_ui_observer_trace_step("nav/before-user-id", "observe_navigation_interaction", controller, context, user_id);
        entry.userId = to_string(user_id);
        append_ui_observer_trace_step("nav/after-user-id", "observe_navigation_interaction", controller, context, user_id);
      }
      entry.isMarauder = context->IsMarauder;
      entry.threatLevel = context->ThreatLevel;
      entry.validNavigationInput = context->ValidNavigationInput;
      entry.showSetCourseArm = context->ShowSetCourseArm;
      entry.locationTranslationId = context->LocationTranslationId;
      append_ui_observer_trace_step("nav/after-extra-context-fields", "observe_navigation_interaction", controller, context);
      if (auto poi = context->Poi; poi) {
        entry.poiPointer = pointer_to_string(poi);
        append_ui_observer_trace_step("nav/after-poi-pointer", "observe_navigation_interaction", controller, context, poi);
      }
    }

    observation.entries.push_back(std::move(entry));
    append_ui_observer_trace_step("nav/after-entry", "observe_navigation_interaction", controller, context);
  }

  append_ui_observer_trace_step("nav/return", "observe_navigation_interaction");
  return observation;
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
      kEnableLiveDebugStationWarningPolling ? observe_station_warning() : StationWarningObservation{};
    const auto navigation_interaction = kEnableLiveDebugNavigationInteractionPolling
      ? observe_navigation_interaction()
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

bool is_navigation_interaction_top_canvas(const TopCanvasObservation& observation)
{
  return observation.found && observation.name == "NavigationInteractionUI";
}

void append_top_canvas_change_events(const TopCanvasObservation& previous,
                                     const TopCanvasObservation& current)
{
  live_debug_events::RecordEvent(classify_top_canvas_change_kind(previous, current),
                                 json{{"from", top_canvas_observation_to_json(previous)},
                                      {"to", top_canvas_observation_to_json(current)}});
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

void append_station_warning_change_events(const StationWarningObservation& previous,
                                          const StationWarningObservation& current)
{
  live_debug_events::RecordEvent(classify_station_warning_change_kind(previous, current),
                                 json{{"from", station_warning_observation_to_json(previous)},
                                      {"to", station_warning_observation_to_json(current)}});
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

void append_navigation_interaction_change_events(const NavigationInteractionObservation& previous,
                                                 const NavigationInteractionObservation& current)
{
  live_debug_events::RecordEvent(classify_navigation_interaction_change_kind(previous, current),
                                 json{{"from", navigation_interaction_observation_to_json(previous)},
                                      {"to", navigation_interaction_observation_to_json(current)}});
  append_navigation_hook_actionable_follow_up_event(previous, current);
  append_navigation_poll_actionable_event(previous, current);
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
      kEnableLiveDebugStationWarningPolling ? observe_station_warning() : g_last_station_warning;
  append_ui_observer_trace_step("poll/after-station-warning", "poll_recent_ui_events");
  const bool trace_navigation_poll =
      kEnableLiveDebugNavigationInteractionPolling &&
      (is_navigation_interaction_top_canvas(top_canvas) ||
       is_navigation_interaction_top_canvas(g_last_top_canvas));
  if (trace_navigation_poll) {
    append_navigation_hook_trace_step("poll/before-observe-navigation-interaction", "poll_recent_ui_events");
  }
  const auto navigation_interaction = kEnableLiveDebugNavigationInteractionPolling
      ? observe_navigation_interaction()
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
    append_navigation_interaction_change_events(g_last_navigation_interaction, navigation_interaction);
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

const char* classify_fleet_transition_kind(const FleetObservation& previous, const FleetObservation& current)
{
  const auto from = previous.currentState;
  const auto to   = current.currentState;

  if (!previous.hasFleet && current.hasFleet) {
    return "selected-fleet-visible";
  }
  if (previous.hasFleet && !current.hasFleet) {
    return "selected-fleet-cleared";
  }
  if (from == static_cast<int>(FleetState::Docked) && to == static_cast<int>(FleetState::Repairing)) {
    return "fleet-repair-started";
  }
  if (from == static_cast<int>(FleetState::Repairing) && to == static_cast<int>(FleetState::Docked)) {
    return "fleet-repair-completed";
  }
  if (to == static_cast<int>(FleetState::Battling) && from != static_cast<int>(FleetState::Battling)) {
    return "fleet-combat-started";
  }
  if (from == static_cast<int>(FleetState::Battling) && to != static_cast<int>(FleetState::Battling)) {
    return "fleet-combat-ended";
  }
  if (to == static_cast<int>(FleetState::WarpCharging)) {
    return "fleet-warp-started";
  }
  if (to == static_cast<int>(FleetState::Warping)) {
    return "fleet-warp-engaged";
  }
  if (from == static_cast<int>(FleetState::Warping) && to == static_cast<int>(FleetState::Impulsing)) {
    return "fleet-arrived-in-system";
  }
  if (to == static_cast<int>(FleetState::Docked) && from != static_cast<int>(FleetState::Repairing)) {
    return "fleet-docked";
  }
  if (to == static_cast<int>(FleetState::Mining) && from != static_cast<int>(FleetState::Mining)) {
    return "fleet-mining-started";
  }
  if (from == static_cast<int>(FleetState::Mining) && to != static_cast<int>(FleetState::Mining)) {
    return "fleet-mining-stopped";
  }

  return "fleet-state-changed";
}

json fleet_transition_to_json(const FleetObservation& previous, const FleetObservation& current)
{
  return json{{"selectedIndex", current.selectedIndex},
              {"fleetId", current.fleetId},
              {"hullName", current.hullName},
              {"fromState", previous.currentState},
              {"fromStateName", fleet_state_name_from_value(previous.currentState)},
              {"toState", current.currentState},
              {"toStateName", fleet_state_name_from_value(current.currentState)},
              {"modelPreviousState", current.previousState},
              {"modelPreviousStateName", fleet_state_name_from_value(current.previousState)},
              {"cargoFillBasisPoints", current.cargoFillBasisPoints}};
}

void append_fleet_change_events(const FleetObservation& previous, const FleetObservation& current)
{
  const bool selection_changed =
      previous.selectedIndex != current.selectedIndex || previous.fleetId != current.fleetId;

  if (selection_changed) {
    live_debug_events::RecordEvent(
        "selected-fleet-changed",
        json{{"from", fleet_observation_to_json(previous)}, {"to", fleet_observation_to_json(current)}});
  }
}

const char* classify_fleet_slot_transition_kind(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  const auto from = previous.currentState;
  const auto to = current.currentState;

  if (!previous.present && current.present) {
    return "fleet-slot-visible";
  }
  if (previous.present && !current.present) {
    return "fleet-slot-cleared";
  }
  if (from == static_cast<int>(FleetState::Docked) && to == static_cast<int>(FleetState::Repairing)) {
    return "fleet-slot-repair-started";
  }
  if (from == static_cast<int>(FleetState::Repairing) && to == static_cast<int>(FleetState::Docked)) {
    return "fleet-slot-repair-completed";
  }
  if (to == static_cast<int>(FleetState::Battling) && from != static_cast<int>(FleetState::Battling)) {
    return "fleet-slot-combat-started";
  }
  if (from == static_cast<int>(FleetState::Battling) && to != static_cast<int>(FleetState::Battling)) {
    return "fleet-slot-combat-ended";
  }
  if (to == static_cast<int>(FleetState::WarpCharging)) {
    return "fleet-slot-warp-started";
  }
  if (to == static_cast<int>(FleetState::Warping)) {
    return "fleet-slot-warp-engaged";
  }
  if (from == static_cast<int>(FleetState::Warping) && to == static_cast<int>(FleetState::Impulsing)) {
    return "fleet-slot-arrived-in-system";
  }
  if (to == static_cast<int>(FleetState::Docked) && from != static_cast<int>(FleetState::Repairing)) {
    return "fleet-slot-docked";
  }
  if (to == static_cast<int>(FleetState::Mining) && from != static_cast<int>(FleetState::Mining)) {
    return "fleet-slot-mining-started";
  }
  if (from == static_cast<int>(FleetState::Mining) && to != static_cast<int>(FleetState::Mining)) {
    return "fleet-slot-mining-stopped";
  }

  return "fleet-slot-state-changed";
}

json fleet_slot_transition_to_json(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  return json{{"slotIndex", current.slotIndex},
              {"selected", current.selected},
              {"fleetId", current.fleetId},
              {"hullName", current.hullName},
              {"fromState", previous.currentState},
              {"fromStateName", fleet_state_name_from_value(previous.currentState)},
              {"toState", current.currentState},
              {"toStateName", fleet_state_name_from_value(current.currentState)},
              {"modelPreviousState", current.previousState},
              {"modelPreviousStateName", fleet_state_name_from_value(current.previousState)},
              {"cargoFillBasisPoints", current.cargoFillBasisPoints}};
}

void append_fleet_slot_change_events(const FleetSlotObservation& previous, const FleetSlotObservation& current)
{
  bool emitted = false;
  const bool same_fleet = previous.present && current.present && previous.fleetId == current.fleetId;
  const bool fleet_changed = previous.present && current.present && previous.fleetId != current.fleetId;

  if (fleet_changed) {
    live_debug_events::RecordEvent("fleet-slot-fleet-changed",
                                   json{{"slotIndex", current.slotIndex},
                                        {"from", fleet_slot_observation_to_json(previous)},
                                        {"to", fleet_slot_observation_to_json(current)}});
    emitted = true;
  }

  if ((previous.present != current.present) || (same_fleet && previous.currentState != current.currentState)) {
    live_debug_events::RecordEvent(classify_fleet_slot_transition_kind(previous, current),
                                   fleet_slot_transition_to_json(previous, current));
    emitted = true;
  }

  if (same_fleet && previous.hullName != current.hullName) {
    live_debug_events::RecordEvent("fleet-slot-hull-changed",
                                   json{{"slotIndex", current.slotIndex},
                                        {"selected", current.selected},
                                        {"fleetId", current.fleetId},
                                        {"fromHullName", previous.hullName},
                                        {"toHullName", current.hullName},
                                        {"state", current.currentState},
                                        {"stateName", fleet_state_name_from_value(current.currentState)}});
    emitted = true;
  }

  if (same_fleet && current.cargoFillBasisPoints > previous.cargoFillBasisPoints) {
    live_debug_events::RecordEvent("fleet-slot-cargo-gained",
                                   json{{"slotIndex", current.slotIndex},
                                        {"selected", current.selected},
                                        {"fleetId", current.fleetId},
                                        {"fromCargoFillBasisPoints", previous.cargoFillBasisPoints},
                                        {"toCargoFillBasisPoints", current.cargoFillBasisPoints},
                                        {"deltaCargoFillBasisPoints", current.cargoFillBasisPoints - previous.cargoFillBasisPoints},
                                        {"state", current.currentState},
                                        {"stateName", fleet_state_name_from_value(current.currentState)}});
    emitted = true;
  }

  if (!emitted) {
    live_debug_events::RecordEvent("fleet-slot-changed", fleet_slot_observation_to_json(current));
  }
}

json handle_ping(const std::string& request_id)
{
  return make_ok_response(request_id, json{{"pong", true}, {"version", VER_PRODUCT_VERSION_STR}});
}

json handle_tracker_list(const std::string& request_id)
{
  const auto summaries = GetTrackedObjectSummary();

  json   classes = json::array();
  size_t total_objects = 0;
  for (const auto& summary : summaries) {
    total_objects += summary.count;
    const auto full_name = summary.classNamespace.empty()
        ? summary.className
        : summary.classNamespace + "." + summary.className;

    classes.push_back(json{{"classPointer", summary.classPointer},
                 {"classNamespace", summary.classNamespace},
                           {"className", summary.className},
                           {"fullName", full_name},
                           {"count", summary.count}});
  }

  return make_ok_response(request_id, json{{"trackedClassCount", summaries.size()},
                                           {"trackedObjectCount", total_objects},
                                           {"classes", std::move(classes)}});
}

json handle_top_canvas(const std::string& request_id)
{
  return make_ok_response(request_id, top_canvas_observation_to_json(observe_top_canvas()));
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

json handle_fleetbar_state(const std::string& request_id)
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  json result = {{"tracked", fleet_bar != nullptr}};

  if (!fleet_bar) {
    return make_ok_response(request_id, result);
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;

  result["pointer"]       = pointer_to_string(fleet_bar);
  result["selectedIndex"] = get_selected_fleet_index(fleet_bar);
  result["hasController"] = fleet_controller != nullptr;
  result["fleet"]         = fleet_to_json(fleet);

  return make_ok_response(request_id, result);
}

json handle_fleet_slots_state(const std::string& request_id)
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  const auto slot_observations = observe_fleet_slots();
  size_t present_count = 0;
  for (const auto& slot : slot_observations) {
    if (slot.present) {
      ++present_count;
    }
  }

  return make_ok_response(request_id,
                          json{{"fleetBarTracked", fleet_bar != nullptr},
                               {"slotCount", kFleetIndexMax},
                               {"presentSlotCount", present_count},
                               {"slots", fleet_slots_to_json(slot_observations)}});
}

FleetObservation observe_fleetbar()
{
  FleetObservation observation;
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
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

    if (auto hull = fleet->Hull; hull && hull->Name) {
      observation.hullName = to_string(hull->Name);
    }
  }

  return observation;
}

json timer_context_to_json(TimerDataContext* timer)
{
  if (!timer) {
    return nullptr;
  }

  return json{{"remainingTicks", timer->RemainingTime.Ticks},
              {"remainingSeconds", timer->RemainingTime.TotalSeconds()},
              {"timerType", timer->TimerTypeValue},
              {"timerState", timer->TimerStateValue},
              {"showTimerLabel", timer->ShowTimerLabel}};
}

int64_t get_timer_remaining_bucket(TimerDataContext* timer)
{
  if (!timer) {
    return -1;
  }

  return static_cast<int64_t>(timer->RemainingTime.TotalSeconds()) / kMineTimerBucketSeconds;
}

json mining_viewer_to_json(MiningObjectViewerWidget* mining_viewer)
{
  if (!mining_viewer) {
    return nullptr;
  }

  auto parent = mining_viewer->Parent;

  return json{{"pointer", pointer_to_string(mining_viewer)},
              {"enabled", mining_viewer->enabled},
              {"isActiveAndEnabled", mining_viewer->isActiveAndEnabled},
              {"isInfoShown", mining_viewer->IsInfoShown},
              {"hasParent", parent != nullptr},
              {"parentIsShowing", parent ? parent->IsShowing : false},
              {"occupiedState", static_cast<int>(mining_viewer->_occupiedState)},
              {"occupiedStateName", occupied_state_name_from_value(static_cast<int>(mining_viewer->_occupiedState))},
              {"hasScanEngageButtons", mining_viewer->_scanEngageButtonsWidget != nullptr},
              {"timer", timer_context_to_json(mining_viewer->_miningTimerWidgetContext)}};
}

json star_node_viewer_to_json(StarNodeObjectViewerWidget* star_node_viewer)
{
  if (!star_node_viewer) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(star_node_viewer)},
              {"enabled", star_node_viewer->enabled},
              {"isActiveAndEnabled", star_node_viewer->isActiveAndEnabled}};
}

json prescan_target_to_json(PreScanTargetWidget* prescan_target)
{
  if (!prescan_target) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(prescan_target)}};
}

json celestial_viewer_to_json(CelestialObjectViewerWidget* celestial_viewer)
{
  if (!celestial_viewer) {
    return nullptr;
  }

  return json{{"pointer", pointer_to_string(celestial_viewer)}};
}

TargetViewerObservation observe_target_viewer()
{
  TargetViewerObservation observation;
  auto prescan_target = GetLatestTrackedObject<PreScanTargetWidget>();
  auto prescan_station_target = GetLatestTrackedObject<PreScanStationTargetWidget>();
  auto celestial_viewer = GetLatestTrackedObject<CelestialObjectViewerWidget>();

  observation.preScanTargetTracked = prescan_target != nullptr;
  observation.preScanStationTargetTracked = prescan_station_target != nullptr;
  observation.celestialViewerTracked = celestial_viewer != nullptr;

  if (prescan_target) {
    observation.preScanTargetPointer = pointer_to_string(prescan_target);
  }
  if (prescan_station_target) {
    observation.preScanStationTargetPointer = pointer_to_string(prescan_station_target);
  }
  if (celestial_viewer) {
    observation.celestialViewerPointer = pointer_to_string(celestial_viewer);
  }

  return observation;
}

MineViewerObservation observe_mine_viewer()
{
  MineViewerObservation observation;
  auto mining_viewer = GetLatestTrackedObject<MiningObjectViewerWidget>();
  auto star_node_viewer = GetLatestTrackedObject<StarNodeObjectViewerWidget>();

  observation.miningViewerTracked = mining_viewer != nullptr;
  observation.starNodeViewerTracked = star_node_viewer != nullptr;

  if (mining_viewer) {
    auto timer = mining_viewer->_miningTimerWidgetContext;
    auto parent = mining_viewer->Parent;

    observation.miningPointer = pointer_to_string(mining_viewer);
    observation.enabled = mining_viewer->enabled;
    observation.isActiveAndEnabled = mining_viewer->isActiveAndEnabled;
    observation.isInfoShown = mining_viewer->IsInfoShown;
    observation.hasParent = parent != nullptr;
    observation.parentIsShowing = parent ? parent->IsShowing : false;
    observation.occupiedState = static_cast<int>(mining_viewer->_occupiedState);
    observation.hasScanEngageButtons = mining_viewer->_scanEngageButtonsWidget != nullptr;
    observation.hasTimer = timer != nullptr;

    if (timer) {
      observation.timerState = timer->TimerStateValue;
      observation.timerType = timer->TimerTypeValue;
      observation.timerRemainingSeconds = static_cast<int64_t>(timer->RemainingTime.TotalSeconds());
      observation.timerRemainingBucket = get_timer_remaining_bucket(timer);
    }
  }

  if (star_node_viewer) {
    observation.starNodePointer = pointer_to_string(star_node_viewer);
    observation.starNodeEnabled = star_node_viewer->enabled;
    observation.starNodeActiveAndEnabled = star_node_viewer->isActiveAndEnabled;
  }

  return observation;
}

TopCanvasObservation observe_top_canvas()
{
  TopCanvasObservation observation;
  auto top_canvas = ScreenManager::GetTopCanvas(true);
  observation.found = top_canvas != nullptr;

  if (!top_canvas) {
    return observation;
  }

  const auto canvas_object = reinterpret_cast<Il2CppObject*>(top_canvas);
  const auto canvas_class = canvas_object ? canvas_object->klass : nullptr;
  const auto canvas_controller = reinterpret_cast<CanvasController*>(top_canvas);

  observation.pointer = pointer_to_string(top_canvas);
  observation.className = (canvas_class && canvas_class->name) ? canvas_class->name : "";
  observation.classNamespace = (canvas_class && canvas_class->namespaze) ? canvas_class->namespaze : "";
  observation.name = (canvas_controller && canvas_controller->name) ? to_string(canvas_controller->name) : "";
  observation.visible = canvas_controller && canvas_controller->Visible();
  observation.enabled = canvas_controller && canvas_controller->get_enabled();
  observation.internalVisible = canvas_controller && canvas_controller->m_Visible();
  observation.activeChildNames = collect_active_child_names(canvas_controller ? canvas_controller->transform : nullptr);
  return observation;
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

json handle_mine_viewer_state(const std::string& request_id)
{
  auto mining_viewer    = GetLatestTrackedObject<MiningObjectViewerWidget>();
  auto star_node_viewer = GetLatestTrackedObject<StarNodeObjectViewerWidget>();

  json result = {{"miningViewerTracked", mining_viewer != nullptr},
                 {"starNodeViewerTracked", star_node_viewer != nullptr}};

  result["miningViewer"]   = mining_viewer_to_json(mining_viewer);
  result["starNodeViewer"] = star_node_viewer_to_json(star_node_viewer);

  return make_ok_response(request_id, result);
}

json handle_target_viewer_state(const std::string& request_id)
{
  auto prescan_target = GetLatestTrackedObject<PreScanTargetWidget>();
  auto prescan_station_target = GetLatestTrackedObject<PreScanStationTargetWidget>();
  auto celestial_viewer = GetLatestTrackedObject<CelestialObjectViewerWidget>();

  return make_ok_response(
      request_id,
      json{{"preScanTargetTracked", prescan_target != nullptr},
           {"preScanStationTargetTracked", prescan_station_target != nullptr},
           {"celestialViewerTracked", celestial_viewer != nullptr},
           {"preScanTarget", prescan_target_to_json(prescan_target)},
           {"preScanStationTarget", prescan_target_to_json(prescan_station_target)},
           {"celestialViewer", celestial_viewer_to_json(celestial_viewer)}});
}

json handle_recent_events(const std::string& request_id, const json& request)
{
  LiveDebugRecentEventStoreQuery query;

  if (const auto after_seq_it = request.find("afterSeq");
      after_seq_it != request.end() && after_seq_it->is_number_integer()) {
    query.afterSeq = after_seq_it->get<int64_t>();
  }

  if (const auto limit_it = request.find("limit");
      limit_it != request.end() && limit_it->is_number_integer()) {
    query.limit = static_cast<size_t>(std::max<int64_t>(0, limit_it->get<int64_t>()));
  } else if (const auto last_it = request.find("last");
             last_it != request.end() && last_it->is_number_integer()) {
    query.limit = static_cast<size_t>(std::max<int64_t>(0, last_it->get<int64_t>()));
  }

  if (const auto kinds_it = request.find("kinds"); kinds_it != request.end() && kinds_it->is_array()) {
    for (const auto& kind : *kinds_it) {
      if (kind.is_string()) {
        query.kinds.push_back(kind.get<std::string>());
      }
    }
  }

  if (const auto kind_it = request.find("kind"); kind_it != request.end() && kind_it->is_string()) {
    if (query.kinds.empty()) {
      query.kind = kind_it->get<std::string>();
    } else {
      const auto kind = kind_it->get<std::string>();
      if (std::find(query.kinds.begin(), query.kinds.end(), kind) == query.kinds.end()) {
        query.kinds.push_back(kind);
      }
    }
  }

  if (const auto match_it = request.find("match"); match_it != request.end() && match_it->is_string()) {
    query.match = match_it->get<std::string>();
  }

  if (const auto exact_it = request.find("exact"); exact_it != request.end() && exact_it->is_boolean()) {
    query.exact = exact_it->get<bool>();
  }

  if (const auto include_details_it = request.find("includeDetails");
      include_details_it != request.end() && include_details_it->is_boolean()) {
    query.includeDetails = include_details_it->get<bool>();
  } else if (const auto summary_it = request.find("summary");
             summary_it != request.end() && summary_it->is_boolean()) {
    query.includeDetails = !summary_it->get<bool>();
  }

  auto snapshot = live_debug_events::Snapshot(query);

  return make_ok_response(request_id,
                          json{{"count", snapshot.count},
                               {"returnedCount", snapshot.returnedCount},
                               {"matchedCount", snapshot.matchedCount},
                               {"capacity", snapshot.capacity},
                               {"firstSeq", snapshot.firstSeq == 0 ? json(nullptr) : json(snapshot.firstSeq)},
                               {"lastSeq", snapshot.lastSeq == 0 ? json(nullptr) : json(snapshot.lastSeq)},
                               {"nextSeq", snapshot.nextSeq},
                               {"evictedCount", snapshot.evictedCount},
                               {"clearCount", snapshot.clearCount},
                               {"queryGap", snapshot.queryGap},
                               {"missingCountBeforeFirstReturned", snapshot.missingCountBeforeFirstReturned},
                               {"kindCounts", std::move(snapshot.kindCounts)},
                               {"bufferKindCounts", std::move(snapshot.bufferKindCounts)},
                               {"events", std::move(snapshot.events)}});
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
      return handle_ping(request_id);
    }
    if (cmd == "tracker-list") {
      return handle_tracker_list(request_id);
    }
    if (cmd == "top-canvas") {
      return handle_top_canvas(request_id);
    }
    if (cmd == "fleetbar-state") {
      return handle_fleetbar_state(request_id);
    }
    if (cmd == "fleet-slots-state") {
      return handle_fleet_slots_state(request_id);
    }
    if (cmd == "mine-viewer-state") {
      return handle_mine_viewer_state(request_id);
    }
    if (cmd == "target-viewer-state") {
      return handle_target_viewer_state(request_id);
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

