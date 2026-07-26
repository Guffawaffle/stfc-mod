#include <doctest/doctest.h>

#include "patches/fleet_notification_scan_policy.h"

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
