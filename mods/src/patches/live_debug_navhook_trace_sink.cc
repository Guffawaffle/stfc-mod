/**
 * @file live_debug_navhook_trace_sink.cc
 * @brief Bounded navhook trace-file sink used by live-debug diagnostics.
 */
#include "patches/live_debug_navhook_trace_sink.h"

#include "file.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view kNavigationHookTraceFile                = "community_patch_navhook_trace.log";
constexpr auto             kNavigationHookTraceMaxBytes            = 512 * 1024;
constexpr auto             kNavigationHookTraceRotateCheckInterval = 128;
constexpr auto             kNavigationHookTraceBackupCount         = 3;

std::FILE*            g_navigation_hook_trace_file = nullptr;
std::filesystem::path g_navigation_hook_trace_open_path;

std::filesystem::path get_live_debug_path(std::string_view filename)
{
  return std::filesystem::path(File::MakePathString(filename));
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

std::filesystem::path rotated_navigation_hook_trace_path(const std::filesystem::path& trace_path, int index)
{
  auto rotated_path = trace_path;
  rotated_path.replace_extension("." + std::to_string(index) + ".log");
  return rotated_path;
}

void rotate_navigation_hook_trace_if_needed(const std::filesystem::path& trace_path)
{
  static uint32_t check_counter = 0;
  if (++check_counter % kNavigationHookTraceRotateCheckInterval != 0) {
    return;
  }

  std::error_code error;
  const auto      trace_size = std::filesystem::file_size(trace_path, error);
  if (error || trace_size <= kNavigationHookTraceMaxBytes) {
    return;
  }

  close_navigation_hook_trace_file();

  std::filesystem::remove(rotated_navigation_hook_trace_path(trace_path, kNavigationHookTraceBackupCount), error);
  error.clear();
  for (int index = kNavigationHookTraceBackupCount - 1; index >= 1; --index) {
    const auto source = rotated_navigation_hook_trace_path(trace_path, index);
    const auto target = rotated_navigation_hook_trace_path(trace_path, index + 1);
    if (std::filesystem::exists(source, error)) {
      error.clear();
      std::filesystem::rename(source, target, error);
      error.clear();
    }
  }

  std::filesystem::rename(trace_path, rotated_navigation_hook_trace_path(trace_path, 1), error);
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
  const auto trace_path = get_live_debug_path(kNavigationHookTraceFile);
  auto*      trace_file = open_navigation_hook_trace_file(trace_path);
  if (!trace_file) {
    return;
  }

  std::fprintf(trace_file, "[%lld] step=%s phase='%s' controller=%p sender=%p callbackContext=%p\n",
               static_cast<long long>(current_time_millis_utc()), step ? step : "", phase ? phase : "", controller,
               sender, callback_context);
  std::fflush(trace_file);
  rotate_navigation_hook_trace_if_needed(trace_path);
}

} // namespace live_debug_navhook_trace_sink
