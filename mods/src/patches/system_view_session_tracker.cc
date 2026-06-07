/**
 * @file system_view_session_tracker.cc
 * @brief Diagnostics-only system-view section transition tracker.
 */
#include "patches/system_view_session_tracker.h"

#include "config.h"
#include "patches/system_view_session_model.h"
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
#include "patches/live_debug_event_dispatcher.h"
#endif

#include "prime/Hub.h"
#include "prime/ScreenManager.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using json = nlohmann::json;

struct SystemViewSectionAuditState {
  SystemViewSessionModel model;
  int64_t                lastKnownSystemId             = 0;
  int64_t                lastKnownGalaxyId             = 0;
  int64_t                lastKnownInstanceId           = 0;
  uint64_t               passiveBindLogCount           = 0;
  uint64_t               passiveRebindLogCount         = 0;
  uint64_t               passiveOutsideSessionNotices  = 0;
};

std::mutex                  g_audit_mutex;
SystemViewSectionAuditState g_audit_state;

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool system_view_section_audit_enabled()
{ return SidecarProbesSettings().hostile_observation; }

int64_t json_to_int64(const json& value)
{
  if (value.is_number_integer()) {
    return value.get<int64_t>();
  }

  if (value.is_number_unsigned()) {
    const auto parsed = value.get<uint64_t>();
    if (parsed <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      return static_cast<int64_t>(parsed);
    }
    return 0;
  }

  if (value.is_string()) {
    const auto& text = value.get_ref<const std::string&>();
    if (text.empty()) {
      return 0;
    }

    char* end         = nullptr;
    const auto parsed = std::strtoll(text.c_str(), &end, 10);
    return end && *end == '\0' ? parsed : 0;
  }

  return 0;
}

int64_t json_field_int64(const json& object, std::string_view key)
{
  const auto it = object.find(std::string(key));
  if (it == object.end()) {
    return 0;
  }

  return json_to_int64(*it);
}

void record_recent_event(std::string_view kind, json details)
{
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  live_debug_events::RecordEvent(kind, std::move(details));
#else
  (void)kind;
  (void)details;
#endif
}

void log_section_transition(const SystemViewSectionAuditState& state, const SystemViewSessionSectionUpdate& update)
{
  const auto previous_name = update.initialized && !update.sectionChanged
                                 ? std::string{"Uninitialized"}
                                 : system_view_section_name(update.previousSection);
  const auto current_name      = system_view_section_name(update.currentSection);
  const auto current_is_system = is_system_view_section_id(update.currentSection);

  spdlog::info(
      "[SystemViewAudit] source=screen_manager_update_poll timestamp_ms={} previous={}({}) current={}({}) "
      "bootstrap={} current_is_system={} session_active={} session_started={} session_ended={} session_id={} "
      "ended_session_id={} last_known_system_id={} last_known_galaxy_id={} last_known_instance_id={}",
      update.timestampUnixMs, update.previousSection, previous_name, update.currentSection, current_name,
      update.bootstrap, current_is_system, state.model.sessionActive, update.sessionStarted, update.sessionEnded,
      state.model.activeSessionId, update.endedSessionId, state.lastKnownSystemId, state.lastKnownGalaxyId,
      state.lastKnownInstanceId);

  record_recent_event(
      "system-view-section-transition",
      json{{"source", "screen_manager_update_poll"},
           {"timestampUnixMs", update.timestampUnixMs},
           {"bootstrap", update.bootstrap},
           {"previousSectionId", update.previousSection},
           {"previousSectionName", previous_name},
           {"currentSectionId", update.currentSection},
           {"currentSectionName", current_name},
           {"currentSectionIsNavigationSystem", current_is_system},
           {"sessionActive", state.model.sessionActive},
           {"sessionStarted", update.sessionStarted},
           {"sessionEnded", update.sessionEnded},
           {"systemViewSessionId", state.model.activeSessionId},
           {"endedSystemViewSessionId", update.endedSessionId},
           {"lastKnownSystemId", state.lastKnownSystemId},
           {"lastKnownGalaxyId", state.lastKnownGalaxyId},
           {"lastKnownInstanceId", state.lastKnownInstanceId}});
}

void log_passive_system_bind(const SystemViewSectionAuditState& state, const SystemViewSessionPassiveUpdate& update,
                             std::string_view phase)
{
  spdlog::info(
      "[SystemViewAudit] source=fleet_data_system_passive phase={} section={}({}) session_active={} session_id={} "
      "system_id={} previous_system_id={} galaxy_id={} instance_id={}",
      phase, state.model.lastSectionId, system_view_section_name(state.model.lastSectionId), state.model.sessionActive,
      state.model.activeSessionId, state.model.activeSessionSystemId, update.previousSystemId, state.lastKnownGalaxyId,
      state.lastKnownInstanceId);

  record_recent_event(
      "system-view-passive-system-bind",
      json{{"source", "fleet_data_system_passive"},
           {"phase", phase},
           {"currentSectionId", state.model.lastSectionId},
           {"currentSectionName", system_view_section_name(state.model.lastSectionId)},
           {"sessionActive", state.model.sessionActive},
           {"systemViewSessionId", state.model.activeSessionId},
           {"systemId", state.model.activeSessionSystemId},
           {"previousSystemId", update.previousSystemId},
           {"galaxyId", state.lastKnownGalaxyId},
           {"instanceId", state.lastKnownInstanceId}});
}

