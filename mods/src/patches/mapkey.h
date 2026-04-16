/**
 * @file mapkey.h
 * @brief Maps physical key combinations to game functions.
 *
 * MapKey is the binding layer between raw input (Key) and game actions
 * (GameFunction). Each MapKey holds one primary KeyCode plus zero or more
 * ModifierKeys. The static side maintains an array of bindings per
 * GameFunction, populated at startup from the TOML config, and exposes
 * IsPressed / IsDown queries used by the patch hooks.
 *
 * Config syntax example: "CTRL-SHIFT-A" → ModifierKey(CTRL) + ModifierKey(SHIFT) + Key(A).
 *
 * @see Key         — low-level cached key queries.
 * @see ModifierKey — left/right modifier grouping.
 */
#pragma once

#include "modifierkey.h"
#include <patches/gamefunctions.h>
#include <prime/KeyCode.h>

#include <array>
#include <string>
#include <vector>

class MapKey
{
public:
  MapKey();

  /**
   * @brief Parses a hyphen-delimited key string (e.g. "CTRL-A") into a MapKey.
   * @param key  Key combo string from TOML config (case-insensitive).
   * @return Parsed MapKey with Key and Modifiers populated.
   */
  static MapKey Parse(std::string_view key);

  /// Registers a parsed MapKey for the given game function.
  static void   AddMappedKey(GameFunction gameFunction, MapKey mappedKey);

  /// Returns true if any binding for @p gameFunction is held this frame (with correct modifiers).
  static bool   IsPressed(GameFunction gameFunction);

  /// Returns true on the frame any binding for @p gameFunction was first pressed.
  static bool   IsDown(GameFunction gameFunction);

  /// Validates that exactly the required modifiers are held (and no extras).
  static bool   HasCorrectModifiers(MapKey mapKey);

  /**
   * @brief Builds a display string of all shortcuts for a game function.
   * @return Pipe-separated shortcut list, e.g. "CTRL-A | F5".
   */
  static std::string GetShortcuts(GameFunction gameFunction);

  /// Returns this binding's shortcut string (e.g. "CTRL-A").
  std::string GetParsedValues() const;

  std::vector<ModifierKey> Modifiers; ///< Modifier groups required for this binding.
  std::vector<std::string> Shortcuts; ///< Individual segments of the parsed key string.

  KeyCode Key; ///< The primary (non-modifier) key.

private:
  /// All registered bindings, indexed by GameFunction ordinal.
  static std::array<std::vector<MapKey>, (int)GameFunction::Max> mappedKeys;

  bool hasModifiers; ///< True if this binding requires at least one modifier.
};
