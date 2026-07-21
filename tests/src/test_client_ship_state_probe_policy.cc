#include <doctest/doctest.h>

#include "patches/client_ship_state_probe_policy.h"

TEST_SUITE("client_ship_state_probe_policy")
{
  TEST_CASE("repair status events are emitted only for first observations and transitions")
  {
    ship_state_probe::RepairStatusTransitionCache cache;

    CHECK(cache.record(41, 0));
    CHECK_FALSE(cache.record(41, 0));
    CHECK(cache.record(41, 200));
    CHECK_FALSE(cache.record(41, 200));
    CHECK(cache.record(42, 0));
  }

  TEST_CASE("repair status cache stays bounded and evicts deterministically")
  {
    ship_state_probe::RepairStatusTransitionCache cache;

    for (uint64_t fleet_id = 1; fleet_id <= ship_state_probe::RepairStatusTransitionCache::kCapacity; ++fleet_id) {
      CHECK(cache.record(fleet_id, 0));
    }

    CHECK(cache.record(99, 0));
    CHECK(cache.record(1, 0));
  }

  TEST_CASE("repair status transitions expose the prior distinct status")
  {
    ship_state_probe::RepairStatusTransitionCache cache;

    const auto first = cache.observe(41, 201);
    CHECK(first.changed);
    CHECK_FALSE(first.has_previous);

    const auto complete = cache.observe(41, 300);
    CHECK(complete.changed);
    CHECK(complete.has_previous);
    CHECK(complete.previous_status == 201);

    const auto ready = cache.observe(41, 100);
    CHECK(ready.changed);
    CHECK(ready.has_previous);
    CHECK(ready.previous_status == 300);
    CHECK(ship_state_probe::should_capture_ready_while_repairing_caller(ready, 100, 32));
    CHECK_FALSE(ship_state_probe::should_capture_ready_while_repairing_caller(ready, 100, 2));
    CHECK_FALSE(ship_state_probe::should_capture_ready_while_repairing_caller(ready, 201, 32));

    ship_state_probe::RepairStatusTransitionCache ask_for_help_cache;
    const auto                                    ask_for_help_first    = ask_for_help_cache.observe(42, 202);
    const auto                                    ask_for_help_to_ready = ask_for_help_cache.observe(42, 100);
    CHECK_FALSE(ship_state_probe::should_capture_ready_while_repairing_caller(ask_for_help_first, 100, 32));
    CHECK(ship_state_probe::should_capture_ready_while_repairing_caller(ask_for_help_to_ready, 100, 32));
  }

  TEST_CASE("repair guard suppresses only Ready while the fleet remains Repairing")
  {
    CHECK(ship_state_probe::project_repair_action_status(100, 32, true) == 0);
    CHECK(ship_state_probe::project_repair_action_status(100, 2, true) == 100);
    CHECK(ship_state_probe::project_repair_action_status(202, 32, true) == 202);
    CHECK(ship_state_probe::project_repair_action_status(100, 32, false) == 100);
  }

  TEST_CASE("repair Instant-context observations emit only first observations and transitions")
  {
    ship_state_probe::RepairInstantContextTransitionCache cache;
    const ship_state_probe::RepairInstantContextSnapshot  repairing_paid{32, 2, 970810, true, true, true};
    const ship_state_probe::RepairInstantContextSnapshot  repairing_zero{32, 2, 0, true, true, true};

    CHECK(cache.record(41, repairing_paid));
    CHECK_FALSE(cache.record(41, repairing_paid));
    CHECK(cache.record(41, repairing_zero));
    CHECK(cache.record(42, repairing_zero));

    ship_state_probe::RepairInstantContextSnapshot observed{};
    CHECK(cache.get(41, observed));
    CHECK(observed == repairing_zero);
    CHECK_FALSE(cache.get(99, observed));
  }

  TEST_CASE("repair click correlation retains the latest original and returned statuses per fleet")
  {
    ship_state_probe::RepairStatusSnapshotCache cache;

    cache.record(41, {202, 202});
    cache.record(41, {100, 202});
    cache.record(42, {201, 201});

    ship_state_probe::RepairStatusSnapshot observed{};
    CHECK(cache.get(41, observed));
    CHECK(observed.original_status == 100);
    CHECK(observed.returned_status == 202);
    CHECK(cache.get(42, observed));
    CHECK(observed.original_status == 201);
    CHECK_FALSE(cache.get(99, observed));
  }

  TEST_CASE("repair status hold preserves the last coherent status until the lifecycle settles")
  {
    ship_state_probe::RepairCoherentStatusHoldCache cache;

    CHECK(cache.project(41, 100, 8, 0) == 100);
    CHECK(cache.project(41, 202, 32, 2) == 202);
    CHECK(cache.project(41, 100, 32, 2) == 202);
    CHECK(cache.project(41, 100, 32, 2) == 202);
    CHECK(cache.project(41, 100, 2, 32) == 100);
    CHECK(cache.project(41, 0, 2, 32) == 0);
    CHECK(cache.project(41, 100, 8, 2) == 100);
  }

  TEST_CASE("repair status hold keeps Complete for the zero-cost finish window")
  {
    ship_state_probe::RepairCoherentStatusHoldCache cache;

    CHECK(cache.project(41, 201, 32, 2) == 201);
    CHECK(cache.project(41, 300, 32, 2) == 300);
    CHECK(cache.project(41, 100, 32, 2) == 300);
    CHECK(cache.project(41, 0, 2, 32) == 0);
  }

  TEST_CASE("repair status hold never disguises a docked paid action after completion")
  {
    ship_state_probe::RepairCoherentStatusHoldCache cache;

    CHECK(cache.project(41, 202, 32, 2) == 202);
    CHECK(cache.project(41, 100, 2, 32) == 100);
  }

  TEST_CASE("instant click interlock matches both proven post-repair stale contexts")
  {
    const ship_state_probe::RepairInstantContextSnapshot paid_post_completion{2, 32, 101558, true, true, true};
    const ship_state_probe::RepairInstantContextSnapshot zero_post_completion{2, 32, 0, true, true, true};
    const ship_state_probe::RepairInstantContextSnapshot normal_help{32, 2, 7, true, true, true};
    const ship_state_probe::RepairInstantContextSnapshot missing_context{2, 32, 0, false, false, false};

    CHECK(ship_state_probe::should_suppress_stale_instant_click_after_repair(2, 32, 100, paid_post_completion));
    CHECK(ship_state_probe::should_suppress_stale_instant_click_after_repair(2, 32, 100, zero_post_completion));
    CHECK_FALSE(ship_state_probe::should_suppress_stale_instant_click_after_repair(32, 2, 100, paid_post_completion));
    CHECK_FALSE(ship_state_probe::should_suppress_stale_instant_click_after_repair(2, 32, 202, paid_post_completion));
    CHECK_FALSE(ship_state_probe::should_suppress_stale_instant_click_after_repair(32, 2, 100, normal_help));
    CHECK_FALSE(ship_state_probe::should_suppress_stale_instant_click_after_repair(2, 32, 100, missing_context));
  }

  TEST_CASE("repair presentation hold debounces only the proven post-completion race")
  {
    ship_state_probe::RepairPresentationHoldCache cache;

    const auto first = cache.evaluate(41, 2, 32, 100, 1000);
    CHECK(first.hold);
    CHECK(first.changed);
    CHECK(first.elapsed_ms == 0);

    const auto bounded = cache.evaluate(41, 2, 32, 100, 3499);
    CHECK(bounded.hold);
    CHECK_FALSE(bounded.changed);
    CHECK(bounded.elapsed_ms == 2499);

    const auto released = cache.evaluate(41, 2, 32, 100, 3500);
    CHECK_FALSE(released.hold);
    CHECK(released.changed);
    CHECK(released.elapsed_ms == ship_state_probe::RepairPresentationHoldCache::kHoldDurationMs);

    const auto remains_released = cache.evaluate(41, 2, 32, 100, 4000);
    CHECK_FALSE(remains_released.hold);
    CHECK_FALSE(remains_released.changed);

    const auto reset = cache.evaluate(41, 32, 2, 202, 4100);
    CHECK_FALSE(reset.hold);
    CHECK_FALSE(reset.changed);

    const auto next_race = cache.evaluate(41, 2, 32, 100, 5000);
    CHECK(next_race.hold);
    CHECK(next_race.changed);
  }

  TEST_CASE("repair presentation hold leaves genuine Repair states untouched")
  {
    ship_state_probe::RepairPresentationHoldCache cache;

    CHECK_FALSE(cache.evaluate(41, 32, 2, 202, 1000).hold);
    CHECK_FALSE(cache.evaluate(41, 32, 2, 200, 1100).hold);
    CHECK_FALSE(cache.evaluate(41, 32, 2, 201, 1200).hold);
    CHECK_FALSE(cache.evaluate(41, 2, 32, 0, 1300).hold);
    CHECK_FALSE(cache.evaluate(41, 2, 1, 100, 1400).hold);

    CHECK(cache.evaluate(42, 2, 32, 100, 2000).hold);
    const auto coherent_reentry = cache.evaluate(42, 32, 2, 202, 2100);
    CHECK_FALSE(coherent_reentry.hold);
    CHECK(coherent_reentry.changed);
  }
}
