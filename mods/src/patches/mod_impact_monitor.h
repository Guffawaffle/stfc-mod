#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

enum class ModImpactProbe : uint8_t {
  FrameTickTotal = 0,
  FrameTickHotkeys,
  FrameTickLiveDebug,
  UiScaleUpdate,
  NavigationZoomUpdate,
  NavigationPanLateUpdate,
  AspectRatioUpdate,
  Max,
};

constexpr size_t kModImpactProbeCount = static_cast<size_t>(ModImpactProbe::Max);

struct ModImpactStats {
  uint64_t samples = 0;
  uint64_t total_ns = 0;
  uint64_t max_ns = 0;
  uint64_t over_250us = 0;
  uint64_t over_1000us = 0;

  [[nodiscard]] uint64_t average_ns() const;
};

struct ModImpactReport {
  int64_t window_ms = 0;
  std::array<ModImpactStats, kModImpactProbeCount> probes{};
};

class ModImpactAggregator
{
public:
  explicit ModImpactAggregator(int64_t report_interval_ms = 5000);

  [[nodiscard]] std::optional<ModImpactReport> Record(ModImpactProbe probe, uint64_t duration_ns, int64_t now_ms);

private:
  int64_t report_interval_ms_ = 5000;
  int64_t window_start_ms_ = -1;
  std::array<ModImpactStats, kModImpactProbeCount> probes_{};
};

const char* ModImpactProbeName(ModImpactProbe probe);
void mod_impact_monitor_record(ModImpactProbe probe, uint64_t duration_ns);

class ScopedModImpactTimer
{
public:
  ScopedModImpactTimer(ModImpactProbe probe, bool enabled);
  ~ScopedModImpactTimer();

  template <typename Callback>
  decltype(auto) ExcludeCall(Callback&& callback)
  {
    if (!enabled_) {
      return callback();
    }

    const auto start = Clock::now();
    if constexpr (std::is_void_v<decltype(callback())>) {
      callback();
      excluded_ns_ += elapsed_ns(start, Clock::now());
      return;
    } else {
      auto result = callback();
      excluded_ns_ += elapsed_ns(start, Clock::now());
      return result;
    }
  }

private:
  using Clock = std::chrono::steady_clock;

  [[nodiscard]] static uint64_t elapsed_ns(Clock::time_point start, Clock::time_point end);

  ModImpactProbe    probe_ = ModImpactProbe::FrameTickTotal;
  Clock::time_point start_{};
  uint64_t          excluded_ns_ = 0;
  bool              enabled_ = false;
};