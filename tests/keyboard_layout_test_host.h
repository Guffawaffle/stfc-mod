#pragma once
#include "keyboard_layout.h"

namespace keyboard_layout::fixture
{
void Reset();
void Map(KeyCode configured, KeyCode physical, bool shift = false);
bool Registered(KeyCode key);
} // namespace keyboard_layout::fixture
