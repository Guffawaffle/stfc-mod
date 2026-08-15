/**
 * @file runtime_impact_diagnostics.h
 * @brief Temporary #257 runtime-impact diagnostic concern.
 */
#pragma once

#include "targeted_diagnostics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace runtime_impact_diagnostics
{
inline constexpr targeted_diagnostics::ConcernSpec kConcernSpec{
    .id             = "runtime-impact",
    .owner          = "runtime-observability",
    .tracking_issue = "#257",
    .status         = targeted_diagnostics::ConcernStatus::Temporary,
    .introduced_in  = {2, 1, 0},
    .sunset_at      = {2, 2, 0},
    .remove_or_revise_when =
        "Hostile-interaction and aged-idle captures identify whether a mod-owned path causes frame stalls.",
    .promotion_criteria =
        "A supported performance workflow and measured low overhead justify a permanent impact schema.",
};

inline constexpr int64_t kReportIntervalMs             = 5'000;
inline constexpr bool    kTrackInstrumentationOverhead = true;

struct ProbeWindow {
  uint8_t  probe       = 0;
  int64_t  window_ms   = 0;
  uint64_t samples     = 0;
  uint64_t total_ns    = 0;
  uint64_t max_ns      = 0;
  uint64_t over_250us  = 0;
  uint64_t over_1000us = 0;
};

inline constexpr size_t kOutcomeBytes = 48;

struct SpaceActionTiming {
  std::array<char, kOutcomeBytes> outcome{};
  uint64_t                        duration_us           = 0;
  uint64_t                        context_us            = 0;
  uint64_t                        pre_scan_fallback_us  = 0;
  uint64_t                        outcome_execution_us  = 0;
  uint64_t                        queue_button_press_us = 0;
  uint64_t                        hide_viewers_us       = 0;
  int32_t                         fleet_state           = -1;
  int32_t                         previous_state        = -1;
  int32_t                         visible_pre_scan      = 0;
  int32_t                         resolved_pre_scan     = 0;
  int32_t                         unresolved_pre_scan   = 0;
  uint16_t                        input_flags           = 0;
  uint8_t                         context_flags         = 0;
  bool                            handled               = false;
  bool                            slow                  = false;
};

targeted_diagnostics::Concern&  Concern();
[[nodiscard]] bool              Enabled();
void                            RecordProbeWindow(const ProbeWindow& event);
void                            RecordSpaceActionTiming(const SpaceActionTiming& event);
std::array<char, kOutcomeBytes> CopyOutcome(std::string_view outcome);
} // namespace runtime_impact_diagnostics

namespace targeted_diagnostics
{
template <> struct EventTraits<runtime_impact_diagnostics::ProbeWindow> {
  static constexpr std::string_view event_type       = "probe-window";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const runtime_impact_diagnostics::ProbeWindow&, nlohmann::ordered_json&);
};

template <> struct EventTraits<runtime_impact_diagnostics::SpaceActionTiming> {
  static constexpr std::string_view event_type       = "space-action-timing";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const runtime_impact_diagnostics::SpaceActionTiming&, nlohmann::ordered_json&);
};
} // namespace targeted_diagnostics
