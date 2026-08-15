/**
 * @file fleet_notification_diagnostics.cc
 * @brief Typed fleet-notification records and temporary #255 aggregation.
 */
#include "patches/fleet_notification_diagnostics.h"

#include "patches/fleet_notification_scan_policy.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace fleet_notification_diagnostics
{
namespace
{
  constexpr int64_t kSummaryIntervalMs = 10'000;

  targeted_diagnostics::Concern s_concern{kConcernSpec};

  struct DiagnosticState {
    bool                session_active         = false;
    uint64_t            session_id             = 0;
    uint64_t            session_scan_count     = 0;
    uint64_t            window_sequence        = 0;
    uint64_t            window_scan_count      = 0;
    uint64_t            observed_total         = 0;
    uint64_t            follow_through_total   = 0;
    uint64_t            scans_over_1ms         = 0;
    uint64_t            scans_over_5ms         = 0;
    int64_t             session_started_ms     = 0;
    int64_t             window_started_ms      = 0;
    int64_t             scan_elapsed_total_us  = 0;
    int64_t             scan_elapsed_max_us    = 0;
    int64_t             last_cache_snapshot_ms = 0;
    FollowedStateCounts followed_states;
    CacheSnapshot       cache;
    CacheSnapshot       cache_high_water;
  };

  DiagnosticState s_state;
  uint64_t        s_next_session_id  = 0;
  uint64_t        s_next_scan_id     = 0;
  uint64_t        s_activation_count = 0;

  int64_t active_age_ms(const int64_t now_ms)
  { return s_state.session_active ? std::max<int64_t>(0, now_ms - s_state.session_started_ms) : 0; }

  int64_t cadence_ms(const int64_t age_ms)
  {
    return age_ms >= kFleetNotificationScanBackoffAfterMs ? kFleetNotificationScanBackoffIntervalMs
                                                          : kFleetNotificationScanIntervalMs;
  }

  void update_high_water(const CacheSnapshot& current, CacheSnapshot& high_water)
  {
    high_water.states                = std::max(high_water.states, current.states);
    high_water.ship_names            = std::max(high_water.ship_names, current.ship_names);
    high_water.resource_names        = std::max(high_water.resource_names, current.resource_names);
    high_water.cargo_fill_levels     = std::max(high_water.cargo_fill_levels, current.cargo_fill_levels);
    high_water.mining_eta            = std::max(high_water.mining_eta, current.mining_eta);
    high_water.stale_states          = std::max(high_water.stale_states, current.stale_states);
    high_water.stale_ship_names      = std::max(high_water.stale_ship_names, current.stale_ship_names);
    high_water.stale_resource_names  = std::max(high_water.stale_resource_names, current.stale_resource_names);
    high_water.stale_cargo_levels    = std::max(high_water.stale_cargo_levels, current.stale_cargo_levels);
    high_water.stale_mining_eta      = std::max(high_water.stale_mining_eta, current.stale_mining_eta);
    high_water.stale_scan_limit      = std::max(high_water.stale_scan_limit, current.stale_scan_limit);
    high_water.stale_scan_truncated  = high_water.stale_scan_truncated || current.stale_scan_truncated;
    high_water.collection_elapsed_us = std::max(high_water.collection_elapsed_us, current.collection_elapsed_us);
  }

  void add_followed_states(const FollowedStateCounts& source, FollowedStateCounts& target)
  {
    target.tiering_up += source.tiering_up;
    target.repairing += source.repairing;
    target.battling += source.battling;
    target.warp_charging += source.warp_charging;
    target.warping += source.warping;
    target.impulsing += source.impulsing;
    target.capturing += source.capturing;
  }

  void reset_window(const int64_t now_ms)
  {
    s_state.window_started_ms     = now_ms;
    s_state.window_scan_count     = 0;
    s_state.observed_total        = 0;
    s_state.follow_through_total  = 0;
    s_state.scans_over_1ms        = 0;
    s_state.scans_over_5ms        = 0;
    s_state.scan_elapsed_total_us = 0;
    s_state.scan_elapsed_max_us   = 0;
    s_state.followed_states       = {};
  }

  void emit_summary(const int64_t now_ms)
  {
    if (!s_state.session_active || s_state.window_scan_count == 0) {
      return;
    }

    const auto  age_ms = active_age_ms(now_ms);
    ScanSummary event{
        .session_id            = s_state.session_id,
        .window_sequence       = ++s_state.window_sequence,
        .session_scan_count    = s_state.session_scan_count,
        .window_scan_count     = s_state.window_scan_count,
        .active_age_ms         = age_ms,
        .cadence_ms            = cadence_ms(age_ms),
        .window_elapsed_ms     = std::max<int64_t>(0, now_ms - s_state.window_started_ms),
        .scan_elapsed_total_us = s_state.scan_elapsed_total_us,
        .scan_elapsed_max_us   = s_state.scan_elapsed_max_us,
        .scans_over_1ms        = s_state.scans_over_1ms,
        .scans_over_5ms        = s_state.scans_over_5ms,
        .observed_total        = s_state.observed_total,
        .follow_through_total  = s_state.follow_through_total,
        .followed_states       = s_state.followed_states,
        .cache                 = s_state.cache,
        .cache_high_water      = s_state.cache_high_water,
    };
    (void)TARGET_DIAGNOSTIC_WRITE(s_concern, event);
    reset_window(now_ms);
  }
} // namespace

targeted_diagnostics::Concern& Concern()
{ return s_concern; }

void Reset()
{
  s_state            = {};
  s_next_session_id  = 0;
  s_next_scan_id     = 0;
  s_activation_count = 0;
}

void ScanWasRequested(const int trigger_state, const int64_t now_ms)
{
  if (!TARGET_DIAGNOSTIC_ENABLED(s_concern)) {
    return;
  }

  s_state                    = {};
  s_state.session_active     = true;
  s_state.session_id         = ++s_next_session_id;
  s_state.session_started_ms = now_ms;
  s_state.window_started_ms  = now_ms;

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, ScanRequested{.session_id       = s_state.session_id,
                                                         .activation_count = ++s_activation_count,
                                                         .trigger_state    = trigger_state});
}

