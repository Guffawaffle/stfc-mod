#pragma once

// Forced into only the pinned AAI battle_log_analysis.cc. Its two existing
// metadata-qualified hook sites retain their callbacks and real trampoline
// result, while installation uses the host's transaction/unsafe-state owner.
// No analytical or UI implementation is copied into the host.
#include <il2cpp/hook.h>

#undef SPUD_STATIC_DETOUR
#define SPUD_STATIC_DETOUR(address, callback)                                                                          \
  ([=]() {                                                                                                             \
    static const auto aai_host_trampoline = [&]() -> decltype(address) {                                               \
      auto aai_host_address =                                                                                          \
          reinterpret_cast<decltype(address)>(::il2cpp::CheckedNativeEntry(reinterpret_cast<void*>(address)));         \
      ::il2cpp::HookSet aai_host_hooks("AAI::" #callback);                                                             \
      if (!aai_host_address) {                                                                                         \
        aai_host_hooks.Missing();                                                                                      \
        (void)aai_host_hooks.Commit();                                                                                 \
        return nullptr;                                                                                                \
      }                                                                                                                \
      auto aai_host_handle = aai_host_hooks.StageRetained(SPUD_AUTO_HOOK(aai_host_address, callback));                 \
      if (!aai_host_hooks.Commit())                                                                                    \
        return nullptr;                                                                                                \
      return aai_host_handle->trampoline();                                                                            \
    }();                                                                                                               \
    return aai_host_trampoline;                                                                                        \
  }())
