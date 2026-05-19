/**
 * @file fleet_runtime_diagnostics.h
 * @brief Redacted fleet-runtime diagnostics for logs and recent-event breadcrumbs.
 */
#pragma once

#include "patches/live_debug_fleet_runtime_observers.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

struct FleetRuntimeTraceContext {
  uint64_t    localSequence = 0;
  int64_t     observedAtMs = 0;
  int64_t     captureDurationMs = 0;
  int         slotCount = 0;
  int         presentShipCount = 0;
  std::string source;
  std::string statusSummary;
};

struct FleetRuntimeDiagnosticsSnapshot {
  uint64_t    triggerCount = 0;
  uint64_t    captureAttemptCount = 0;
  uint64_t    suppressedUnchangedCount = 0;
  uint64_t    suppressedNonMeaningfulCount = 0;
  uint64_t    schedulerQueueAcceptedCount = 0;
  uint64_t    schedulerQueueDroppedCount = 0;
  uint64_t    targetQueueAcceptedCount = 0;
  uint64_t    targetQueueDroppedCount = 0;
  uint64_t    postSuccessCount = 0;
  uint64_t    postFailureCount = 0;
  std::string latestTriggerSource;
  int64_t     latestTriggerAtMs = 0;
  uint64_t    latestLocalSequence = 0;
  int64_t     latestObservedAtMs = 0;
};

void fleet_runtime_diagnostics_trigger(std::string_view source);
void fleet_runtime_diagnostics_capture_attempt(std::string_view source, int64_t capture_duration_ms);
void fleet_runtime_diagnostics_suppressed_unchanged(std::string_view source, int64_t capture_duration_ms);
void fleet_runtime_diagnostics_suppressed_non_meaningful(std::string_view source, int64_t capture_duration_ms);

FleetRuntimeTraceContext fleet_runtime_diagnostics_make_trace(
    std::string_view source,
    const FleetObservation& fleet,
    const std::array<FleetSlotObservation, kFleetIndexMax>& slots,
    int64_t observed_at_ms,
    int64_t capture_duration_ms);

void fleet_runtime_diagnostics_scheduler_queue(const FleetRuntimeTraceContext& trace,
                                               bool accepted,
                                               size_t queue_depth,
                                               std::string_view reason = {});

void fleet_runtime_diagnostics_target_queue(const FleetRuntimeTraceContext& trace,
                                            std::string_view target,
                                            std::string_view mode,
                                            bool accepted,
                                            size_t queue_depth,
                                            uint64_t dropped_count = 0,
                                            std::string_view reason = {});

void fleet_runtime_diagnostics_post_result(const FleetRuntimeTraceContext& trace,
                                           std::string_view target,
                                           std::string_view mode,
                                           bool success,
                                           long status_code,
                                           std::string_view error_class,
                                           int64_t elapsed_ms);

FleetRuntimeDiagnosticsSnapshot fleet_runtime_diagnostics_snapshot();
void                            fleet_runtime_diagnostics_reset();