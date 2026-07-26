#include "patches/fleet_notification_scan_policy.h"

void FleetNotificationScanPolicy::RequestScan()
{ scan_requested_ = true; }

void FleetNotificationScanPolicy::Suspend()
{
  scan_requested_ = false;
  last_scan_ms_.reset();
}

void FleetNotificationScanPolicy::Reset()
{ Suspend(); }

bool FleetNotificationScanPolicy::ShouldScan(const int64_t now_ms)
{
  if (!scan_requested_) {
    return false;
  }

  if (!last_scan_ms_.has_value() || now_ms < *last_scan_ms_
      || now_ms - *last_scan_ms_ >= kFleetNotificationScanIntervalMs) {
    last_scan_ms_ = now_ms;
    return true;
  }

  return false;
}

bool FleetNotificationScanPolicy::ScanRequested() const
{ return scan_requested_; }