uint64_t BeginScan(const int64_t now_ms)
{
  if (!TARGET_DIAGNOSTIC_ENABLED(s_concern) || !s_state.session_active) {
    return 0;
  }

  const auto scan_id = ++s_next_scan_id;
  const auto age_ms  = active_age_ms(now_ms);
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, ScanStarted{.session_id    = s_state.session_id,
                                                       .scan_id       = scan_id,
                                                       .active_age_ms = age_ms,
                                                       .cadence_ms    = cadence_ms(age_ms)});
  return scan_id;
}

bool CacheSnapshotDue(const int64_t now_ms)
{
  return TARGET_DIAGNOSTIC_ENABLED(s_concern) && s_state.session_active
         && (s_state.last_cache_snapshot_ms == 0 || now_ms - s_state.last_cache_snapshot_ms >= kSummaryIntervalMs);
}

void BeginPhase(const uint64_t scan_id, const Phase phase)
{
  if (scan_id == 0 || !TARGET_DIAGNOSTIC_ENABLED(s_concern) || !s_state.session_active) {
    return;
  }
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern,
                                PhaseStarted{.session_id = s_state.session_id, .scan_id = scan_id, .phase = phase});
}

void CompletePhase(const uint64_t scan_id, const Phase phase, const int64_t elapsed_us)
{
  if (scan_id == 0 || !TARGET_DIAGNOSTIC_ENABLED(s_concern) || !s_state.session_active) {
    return;
  }
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, PhaseCompleted{.session_id = s_state.session_id,
                                                          .scan_id    = scan_id,
                                                          .phase      = phase,
                                                          .elapsed_us = std::max<int64_t>(0, elapsed_us)});
}

