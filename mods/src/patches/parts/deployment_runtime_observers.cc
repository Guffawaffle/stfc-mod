/**
 * @file deployment_runtime_observers.cc
 * @brief Public deployment-event observers for fleet runtime sync.
 */
#include "patches/deployment_runtime_observers.h"

#include "errormsg.h"
#include "patches/fleet_runtime_sync.h"

#include "prime/IList.h"

#include <spud/detour.h>

#include <string_view>

namespace {
constexpr std::string_view kDeploymentRuntimeObserverOwner = "DeploymentRuntimeObservers";
constexpr std::string_view kFleetRuntimeSyncEffect = "defer-fleet-runtime-snapshot";

void observe_runtime_deployment_event(const char* seam, const char* reason)
{
  fleet_runtime_sync_trigger(gameplay_dispatch_context(reason,
                                                       kDeploymentRuntimeObserverOwner,
                                                       seam,
                                                       reason,
                                                       kFleetRuntimeSyncEffect));
}

void DeploymentEvents_TriggerFleetStateChangeEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerFleetStateChangeEvent",
                                   "deployment-fleet-state-event");
}

void DeploymentEvents_TriggerPlayerFleetsUpdatedEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerPlayerFleetsUpdatedEvent",
                                   "deployment-player-fleets-updated-event");
}

void DeploymentEvents_TriggerCoursePlannedEvent_Hook(auto original, IList* courses)
{
  original(courses);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerCoursePlannedEvent",
                                   "deployment-course-planned-event");
}

void DeploymentEvents_TriggerCourseStartEvent_Hook(auto original, IList* courses)
{
  original(courses);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerCourseStartEvent",
                                   "deployment-course-start-event");
}

void DeploymentEvents_TriggerCourseChangeEvent_Hook(auto original, IList* old_courses, IList* new_courses)
{
  original(old_courses, new_courses);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerCourseChangeEvent",
                                   "deployment-course-change-event");
}

void DeploymentEvents_TriggerCourseEndEvent_Hook(auto original, IList* courses)
{
  original(courses);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerCourseEndEvent",
                                   "deployment-course-end-event");
}

void DeploymentEvents_TriggerSetCourseResponseEvent_Hook(auto original, long fleet_id, bool success,
                                                         bool is_recall_course, void* planned_course_data)
{
  original(fleet_id, success, is_recall_course, planned_course_data);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerSetCourseResponseEvent",
                                   "deployment-set-course-response-event");
}

void DeploymentEvents_TriggerBattleStartEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerBattleStartEvent",
                                   "deployment-battle-start-event");
}

void DeploymentEvents_TriggerBattleEndEvent_Hook(auto original, IList* fleets)
{
  original(fleets);
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerBattleEndEvent",
                                   "deployment-battle-end-event");
}

void DeploymentEvents_TriggerStaleFleetDataDetected_Hook(auto original)
{
  original();
  observe_runtime_deployment_event("Digit.PrimeServer.Events.DeploymentEvents.TriggerStateFleetDataDetected",
                                   "deployment-stale-fleet-data-detected-event");
}
}

void InstallDeploymentRuntimeObserverHooks()
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
