#pragma once

#include "config.h"
#include "shortcuts.h"

namespace dock_assignment
{
// Raw keys remain readable by Settings' recorder. This separate frame consumer
// must honor capture itself; HandleShortcuts runs after feature callbacks.
inline bool EnterRequested(bool installed)
{
  const auto& config = Config::Get();
  return installed && config.patches.hotkeyhooks && config.patches.doubleclickassignshiphooks
         && config.ui.double_click_to_assign_ship && !Shortcut::CaptureBlocksShortcuts()
         && (unity::Key::Down(unity::KeyCode::Return) || unity::Key::Down(unity::KeyCode::KeypadEnter))
         && !unity::Key::IsInputFocused();
}
} // namespace dock_assignment
