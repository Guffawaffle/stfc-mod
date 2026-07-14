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
}
