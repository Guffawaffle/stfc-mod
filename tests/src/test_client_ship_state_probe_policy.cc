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
}
