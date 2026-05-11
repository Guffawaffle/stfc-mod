#include <doctest/doctest.h>

#include "patches/mod_impact_monitor.h"

TEST_SUITE("mod_impact_monitor")
{
  TEST_CASE("aggregator accumulates samples until reporting interval elapses")
  {
    ModImpactAggregator aggregator(1000);

    CHECK_FALSE(aggregator.Record(ModImpactProbe::FrameTickTotal, 100'000, 0).has_value());
    CHECK_FALSE(aggregator.Record(ModImpactProbe::FrameTickTotal, 200'000, 999).has_value());

    auto report = aggregator.Record(ModImpactProbe::FrameTickHotkeys, 300'000, 1000);
    REQUIRE(report.has_value());
    CHECK(report->window_ms == 1000);
    CHECK(report->probes[static_cast<size_t>(ModImpactProbe::FrameTickTotal)].samples == 2);
    CHECK(report->probes[static_cast<size_t>(ModImpactProbe::FrameTickTotal)].total_ns == 300'000);
    CHECK(report->probes[static_cast<size_t>(ModImpactProbe::FrameTickHotkeys)].samples == 1);
    CHECK(report->probes[static_cast<size_t>(ModImpactProbe::FrameTickHotkeys)].max_ns == 300'000);
  }

  TEST_CASE("aggregator tracks threshold counters and resets after report")
  {
    ModImpactAggregator aggregator(10);

    CHECK_FALSE(aggregator.Record(ModImpactProbe::NavigationZoomUpdate, 249'999, 0).has_value());
    CHECK_FALSE(aggregator.Record(ModImpactProbe::NavigationZoomUpdate, 250'000, 5).has_value());
    auto report = aggregator.Record(ModImpactProbe::NavigationZoomUpdate, 1'000'000, 10);
    REQUIRE(report.has_value());

    const auto& stats = report->probes[static_cast<size_t>(ModImpactProbe::NavigationZoomUpdate)];
    CHECK(stats.samples == 3);
    CHECK(stats.over_250us == 2);
    CHECK(stats.over_1000us == 1);
    CHECK(stats.average_ns() == (249'999 + 250'000 + 1'000'000) / 3);

    auto next_report = aggregator.Record(ModImpactProbe::NavigationZoomUpdate, 100'000, 20);
    REQUIRE(next_report.has_value());
    CHECK(next_report->probes[static_cast<size_t>(ModImpactProbe::NavigationZoomUpdate)].samples == 1);
  }

  TEST_CASE("runtime trace levels gate detailed probes")
  {
    CHECK_FALSE(ModImpactProbeEnabledForLevel(ModImpactProbe::FrameTickTotal, RuntimeTraceLevel::Off));
    CHECK(ModImpactProbeEnabledForLevel(ModImpactProbe::FrameTickTotal, RuntimeTraceLevel::Summary));
    CHECK(ModImpactProbeEnabledForLevel(ModImpactProbe::HotkeyDispatchPlan, RuntimeTraceLevel::Summary));
    CHECK_FALSE(ModImpactProbeEnabledForLevel(ModImpactProbe::HotkeyShipLocateRequestView, RuntimeTraceLevel::Summary));
    CHECK(ModImpactProbeEnabledForLevel(ModImpactProbe::HotkeyShipLocateRequestView, RuntimeTraceLevel::Detailed));
    CHECK(ModImpactProbeEnabledForLevel(ModImpactProbe::HotkeyShipLocateRequestView, RuntimeTraceLevel::Verbose));
  }
}