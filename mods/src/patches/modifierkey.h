/**
 * @file modifierkey.h
 * @brief Groups left/right physical modifier keys under a single logical name.
 *
 * When the user writes "SHIFT-A" in a key binding, ModifierKey::Parse creates
 * a group containing both LeftShift and RightShift so that either physical key
 * satisfies the modifier. Generic names ("SHIFT", "ALT", "CTRL", "CMD", "WIN",
 * "APPLE") expand to both sides; specific names ("LSHIFT", "RCTRL") resolve to
 * a single physical key via Key::Parse.
 *
 * @see Key    — raw per-frame key state cache.
 * @see MapKey — composes ModifierKeys with a primary key into a full binding.
 */
#pragma once


#include <patches/gamefunctions.h>
#include <prime/KeyCode.h>

#include "key.h"

#include <string>
#include <vector>

struct ModifierKey
{
public:
  ModifierKey();

  /**
   * @brief Parses a modifier name (e.g. "SHIFT", "LCTRL") into a ModifierKey.
   * @param key  Modifier string segment, case-insensitive.
   * @return ModifierKey with matching KeyCodes, or empty if not a modifier.
   */
  static ModifierKey Parse(std::string_view key);

  /**
   * @brief Adds a left/right key pair under a single shortcut label.
   * @param shortcut  Display name for the modifier (e.g. "SHIFT").
   * @param modifier1 Primary physical key (e.g. LeftShift).
   * @param modifier2 Secondary physical key (e.g. RightShift), or KeyCode::None.
   */
  void AddModifier(std::string_view shortcut, KeyCode modifier1, KeyCode modifier2);

  /// Returns true if @p modifier is already in this group.
  bool Contains(KeyCode modifier);

  /// Returns true if any key in this modifier group is currently held.
  bool IsPressed();

  /// Returns true on the frame any key in this group was first pressed.
  bool IsDown();

  /// Returns true if at least one modifier KeyCode has been added.
  bool HasModifiers();

  /// Returns the display string for this modifier group (e.g. "SHIFT").
  std::string GetParsedValues();

private:
  std::vector<KeyCode>     Modifiers; ///< Physical KeyCodes in this group.
  std::vector<std::string> Shortcuts; ///< Display labels for each added modifier.
  bool                     hasModifier;
};
