/**
 * @file live_debug_viewer_runtime.cc
 * @brief Runtime viewer/top-canvas observation and JSON helpers for live-debug.
 */
#include "patches/live_debug_viewer_runtime.h"

#include "errormsg.h"
#include "patches/object_tracker_state.h"

#include "prime/CanvasController.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/PreScanStationTargetWidget.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include "str_utils.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int64_t kMineTimerBucketSeconds = 30;
constexpr int     kTopCanvasChildNameLimit = 24;

std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}

int64_t get_timer_remaining_bucket(TimerDataContext* timer)
{
  if (!timer) {
    return -1;
  }

  return static_cast<int64_t>(timer->RemainingTime.TotalSeconds()) / kMineTimerBucketSeconds;
}

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
}

nlohmann::json timer_context_to_json(TimerDataContext* timer)
{
  if (!timer) {
    return nullptr;
  }

  return nlohmann::json{{"remainingTicks", timer->RemainingTime.Ticks},
                        {"remainingSeconds", timer->RemainingTime.TotalSeconds()},
                        {"timerType", timer->TimerTypeValue},
                        {"timerState", timer->TimerStateValue},
                        {"showTimerLabel", timer->ShowTimerLabel}};
}

nlohmann::json mining_viewer_to_json(MiningObjectViewerWidget* mining_viewer)
{
  if (!mining_viewer) {
    return nullptr;
  }

  auto parent = mining_viewer->Parent;

  return nlohmann::json{{"pointer", pointer_to_string(mining_viewer)},
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

nlohmann::json star_node_viewer_to_json(StarNodeObjectViewerWidget* star_node_viewer)
{
  if (!star_node_viewer) {
    return nullptr;
  }

  return nlohmann::json{{"pointer", pointer_to_string(star_node_viewer)},
                        {"enabled", star_node_viewer->enabled},
                        {"isActiveAndEnabled", star_node_viewer->isActiveAndEnabled}};
}

nlohmann::json prescan_target_to_json(PreScanTargetWidget* prescan_target)
{
  if (!prescan_target) {
    return nullptr;
  }

  return nlohmann::json{{"pointer", pointer_to_string(prescan_target)}};
}

nlohmann::json celestial_viewer_to_json(CelestialObjectViewerWidget* celestial_viewer)
{
  if (!celestial_viewer) {
    return nullptr;
  }

  return nlohmann::json{{"pointer", pointer_to_string(celestial_viewer)}};
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