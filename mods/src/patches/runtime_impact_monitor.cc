#include "patches/runtime_impact_monitor.h"

#include "patches/runtime_impact_diagnostics.h"

#include <algorithm>

namespace
{
int64_t now_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void log_report(const RuntimeImpactReport& report)
{
  for (size_t index = 0; index < report.probes.size(); ++index) {
    const auto& stats = report.probes[index];
    if (stats.samples == 0) {
      continue;
    }

    runtime_impact_diagnostics::RecordProbeWindow({
        .probe       = static_cast<uint8_t>(index),
        .window_ms   = report.window_ms,
        .samples     = stats.samples,
        .total_ns    = stats.total_ns,
        .max_ns      = stats.max_ns,
        .over_250us  = stats.over_250us,
        .over_1000us = stats.over_1000us,
    });
  }
}

void runtime_impact_monitor_record_raw(const RuntimeImpactProbe probe, const uint64_t duration_ns)
{
  static RuntimeImpactAggregator aggregator(runtime_impact_diagnostics::kReportIntervalMs);
  if (auto report = aggregator.Record(probe, duration_ns, now_ms())) {
    log_report(*report);
  }
}
} // namespace

uint64_t RuntimeImpactStats::average_ns() const
{ return samples == 0 ? 0 : (total_ns / samples); }

RuntimeImpactAggregator::RuntimeImpactAggregator(const int64_t report_interval_ms)
    : report_interval_ms_(report_interval_ms)
{
}

std::optional<RuntimeImpactReport>
RuntimeImpactAggregator::Record(const RuntimeImpactProbe probe, const uint64_t duration_ns, const int64_t now_ms_value)
{
  if (window_start_ms_ < 0) {
    window_start_ms_ = now_ms_value;
  }

  auto& stats = probes_[static_cast<size_t>(probe)];
  ++stats.samples;
  stats.total_ns += duration_ns;
  stats.max_ns = std::max(stats.max_ns, duration_ns);
  if (duration_ns >= 250'000) {
    ++stats.over_250us;
  }
  if (duration_ns >= 1'000'000) {
    ++stats.over_1000us;
  }

  const auto elapsed_ms = now_ms_value - window_start_ms_;
  if (elapsed_ms < report_interval_ms_) {
    return std::nullopt;
  }

  RuntimeImpactReport report;
  report.window_ms = elapsed_ms;
  report.probes    = probes_;

  probes_          = {};
  window_start_ms_ = now_ms_value;
  return report;
}

