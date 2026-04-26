#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

struct MineViewerObservation {
  bool        miningViewerTracked = false;
  std::string miningPointer;
  bool        enabled = false;
  bool        isActiveAndEnabled = false;
  bool        isInfoShown = false;
  bool        hasParent = false;
  bool        parentIsShowing = false;
  int         occupiedState = -1;
  bool        hasScanEngageButtons = false;
  bool        hasTimer = false;
  int         timerState = -1;
  int         timerType = -1;
  int64_t     timerRemainingSeconds = -1;
  int64_t     timerRemainingBucket = -1;
  bool        starNodeViewerTracked = false;
  std::string starNodePointer;
  bool        starNodeEnabled = false;
  bool        starNodeActiveAndEnabled = false;
};

struct TargetViewerObservation {
  bool        preScanTargetTracked = false;
  std::string preScanTargetPointer;
  bool        preScanStationTargetTracked = false;
  std::string preScanStationTargetPointer;
  bool        celestialViewerTracked = false;
  std::string celestialViewerPointer;
};

const char* occupied_state_name_from_value(int state);
nlohmann::json target_viewer_observation_to_json(const TargetViewerObservation& observation);
nlohmann::json mine_viewer_observation_to_json(const MineViewerObservation& observation);