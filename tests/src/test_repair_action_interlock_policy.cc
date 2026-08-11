#include <doctest/doctest.h>

#include "patches/repair_action_interlock_policy.h"

TEST_SUITE("repair_action_interlock_policy")
{
  using repair_action_interlock::InstantContextSnapshot;
  using repair_action_interlock::State;

  TEST_CASE("holds the last coherent Repair progress status across transient Ready")
  {
    State state;

    CHECK(state.project_status(7, 202, 32) == 202);
    CHECK(state.project_status(7, 100, 32) == 202);
    CHECK(state.project_status(7, 100, 32) == 202);
    CHECK(state.project_status(7, 200, 32) == 200);
  }

  TEST_CASE("does not invent progress without a coherent prior status")
  {
    State state;

    CHECK(state.project_status(7, 100, 32) == 100);
    CHECK(state.project_status(8, 202, 32) == 202);
    CHECK(state.project_status(8, 100, 2) == 100);
  }

  TEST_CASE("holds stale post-completion presentation for a bounded window")
  {
    State state;
    state.project_status(7, 202, 32);
    state.project_status(7, 100, 2);

    const InstantContextSnapshot stale_paid{101558, true, true, true};
    const auto                   first = state.observe_instant_context(7, 2, 32, stale_paid, 1000);
    CHECK(first.hold);
    CHECK(first.changed);
    CHECK(first.elapsed_ms == 0);

    const auto still_held = state.observe_instant_context(7, 2, 32, stale_paid, 3499);
    CHECK(still_held.hold);
    CHECK_FALSE(still_held.changed);

    const auto released = state.observe_instant_context(7, 2, 32, stale_paid, 3500);
    CHECK_FALSE(released.hold);
    CHECK(released.changed);
    CHECK(released.elapsed_ms == State::kHoldDurationMs);
  }

  TEST_CASE("suppresses only actionable stale clicks inside the hold window")
  {
    State state;
    state.project_status(7, 202, 32);
    state.project_status(7, 100, 2);
    state.observe_instant_context(7, 2, 32, {0, true, true, true}, 1000);

    CHECK(state.should_suppress_instant_click(7, 2, 32, 1001));
    CHECK_FALSE(state.should_suppress_instant_click(7, 32, 2, 1001));
    CHECK_FALSE(state.should_suppress_instant_click(7, 2, 32, 3500));

    State missing_amount;
    missing_amount.project_status(8, 202, 32);
    missing_amount.project_status(8, 100, 2);
    missing_amount.observe_instant_context(8, 2, 32, {0, true, true, false}, 1000);
    CHECK_FALSE(missing_amount.should_suppress_instant_click(8, 2, 32, 1001));
  }

  TEST_CASE("coherent re-entry releases presentation and click suppression immediately")
  {
    State state;
    state.project_status(7, 202, 32);
    state.project_status(7, 100, 2);
    state.observe_instant_context(7, 2, 32, {970810, true, true, true}, 1000);
    CHECK(state.should_suppress_instant_click(7, 2, 32, 1001));

    state.project_status(7, 202, 32);
    const auto released = state.observe_instant_context(7, 32, 2, {7, true, true, true}, 1100);
    CHECK_FALSE(released.hold);
    CHECK(released.changed);
    CHECK_FALSE(state.should_suppress_instant_click(7, 32, 2, 1101));
  }

  TEST_CASE("retains a fixed maximum number of fleet records")
  {
    State state;
    for (uint64_t fleet_id = 1; fleet_id <= State::kCapacity + 4; ++fleet_id) {
      state.project_status(fleet_id, 202, 32);
    }
    CHECK(state.occupied_count() == State::kCapacity);
    CHECK(sizeof(State) <= 2048);
  }
}
