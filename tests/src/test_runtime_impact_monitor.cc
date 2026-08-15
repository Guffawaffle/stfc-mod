#include <doctest/doctest.h>

#include "patches/runtime_impact_monitor.h"

#include <string_view>

TEST_SUITE("runtime_impact_monitor")
{
  TEST_CASE("aggregator accumulates samples until reporting interval elapses")
  {
    RuntimeImpactAggregator aggregator(1000);

    CHECK_FALSE(aggregator.Record(RuntimeImpactProbe::FrameTickTotal, 100'000, 0).has_value());
    CHECK_FALSE(aggregator.Record(RuntimeImpactProbe::FrameTickTotal, 200'000, 999).has_value());

    auto report = aggregator.Record(RuntimeImpactProbe::FrameTickHotkeys, 300'000, 1000);
    REQUIRE(report.has_value());
    CHECK(report->window_ms == 1000);
    CHECK(report->probes[static_cast<size_t>(RuntimeImpactProbe::FrameTickTotal)].samples == 2);
    CHECK(report->probes[static_cast<size_t>(RuntimeImpactProbe::FrameTickTotal)].total_ns == 300'000);
    CHECK(report->probes[static_cast<size_t>(RuntimeImpactProbe::FrameTickHotkeys)].samples == 1);
    CHECK(report->probes[static_cast<size_t>(RuntimeImpactProbe::FrameTickHotkeys)].max_ns == 300'000);
  }

  TEST_CASE("aggregator tracks threshold counters and resets after report")
  {
    RuntimeImpactAggregator aggregator(10);

    CHECK_FALSE(aggregator.Record(RuntimeImpactProbe::NavigationZoomUpdate, 249'999, 0).has_value());
    CHECK_FALSE(aggregator.Record(RuntimeImpactProbe::NavigationZoomUpdate, 250'000, 5).has_value());
    auto report = aggregator.Record(RuntimeImpactProbe::NavigationZoomUpdate, 1'000'000, 10);
    REQUIRE(report.has_value());

    const auto& stats = report->probes[static_cast<size_t>(RuntimeImpactProbe::NavigationZoomUpdate)];
    CHECK(stats.samples == 3);
    CHECK(stats.over_250us == 2);
    CHECK(stats.over_1000us == 1);
    CHECK(stats.average_ns() == (249'999 + 250'000 + 1'000'000) / 3);

    auto next_report = aggregator.Record(RuntimeImpactProbe::NavigationZoomUpdate, 100'000, 20);
    REQUIRE(next_report.has_value());
    CHECK(next_report->probes[static_cast<size_t>(RuntimeImpactProbe::NavigationZoomUpdate)].samples == 1);
  }

  TEST_CASE("probe taxonomy remains stable for targeted diagnostic records")
  {
    CHECK(std::string_view(RuntimeImpactProbeName(RuntimeImpactProbe::FrameTickTotal)) == "frame_tick.total");
    CHECK(std::string_view(RuntimeImpactProbeName(RuntimeImpactProbe::HotkeySpaceActionContext))
          == "hotkey.space_action.context");
    CHECK(std::string_view(RuntimeImpactProbeName(RuntimeImpactProbe::TraceInstrumentationOverhead))
          == "trace.instrumentation_overhead");
  }
}
