#pragma once

#include <spud/detour.h>

namespace native_hooks
{
// These vendor errors do not distinguish pre-mutation refusal from failure
// after target bytes or thread IPs changed. Unknown future codes fail closed.
constexpr bool SpudInstallRequiresStop(spud::detour_error result) noexcept
{
  switch (result) {
    case spud::detour_error::kOk:
    case spud::detour_error::kUnsupportedArch:
    case spud::detour_error::kDecodeFailed:
    case spud::detour_error::kInsufficientBytes:
    case spud::detour_error::kTargetChanged:
    case spud::detour_error::kBusy:
    case spud::detour_error::kChainFull:
    case spud::detour_error::kControlFlowEnforced:
    case spud::detour_error::kNotFound:
    case spud::detour_error::kUnwindFailed:
      return false;
    case spud::detour_error::kProtectionFailed:
    case spud::detour_error::kSuspensionUnavailable:
    case spud::detour_error::kRollbackFailed:
      return true;
  }
  return true;
}

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
