/**
 * @file runtime_impact_diagnostics.cc
 * @brief Typed records for the temporary #257 runtime-impact concern.
 */
#include "patches/runtime_impact_diagnostics.h"

#include "patches/runtime_impact_monitor.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace runtime_impact_diagnostics
{
namespace
{
  targeted_diagnostics::Concern s_concern{kConcernSpec};
}

targeted_diagnostics::Concern& Concern()
{ return s_concern; }

bool Enabled()
{ return TARGET_DIAGNOSTIC_ENABLED(s_concern); }

void RecordProbeWindow(const ProbeWindow& event)
{
  if (!TARGET_DIAGNOSTIC_ENABLED(s_concern)) {
    return;
  }
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, event);
}

void RecordSpaceActionTiming(const SpaceActionTiming& event)
{
  if (!TARGET_DIAGNOSTIC_ENABLED(s_concern)) {
    return;
  }
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, event);
}

std::array<char, kOutcomeBytes> CopyOutcome(const std::string_view outcome)
{
  std::array<char, kOutcomeBytes> copy{};
  const auto                      length = std::min(outcome.size(), copy.size() - 1);
  std::copy_n(outcome.begin(), length, copy.begin());
  return copy;
}
} // namespace runtime_impact_diagnostics

namespace targeted_diagnostics
{
void EventTraits<runtime_impact_diagnostics::ProbeWindow>::SerializeFields(
    const runtime_impact_diagnostics::ProbeWindow& event, nlohmann::ordered_json& fields)
{
  const auto probe = static_cast<RuntimeImpactProbe>(event.probe);
  fields = {{"probe", RuntimeImpactProbeName(probe)},
            {"window_ms", event.window_ms},
            {"samples", event.samples},
            {"average_us", event.samples == 0 ? 0.0 : static_cast<double>(event.total_ns) / event.samples / 1000.0},
            {"max_us", static_cast<double>(event.max_ns) / 1000.0},
            {"over_250us", event.over_250us},
            {"over_1000us", event.over_1000us}};
}

void EventTraits<runtime_impact_diagnostics::SpaceActionTiming>::SerializeFields(
    const runtime_impact_diagnostics::SpaceActionTiming& event, nlohmann::ordered_json& fields)
{
  const auto has_input   = [&event](const uint8_t bit) { return (event.input_flags & (uint16_t{1} << bit)) != 0; };
  const auto has_context = [&event](const uint8_t bit) { return (event.context_flags & (uint8_t{1} << bit)) != 0; };

  fields = {{"outcome", event.outcome.data()},
            {"duration_us", event.duration_us},
            {"context_us", event.context_us},
            {"pre_scan_fallback_us", event.pre_scan_fallback_us},
            {"outcome_execution_us", event.outcome_execution_us},
            {"queue_button_press_us", event.queue_button_press_us},
            {"hide_viewers_us", event.hide_viewers_us},
            {"fleet_state", event.fleet_state},
            {"previous_state", event.previous_state},
            {"visible_pre_scan", event.visible_pre_scan},
            {"resolved_pre_scan", event.resolved_pre_scan},
            {"unresolved_pre_scan", event.unresolved_pre_scan},
            {"input_flags", event.input_flags},
            {"context_flags", event.context_flags},
            {"inputs",
             {{"physical_primary", has_input(0)},
              {"deferred_primary_for_fleet", has_input(1)},
              {"deferred_pending", has_input(2)},
              {"secondary", has_input(3)},
              {"queue", has_input(4)},
              {"queue_clear", has_input(5)},
              {"recall", has_input(6)},
              {"repair", has_input(7)},
              {"recall_cancel", has_input(8)}}},
            {"context",
             {{"mining_visible", has_context(0)},
              {"star_node_visible", has_context(1)},
              {"navigation_visible", has_context(2)},
              {"pre_scan_fallback_used", has_context(3)}}},
            {"handled", event.handled},
            {"slow", event.slow}};
}
} // namespace targeted_diagnostics
