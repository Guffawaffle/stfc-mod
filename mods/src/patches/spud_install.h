#pragma once

#include <spud/detour.h>

namespace native_hooks
{
// Installation-only reporting: no work is added to the hooked call path.
void ReportSpudInstall(spud::detour_error result, int os_error, spud::failure_detail detail, const char* file, int line,
                       const char* hook) noexcept;
void ReportSpudSummary() noexcept;
} // namespace native_hooks

// Preserve the vendor's per-call-site, permanent installation and cached null
// failure semantics. Capture diagnostics before logging can make other OS calls.
// Keep this adapter outside the unmodified vendor snapshot.
#undef SPUD_STATIC_DETOUR
#define SPUD_STATIC_DETOUR(addr, fn)                                                                                   \
  (([=]() {                                                                                                            \
    static const auto spud_static_trampoline = [&] {                                                                   \
      auto       spud_static_hook = SPUD_AUTO_HOOK(addr, fn);                                                          \
      const auto spud_result      = spud_static_hook.try_install_permanent();                                          \
      const auto spud_os_error    = spud_static_hook.last_os_error();                                                  \
      const auto spud_detail      = spud::last_failure_detail();                                                       \
      native_hooks::ReportSpudInstall(spud_result, spud_os_error, spud_detail, __FILE__, __LINE__, #fn);               \
      return spud_result == spud::detour_error::kOk ? spud_static_hook.trampoline() : nullptr;                         \
    }();                                                                                                               \
    return spud_static_trampoline;                                                                                     \
  })())