const char* RuntimeImpactProbeName(const RuntimeImpactProbe probe)
{
  switch (probe) {
    case RuntimeImpactProbe::FrameTickTotal:
      return "frame_tick.total";
    case RuntimeImpactProbe::FrameTickHotkeys:
      return "frame_tick.hotkeys";
    case RuntimeImpactProbe::FrameTickLiveDebug:
      return "frame_tick.live_debug";
    case RuntimeImpactProbe::UiScaleUpdate:
      return "ui_scale.update_canvas_root";
    case RuntimeImpactProbe::NavigationZoomUpdate:
      return "navigation_zoom.update";
    case RuntimeImpactProbe::NavigationPanLateUpdate:
      return "navigation_pan.late_update";
    case RuntimeImpactProbe::NavigationPanOriginalLateUpdate:
      return "navigation_pan.original_late_update";
    case RuntimeImpactProbe::NavigationTouchPopulate:
      return "navigation_touch.populate";
    case RuntimeImpactProbe::AspectRatioUpdate:
      return "aspect_ratio.update";
    case RuntimeImpactProbe::HotkeyResetCache:
      return "hotkey.reset_cache";
    case RuntimeImpactProbe::HotkeyDispatchPlan:
      return "hotkey.dispatch_plan";
    case RuntimeImpactProbe::HotkeySpaceAction:
      return "hotkey.space_action";
    case RuntimeImpactProbe::HotkeySpaceActionContext:
      return "hotkey.space_action.context";
    case RuntimeImpactProbe::HotkeySpaceActionPreScanFallback:
      return "hotkey.space_action.prescan_fallback";
    case RuntimeImpactProbe::HotkeyContextState:
      return "hotkey.context_state";
    case RuntimeImpactProbe::HotkeyShipSelection:
      return "hotkey.ship_selection";
    case RuntimeImpactProbe::HotkeyUiRouting:
      return "hotkey.ui_routing";
    case RuntimeImpactProbe::HotkeyFleetRouting:
      return "hotkey.fleet_routing";
    case RuntimeImpactProbe::HotkeyShipFleetBarLookup:
      return "hotkey.ship.fleet_bar_lookup";
    case RuntimeImpactProbe::HotkeyShipTow:
      return "hotkey.ship.tow";
    case RuntimeImpactProbe::HotkeyShipLocate:
      return "hotkey.ship.locate";
    case RuntimeImpactProbe::HotkeyShipSelectPanel:
      return "hotkey.ship.select_panel";
    case RuntimeImpactProbe::HotkeyUiSelectCurrent:
      return "hotkey.ui.select_current";
    case RuntimeImpactProbe::HotkeyUiQueueToggle:
      return "hotkey.ui.queue_toggle";
    case RuntimeImpactProbe::HotkeyUiChatOpen:
      return "hotkey.ui.chat_open";
    case RuntimeImpactProbe::HotkeyUiOfficerCanvas:
      return "hotkey.ui.officer_canvas";
    case RuntimeImpactProbe::HotkeyUiTableDispatch:
      return "hotkey.ui.table_dispatch";
    case RuntimeImpactProbe::HotkeyUiChatChannel:
      return "hotkey.ui.chat_channel";
    case RuntimeImpactProbe::HotkeyUiChatManagerLookup:
      return "hotkey.ui.chat.manager_lookup";
    case RuntimeImpactProbe::HotkeyUiChatActivateInput:
      return "hotkey.ui.chat.activate_input";
    case RuntimeImpactProbe::HotkeyUiChatOpenChannel:
      return "hotkey.ui.chat.open_channel";
    case RuntimeImpactProbe::HotkeyShipLocateHideInteraction:
      return "hotkey.ship.locate.hide_interaction";
    case RuntimeImpactProbe::HotkeyShipLocateRequestView:
      return "hotkey.ship.locate.request_view";
    case RuntimeImpactProbe::HotkeyShipRequestSelect:
      return "hotkey.ship.select.request_select";
    case RuntimeImpactProbe::HotkeyShipElementAction:
      return "hotkey.ship.select.element_action";
    case RuntimeImpactProbe::HotkeyShipTogglePanel:
      return "hotkey.ship.select.toggle_panel";
    case RuntimeImpactProbe::TraceInstrumentationOverhead:
      return "trace.instrumentation_overhead";
    case RuntimeImpactProbe::Max:
      return "unknown";
  }

  return "unknown";
}

void runtime_impact_monitor_record(const RuntimeImpactProbe probe, const uint64_t duration_ns)
{ runtime_impact_monitor_record_raw(probe, duration_ns); }

bool RuntimeImpactDiagnosticsEnabled()
{ return runtime_impact_diagnostics::Enabled(); }

ScopedRuntimeImpactTimer::ScopedRuntimeImpactTimer(const RuntimeImpactProbe probe, const bool enabled)
    : probe_(probe)
    , enabled_(enabled)
{
  if (enabled_) {
    start_ = Clock::now();
  }
}

ScopedRuntimeImpactTimer::~ScopedRuntimeImpactTimer()
{
  if (!enabled_) {
    return;
  }

  const auto total_ns    = elapsed_ns(start_, Clock::now());
  const auto measured_ns = total_ns > excluded_ns_ ? total_ns - excluded_ns_ : 0;
  if (!runtime_impact_diagnostics::kTrackInstrumentationOverhead) {
    runtime_impact_monitor_record(probe_, measured_ns);
    return;
  }

  const auto overhead_start = Clock::now();
  runtime_impact_monitor_record(probe_, measured_ns);
  const auto overhead_ns = elapsed_ns(overhead_start, Clock::now());
  runtime_impact_monitor_record_raw(RuntimeImpactProbe::TraceInstrumentationOverhead, overhead_ns);
}

uint64_t ScopedRuntimeImpactTimer::elapsed_ns(const Clock::time_point start, const Clock::time_point end)
{ return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()); }
