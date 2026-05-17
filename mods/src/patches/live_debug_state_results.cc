/**
 * @file live_debug_state_results.cc
 * @brief Result builders for simple live-debug state request commands.
 */
#include "patches/live_debug_state_results.h"

#include "errormsg.h"

#include "patches/live_debug_fleet_runtime_observers.h"
#include "patches/live_debug_fleet_runtime_serializers.h"
#include "patches/live_debug_ui_serializers.h"
#include "patches/live_debug_viewer_runtime.h"
#include "patches/object_tracker_state.h"

#include "prime/FleetBarViewController.h"

#include "version.h"

#include <sstream>
#include <string>

namespace {
std::string pointer_to_string(const void* pointer)
{
  std::ostringstream stream;
  stream << pointer;
  return stream.str();
}
}

namespace live_debug_state_results {
nlohmann::json Ping()
{
  return nlohmann::json{{"pong", true}, {"version", VER_PRODUCT_VERSION_STR}};
}

nlohmann::json TrackerList()
{
  const auto summaries = GetTrackedObjectSummary();

  nlohmann::json classes = nlohmann::json::array();
  size_t total_objects = 0;
  for (const auto& summary : summaries) {
    total_objects += summary.count;
    const auto full_name = summary.classNamespace.empty() ? summary.className
                                                          : summary.classNamespace + "." + summary.className;

    classes.push_back(nlohmann::json{{"classPointer", summary.classPointer},
                                     {"classNamespace", summary.classNamespace},
                                     {"className", summary.className},
                                     {"fullName", full_name},
                                     {"count", summary.count}});
  }

  return nlohmann::json{{"trackedClassCount", summaries.size()},
                        {"trackedObjectCount", total_objects},
                        {"classes", std::move(classes)}};
}

nlohmann::json TopCanvas()
{
  return top_canvas_observation_to_json(observe_top_canvas());
}

nlohmann::json FleetbarState()
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();
  const auto snapshot = observe_fleet_runtime_snapshot();

  nlohmann::json result = {{"tracked", snapshot.fleet.tracked}};

  if (!fleet_bar) {
    return result;
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet = fleet_controller ? fleet_controller->fleet : nullptr;

  result["pointer"] = pointer_to_string(fleet_bar);
  result["selectedIndex"] = snapshot.fleet.selectedIndex;
  result["hasController"] = fleet_controller != nullptr;
  result["fleet"] = {{"present", snapshot.fleet.hasFleet}};
  if (snapshot.fleet.hasFleet) {
    result["fleet"]["id"] = snapshot.fleet.fleetId;
    result["fleet"]["currentState"] = snapshot.fleet.currentState;
    result["fleet"]["currentStateName"] = fleet_state_name_from_value(snapshot.fleet.currentState);
    result["fleet"]["previousState"] = snapshot.fleet.previousState;
    result["fleet"]["previousStateName"] = fleet_state_name_from_value(snapshot.fleet.previousState);
    result["fleet"]["cargoFill"] = fleet ? fleet->CargoResourceFillLevel : 0.0f;

    if (fleet && fleet->Hull) {
      result["fleet"]["hull"] = fleet_to_json(fleet)["hull"];
    } else {
      result["fleet"]["hull"] = nullptr;
    }
  }
  return result;
}

nlohmann::json FleetSlotsState()
{
  auto fleet_bar = GetLatestTrackedObject<FleetBarViewController>();

  const auto snapshot = observe_fleet_runtime_snapshot();
  const auto& slot_observations = snapshot.slots;
  size_t present_count = 0;
  for (const auto& slot : slot_observations) {
    if (slot.present) {
      ++present_count;
    }
  }

  return nlohmann::json{{"fleetBarTracked", fleet_bar != nullptr},
                        {"slotCount", kFleetIndexMax},
                        {"presentSlotCount", present_count},
                        {"slots", fleet_slots_to_json(slot_observations)}};
}

nlohmann::json MineViewerState()
{
  return mine_viewer_observation_to_json(observe_mine_viewer());
}

nlohmann::json TargetViewerState()
{
  return target_viewer_observation_to_json(observe_target_viewer());
}
}
