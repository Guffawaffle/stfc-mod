#include "patches/live_debug_viewer_serializers.h"

using nlohmann::json;

const char* occupied_state_name_from_value(int state)
{
  switch (state) {
    case 0:
      return "NotOccupied";
    case 1:
      return "LocalPlayerOccupied";
    case 2:
      return "OtherPlayerOccupied";
    default:
      return "Unknown";
  }
}

json target_viewer_observation_to_json(const TargetViewerObservation& observation)
{
  return json{{"preScanTargetTracked", observation.preScanTargetTracked},
              {"preScanTarget", observation.preScanTargetTracked
                                     ? json{{"pointer", observation.preScanTargetPointer}}
                                     : json(nullptr)},
              {"preScanStationTargetTracked", observation.preScanStationTargetTracked},
              {"preScanStationTarget", observation.preScanStationTargetTracked
                                            ? json{{"pointer", observation.preScanStationTargetPointer}}
                                            : json(nullptr)},
              {"celestialViewerTracked", observation.celestialViewerTracked},
              {"celestialViewer", observation.celestialViewerTracked
                                      ? json{{"pointer", observation.celestialViewerPointer}}
                                      : json(nullptr)}};
}

json mine_viewer_observation_to_json(const MineViewerObservation& observation)
{
  json result = {{"miningViewerTracked", observation.miningViewerTracked},
                 {"starNodeViewerTracked", observation.starNodeViewerTracked}};

  if (observation.miningViewerTracked) {
    result["miningViewer"] = {{"pointer", observation.miningPointer},
                               {"enabled", observation.enabled},
                               {"isActiveAndEnabled", observation.isActiveAndEnabled},
                               {"isInfoShown", observation.isInfoShown},
                               {"hasParent", observation.hasParent},
                               {"parentIsShowing", observation.parentIsShowing},
                               {"occupiedState", observation.occupiedState},
                               {"occupiedStateName", occupied_state_name_from_value(observation.occupiedState)},
                               {"hasScanEngageButtons", observation.hasScanEngageButtons},
                               {"timer", observation.hasTimer
                                             ? json{{"remainingSeconds", observation.timerRemainingSeconds},
                                                    {"remainingSecondsBucket", observation.timerRemainingBucket},
                                                    {"timerState", observation.timerState},
                                                    {"timerType", observation.timerType}}
                                             : json(nullptr)}};
  } else {
    result["miningViewer"] = nullptr;
  }

  if (observation.starNodeViewerTracked) {
    result["starNodeViewer"] = {{"pointer", observation.starNodePointer},
                                 {"enabled", observation.starNodeEnabled},
                                 {"isActiveAndEnabled", observation.starNodeActiveAndEnabled}};
  } else {
    result["starNodeViewer"] = nullptr;
  }

  return result;
}