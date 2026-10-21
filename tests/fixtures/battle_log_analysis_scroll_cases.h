#pragma once

#include <cstddef>

namespace battle_analysis::fixtures
{
struct ScrollCase {
  const char* name;
  std::size_t count;
  float       row, available, gutter, expected_visible;
  bool        cues;
};

// Anonymous, reusable display scenarios; no captured player/log content.
inline constexpr ScrollCase scroll_cases[] = {
    {"empty timeline", 0, 54, 432, 20, 432, false},           {"short timeline", 3, 54, 432, 20, 432, false},
    {"exactly eight shots", 8, 113.25f, 906, 28, 906, false}, {"twenty shots", 20, 113.25f, 906, 28, 850, true},
    {"Armada timeline", 227, 54, 432, 20, 392, true},         {"event limit", 200000, 54, 432, 20, 392, true},
    {"eight combatants", 8, 65, 548, 20, 548, false},         {"combatant limit", 128, 65, 548, 20, 508, true},
    {"very narrow viewport", 1, 54, 20, 20, 10, true},
};
} // namespace battle_analysis::fixtures
