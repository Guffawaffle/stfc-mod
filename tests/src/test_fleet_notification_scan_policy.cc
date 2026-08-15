#include <doctest/doctest.h>

#include "patches/fleet_notification_cache_policy.h"
#include "patches/fleet_notification_scan_policy.h"

#include <array>
#include <unordered_map>

TEST_SUITE("fleet_notification_scan_policy")
{
  TEST_CASE("waits for a bounded follow-through request before scanning")
  {
    FleetNotificationScanPolicy policy;

    CHECK_FALSE(policy.ScanRequested());
    CHECK(policy.Evaluate(1'000) == FleetNotificationScanDecision::Idle);

    policy.RequestScan();

    CHECK(policy.ScanRequested());
    CHECK(policy.Evaluate(1'000) == FleetNotificationScanDecision::Scan);
    CHECK(policy.Evaluate(1'249) == FleetNotificationScanDecision::Wait);
    CHECK(policy.Evaluate(1'250) == FleetNotificationScanDecision::Scan);
    CHECK(policy.Evaluate(1'499) == FleetNotificationScanDecision::Wait);
    CHECK(policy.Evaluate(1'500) == FleetNotificationScanDecision::Scan);
  }

  TEST_CASE("recovers if the supplied monotonic clock moves backwards")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();

    CHECK(policy.Evaluate(5'000) == FleetNotificationScanDecision::Scan);
    CHECK(policy.Evaluate(4'000) == FleetNotificationScanDecision::Scan);
    CHECK(policy.Evaluate(4'249) == FleetNotificationScanDecision::Wait);
    CHECK(policy.Evaluate(4'250) == FleetNotificationScanDecision::Scan);
  }

  TEST_CASE("backs off long-lived transitional scans")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();
    REQUIRE(policy.Evaluate(10'000) == FleetNotificationScanDecision::Scan);

    CHECK(policy.Evaluate(10'000 + kFleetNotificationScanBackoffAfterMs) == FleetNotificationScanDecision::Scan);
    CHECK(policy.Evaluate(10'000 + kFleetNotificationScanBackoffAfterMs + 4'999)
          == FleetNotificationScanDecision::Wait);
    CHECK(policy.Evaluate(10'000 + kFleetNotificationScanBackoffAfterMs + 5'000)
          == FleetNotificationScanDecision::Scan);
  }

  TEST_CASE("suspends after repeated zero-fleet observations until rearmed")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();
    REQUIRE(policy.Evaluate(10'000) == FleetNotificationScanDecision::Scan);

    for (int index = 1; index < kFleetNotificationScanMaxConsecutiveEmpty; ++index) {
      CHECK(policy.RecordObservation(0, 0) == FleetNotificationScanObservation::Continue);
    }
    CHECK(policy.RecordObservation(0, 0) == FleetNotificationScanObservation::NoFleets);
    CHECK_FALSE(policy.ScanRequested());
    CHECK(policy.Evaluate(10'250) == FleetNotificationScanDecision::Idle);

    policy.RequestScan();
    CHECK(policy.Evaluate(10'250) == FleetNotificationScanDecision::Scan);
  }

  TEST_CASE("settles when observed fleets no longer require follow-through")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();
    REQUIRE(policy.Evaluate(10'000) == FleetNotificationScanDecision::Scan);

    CHECK(policy.RecordObservation(7, 1) == FleetNotificationScanObservation::Continue);
    CHECK(policy.RecordObservation(7, 0) == FleetNotificationScanObservation::Settled);
    CHECK_FALSE(policy.ScanRequested());
  }

  TEST_CASE("expires a scan request after the hard lifetime")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();
    REQUIRE(policy.Evaluate(10'000) == FleetNotificationScanDecision::Scan);

    CHECK(policy.Evaluate(10'000 + kFleetNotificationScanMaxLifetimeMs) == FleetNotificationScanDecision::Expired);

    CHECK_FALSE(policy.ScanRequested());
    CHECK(policy.Evaluate(10'000 + kFleetNotificationScanMaxLifetimeMs + 250) == FleetNotificationScanDecision::Idle);
  }
}

TEST_SUITE("fleet_notification_cache_policy")
{
  TEST_CASE("derives exact stale cardinality from current fleet membership")
  {
    std::unordered_map<uint64_t, int> cache;
    for (uint64_t fleet_id = 1; fleet_id <= 10'000; ++fleet_id) {
      cache.emplace(fleet_id, 0);
    }
    const std::array<uint64_t, 8> current{1, 2, 3, 4, 5, 6, 7, 8};

    CHECK(CountStaleFleetCacheEntries(cache, std::span<const uint64_t>(current)) == 9'992);
  }

  TEST_CASE("does not double count duplicate current fleet IDs")
  {
    const std::unordered_map<uint64_t, int> cache{{10, 0}, {20, 0}, {30, 0}};
    const std::array<uint64_t, 3>           current{10, 10, 99};

    CHECK(CountStaleFleetCacheEntries(cache, std::span<const uint64_t>(current)) == 2);
    CHECK(CountStaleFleetCacheEntries(cache, std::span<const uint64_t>{}) == 3);
  }
}