void log_passive_outside_session(const SystemViewSectionAuditState& state, const int64_t system_id, const int64_t galaxy_id,
                                 const int64_t instance_id)
{
  spdlog::info(
      "[SystemViewAudit] source=fleet_data_system_passive phase=outside_session_notice section={}({}) session_active={} "
      "session_id={} system_id={} galaxy_id={} instance_id={}",
      state.model.lastSectionId, system_view_section_name(state.model.lastSectionId), state.model.sessionActive,
      state.model.activeSessionId, system_id, galaxy_id, instance_id);

  record_recent_event(
      "system-view-passive-outside-session",
      json{{"source", "fleet_data_system_passive"},
           {"phase", "outside_session_notice"},
           {"currentSectionId", state.model.lastSectionId},
           {"currentSectionName", system_view_section_name(state.model.lastSectionId)},
           {"sessionActive", state.model.sessionActive},
           {"systemViewSessionId", state.model.activeSessionId},
           {"systemId", system_id},
           {"galaxyId", galaxy_id},
           {"instanceId", instance_id}});
}
} // namespace

bool system_view_session_frame_subscriber_enabled()
{ return system_view_section_audit_enabled(); }

void system_view_session_tick(ScreenManager* screen_manager)
{
  (void)screen_manager;

  if (!system_view_section_audit_enabled()) {
    return;
  }

  auto* section_manager = Hub::get_SectionManager();
  if (!section_manager) {
    return;
  }

  const auto current_section_id = static_cast<int64_t>(section_manager->CurrentSection);
  const auto now_unix_ms        = current_time_millis_utc();

  std::scoped_lock lock(g_audit_mutex);
  const auto       update = system_view_session_note_section(g_audit_state.model, current_section_id, now_unix_ms);
  if (!update.bootstrap && !update.sectionChanged) {
    return;
  }

  log_section_transition(g_audit_state, update);
}

void system_view_session_note_passive_observation(const nlohmann::json& observation)
{
  if (!system_view_section_audit_enabled() || !observation.is_object()) {
    return;
  }

  const auto system_id   = json_field_int64(observation, "systemId");
  const auto galaxy_id   = json_field_int64(observation, "galaxyId");
  const auto instance_id = json_field_int64(observation, "instanceId");

  std::scoped_lock lock(g_audit_mutex);
  if (system_id > 0) {
    g_audit_state.lastKnownSystemId = system_id;
  }
  if (galaxy_id > 0) {
    g_audit_state.lastKnownGalaxyId = galaxy_id;
  }
  if (instance_id > 0) {
    g_audit_state.lastKnownInstanceId = instance_id;
  }

  const auto update = system_view_session_note_passive_system(g_audit_state.model, system_id, galaxy_id, instance_id);
  if (update.outsideSessionNotice) {
    ++g_audit_state.passiveOutsideSessionNotices;
    log_passive_outside_session(g_audit_state, system_id, galaxy_id, instance_id);
    return;
  }

  if (update.sessionBound) {
    ++g_audit_state.passiveBindLogCount;
    log_passive_system_bind(g_audit_state, update, "session_bind");
    return;
  }

  if (update.sessionRebound) {
    ++g_audit_state.passiveRebindLogCount;
    log_passive_system_bind(g_audit_state, update, "session_rebind");
  }
}

nlohmann::json system_view_session_state()
{
  std::scoped_lock lock(g_audit_mutex);

  const auto current_section_id = g_audit_state.model.initialized ? g_audit_state.model.lastSectionId : 0;

  return json{{"enabled", system_view_section_audit_enabled()},
              {"provisional", true},
              {"source", "screen_manager_update_poll"},
              {"initialized", g_audit_state.model.initialized},
              {"currentSectionId", current_section_id},
              {"currentSectionName",
               g_audit_state.model.initialized ? system_view_section_name(current_section_id)
                                               : std::string{"Uninitialized"}},
              {"currentSectionIsNavigationSystem",
               g_audit_state.model.initialized && is_system_view_section_id(current_section_id)},
              {"sessionActive", g_audit_state.model.sessionActive},
              {"activeSessionId", g_audit_state.model.activeSessionId},
              {"activeSessionStartedAtUnixMs", g_audit_state.model.activeSessionStartedAtUnixMs},
              {"activeSessionSystemId", g_audit_state.model.activeSessionSystemId},
              {"activeSessionGalaxyId", g_audit_state.model.activeSessionGalaxyId},
              {"activeSessionInstanceId", g_audit_state.model.activeSessionInstanceId},
              {"lastEndedSessionId", g_audit_state.model.lastEndedSessionId},
              {"lastSessionEndedAtUnixMs", g_audit_state.model.lastSessionEndedAtUnixMs},
              {"lastSessionSystemId", g_audit_state.model.lastSessionSystemId},
              {"lastKnownSystemId", g_audit_state.lastKnownSystemId},
              {"lastKnownGalaxyId", g_audit_state.lastKnownGalaxyId},
              {"lastKnownInstanceId", g_audit_state.lastKnownInstanceId},
              {"transitionCount", g_audit_state.model.transitionCount},
              {"sessionStartCount", g_audit_state.model.sessionStartCount},
              {"sessionEndCount", g_audit_state.model.sessionEndCount},
              {"systemBindCount", g_audit_state.model.systemBindCount},
              {"systemRebindCount", g_audit_state.model.systemRebindCount},
              {"passiveInsideSessionCount", g_audit_state.model.passiveInsideSessionCount},
              {"passiveOutsideSessionCount", g_audit_state.model.passiveOutsideSessionCount},
              {"passiveBindLogCount", g_audit_state.passiveBindLogCount},
              {"passiveRebindLogCount", g_audit_state.passiveRebindLogCount},
              {"passiveOutsideSessionNoticeCount", g_audit_state.passiveOutsideSessionNotices}};
}
