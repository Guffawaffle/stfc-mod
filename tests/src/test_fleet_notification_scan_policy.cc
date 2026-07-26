#include <doctest/doctest.h>

#include "patches/fleet_notification_scan_policy.h"

TEST_SUITE("fleet_notification_scan_policy")
{
  TEST_CASE("waits for a bounded follow-through request before scanning")
  {
    FleetNotificationScanPolicy policy;

    CHECK_FALSE(policy.ScanRequested());
    CHECK_FALSE(policy.ShouldScan(1'000));

    policy.RequestScan();

    CHECK(policy.ScanRequested());
    CHECK(policy.ShouldScan(1'000));
    CHECK_FALSE(policy.ShouldScan(1'249));
    CHECK(policy.ShouldScan(1'250));
    CHECK_FALSE(policy.ShouldScan(1'499));
    CHECK(policy.ShouldScan(1'500));
  }

  TEST_CASE("recovers if the supplied monotonic clock moves backwards")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();

    CHECK(policy.ShouldScan(5'000));
    CHECK(policy.ShouldScan(4'000));
    CHECK_FALSE(policy.ShouldScan(4'249));
    CHECK(policy.ShouldScan(4'250));
  }

  TEST_CASE("suspends after runtime access failure until rearmed")
  {
    FleetNotificationScanPolicy policy;
    policy.RequestScan();
    REQUIRE(policy.ShouldScan(10'000));

    policy.Suspend();

    CHECK_FALSE(policy.ScanRequested());
    CHECK_FALSE(policy.ShouldScan(10'250));

    policy.RequestScan();
    CHECK(policy.ShouldScan(10'250));
  }
}
