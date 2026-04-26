/**
 * @file live_debug_ui_runtime_observers.cc
 * @brief Runtime UI observation helpers for live-debug station and navigation state.
 */
#include "patches/live_debug_ui_runtime_observers.h"

#include "patches/object_tracker_state.h"

#include "prime/NavigationInteractionUIViewController.h"
#include "prime/StationWarningViewController.h"

#include "str_utils.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace {
std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}

void trace_step(const LiveDebugUiObserverTraceHooks& trace_hooks,
                const char* step,
                const char* phase,
                const void* controller = nullptr,
                const void* sender = nullptr,
                const void* callback_context = nullptr)
{
  if (trace_hooks.trace_step) {
    trace_hooks.trace_step(step, phase, controller, sender, callback_context);
  }
}

void mark_current_poll_visible(const LiveDebugUiObserverTraceHooks& trace_hooks)
{
  if (trace_hooks.mark_current_poll_visible) {
    trace_hooks.mark_current_poll_visible();
  }
}
}

StationWarningObservation observe_station_warning(const LiveDebugUiObserverTraceHooks& trace_hooks)
{
  trace_step(trace_hooks, "station/enter", "observe_station_warning");
  StationWarningObservation observation;
  auto controller = GetLatestTrackedObject<StationWarningViewController>();
  if (controller) {
    mark_current_poll_visible(trace_hooks);
  }
  trace_step(trace_hooks, "station/after-get-controller", "observe_station_warning", controller);
  observation.tracked = controller != nullptr;

  if (!controller) {
    trace_step(trace_hooks, "station/no-controller", "observe_station_warning");
    return observation;
  }

  observation.pointer = pointer_to_string(controller);

  trace_step(trace_hooks, "station/before-canvas-context", "observe_station_warning", controller);
  auto context = controller->CanvasContext;
  trace_step(trace_hooks, "station/after-canvas-context", "observe_station_warning", controller, context);
  observation.hasContext = context != nullptr;
  if (!context) {
    trace_step(trace_hooks, "station/no-context", "observe_station_warning", controller);
    return observation;
  }

  trace_step(trace_hooks, "station/before-target-fields", "observe_station_warning", controller, context);
  observation.targetType = static_cast<int>(context->TargetType);
  observation.targetFleetId = static_cast<uint64_t>(context->TargetFleetId);
  trace_step(trace_hooks, "station/after-target-fields", "observe_station_warning", controller, context);

  if (auto target_user_id = context->TargetUserId; target_user_id) {
    trace_step(trace_hooks, "station/before-target-user-id", "observe_station_warning", controller, context,
               target_user_id);
    observation.targetUserId = to_string(target_user_id);
    trace_step(trace_hooks, "station/after-target-user-id", "observe_station_warning", controller, context,
               target_user_id);
  }

  trace_step(trace_hooks, "station/before-quick-scan-result", "observe_station_warning", controller, context);
  if (auto quick_scan_result = context->QuickScanResult; quick_scan_result) {
    trace_step(trace_hooks, "station/after-quick-scan-result", "observe_station_warning", controller, context,
               quick_scan_result);
    observation.quickScanTargetFleetId = static_cast<uint64_t>(quick_scan_result->TargetFleetId);
    if (auto quick_scan_target_id = quick_scan_result->TargetId; quick_scan_target_id) {
      trace_step(trace_hooks, "station/before-quick-scan-target-id", "observe_station_warning", controller,
                 quick_scan_result, quick_scan_target_id);
      observation.quickScanTargetId = to_string(quick_scan_target_id);
      trace_step(trace_hooks, "station/after-quick-scan-target-id", "observe_station_warning", controller,
                 quick_scan_result, quick_scan_target_id);
    }
  } else {
    trace_step(trace_hooks, "station/no-quick-scan-result", "observe_station_warning", controller, context);
  }

  trace_step(trace_hooks, "station/return", "observe_station_warning", controller, context);
  return observation;
}

NavigationInteractionObservation observe_navigation_interaction(const LiveDebugUiObserverTraceHooks& trace_hooks)
{
  trace_step(trace_hooks, "nav/enter", "observe_navigation_interaction");
  NavigationInteractionObservation observation;
  const auto controllers = GetTrackedObjects<NavigationInteractionUIViewController>();
  if (!controllers.empty()) {
    mark_current_poll_visible(trace_hooks);
  }
  trace_step(trace_hooks, "nav/after-get-controllers", "observe_navigation_interaction");
  observation.tracked = !controllers.empty();
  observation.trackedCount = controllers.size();

  if (controllers.empty()) {
    trace_step(trace_hooks, "nav/no-controllers", "observe_navigation_interaction");
    return observation;
  }

  observation.entries.reserve(controllers.size());
  for (auto* controller : controllers) {
    trace_step(trace_hooks, "nav/before-entry", "observe_navigation_interaction", controller);
    NavigationInteractionObservation::Entry entry;
    entry.pointer = pointer_to_string(controller);

    trace_step(trace_hooks, "nav/before-canvas-context", "observe_navigation_interaction", controller);
    auto context = controller->CanvasContext;
    trace_step(trace_hooks, "nav/after-canvas-context", "observe_navigation_interaction", controller, context);
    entry.hasContext = context != nullptr;
    if (context) {
      trace_step(trace_hooks, "nav/before-context-fields", "observe_navigation_interaction", controller, context);
      entry.contextDataState = context->ContextDataState;
      entry.inputInteractionType = context->InputInteractionType;
      trace_step(trace_hooks, "nav/after-basic-context-fields", "observe_navigation_interaction", controller,
                 context);
      if (auto user_id = context->UserId; user_id) {
        trace_step(trace_hooks, "nav/before-user-id", "observe_navigation_interaction", controller, context,
                   user_id);
        entry.userId = to_string(user_id);
        trace_step(trace_hooks, "nav/after-user-id", "observe_navigation_interaction", controller, context,
                   user_id);
      }
      entry.isMarauder = context->IsMarauder;
      entry.threatLevel = context->ThreatLevel;
      entry.validNavigationInput = context->ValidNavigationInput;
      entry.showSetCourseArm = context->ShowSetCourseArm;
      entry.locationTranslationId = context->LocationTranslationId;
      trace_step(trace_hooks, "nav/after-extra-context-fields", "observe_navigation_interaction", controller,
                 context);
      if (auto poi = context->Poi; poi) {
        entry.poiPointer = pointer_to_string(poi);
        trace_step(trace_hooks, "nav/after-poi-pointer", "observe_navigation_interaction", controller, context,
                   poi);
      }
    }

    observation.entries.push_back(std::move(entry));
    trace_step(trace_hooks, "nav/after-entry", "observe_navigation_interaction", controller, context);
  }

  trace_step(trace_hooks, "nav/return", "observe_navigation_interaction");
  return observation;
}