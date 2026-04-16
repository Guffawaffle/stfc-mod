/**
 * @file key.h
 * @brief Low-level key state tracking with per-frame caching.
 *
 * Key is the foundation of the input system. It wraps Unity's Input::GetKey
 * and Input::GetKeyDown icalls behind a per-frame cache so each physical key
 * is queried from the engine at most once per update. Higher layers (MapKey,
 * ModifierKey) build on Key to implement configurable key bindings.
 *
 * @see MapKey   — maps cached key states to game functions.
 * @see ModifierKey — groups left/right variants of Shift, Alt, Ctrl, etc.
 */
#pragma once

#include <prime/KeyCode.h>

#include <array>
#include <string>
#include <unordered_map>

class Key
{
private:
  /// Lookup table from user-facing key name strings to Unity KeyCode values.
  static const std::unordered_map<std::string, KeyCode> mappedKeys;

  // ─── Per-frame Cache ─────────────────────────────────────────────────────
  // Values use a tri-state int: 0 = unchecked, 1 = true, -1 = false.
  // ResetCache() zeroes everything at the start of each frame.

  static int cacheInputFocused;  ///< Whether a TMP_InputField currently has focus.
  static int cacheInputModified; ///< Whether any modifier key is held this frame.

  static std::array<int, (int)KeyCode::Max> cacheKeyPressed; ///< Tri-state cache for GetKey (held).
  static std::array<int, (int)KeyCode::Max> cacheKeyDown;    ///< Tri-state cache for GetKeyDown (just pressed).

public:
  /// Deselects the currently focused TMP_InputField, if any.
  static void    ClearInputFocus();

  /// Resets all per-frame caches. Must be called once at the start of each update.
  static void    ResetCache();

  /**
   * @brief Converts a user-facing key name (e.g. "LSHIFT", "A") to a KeyCode.
   * @param key  Case-insensitive key name from the config file.
   * @return Matching KeyCode, or KeyCode::None if unrecognised.
   */
  static KeyCode Parse(std::string_view key);

  /// Returns true while the key is held down (cached GetKey).
  static bool Pressed(KeyCode key);

  /// Returns true on the frame the key was first pressed (cached GetKeyDown).
  static bool Down(KeyCode key);

  /// Returns true if @p key is any modifier (Shift, Ctrl, Alt, Cmd, Win).
  static bool IsModifier(KeyCode key);

  /// Returns true if any modifier key is currently held.
  static bool IsModified();

  /// Returns true if a TMP_InputField has keyboard focus (suppress hotkeys).
  static bool IsInputFocused();

  /// @name Convenience modifier checks
  /// @{
  static bool HasShift();
  static bool HasAlt();
  static bool HasCtrl();
  /// @}
};
