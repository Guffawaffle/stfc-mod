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

}
