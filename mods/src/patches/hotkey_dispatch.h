/**
 * @file hotkey_dispatch.h
 * @brief Data-driven hotkey dispatch table mapping GameFunctions to handlers.
 *
 * Defines the dispatch table consumed by the hotkey router. Each entry binds a
 * GameFunction enum value to a handler callback and an input mode (single-press
 * vs. repeat-while-held). The router iterates the table on every frame and
 * invokes the first matching handler.
 */
#pragma once

#include <patches/gamefunctions.h>

#include <span>

/** Outcome returned by a dispatch handler to control post-handler flow. */
enum class DispatchDecision {
  NoMatch,              ///< Handler didn't match or didn't act.
  HandledStop,          ///< Handler acted; suppress the original method call.
  HandledAllowOriginal  ///< Handler acted; still call the original method.
};

/** How the router samples key state for a dispatch entry. */
enum class InputMode {
  Down,     ///< MapKey::IsDown — fires once on initial key press.
  Pressed   ///< MapKey::IsPressed — fires every frame while held.
};

/** A single row in the hotkey dispatch table. */
struct HotkeyEntry {
  GameFunction    game_function;          ///< The bound game function / key binding.
  DispatchDecision (*handler)();          ///< Callback invoked when the key is active.
  InputMode       input_mode = InputMode::Down; ///< Sampling mode (single-press vs. held).
};

/** @brief Returns the global dispatch table (static constexpr array). */
std::span<const HotkeyEntry> GetHotkeyDispatchTable();