void CompleteScan(const uint64_t scan_id, const int64_t now_ms, const int64_t elapsed_us, const int observed_count,
                  const int follow_through_count, const FollowedStateCounts& followed_states,
                  const CacheSnapshot* cache)
{
  if (scan_id == 0 || !TARGET_DIAGNOSTIC_ENABLED(s_concern) || !s_state.session_active) {
    return;
  }

  const auto bounded_elapsed_us = std::max<int64_t>(0, elapsed_us);
  ++s_state.session_scan_count;
  ++s_state.window_scan_count;
  s_state.observed_total += static_cast<uint64_t>(std::max(0, observed_count));
  s_state.follow_through_total += static_cast<uint64_t>(std::max(0, follow_through_count));
  s_state.scan_elapsed_total_us += bounded_elapsed_us;
  s_state.scan_elapsed_max_us = std::max(s_state.scan_elapsed_max_us, bounded_elapsed_us);
  s_state.scans_over_1ms += bounded_elapsed_us >= 1'000 ? 1 : 0;
  s_state.scans_over_5ms += bounded_elapsed_us >= 5'000 ? 1 : 0;
  add_followed_states(followed_states, s_state.followed_states);
  if (cache) {
    s_state.cache                  = *cache;
    s_state.last_cache_snapshot_ms = now_ms;
    update_high_water(*cache, s_state.cache_high_water);
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, ScanCompleted{.session_id           = s_state.session_id,
                                                         .scan_id              = scan_id,
                                                         .elapsed_us           = bounded_elapsed_us,
                                                         .observed_count       = observed_count,
                                                         .follow_through_count = follow_through_count});

  if (now_ms - s_state.window_started_ms >= kSummaryIntervalMs) {
    emit_summary(now_ms);
  }
}

void EndScanSession(const EndReason reason, const int64_t now_ms)
{
  if (!TARGET_DIAGNOSTIC_ENABLED(s_concern) || !s_state.session_active) {
    return;
  }

  emit_summary(now_ms);
  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, ScanEnded{.session_id         = s_state.session_id,
                                                     .session_scan_count = s_state.session_scan_count,
                                                     .active_age_ms      = active_age_ms(now_ms),
                                                     .reason             = reason});
  s_state.session_active = false;
}

std::string_view PhaseName(const Phase phase)
{
  switch (phase) {
    case Phase::AcquireManager:
      return "acquire-manager";
    case Phase::EnumerateSlots:
      return "enumerate-slots";
  }
  return "unknown";
}

std::string_view EndReasonName(const EndReason reason)
{
  switch (reason) {
    case EndReason::Settled:
      return "settled";
    case EndReason::NoFleets:
      return "no-fleets";
    case EndReason::MaxLifetime:
      return "max-lifetime";
    case EndReason::Suspended:
      return "suspended";
  }
  return "unknown";
}
} // namespace fleet_notification_diagnostics

namespace targeted_diagnostics
{
namespace
{
  using json = nlohmann::ordered_json;

  json state_counts_json(const fleet_notification_diagnostics::FollowedStateCounts& counts)
  {
    return {{"tiering_up", counts.tiering_up},       {"repairing", counts.repairing}, {"battling", counts.battling},
            {"warp_charging", counts.warp_charging}, {"warping", counts.warping},     {"impulsing", counts.impulsing},
            {"capturing", counts.capturing}};
  }

  json cache_json(const fleet_notification_diagnostics::CacheSnapshot& cache)
  {
    return {{"states", cache.states},
            {"ship_names", cache.ship_names},
            {"resource_names", cache.resource_names},
            {"cargo_fill_levels", cache.cargo_fill_levels},
            {"mining_eta", cache.mining_eta},
            {"stale_states", cache.stale_states},
            {"stale_ship_names", cache.stale_ship_names},
            {"stale_resource_names", cache.stale_resource_names},
            {"stale_cargo_levels", cache.stale_cargo_levels},
            {"stale_mining_eta", cache.stale_mining_eta},
            {"stale_scan_limit", cache.stale_scan_limit},
            {"stale_scan_truncated", cache.stale_scan_truncated},
            {"collection_elapsed_us", cache.collection_elapsed_us}};
  }

