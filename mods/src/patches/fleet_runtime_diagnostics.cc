/**
 * @file fleet_runtime_diagnostics.cc
 * @brief Redacted fleet-runtime diagnostics for logs and recent-event breadcrumbs.
 */
#include "patches/fleet_runtime_diagnostics.h"

#ifndef STFC_MOD_TESTS
#include "patches/live_debug_event_dispatcher.h"
#endif

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace {
using json = nlohmann::json;

std::mutex                     s_state_mutex;
FleetRuntimeDiagnosticsSnapshot s_snapshot;
std::atomic_uint64_t           s_next_local_sequence{0};

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string ascii_lower(std::string_view text)
{
  std::string lowered;
  lowered.reserve(text.size());

  for (const auto ch : text) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }

  return lowered;
}

std::string slot_state_label(const FleetSlotObservation& slot)
{
  if (!slot.present) {
    return "empty";
  }

  auto label = ascii_lower(fleet_state_name_from_value(slot.currentState));
  if (label.empty() || label == "none") {
    return "unknown";
  }

  return label;
}

std::string build_status_summary(const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  std::map<std::string, int> counts;
  for (const auto& slot : slots) {
    ++counts[slot_state_label(slot)];
  }

  std::ostringstream stream;
  bool               first = true;
  for (const auto& entry : counts) {
    if (!first) {
      stream << ",";
    }
    first = false;
    stream << entry.first << ":" << entry.second;
  }

  return stream.str();
}

template <typename Mutator>
FleetRuntimeDiagnosticsSnapshot update_snapshot(Mutator&& mutator)
{
  std::lock_guard<std::mutex> lock(s_state_mutex);
  mutator(s_snapshot);
  return s_snapshot;
}

FleetRuntimeDiagnosticsSnapshot snapshot_copy()
{
  std::lock_guard<std::mutex> lock(s_state_mutex);
  return s_snapshot;
}

json snapshot_to_json(const FleetRuntimeDiagnosticsSnapshot& snapshot)
{
  return json{{"triggerCount", snapshot.triggerCount},
              {"captureAttemptCount", snapshot.captureAttemptCount},
              {"suppressedUnchangedCount", snapshot.suppressedUnchangedCount},
              {"suppressedNonMeaningfulCount", snapshot.suppressedNonMeaningfulCount},
              {"schedulerQueueAcceptedCount", snapshot.schedulerQueueAcceptedCount},
              {"schedulerQueueDroppedCount", snapshot.schedulerQueueDroppedCount},
              {"targetQueueAcceptedCount", snapshot.targetQueueAcceptedCount},
              {"targetQueueDroppedCount", snapshot.targetQueueDroppedCount},
              {"postSuccessCount", snapshot.postSuccessCount},
              {"postFailureCount", snapshot.postFailureCount},
              {"latestTriggerSource", snapshot.latestTriggerSource},
              {"latestTriggerAtMs", snapshot.latestTriggerAtMs},
              {"latestLocalSequence", snapshot.latestLocalSequence},
              {"latestObservedAtMs", snapshot.latestObservedAtMs}};
}

json with_summary(json details, const FleetRuntimeDiagnosticsSnapshot& snapshot)
{
  details["summary"] = snapshot_to_json(snapshot);
  return details;
}

void record_recent_event(std::string_view kind, json details)
{
#ifndef STFC_MOD_TESTS
  live_debug_events::RecordEvent(kind, std::move(details));
#else
  (void)kind;
  (void)details;
#endif
}
} // namespace

void fleet_runtime_diagnostics_trigger(std::string_view source)
{
  const auto trigger_at_ms = current_time_millis_utc();
  const auto snapshot = update_snapshot([&](auto& state) {
    ++state.triggerCount;
    state.latestTriggerSource = std::string(source);
    state.latestTriggerAtMs = trigger_at_ms;
  });

  spdlog::info("[FleetRuntimeTrigger] source={} triggerMs={}", source, trigger_at_ms);
  record_recent_event("fleet-runtime-trigger",
                      with_summary(json{{"source", source}, {"triggerMs", trigger_at_ms}}, snapshot));
}

