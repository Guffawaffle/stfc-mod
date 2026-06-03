/**
 * @file live_debug_navhook_trace_sink.cc
 * @brief Bounded navhook trace-file sink used by live-debug diagnostics.
 */
#include "patches/live_debug_navhook_trace_sink.h"

#include "config.h"
#include "diagnostics_file_policy.h"
#include "file.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace {

constexpr std::string_view kNavigationHookTraceFile = "community_patch_navhook_trace.log";

std::FILE*            g_navigation_hook_trace_file = nullptr;
std::filesystem::path g_navigation_hook_trace_open_path;

const std::filesystem::path& navigation_hook_trace_path()
{
  static const auto trace_path = []() {
    const auto& settings = AdvancedDiagnosticsFileSettings();
    const auto  target   = ResolveDiagnosticsFileTarget(
        kNavigationHookTraceFile, std::filesystem::path(File::MakePathString(kNavigationHookTraceFile)), settings.root);
    if (target.warning.has_value()) {
      spdlog::warn("[LiveDebugNavhookTrace] {}", *target.warning);
    }
    return target.path;
  }();

  return trace_path;
}

std::uintmax_t navigation_hook_trace_max_bytes()
{
  const auto& settings = AdvancedDiagnosticsFileSettings();
  return static_cast<std::uintmax_t>(std::max(1, settings.navhook_trace_max_kb)) * 1024u;
}

int navigation_hook_trace_total_files()
{
  return std::max(1, AdvancedDiagnosticsFileSettings().navhook_trace_files);
}

void close_navigation_hook_trace_file()
{
  if (!g_navigation_hook_trace_file) {
    return;
  }

  std::fclose(g_navigation_hook_trace_file);
  g_navigation_hook_trace_file = nullptr;
  g_navigation_hook_trace_open_path.clear();
}

std::FILE* open_navigation_hook_trace_file(const std::filesystem::path& trace_path)
{
  if (g_navigation_hook_trace_file && g_navigation_hook_trace_open_path == trace_path) {
    return g_navigation_hook_trace_file;
  }

  close_navigation_hook_trace_file();

  const auto path_text = trace_path.string();
  g_navigation_hook_trace_file = std::fopen(path_text.c_str(), "ab");
  if (g_navigation_hook_trace_file) {
    g_navigation_hook_trace_open_path = trace_path;
  }

  return g_navigation_hook_trace_file;
}

void warn_navigation_hook_trace_policy_once(const std::optional<std::string>& warning)
{
  if (!warning.has_value()) {
    return;
  }

  static bool warned = false;
  if (warned) {
    return;
  }

  warned = true;
  spdlog::warn("[LiveDebugNavhookTrace] {}", *warning);
}

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace

namespace live_debug_navhook_trace_sink {

void AppendStep(const char* step,
                const char* phase,
                const void* controller,
                const void* sender,
                const void* callback_context)
{
  std::ostringstream line;
  line << '[' << current_time_millis_utc() << "] step=" << (step ? step : "") << " phase='" << (phase ? phase : "")
       << "' controller=" << controller << " sender=" << sender << " callbackContext=" << callback_context << '\n';

  const auto payload    = line.str();
  const auto trace_path = navigation_hook_trace_path();
  close_navigation_hook_trace_file();
  const auto prepare = PrepareDiagnosticsFileForAppend(
      trace_path, navigation_hook_trace_max_bytes(), navigation_hook_trace_total_files(), payload.size());
  warn_navigation_hook_trace_policy_once(prepare.warning);
  if (!prepare.append_allowed) {
    return;
  }

  auto*      trace_file = open_navigation_hook_trace_file(trace_path);
  if (!trace_file) {
    return;
  }

  std::fwrite(payload.data(), sizeof(char), payload.size(), trace_file);
  std::fflush(trace_file);
}

} // namespace live_debug_navhook_trace_sink
