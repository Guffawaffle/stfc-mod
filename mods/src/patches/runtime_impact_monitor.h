#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

enum class RuntimeImpactProbe : uint8_t {
  FrameTickTotal = 0,
  FrameTickHotkeys,
  FrameTickLiveDebug,
  UiScaleUpdate,
  NavigationZoomUpdate,
  NavigationPanLateUpdate,
  NavigationPanOriginalLateUpdate,
  NavigationTouchPopulate,
  AspectRatioUpdate,
  HotkeyResetCache,
  HotkeyDispatchPlan,
  HotkeySpaceAction,
  HotkeySpaceActionContext,
  HotkeySpaceActionPreScanFallback,
  HotkeyContextState,
  HotkeyShipSelection,
  HotkeyUiRouting,
  HotkeyFleetRouting,
  HotkeyShipFleetBarLookup,
  HotkeyShipTow,
  HotkeyShipLocate,
  HotkeyShipSelectPanel,
  HotkeyUiSelectCurrent,
  HotkeyUiQueueToggle,
  HotkeyUiChatOpen,
  HotkeyUiOfficerCanvas,
  HotkeyUiTableDispatch,
  HotkeyUiChatChannel,
  HotkeyUiChatManagerLookup,
  HotkeyUiChatActivateInput,
  HotkeyUiChatOpenChannel,
  HotkeyShipLocateHideInteraction,
  HotkeyShipLocateRequestView,
  HotkeyShipRequestSelect,
  HotkeyShipElementAction,
  HotkeyShipTogglePanel,
  TraceInstrumentationOverhead,
  Max,
};

constexpr size_t kRuntimeImpactProbeCount = static_cast<size_t>(RuntimeImpactProbe::Max);

struct RuntimeImpactStats {
  uint64_t samples     = 0;
  uint64_t total_ns    = 0;
  uint64_t max_ns      = 0;
  uint64_t over_250us  = 0;
  uint64_t over_1000us = 0;

  [[nodiscard]] uint64_t average_ns() const;
};

struct RuntimeImpactReport {
  int64_t                                                  window_ms = 0;
  std::array<RuntimeImpactStats, kRuntimeImpactProbeCount> probes{};
};

class RuntimeImpactAggregator
{
public:
  explicit RuntimeImpactAggregator(int64_t report_interval_ms = 5000);

  [[nodiscard]] std::optional<RuntimeImpactReport> Record(RuntimeImpactProbe probe, uint64_t duration_ns,
                                                          int64_t now_ms);

private:
  int64_t                                                  report_interval_ms_ = 5000;
  int64_t                                                  window_start_ms_    = -1;
  std::array<RuntimeImpactStats, kRuntimeImpactProbeCount> probes_{};
};

const char*        RuntimeImpactProbeName(RuntimeImpactProbe probe);
void               runtime_impact_monitor_record(RuntimeImpactProbe probe, uint64_t duration_ns);
[[nodiscard]] bool RuntimeImpactDiagnosticsEnabled();

class ScopedRuntimeImpactTimer
{
public:
  ScopedRuntimeImpactTimer(RuntimeImpactProbe probe, bool enabled);
  ~ScopedRuntimeImpactTimer();

  template <typename Callback> decltype(auto) ExcludeCall(Callback&& callback)
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

  RuntimeImpactProbe probe_ = RuntimeImpactProbe::FrameTickTotal;
  Clock::time_point  start_{};
  uint64_t           excluded_ns_ = 0;
  bool               enabled_     = false;
};
