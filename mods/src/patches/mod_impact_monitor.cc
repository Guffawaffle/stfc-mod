#include "patches/mod_impact_monitor.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace {
int64_t now_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void log_report(const ModImpactReport& report)
{
  for (size_t index = 0; index < report.probes.size(); ++index) {
    const auto& stats = report.probes[index];
    if (stats.samples == 0) {
      continue;
    }

    spdlog::info("[Impact] window_ms={} probe={} samples={} avg_us={:.3f} max_us={:.3f} over_250us={} over_1000us={}",
                 report.window_ms,
                 ModImpactProbeName(static_cast<ModImpactProbe>(index)),
                 stats.samples,
                 static_cast<double>(stats.average_ns()) / 1000.0,
                 static_cast<double>(stats.max_ns) / 1000.0,
                 stats.over_250us,
                 stats.over_1000us);
  }
}
}

uint64_t ModImpactStats::average_ns() const
{
  return samples == 0 ? 0 : (total_ns / samples);
}

ModImpactAggregator::ModImpactAggregator(const int64_t report_interval_ms)
  : report_interval_ms_(report_interval_ms)
{
}

std::optional<ModImpactReport> ModImpactAggregator::Record(const ModImpactProbe probe,
                                                           const uint64_t duration_ns,
                                                           const int64_t now_ms_value)
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

  ModImpactReport report;
  report.window_ms = elapsed_ms;
  report.probes = probes_;

  probes_ = {};
  window_start_ms_ = now_ms_value;
  return report;
}

const char* ModImpactProbeName(const ModImpactProbe probe)
{
  switch (probe) {
    case ModImpactProbe::FrameTickTotal:
      return "frame_tick.total";
    case ModImpactProbe::FrameTickHotkeys:
      return "frame_tick.hotkeys";
    case ModImpactProbe::FrameTickLiveDebug:
      return "frame_tick.live_debug";
    case ModImpactProbe::UiScaleUpdate:
      return "ui_scale.update_canvas_root";
    case ModImpactProbe::NavigationZoomUpdate:
      return "navigation_zoom.update";
    case ModImpactProbe::NavigationPanLateUpdate:
      return "navigation_pan.late_update";
    case ModImpactProbe::NavigationPanOriginalLateUpdate:
      return "navigation_pan.original_late_update";
    case ModImpactProbe::NavigationTouchPopulate:
      return "navigation_touch.populate";
    case ModImpactProbe::AspectRatioUpdate:
      return "aspect_ratio.update";
    case ModImpactProbe::HotkeyResetCache:
      return "hotkey.reset_cache";
    case ModImpactProbe::HotkeyDispatchPlan:
      return "hotkey.dispatch_plan";
    case ModImpactProbe::HotkeySpaceAction:
      return "hotkey.space_action";
    case ModImpactProbe::HotkeyContextState:
      return "hotkey.context_state";
    case ModImpactProbe::HotkeyShipSelection:
      return "hotkey.ship_selection";
    case ModImpactProbe::HotkeyUiRouting:
      return "hotkey.ui_routing";
    case ModImpactProbe::HotkeyFleetRouting:
      return "hotkey.fleet_routing";
    case ModImpactProbe::HotkeyShipFleetBarLookup:
      return "hotkey.ship.fleet_bar_lookup";
    case ModImpactProbe::HotkeyShipTow:
      return "hotkey.ship.tow";
    case ModImpactProbe::HotkeyShipLocate:
      return "hotkey.ship.locate";
    case ModImpactProbe::HotkeyShipSelectPanel:
      return "hotkey.ship.select_panel";
    case ModImpactProbe::HotkeyUiSelectCurrent:
      return "hotkey.ui.select_current";
    case ModImpactProbe::HotkeyUiQueueToggle:
      return "hotkey.ui.queue_toggle";
    case ModImpactProbe::HotkeyUiChatOpen:
      return "hotkey.ui.chat_open";
    case ModImpactProbe::HotkeyUiOfficerCanvas:
      return "hotkey.ui.officer_canvas";
    case ModImpactProbe::HotkeyUiTableDispatch:
      return "hotkey.ui.table_dispatch";
    case ModImpactProbe::HotkeyUiChatChannel:
      return "hotkey.ui.chat_channel";
    case ModImpactProbe::HotkeyUiChatManagerLookup:
      return "hotkey.ui.chat.manager_lookup";
    case ModImpactProbe::HotkeyUiChatActivateInput:
      return "hotkey.ui.chat.activate_input";
    case ModImpactProbe::HotkeyUiChatOpenChannel:
      return "hotkey.ui.chat.open_channel";
    case ModImpactProbe::HotkeyShipLocateHideInteraction:
      return "hotkey.ship.locate.hide_interaction";
    case ModImpactProbe::HotkeyShipLocateRequestView:
      return "hotkey.ship.locate.request_view";
    case ModImpactProbe::HotkeyShipRequestSelect:
      return "hotkey.ship.select.request_select";
    case ModImpactProbe::HotkeyShipElementAction:
      return "hotkey.ship.select.element_action";
    case ModImpactProbe::HotkeyShipTogglePanel:
      return "hotkey.ship.select.toggle_panel";
    case ModImpactProbe::Max:
      return "unknown";
  }

  return "unknown";
}

void mod_impact_monitor_record(const ModImpactProbe probe, const uint64_t duration_ns)
{
  static ModImpactAggregator aggregator;
  if (auto report = aggregator.Record(probe, duration_ns, now_ms())) {
    log_report(*report);
  }
}

ScopedModImpactTimer::ScopedModImpactTimer(const ModImpactProbe probe, const bool enabled)
  : probe_(probe),
    enabled_(enabled)
{
  if (enabled_) {
    start_ = Clock::now();
  }
}

ScopedModImpactTimer::~ScopedModImpactTimer()
{
  if (!enabled_) {
    return;
  }

  const auto total_ns = elapsed_ns(start_, Clock::now());
  mod_impact_monitor_record(probe_, total_ns > excluded_ns_ ? total_ns - excluded_ns_ : 0);
}

uint64_t ScopedModImpactTimer::elapsed_ns(const Clock::time_point start, const Clock::time_point end)
{
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}