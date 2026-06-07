#include "test_pure_common.h"

#include "patches/system_view_session_model.h"

namespace
{
constexpr int64_t kGalaxySection    = kSectionIdNavigationGalaxy;
constexpr int64_t kSystemSection    = kSectionIdNavigationSystem;
constexpr int64_t kInventorySection = kSectionIdInventoryList;
}

TEST_SUITE("system_view_session_model")
{
  TEST_CASE("starts and ends a provisional session on section boundaries")
  {
    SystemViewSessionModel model;

    const auto bootstrap = system_view_session_note_section(model, kGalaxySection, 100);
    CHECK(bootstrap.initialized);
    CHECK(bootstrap.bootstrap);
    CHECK_FALSE(bootstrap.sectionChanged);
    CHECK_FALSE(bootstrap.sessionStarted);
    CHECK_FALSE(model.sessionActive);

    const auto enter = system_view_session_note_section(model, kSystemSection, 200);
    CHECK(enter.sectionChanged);
    CHECK(enter.sessionStarted);
    CHECK_FALSE(enter.sessionEnded);
    CHECK(model.sessionActive);
    CHECK_FALSE(model.activeSessionId.empty());

    const auto leave = system_view_session_note_section(model, kGalaxySection, 300);
    CHECK(leave.sectionChanged);
    CHECK_FALSE(leave.sessionStarted);
    CHECK(leave.sessionEnded);
    CHECK_FALSE(leave.endedSessionId.empty());
    CHECK_FALSE(model.sessionActive);
    CHECK(model.lastEndedSessionId == leave.endedSessionId);
  }

  TEST_CASE("binds and rebinds system ids only while a session is active")
  {
    SystemViewSessionModel model;

    system_view_session_note_section(model, kSystemSection, 100);
    REQUIRE(model.sessionActive);

    const auto bind = system_view_session_note_passive_system(model, 42, 7, 1);
    CHECK(bind.passiveInsideActiveSession);
    CHECK(bind.sessionBound);
    CHECK_FALSE(bind.sessionRebound);
    CHECK(model.activeSessionSystemId == 42);
    CHECK(model.activeSessionGalaxyId == 7);
    CHECK(model.activeSessionInstanceId == 1);

    const auto same = system_view_session_note_passive_system(model, 42, 7, 1);
    CHECK(same.passiveInsideActiveSession);
    CHECK_FALSE(same.sessionBound);
    CHECK_FALSE(same.sessionRebound);

    const auto rebound = system_view_session_note_passive_system(model, 43, 8, 2);
    CHECK(rebound.passiveInsideActiveSession);
    CHECK_FALSE(rebound.sessionBound);
    CHECK(rebound.sessionRebound);
    CHECK(rebound.previousSystemId == 42);
    CHECK(model.activeSessionSystemId == 43);
    CHECK(model.activeSessionGalaxyId == 8);
    CHECK(model.activeSessionInstanceId == 2);
  }

  TEST_CASE("passive sightings outside a system-view session stay provisional")
  {
    SystemViewSessionModel model;

    system_view_session_note_section(model, kInventorySection, 100);
    REQUIRE_FALSE(model.sessionActive);

    const auto first = system_view_session_note_passive_system(model, 99, 5, 1);
    CHECK_FALSE(first.passiveInsideActiveSession);
    CHECK(first.outsideSessionNotice);
    CHECK_FALSE(first.sessionBound);
    CHECK_FALSE(first.sessionRebound);

    const auto second = system_view_session_note_passive_system(model, 99, 5, 1);
    CHECK_FALSE(second.passiveInsideActiveSession);
    CHECK_FALSE(second.outsideSessionNotice);
  }
}