void fleet_runtime_diagnostics_capture_attempt(std::string_view source, int64_t capture_duration_ms)
{
  const auto snapshot = update_snapshot([&](auto& state) { ++state.captureAttemptCount; });

  spdlog::info("[FleetRuntimeCapture] source={} status=attempt durationMs={}", source, capture_duration_ms);
  record_recent_event("fleet-runtime-capture",
                      with_summary(json{{"source", source},
                                        {"status", "attempt"},
                                        {"durationMs", capture_duration_ms}},
                                   snapshot));
}

void fleet_runtime_diagnostics_suppressed_unchanged(std::string_view source, int64_t capture_duration_ms)
{
  const auto snapshot = update_snapshot([&](auto& state) { ++state.suppressedUnchangedCount; });

  spdlog::info("[FleetRuntimeSuppressed] source={} reason=unchanged durationMs={}", source, capture_duration_ms);
  record_recent_event("fleet-runtime-suppressed",
                      with_summary(json{{"source", source},
                                        {"reason", "unchanged"},
                                        {"durationMs", capture_duration_ms}},
                                   snapshot));
}

void fleet_runtime_diagnostics_suppressed_non_meaningful(std::string_view source, int64_t capture_duration_ms)
{
  const auto snapshot = update_snapshot([&](auto& state) { ++state.suppressedNonMeaningfulCount; });

  spdlog::info("[FleetRuntimeSuppressed] source={} reason=non-meaningful durationMs={}", source,
               capture_duration_ms);
  record_recent_event("fleet-runtime-suppressed",
                      with_summary(json{{"source", source},
                                        {"reason", "non-meaningful"},
                                        {"durationMs", capture_duration_ms}},
                                   snapshot));
}

FleetRuntimeTraceContext fleet_runtime_diagnostics_make_trace(
    std::string_view source,
    const FleetObservation&,
    const std::array<FleetSlotObservation, kFleetIndexMax>& slots,
    const int64_t observed_at_ms,
    const int64_t capture_duration_ms)
{
  FleetRuntimeTraceContext trace;
  trace.localSequence = s_next_local_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  trace.observedAtMs = observed_at_ms;
  trace.captureDurationMs = capture_duration_ms;
  trace.slotCount = static_cast<int>(slots.size());
  trace.presentShipCount = static_cast<int>(std::count_if(slots.begin(), slots.end(), [](const auto& slot) {
    return slot.present;
  }));
  trace.source = std::string(source);
  trace.statusSummary = build_status_summary(slots);

  update_snapshot([&](auto& state) {
    state.latestLocalSequence = trace.localSequence;
    state.latestObservedAtMs = observed_at_ms;
  });

  return trace;
}

void fleet_runtime_diagnostics_scheduler_queue(const FleetRuntimeTraceContext& trace,
                                               const bool accepted,
                                               const size_t queue_depth,
                                               const std::string_view reason)
{
  const auto snapshot = update_snapshot([&](auto& state) {
    if (accepted) {
      ++state.schedulerQueueAcceptedCount;
    } else {
      ++state.schedulerQueueDroppedCount;
    }
  });

  if (accepted) {
    spdlog::info(
        "[FleetRuntimeQueued] stage=scheduler source={} localSeq={} observedAtMs={} slotCount={} presentCount={} "
        "states={} depth={} accepted=true",
        trace.source, trace.localSequence, trace.observedAtMs, trace.slotCount, trace.presentShipCount,
        trace.statusSummary, queue_depth);
  } else {
    spdlog::warn("[FleetRuntimeQueued] stage=scheduler source={} localSeq={} observedAtMs={} depth={} "
                 "accepted=false reason={}",
                 trace.source, trace.localSequence, trace.observedAtMs, queue_depth,
                 reason.empty() ? "unknown" : reason);
  }

  auto details = json{{"stage", "scheduler"},
                      {"accepted", accepted},
                      {"source", trace.source},
                      {"localSequence", trace.localSequence},
                      {"observedAtMs", trace.observedAtMs},
                      {"slotCount", trace.slotCount},
                      {"presentShipCount", trace.presentShipCount},
                      {"statusSummary", trace.statusSummary},
                      {"queueDepth", queue_depth}};
  if (!reason.empty()) {
    details["reason"] = reason;
  }
  record_recent_event("fleet-runtime-queued", with_summary(std::move(details), snapshot));
}