  template <typename Event> size_t event_size(const Event&) noexcept
  { return sizeof(Event); }
} // namespace

size_t EventTraits<fleet_notification_diagnostics::ScanRequested>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::ScanRequested& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::ScanRequested>::SerializeFields(
    const fleet_notification_diagnostics::ScanRequested& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"activation_count", event.activation_count},
            {"trigger_state", event.trigger_state}};
}

size_t EventTraits<fleet_notification_diagnostics::ScanStarted>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::ScanStarted& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::ScanStarted>::SerializeFields(
    const fleet_notification_diagnostics::ScanStarted& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"scan_id", event.scan_id},
            {"active_age_ms", event.active_age_ms},
            {"cadence_ms", event.cadence_ms}};
}

size_t EventTraits<fleet_notification_diagnostics::PhaseStarted>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::PhaseStarted& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::PhaseStarted>::SerializeFields(
    const fleet_notification_diagnostics::PhaseStarted& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"scan_id", event.scan_id},
            {"phase", fleet_notification_diagnostics::PhaseName(event.phase)}};
}

size_t EventTraits<fleet_notification_diagnostics::PhaseCompleted>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::PhaseCompleted& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::PhaseCompleted>::SerializeFields(
    const fleet_notification_diagnostics::PhaseCompleted& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"scan_id", event.scan_id},
            {"phase", fleet_notification_diagnostics::PhaseName(event.phase)},
            {"elapsed_us", event.elapsed_us}};
}

size_t EventTraits<fleet_notification_diagnostics::ScanCompleted>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::ScanCompleted& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::ScanCompleted>::SerializeFields(
    const fleet_notification_diagnostics::ScanCompleted& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"scan_id", event.scan_id},
            {"elapsed_us", event.elapsed_us},
            {"observed_count", event.observed_count},
            {"follow_through_count", event.follow_through_count}};
}

size_t EventTraits<fleet_notification_diagnostics::ScanSummary>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::ScanSummary& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::ScanSummary>::SerializeFields(
    const fleet_notification_diagnostics::ScanSummary& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"window_sequence", event.window_sequence},
            {"session_scan_count", event.session_scan_count},
            {"window_scan_count", event.window_scan_count},
            {"active_age_ms", event.active_age_ms},
            {"cadence_ms", event.cadence_ms},
            {"window_elapsed_ms", event.window_elapsed_ms},
            {"scan_elapsed_total_us", event.scan_elapsed_total_us},
            {"scan_elapsed_max_us", event.scan_elapsed_max_us},
            {"scans_over_1ms", event.scans_over_1ms},
            {"scans_over_5ms", event.scans_over_5ms},
            {"observed_total", event.observed_total},
            {"follow_through_total", event.follow_through_total},
            {"followed_states", state_counts_json(event.followed_states)},
            {"cache", cache_json(event.cache)},
            {"cache_high_water", cache_json(event.cache_high_water)}};
}

size_t EventTraits<fleet_notification_diagnostics::ScanEnded>::EstimatedQueueBytes(
    const fleet_notification_diagnostics::ScanEnded& event) noexcept
{ return event_size(event); }

void EventTraits<fleet_notification_diagnostics::ScanEnded>::SerializeFields(
    const fleet_notification_diagnostics::ScanEnded& event, json& fields)
{
  fields = {{"session_id", event.session_id},
            {"session_scan_count", event.session_scan_count},
            {"active_age_ms", event.active_age_ms},
            {"reason", fleet_notification_diagnostics::EndReasonName(event.reason)}};
}
} // namespace targeted_diagnostics
