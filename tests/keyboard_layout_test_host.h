#pragma once
#include "keyboard_layout.h"

namespace keyboard_layout::fixture
{
void Reset();
void Map(KeyCode configured, KeyCode physical, bool shift = false);
bool Registered(KeyCode key);
void PublishDiagnostics(std::string_view layout, std::string_view status, std::string_view reason);
} // namespace keyboard_layout::fixture
