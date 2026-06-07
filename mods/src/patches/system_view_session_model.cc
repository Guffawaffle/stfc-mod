/**
 * @file system_view_session_model.cc
 * @brief Pure model for section-driven system-view session tracking.
 */
#include "patches/system_view_session_model.h"

#include <sstream>

namespace
{
std::string make_session_id(const uint64_t ordinal, const int64_t started_at_unix_ms)
{
  std::ostringstream stream;
  stream << "sysview-" << started_at_unix_ms << "-" << ordinal;
  return stream.str();
}

void start_session(SystemViewSessionModel& model, const int64_t now_unix_ms)
{
  model.sessionActive                = true;
  model.activeSessionStartedAtUnixMs = now_unix_ms;
  model.activeSessionSystemId        = 0;
  model.activeSessionGalaxyId        = 0;
  model.activeSessionInstanceId      = 0;
  model.activeSessionId              = make_session_id(++model.sessionOrdinal, now_unix_ms);
  ++model.sessionStartCount;
}

std::string end_session(SystemViewSessionModel& model, const int64_t now_unix_ms)
{
  auto ended_session_id = model.activeSessionId;

  model.sessionActive                = false;
  model.lastSessionEndedAtUnixMs     = now_unix_ms;
  model.lastSessionSystemId          = model.activeSessionSystemId;
  model.lastEndedSessionId           = ended_session_id;
  model.activeSessionStartedAtUnixMs = 0;
  model.activeSessionSystemId        = 0;
  model.activeSessionGalaxyId        = 0;
  model.activeSessionInstanceId      = 0;
  model.activeSessionId.clear();
  ++model.sessionEndCount;

  return ended_session_id;
}
} // namespace

bool is_system_view_section_id(const int64_t section_id)
{ return section_id == kSectionIdNavigationSystem; }

std::string system_view_section_name(const int64_t section_id)
{
  switch (section_id) {
    case 0: return "AppInit";
    case 72648470: return "Navigation_Default";
    case kSectionIdNavigationGalaxy: return "Navigation_Galaxy";
    case kSectionIdNavigationSystem: return "Navigation_System";
    case -1033252317: return "Navigation_Planet";
    case -1518342580: return "Starbase_Interior";
    case 385452890: return "Starbase_Exterior";
    case 561547500: return "ShipManagement_Selection";
    case -763928158: return "ShipManagement_Details";
    case 1742145916: return "ShipManagement_Upgrade";
    case -1936877406: return "Research_LandingPage";
    case kSectionIdInventoryList: return "InventoryList";
    case -1833888323: return "Missions_AcceptedList";
    case 1719419183: return "Missions_AvailableList";
    case -644919297: return "Missions_DailyGoals";
    case -935189311: return "Alliance_Main";
    case 2089832472: return "FleetCommander_Management";
    case -1154490138: return "OfficerInventory";
    case 805312080: return "Consumables";
    case -672065984: return "Chat_Main";
    case 492322110: return "Chat_Alliance";
    case -1232447932: return "Chat_Private_Message";
    case -329495679: return "Chat_Private_List";
    case 1754181205: return "GameSettings";
    case -412514502: return "OnScreen_Fleet";
    case 1233999840: return "OnScreen_Self";
    case 1883947040: return "OnScreen_Station";
    default:
      break;
  }

  std::ostringstream stream;
  stream << "UnknownSection(" << section_id << ")";
  return stream.str();
}

SystemViewSessionSectionUpdate system_view_session_note_section(SystemViewSessionModel& model, const int64_t current_section_id,
                                                               const int64_t now_unix_ms)
{
  SystemViewSessionSectionUpdate update;
  update.currentSection  = current_section_id;
  update.timestampUnixMs = now_unix_ms;

  if (!model.initialized) {
    model.initialized                    = true;
    model.lastSectionId                  = current_section_id;
    model.emittedOutsideSessionNoticeForState = false;

    update.initialized = true;
    update.bootstrap   = true;

    if (is_system_view_section_id(current_section_id)) {
      start_session(model, now_unix_ms);
      update.sessionStarted = true;
    }

    return update;
  }

  update.previousSection = model.lastSectionId;
  if (model.lastSectionId == current_section_id) {
    return update;
  }

  update.sectionChanged = true;
  ++model.transitionCount;

  const auto previous_was_system = is_system_view_section_id(model.lastSectionId);
  const auto current_is_system   = is_system_view_section_id(current_section_id);

  model.lastSectionId                        = current_section_id;
  model.emittedOutsideSessionNoticeForState = false;

  if (!previous_was_system && current_is_system) {
    start_session(model, now_unix_ms);
    update.sessionStarted = true;
    return update;
  }

  if (previous_was_system && !current_is_system && model.sessionActive) {
    update.sessionEnded  = true;
    update.endedSessionId = end_session(model, now_unix_ms);
  }

  return update;
}

SystemViewSessionPassiveUpdate system_view_session_note_passive_system(SystemViewSessionModel& model, const int64_t system_id,
                                                                       const int64_t galaxy_id,
                                                                       const int64_t instance_id)
{
  SystemViewSessionPassiveUpdate update;
  update.currentSystemId = system_id;

  const auto inside_active_session = model.sessionActive && is_system_view_section_id(model.lastSectionId);
  update.passiveInsideActiveSession = inside_active_session;

  if (!inside_active_session) {
    ++model.passiveOutsideSessionCount;
    if (!model.emittedOutsideSessionNoticeForState) {
      model.emittedOutsideSessionNoticeForState = true;
      update.outsideSessionNotice               = true;
    }
    return update;
  }

  ++model.passiveInsideSessionCount;

  if (system_id <= 0) {
    return update;
  }

  if (model.activeSessionSystemId == 0) {
    model.activeSessionSystemId   = system_id;
    model.activeSessionGalaxyId   = galaxy_id;
    model.activeSessionInstanceId = instance_id;
    ++model.systemBindCount;
    update.sessionBound = true;
    return update;
  }

  if (model.activeSessionSystemId != system_id) {
    update.previousSystemId       = model.activeSessionSystemId;
    model.activeSessionSystemId   = system_id;
    model.activeSessionGalaxyId   = galaxy_id;
    model.activeSessionInstanceId = instance_id;
    ++model.systemRebindCount;
    update.sessionRebound = true;
  }

  return update;
}
