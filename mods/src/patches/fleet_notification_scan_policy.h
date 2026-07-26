#pragma once

#include <cstdint>
#include <optional>

constexpr int64_t kFleetNotificationScanIntervalMs          = 250;
constexpr int64_t kFleetNotificationScanBackoffAfterMs      = 30'000;
constexpr int64_t kFleetNotificationScanBackoffIntervalMs   = 5'000;
constexpr int64_t kFleetNotificationScanMaxLifetimeMs       = 24LL * 60 * 60 * 1'000;
constexpr int     kFleetNotificationScanMaxConsecutiveEmpty = 8;

enum class FleetNotificationScanDecision : uint8_t {
  Idle,
  Wait,
  Scan,
  Expired,
};

enum class FleetNotificationScanObservation : uint8_t {
  Continue,
  Settled,
  NoFleets,
};

class FleetNotificationScanPolicy
{
public:
  void                                           RequestScan();
  void                                           Suspend();
  void                                           Reset();
  [[nodiscard]] FleetNotificationScanDecision    Evaluate(int64_t now_ms);
  [[nodiscard]] FleetNotificationScanObservation RecordObservation(int observed_count, int follow_through_count);
  [[nodiscard]] bool                             ScanRequested() const;

private:
  bool                   scan_requested_          = false;
  int                    consecutive_empty_scans_ = 0;
  std::optional<int64_t> started_ms_;
  std::optional<int64_t> last_scan_ms_;
};
