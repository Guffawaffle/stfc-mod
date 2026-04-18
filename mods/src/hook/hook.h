/**
 * @file hook.h
 * @brief MinHook wrapper — init, install, and convenience macro.
 *
 * Provides a thin layer over MinHook on Windows.  On non-Windows platforms,
 * hooks are not installed; the original pointer is set to the target so that
 * hook functions can still call through without crashing.
 */
#pragma once

#include <spdlog/spdlog.h>

#if _WIN32
#include <MinHook.h>
#endif

/// Initialize the hooking engine.  Call once before any mh_install().
inline void mh_init()
{
#if _WIN32
  if (auto s = MH_Initialize(); s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
    spdlog::error("MH_Initialize failed: {}", MH_StatusToString(s));
  }
#endif
}

/// Create and immediately enable a single inline hook.
/// @param target    Pointer to the function to hook.
/// @param detour    Pointer to the replacement function.
/// @param original  Out-pointer that receives the trampoline to the original.
/// @return true on success.
inline bool mh_install(void* target, void* detour, void** original)
{
#if _WIN32
  if (auto s = MH_CreateHook(target, detour, original); s != MH_OK) {
    spdlog::error("MH_CreateHook failed: {}", MH_StatusToString(s));
    return false;
  }
  if (auto s = MH_EnableHook(target); s != MH_OK) {
    spdlog::error("MH_EnableHook failed: {}", MH_StatusToString(s));
    return false;
  }
  return true;
#else
  // No inline hooking — set original to real target as a safety fallback.
  if (original) *original = target;
  spdlog::warn("Hook not installed (no hooking library available on this platform)");
  return false;
#endif
}

/// Convenience macro that handles the void* casts.
#define MH_INSTALL(target, hook_fn, orig_ptr) \
  mh_install((void*)(target), (void*)(hook_fn), (void**)&(orig_ptr))
