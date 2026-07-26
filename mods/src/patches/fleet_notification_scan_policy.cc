#include "patches/fleet_notification_scan_policy.h"

void FleetNotificationScanPolicy::RequestScan()
{
  if (scan_requested_) {
    return;
  }

  scan_requested_          = true;
  consecutive_empty_scans_ = 0;
  started_ms_.reset();
  last_scan_ms_.reset();
}

void FleetNotificationScanPolicy::Suspend()
{
  scan_requested_          = false;
  consecutive_empty_scans_ = 0;
  started_ms_.reset();
  last_scan_ms_.reset();
}

void FleetNotificationScanPolicy::Reset()
{ Suspend(); }

FleetNotificationScanDecision FleetNotificationScanPolicy::Evaluate(const int64_t now_ms)
{
  if (!scan_requested_) {
    return FleetNotificationScanDecision::Idle;
  }

  if (!started_ms_.has_value() || now_ms < *started_ms_ || (last_scan_ms_.has_value() && now_ms < *last_scan_ms_)) {
    started_ms_              = now_ms;
    last_scan_ms_            = now_ms;
    consecutive_empty_scans_ = 0;
    return FleetNotificationScanDecision::Scan;
  }

  const auto lifetime_ms = now_ms - *started_ms_;
  if (lifetime_ms >= kFleetNotificationScanMaxLifetimeMs) {
    Suspend();
    return FleetNotificationScanDecision::Expired;
  }

  const auto interval_ms = lifetime_ms >= kFleetNotificationScanBackoffAfterMs ? kFleetNotificationScanBackoffIntervalMs
                                                                               : kFleetNotificationScanIntervalMs;
  if (!last_scan_ms_.has_value() || now_ms - *last_scan_ms_ >= interval_ms) {
    last_scan_ms_ = now_ms;
    return FleetNotificationScanDecision::Scan;
  }

  return FleetNotificationScanDecision::Wait;
}

FleetNotificationScanObservation FleetNotificationScanPolicy::RecordObservation(const int observed_count,
                                                                                const int follow_through_count)
{
  if (!scan_requested_) {
    return FleetNotificationScanObservation::Settled;
  }

  if (observed_count <= 0) {
    ++consecutive_empty_scans_;
    if (consecutive_empty_scans_ >= kFleetNotificationScanMaxConsecutiveEmpty) {
      Suspend();
      return FleetNotificationScanObservation::NoFleets;
    }
    return FleetNotificationScanObservation::Continue;
  }

  consecutive_empty_scans_ = 0;
  if (follow_through_count <= 0) {
    Suspend();
    return FleetNotificationScanObservation::Settled;
  }

  return FleetNotificationScanObservation::Continue;
}

bool FleetNotificationScanPolicy::ScanRequested() const
{ return scan_requested_; }
