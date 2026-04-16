#pragma once

#include <patches/gamefunctions.h>

#include <span>

enum class DispatchDecision {
  NoMatch,              // handler didn't match / didn't act
  HandledStop,          // handler acted, do NOT call original
  HandledAllowOriginal  // handler acted, still call original
};

enum class InputMode {
  Down,     // MapKey::IsDown — single trigger on key press
  Pressed   // MapKey::IsPressed — repeats while held
};

struct HotkeyEntry {
  GameFunction    game_function;
  DispatchDecision (*handler)();
  InputMode       input_mode = InputMode::Down;
};

std::span<const HotkeyEntry> GetHotkeyDispatchTable();
