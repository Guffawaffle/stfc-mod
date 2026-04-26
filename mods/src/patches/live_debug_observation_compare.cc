/**
 * @file live_debug_observation_compare.cc
 * @brief Observation comparison helpers for live-debug state tracking.
 */
#include "patches/live_debug_observation_compare.h"

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