void fleet_runtime_diagnostics_target_queue(const FleetRuntimeTraceContext& trace,
                                            const std::string_view target,
                                            const std::string_view mode,
                                            const bool accepted,
                                            const size_t queue_depth,
                                            const uint64_t dropped_count,
                                            const std::string_view reason)
{
  const auto snapshot = update_snapshot([&](auto& state) {
    if (accepted) {
      ++state.targetQueueAcceptedCount;
    } else {
      ++state.targetQueueDroppedCount;
    }
  });

  if (accepted) {
    spdlog::info(
        "[FleetRuntimeQueued] stage=target source={} localSeq={} target={} mode={} depth={} accepted=true",
        trace.source, trace.localSequence, target, mode, queue_depth);
  } else {
    spdlog::warn(
        "[FleetRuntimeQueued] stage=target source={} localSeq={} target={} mode={} depth={} dropped={} "
        "accepted=false reason={}",
        trace.source, trace.localSequence, target, mode, queue_depth, dropped_count,
        reason.empty() ? "unknown" : reason);
  }

  auto details = json{{"stage", "target"},
                      {"accepted", accepted},
                      {"source", trace.source},
                      {"localSequence", trace.localSequence},
                      {"observedAtMs", trace.observedAtMs},
                      {"slotCount", trace.slotCount},
                      {"presentShipCount", trace.presentShipCount},
                      {"statusSummary", trace.statusSummary},
                      {"target", target},
                      {"mode", mode},
                      {"queueDepth", queue_depth},
                      {"droppedCount", dropped_count}};
  if (!reason.empty()) {
    details["reason"] = reason;
  }
  record_recent_event("fleet-runtime-queued", with_summary(std::move(details), snapshot));
}

void fleet_runtime_diagnostics_post_result(const FleetRuntimeTraceContext& trace,
                                           const std::string_view target,
                                           const std::string_view mode,
                                           const bool success,
                                           const long status_code,
                                           const std::string_view error_class,
                                           const int64_t elapsed_ms)
{
  const auto snapshot = update_snapshot([&](auto& state) {
    if (success) {
      ++state.postSuccessCount;
    } else {
      ++state.postFailureCount;
    }
  });

  if (success) {
    spdlog::info(
        "[FleetRuntimePost] source={} localSeq={} target={} mode={} outcome=success statusCode={} elapsedMs={}",
        trace.source, trace.localSequence, target, mode, status_code, elapsed_ms);
  } else {
    spdlog::warn(
        "[FleetRuntimePost] source={} localSeq={} target={} mode={} outcome=failure statusCode={} errorClass={} "
        "elapsedMs={}",
        trace.source, trace.localSequence, target, mode, status_code, error_class, elapsed_ms);
  }

  record_recent_event("fleet-runtime-post",
                      with_summary(json{{"source", trace.source},
                                        {"localSequence", trace.localSequence},
                                        {"observedAtMs", trace.observedAtMs},
                                        {"target", target},
                                        {"mode", mode},
                                        {"success", success},
                                        {"statusCode", status_code},
                                        {"errorClass", error_class},
                                        {"elapsedMs", elapsed_ms}},
                                   snapshot));
}

FleetRuntimeDiagnosticsSnapshot fleet_runtime_diagnostics_snapshot()
{
  return snapshot_copy();
}

void fleet_runtime_diagnostics_reset()
{
  {
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_snapshot = {};
  }
  s_next_local_sequence.store(0, std::memory_order_relaxed);
}