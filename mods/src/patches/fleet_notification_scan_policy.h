#pragma once

#include <cstdint>
#include <optional>

constexpr int64_t kFleetNotificationScanIntervalMs = 250;

class FleetNotificationScanPolicy
{
public:
  void               RequestScan();
  void               Suspend();
  void               Reset();
  [[nodiscard]] bool ShouldScan(int64_t now_ms);
  [[nodiscard]] bool ScanRequested() const;

private:
  bool                   scan_requested_ = false;
  std::optional<int64_t> last_scan_ms_;
};
