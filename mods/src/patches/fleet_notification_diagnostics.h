/**
 * @file fleet_notification_diagnostics.h
 * @brief Temporary #255 fleet-notification diagnostic concern.
 */
#pragma once

#include "targeted_diagnostics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fleet_notification_diagnostics
{
inline constexpr targeted_diagnostics::ConcernSpec kConcernSpec{
    .id             = "fleet-notification-scan",
    .owner          = "runtime-observability",
    .tracking_issue = "#255",
    .status         = targeted_diagnostics::ConcernStatus::Temporary,
    .introduced_in  = {2, 1, 0},
    .sunset_at      = {2, 2, 0},
    .remove_or_revise_when =
        "Healthy and long-session captures determine whether scan work or cache state accumulates.",
    .promotion_criteria = "A demonstrated support consumer and measured low overhead justify a supported schema.",
};

enum class Phase : uint8_t {
  AcquireManager,
  EnumerateSlots,
};

enum class EndReason : uint8_t {
  Settled,
  NoFleets,
  MaxLifetime,
  Suspended,
};

struct FollowedStateCounts {
  int tiering_up    = 0;
  int repairing     = 0;
  int battling      = 0;
  int warp_charging = 0;
  int warping       = 0;
  int impulsing     = 0;
  int capturing     = 0;
};

struct CacheSnapshot {
  size_t  states                = 0;
  size_t  ship_names            = 0;
  size_t  resource_names        = 0;
  size_t  cargo_fill_levels     = 0;
  size_t  mining_eta            = 0;
  size_t  stale_states          = 0;
  size_t  stale_ship_names      = 0;
  size_t  stale_resource_names  = 0;
  size_t  stale_cargo_levels    = 0;
  size_t  stale_mining_eta      = 0;
  size_t  stale_scan_limit      = 0;
  bool    stale_scan_truncated  = false;
  int64_t collection_elapsed_us = 0;
};

struct ScanRequested {
  uint64_t session_id       = 0;
  uint64_t activation_count = 0;
  int      trigger_state    = 0;
};

struct ScanStarted {
  uint64_t session_id    = 0;
  uint64_t scan_id       = 0;
  int64_t  active_age_ms = 0;
  int64_t  cadence_ms    = 0;
};

struct PhaseStarted {
  uint64_t session_id = 0;
  uint64_t scan_id    = 0;
  Phase    phase      = Phase::AcquireManager;
};

struct PhaseCompleted {
  uint64_t session_id = 0;
  uint64_t scan_id    = 0;
  Phase    phase      = Phase::AcquireManager;
  int64_t  elapsed_us = 0;
};

struct ScanCompleted {
  uint64_t session_id           = 0;
  uint64_t scan_id              = 0;
  int64_t  elapsed_us           = 0;
  int      observed_count       = 0;
  int      follow_through_count = 0;
};

struct ScanSummary {
  uint64_t            session_id                   = 0;
  uint64_t            window_sequence              = 0;
  uint64_t            session_scan_count           = 0;
  uint64_t            window_scan_count            = 0;
  int64_t             active_age_ms                = 0;
  int64_t             cadence_ms                   = 0;
  int64_t             window_elapsed_ms            = 0;
  int64_t             scan_elapsed_total_us        = 0;
  int64_t             scan_elapsed_max_us          = 0;
  int64_t             producer_elapsed_total_us    = 0;
  int64_t             producer_elapsed_max_us      = 0;
  int64_t             diagnostic_overhead_total_us = 0;
  int64_t             diagnostic_overhead_max_us   = 0;
  uint64_t            scans_over_1ms               = 0;
  uint64_t            scans_over_5ms               = 0;
  uint64_t            observed_total               = 0;
  uint64_t            follow_through_total         = 0;
  FollowedStateCounts followed_states;
  CacheSnapshot       cache;
  CacheSnapshot       cache_high_water;
};

struct ScanEnded {
  uint64_t  session_id         = 0;
  uint64_t  session_scan_count = 0;
  int64_t   active_age_ms      = 0;
  EndReason reason             = EndReason::Suspended;
};

targeted_diagnostics::Concern& Concern();
void                           Reset();
void                           ScanWasRequested(int trigger_state, int64_t now_ms);
[[nodiscard]] uint64_t         BeginScan(int64_t now_ms);
[[nodiscard]] bool             CacheSnapshotDue(int64_t now_ms);
void                           BeginPhase(uint64_t scan_id, Phase phase);
void                           CompletePhase(uint64_t scan_id, Phase phase, int64_t elapsed_us);
void CompleteScan(uint64_t scan_id, int64_t now_ms, int64_t elapsed_us, int observed_count, int follow_through_count,
                  const FollowedStateCounts& followed_states, const CacheSnapshot* cache);
void CompleteTick(uint64_t scan_id, int64_t now_ms, int64_t producer_elapsed_us, int64_t game_work_elapsed_us);
void EndScanSession(EndReason reason, int64_t now_ms);

std::string_view PhaseName(Phase phase);
std::string_view EndReasonName(EndReason reason);
} // namespace fleet_notification_diagnostics

namespace targeted_diagnostics
{
template <> struct EventTraits<fleet_notification_diagnostics::ScanRequested> {
  static constexpr std::string_view event_type       = "scan-requested";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::ScanRequested&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::ScanStarted> {
  static constexpr std::string_view event_type       = "scan-started";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::ScanStarted&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::PhaseStarted> {
  static constexpr std::string_view event_type       = "phase-started";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::PhaseStarted&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::PhaseCompleted> {
  static constexpr std::string_view event_type       = "phase-completed";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::PhaseCompleted&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::ScanCompleted> {
  static constexpr std::string_view event_type       = "scan-completed";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::ScanCompleted&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::ScanSummary> {
  static constexpr std::string_view event_type       = "scan-summary";
  static constexpr uint32_t         schema_version   = 2;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::ScanSummary&, nlohmann::ordered_json&);
};

template <> struct EventTraits<fleet_notification_diagnostics::ScanEnded> {
  static constexpr std::string_view event_type       = "scan-ended";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const fleet_notification_diagnostics::ScanEnded&, nlohmann::ordered_json&);
};
} // namespace targeted_diagnostics
