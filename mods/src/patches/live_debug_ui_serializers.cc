#include "patches/live_debug_ui_serializers.h"

using nlohmann::json;

const char* incoming_attack_target_type_name(int target_type)
{
  switch (target_type) {
    case 1:
      return "Fleet";
    case 2:
      return "DockingPoint";
    case 3:
      return "Station";
    case 0:
      return "None";
    default:
      return "Unknown";
  }
}

const char* navigation_context_data_state_name(int state)
{
  switch (state) {
    case 0:
      return "Verified";
    case 1:
      return "Verifying";
    case 2:
      return "Failed";
    default:
      return "Unknown";
  }
}

const char* input_interaction_type_name(int type)
{
  switch (type) {
    case 0:
      return "HideAll";
    case 1:
      return "TapShowScanEngageEnemyFleet";
    case 2:
      return "TapShowEngageEnemyNPCFleetInfo";
    case 3:
      return "TapShowScanEngageEnemyStarbase";
    case 4:
      return "TapLocationPlanningPath";
    case 5:
      return "TapEmptySpace";
    case 6:
      return "TapGalaxyStar";
    case 7:
      return "TapPlanetMoveStarbase";
    case 8:
      return "TapPlanetWithMissions";
    case 9:
      return "TapPlanetWithMining";
    case 10:
      return "TapPlanetWithTerritoryEmbassy";
    case 11:
      return "TapLocationScanLoading";
    case 12:
      return "TapLocationScanLoaded";
    case 13:
      return "TapOutOfBoundsLocation";
    case 14:
      return "TapArmadaLocation";
    case 15:
      return "TapOutpostLocation";
    default:
      return "Unknown";
  }
}

const char* navigation_threat_level_name(int level)
{
  switch (level) {
    case -1:
      return "Unknown";
    case 0:
      return "VeryHard";
    case 1:
      return "Hard";
    case 2:
      return "Normal";
    case 3:
      return "Easy";
    default:
      return "Unmapped";
  }
}

json top_canvas_observation_to_json(const TopCanvasObservation& observation)
{
  json result = {{"found", observation.found}};

  if (!observation.found) {
    return result;
  }

  result["pointer"] = observation.pointer;
  result["className"] = observation.className;
  result["classNamespace"] = observation.classNamespace;
  result["name"] = observation.name;
  result["visible"] = observation.visible;
  result["enabled"] = observation.enabled;
  result["internalVisible"] = observation.internalVisible;
  result["activeChildNames"] = observation.activeChildNames;
  result["visibleOnlyHint"] = true;
  return result;
}

json station_warning_observation_to_json(const StationWarningObservation& observation)
{
  json result = {{"tracked", observation.tracked}};

  if (!observation.tracked) {
    return result;
  }

  result["pointer"] = observation.pointer;
  result["hasContext"] = observation.hasContext;

  if (!observation.hasContext) {
    return result;
  }

  result["targetType"] = observation.targetType;
  result["targetTypeName"] = incoming_attack_target_type_name(observation.targetType);
  result["targetFleetId"] = observation.targetFleetId;
  result["targetUserId"] = observation.targetUserId;
  result["quickScanTargetFleetId"] = observation.quickScanTargetFleetId;
  result["quickScanTargetId"] = observation.quickScanTargetId;
  return result;
}

json navigation_interaction_entry_to_json(const NavigationInteractionObservation::Entry& entry)
{
  json entry_json = {{"pointer", entry.pointer}, {"hasContext", entry.hasContext}};

  if (entry.hasContext) {
    entry_json["contextDataState"] = entry.contextDataState;
    entry_json["contextDataStateName"] = navigation_context_data_state_name(entry.contextDataState);
    entry_json["inputInteractionType"] = entry.inputInteractionType;
    entry_json["inputInteractionTypeName"] = input_interaction_type_name(entry.inputInteractionType);
    entry_json["userId"] = entry.userId;
    entry_json["isMarauder"] = entry.isMarauder;
    entry_json["threatLevel"] = entry.threatLevel;
    entry_json["threatLevelName"] = navigation_threat_level_name(entry.threatLevel);
    entry_json["validNavigationInput"] = entry.validNavigationInput;
    entry_json["showSetCourseArm"] = entry.showSetCourseArm;
    entry_json["locationTranslationId"] = entry.locationTranslationId;
    entry_json["poiPointer"] = entry.poiPointer;
  }

  return entry_json;
}

json navigation_interaction_observation_to_json(const NavigationInteractionObservation& observation)
{
  json result = {{"tracked", observation.tracked}};

  if (!observation.tracked) {
    return result;
  }

  result["trackedCount"] = observation.trackedCount;
  result["entries"] = json::array();

  for (const auto& entry : observation.entries) {
    result["entries"].push_back(navigation_interaction_entry_to_json(entry));
  }

  return result;
}