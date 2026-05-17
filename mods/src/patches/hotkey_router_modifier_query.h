/**
 * @file hotkey_router_modifier_query.h
 * @brief Modifier-mask + Win32 physical-key probes used by the hotkey router.
 *
 * These helpers wrap the two key-state sources we cross-check every frame:
 *   - `Key::Pressed(...)` (Unity-resolved KeyCode tables)
 *   - `GetAsyncKeyState(...)` (Win32 raw input fallback)
 *
 * Splitting them out of `hotkey_router.cc` keeps the platform `#ifdef _WIN32`
 * cruft localized and lets the trace log + the native shortcut guard reuse
 * the same probes without each pulling in `<Windows.h>` directly.
 */
#pragma once

#include "patches/input_binding/input_dispatcher.h"
#include "patches/key.h"

namespace hotkey_router_modifier_query
{
/// Modifier mask derived from Unity-resolved `Key::Pressed` for every modifier KeyCode.
input_binding::ModifierMask held_modifier_mask();

/// Modifier mask derived from Win32 `GetAsyncKeyState` (used when Unity input is bypassed).
input_binding::ModifierMask physical_held_modifier_mask();

/// True when the Win32 virtual-key bit is set in `GetAsyncKeyState`. Always false off Windows.
bool win32_key_pressed(int virtual_key);

/// True when the foreground window belongs to this process. Always true off Windows.
bool process_window_has_foreground();

/**
 * @brief Map a `KeyCode` to the matching Win32 virtual-key code.
 *
 * Returns 0 for unmapped keys (or off Windows). The mapping intentionally
 * covers only the keys that can appear in a runtime binding watched-key set.
 */
int win32_virtual_key_for_key_code(KeyCode key);
} // namespace hotkey_router_modifier_query
