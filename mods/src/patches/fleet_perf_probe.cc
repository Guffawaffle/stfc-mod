#include "patches/fleet_perf_probe.h"

#if defined(_WIN32) && defined(_M_X64)
#include "config.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace fleet_perf
{
namespace
{
  using Clock = std::chrono::steady_clock;
  constexpr std::array names{"watch",       "state_set",   "state_clear",  "flag_set",  "flag_clear",
                             "bind",        "cargo_event", "child_lookup", "highlight", "eta",
                             "sample_read", "sample_miss", "audio",        "toast"};
  static_assert(names.size() == static_cast<std::size_t>(Part::Count));
  struct Stat {
    uint64_t calls = 0, total_ns = 0, max_ns = 0, over_1ms = 0, over_5ms = 0;
  };
  struct Gap {
    double gap_ms = 0, measured_ms = 0;
  };
  // All these observations belong to the ScreenManager thread. No hot-path locks or file writes.
  thread_local bool                             enabled     = false;
  thread_local unsigned                         depth       = 0;
  thread_local uint64_t                         interval_ns = 0;
  thread_local std::array<Stat, names.size()>   stats{};
  std::shared_ptr<spdlog::details::thread_pool> pool;
  std::shared_ptr<spdlog::async_logger>         logger;
} // namespace

bool Enabled()
{ return enabled; }
void Begin()
{ ++depth; }
void End(Part part, Clock::time_point start)
{
  const auto ns =
      static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
  auto& stat = stats[static_cast<std::size_t>(part)];
  ++stat.calls;
  stat.total_ns += ns;
  stat.max_ns = (std::max)(stat.max_ns, ns);
  stat.over_1ms += ns >= 1'000'000;
  stat.over_5ms += ns >= 5'000'000;
  if (--depth == 0)
    interval_ns += ns; // Union of timed scopes, without double-counting nested work.
}

void Frame()
{
  if (!logger)
    return;
  enabled                                  = true;
  static auto               previous       = Clock::now();
  static auto               started        = previous;
  static bool               previous_focus = false;
  static uint64_t           frames = 0, foreground_frames = 0, background_frames = 0, transitions = 0;
  static uint64_t           over33 = 0, over50 = 0, over100 = 0;
  static double             foreground_ms = 0, measured_ms = 0;
  static std::array<Gap, 5> worst{};
  static int                previous_flags     = -1;
  static unsigned           config_changes     = 0;
  static double             previous_report_ms = 0;
  const auto                now                = Clock::now();
  const double              gap                = std::chrono::duration<double, std::milli>(now - previous).count();
  previous                                     = now;
  DWORD foreground_pid                         = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
  const bool   focus = foreground_pid == GetCurrentProcessId();
  const double work  = interval_ns / 1e6;
  interval_ns        = 0;
  ++frames;
  if (focus && previous_focus) {
    ++foreground_frames;
    foreground_ms += gap;
    measured_ms += work;
    over33 += gap >= 33.333;
    over50 += gap >= 50;
    over100 += gap >= 100;
    for (std::size_t i = 0; i < worst.size(); ++i) {
      if (gap > worst[i].gap_ms) {
        for (auto j = worst.size() - 1; j > i; --j)
          worst[j] = worst[j - 1];
        worst[i] = {gap, work};
        break;
      }
    }
  } else if (!focus && !previous_focus) {
    ++background_frames;
  } else {
    ++transitions;
  }
  previous_focus     = focus;
  const auto& config = Config::Get();
  const int   flags  = (config.highlight_opc_fleets ? 1 : 0) | (config.fleet_hud_opc_eta ? 2 : 0);
  if (previous_flags != -1 && flags != previous_flags)
    ++config_changes;
  previous_flags = flags;
  if (now - started < std::chrono::seconds(5))
    return;
  const auto report_started = Clock::now();
  try {
    nlohmann::json parts = nlohmann::json::object();
    for (std::size_t i = 0; i < stats.size(); ++i) {
      const auto& stat = stats[i];
      if (stat.calls)
        parts[names[i]] = {{"calls", stat.calls},
                           {"total_ms", stat.total_ns / 1e6},
                           {"max_ms", stat.max_ns / 1e6},
                           {"over_1ms", stat.over_1ms},
                           {"over_5ms", stat.over_5ms}};
    }
    auto gaps = nlohmann::json::array();
    for (const auto& item : worst) {
      if (item.gap_ms > 0)
        gaps.push_back({{"gap_ms", item.gap_ms}, {"measured_scopes_ms", item.measured_ms}});
    }
    logger->info("{}", nlohmann::json({{"event", "window"},
                                       {"unix_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                                                       std::chrono::system_clock::now().time_since_epoch())
                                                       .count()},
                                       {"window_ms", std::chrono::duration<double, std::milli>(now - started).count()},
                                       {"frames", frames},
                                       {"foreground_frames", foreground_frames},
                                       {"background_frames", background_frames},
                                       {"focus_transitions", transitions},
                                       {"foreground_gap_total_ms", foreground_ms},
                                       {"foreground_measured_scopes_ms", measured_ms},
                                       {"gaps_over_33ms", over33},
                                       {"gaps_over_50ms", over50},
                                       {"gaps_over_100ms", over100},
                                       {"worst_foreground_gaps", gaps},
                                       {"highlight_at_end", config.highlight_opc_fleets},
                                       {"eta_at_end", config.fleet_hud_opc_eta},
                                       {"visual_config_changes", config_changes},
                                       {"desktop_event_mask", config.notify_fleet_events},
                                       {"queue_overruns", pool->overrun_counter()},
                                       {"previous_report_ms", previous_report_ms},
                                       {"scope_totals_include_background", true},
                                       {"inclusive_scopes", parts}})
                           .dump());
  } catch (...) {
    // A diagnostic must never break gameplay because reporting failed.
  }
  previous_report_ms = std::chrono::duration<double, std::milli>(Clock::now() - report_started).count();
  stats              = {};
  worst              = {};
  frames = foreground_frames = background_frames = transitions = over33 = over50 = over100 = 0;
  foreground_ms = measured_ms = 0;
  config_changes              = 0;
  started                     = now;
}

void Install()
{
  const auto stamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
          .count();
  const auto filename =
      "community_fleet_perf_" + std::to_string(stamp) + "_" + std::to_string(GetCurrentProcessId()) + ".jsonl";
  try {
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, 2 * 1024 * 1024, 2);
    pool      = std::make_shared<spdlog::details::thread_pool>(64, 1);
    logger    = std::make_shared<spdlog::async_logger>("fleet-perf-science", sink, pool,
                                                       spdlog::async_overflow_policy::overrun_oldest);
    logger->set_pattern("%v");
    logger->flush_on(spdlog::level::info); // Flush happens on this logger's worker, never the game thread.
    logger->info(
        "{}",
        R"({"event":"start","schema":1,"base":"play-dev-72eeefed","scope":"ScreenManager thread only; inclusive scopes overlap; original OPC callbacks excluded; clear counters count pre/post portions separately; gaps are Update intervals, not GPU/render timings; first 120s can include fleet baseline seeding","cap_bytes":6291456})");
    spdlog::warn("[FleetPerf] local timing science active: {}", filename);
  } catch (const std::exception& error) {
    logger.reset();
    spdlog::warn("[FleetPerf] unavailable: {}", error.what());
  }
}
} // namespace fleet_perf
#else
namespace fleet_perf
{
bool Enabled()
{ return false; }
void Begin() {}
void End(Part, std::chrono::steady_clock::time_point) {}
void Frame() {}
void Install() {}
} // namespace fleet_perf
#endif
