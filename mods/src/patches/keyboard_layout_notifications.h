#pragma once

#include "keyboard_layout_refresh.h"
#include <cstdint>

namespace keyboard_layout::notifications
{
// Game-thread calls only. The observer and refresh state must live until process exit.
// Unsupported platforms or subscription failure return false: retain layout-name polling.
bool Start(RefreshState& refresh);
void Watch(uintptr_t keyboard);
void Stop();
} // namespace keyboard_layout::notifications
