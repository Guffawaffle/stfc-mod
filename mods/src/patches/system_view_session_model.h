/**
 * @file system_view_session_model.h
 * @brief Pure model for section-driven system-view session tracking.
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

inline constexpr int64_t kSectionIdNavigationGalaxy = -1300757605;
inline constexpr int64_t kSectionIdNavigationSystem = -934817094;
inline constexpr int64_t kSectionIdInventoryList    = -726704134;

struct SystemViewSessionSectionUpdate {
  bool     initialized     = false;
  bool     sectionChanged  = false;
  bool     sessionStarted  = false;
  bool     sessionEnded    = false;
  bool     bootstrap       = false;
  int64_t  previousSection = 0;
  int64_t  currentSection  = 0;
  int64_t  timestampUnixMs = 0;
  std::string endedSessionId;
};

struct SystemViewSessionPassiveUpdate {
  bool    passiveInsideActiveSession = false;
  bool    outsideSessionNotice       = false;
  bool    sessionBound               = false;
  bool    sessionRebound             = false;
  int64_t previousSystemId           = 0;
  int64_t currentSystemId            = 0;
};

struct SystemViewSessionModel {
  bool        initialized                         = false;
  int64_t     lastSectionId                       = 0;
  bool        sessionActive                       = false;
  uint64_t    sessionOrdinal                      = 0;
  std::string activeSessionId;
  int64_t     activeSessionStartedAtUnixMs        = 0;
  int64_t     activeSessionSystemId               = 0;
  int64_t     activeSessionGalaxyId               = 0;
  int64_t     activeSessionInstanceId             = 0;
  int64_t     lastSessionEndedAtUnixMs            = 0;
  int64_t     lastSessionSystemId                 = 0;
  std::string lastEndedSessionId;
  uint64_t    transitionCount                     = 0;
  uint64_t    sessionStartCount                   = 0;
  uint64_t    sessionEndCount                     = 0;
  uint64_t    systemBindCount                     = 0;
  uint64_t    systemRebindCount                   = 0;
  uint64_t    passiveInsideSessionCount           = 0;
  uint64_t    passiveOutsideSessionCount          = 0;
  bool        emittedOutsideSessionNoticeForState = false;
};

bool        is_system_view_section_id(int64_t section_id);
std::string system_view_section_name(int64_t section_id);

SystemViewSessionSectionUpdate system_view_session_note_section(SystemViewSessionModel& model, int64_t current_section_id,
                                                               int64_t now_unix_ms);
SystemViewSessionPassiveUpdate system_view_session_note_passive_system(SystemViewSessionModel& model, int64_t system_id,
                                                                       int64_t galaxy_id, int64_t instance_id);
