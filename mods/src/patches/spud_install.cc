#include "spud_install.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace
{
std::atomic<unsigned> installed{0};
std::atomic<unsigned> refused{0};
} // namespace

void native_hooks::ReportSpudInstall(spud::detour_error result, int os_error, spud::failure_detail detail,
                                     const char* file, int line, const char* hook) noexcept
{
  const auto name = spud::to_string(result);
  if (result == spud::detour_error::kOk) {
    installed.fetch_add(1, std::memory_order_relaxed);
  } else {
    refused.fetch_add(1, std::memory_order_relaxed);
    // Also visible if the bootstrap hook fails before the file logger exists.
    std::fprintf(stderr, "[SPUD] %s:%d %s refused: %.*s os=%d reason=%s address=%p\n", file, line, hook,
                 int(name.size()), name.data(), os_error, detail.reason ? detail.reason : "none",
                 reinterpret_cast<void*>(detail.address));
  }
  try {
    if (result == spud::detour_error::kOk) {
      spdlog::debug("[SPUD] installed {} at {}:{}", hook, file, line);
    } else {
      spdlog::error("[SPUD] {} at {}:{} refused: {} os={} reason={} address={}", hook, file, line, name, os_error,
                    detail.reason ? detail.reason : "none", reinterpret_cast<void*>(detail.address));
      spdlog::default_logger()->flush();
    }
  } catch (...) {
    // Diagnostic delivery must not obscure a successful permanent installation.
  }
  if (result == spud::detour_error::kRollbackFailed) {
    // Executable bytes may be indeterminate. Never continue login/gameplay.
    std::abort();
  }
}

void native_hooks::ReportSpudSummary() noexcept
{
  try {
    spdlog::info("[SPUD] permanent hook attempts: installed={} refused={} (not a feature-parity claim)",
                 installed.load(std::memory_order_relaxed), refused.load(std::memory_order_relaxed));
  } catch (...) {
  }
}
