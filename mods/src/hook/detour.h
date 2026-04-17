#pragma once

// MinHook-based replacement for SPUD_STATIC_DETOUR.
//
// Hook signature change (mechanical transformation):
//
//   BEFORE (SPUD):
//     void MyHook(auto original, Foo* self, int x) {
//         original(self, x);
//     }
//     SPUD_STATIC_DETOUR(ptr, MyHook);
//
//   AFTER (MinHook):
//     MH_HOOK(void, MyHook, Foo* self, int x) {
//         MyHook_original(self, x);
//     }
//     MH_ATTACH(ptr, MyHook);
//
// MH_HOOK(ret, name, ...) declares the trampoline pointer and function.
// MH_ATTACH(target, name) installs the hook via MinHook.

#include <MinHook.h>
#include <spdlog/spdlog.h>

#include <cstdlib>

namespace detour {

inline void init()
{
  if (MH_Initialize() != MH_OK) {
    spdlog::critical("MH_Initialize failed");
    std::abort();
  }
}

inline void shutdown()
{
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
}

namespace detail {

inline bool install(void* target, void* detour_fn, void** out_original, const char* name = nullptr)
{
  auto status = MH_CreateHook(target, detour_fn, out_original);
  if (status != MH_OK) {
    spdlog::error("MH_CreateHook failed for {}: {} (target={:p})",
                  name ? name : "?", MH_StatusToString(status), target);
    return false;
  }
  status = MH_EnableHook(target);
  if (status != MH_OK) {
    spdlog::error("MH_EnableHook failed for {}: {} (target={:p})",
                  name ? name : "?", MH_StatusToString(status), target);
    return false;
  }
  spdlog::trace("Hooked {} at {:p}", name ? name : "?", target);
  return true;
}

} // namespace detail
} // namespace detour

// MH_HOOK(ret, name, params...)
//   Declares: static ret(*name_original)(params...) = nullptr;
//             static ret name(params...)
//   Write the function body immediately after.
#define MH_HOOK(ret, name, ...)                                                \
  static ret (*name##_original)(__VA_ARGS__) = nullptr;                        \
  static ret name(__VA_ARGS__)

// MH_HOOK_NOARGS(ret, name)
//   For hooks that take zero parameters.
#define MH_HOOK_NOARGS(ret, name)                                              \
  static ret (*name##_original)() = nullptr;                                   \
  static ret name()

// MH_ATTACH(target, name)
//   Creates and enables a MinHook hook. Stores original in name_original.
#define MH_ATTACH(target, name)                                                \
  detour::detail::install(                                                     \
      reinterpret_cast<void*>(target),                                         \
      reinterpret_cast<void*>(&(name)),                                        \
      reinterpret_cast<void**>(&(name##_original)),                            \
      #name)

// MH_ATTACH_SLOT(target, HookType, slot)
//   For template-based multi-target hooks where the same hook logic is
//   installed on N different targets, each needing its own original pointer.
//
//   Each unique HookType<slot> is a separate type with its own static
//   `original` and `detour`. If two installations accidentally share
//   a slot, the second would silently overwrite the first's original
//   pointer — causing the first hook to call the WRONG target.
//
//   The guard below catches this at startup: if `original` is already
//   non-null, the slot was already used and we have a programming error.
#define MH_ATTACH_SLOT(target, HookType, slot)                                 \
  do {                                                                         \
    if (HookType<slot>::original != nullptr) {                                 \
      spdlog::critical("FATAL: " #HookType "<" #slot "> slot reused! "        \
                       "Each target needs a unique slot number.");              \
      std::abort();                                                            \
    }                                                                          \
    detour::detail::install(                                                   \
        reinterpret_cast<void*>(target),                                       \
        reinterpret_cast<void*>(&HookType<slot>::detour),                      \
        reinterpret_cast<void**>(&HookType<slot>::original),                   \
        #HookType "<" #slot ">");                                              \
  } while (0)
