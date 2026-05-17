/**
 * @file hotkey_router_modifier_query.cc
 * @brief Implementation of the modifier/Win32-key probes declared in the matching header.
 */
#include "patches/hotkey_router_modifier_query.h"

#include "prime/KeyCode.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace hotkey_router_modifier_query
{
input_binding::ModifierMask held_modifier_mask()
{
  input_binding::ModifierMask modifiers;
  for (const auto modifier_key : {KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                                  KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::LeftWindows, KeyCode::RightWindows,
                                  KeyCode::LeftCommand, KeyCode::RightCommand, KeyCode::AltGr}) {
    if (Key::Pressed(modifier_key)) {
      modifiers.Merge(input_binding::ModifierMask::FromPressedKey(modifier_key));
    }
  }

  return modifiers;
}

bool win32_key_pressed(const int virtual_key)
{
#ifdef _WIN32
  return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
#else
  (void)virtual_key;
  return false;
#endif
}

bool process_window_has_foreground()
{
#ifdef _WIN32
  const auto foreground_window = GetForegroundWindow();
  if (!foreground_window) {
    return false;
  }

  auto foreground_process_id = DWORD{};
  GetWindowThreadProcessId(foreground_window, &foreground_process_id);
  return foreground_process_id == GetCurrentProcessId();
#else
  return true;
#endif
}

int win32_virtual_key_for_key_code(const KeyCode key)
{
#ifdef _WIN32
  const auto value = static_cast<int>(key);
  if (key >= KeyCode::Alpha0 && key <= KeyCode::Alpha9) {
    return value;
  }
  if (key >= KeyCode::A && key <= KeyCode::Z) {
    return 'A' + (value - static_cast<int>(KeyCode::A));
  }

  switch (key) {
    case KeyCode::Backspace:
      return VK_BACK;
    case KeyCode::Tab:
      return VK_TAB;
    case KeyCode::Return:
      return VK_RETURN;
    case KeyCode::Escape:
      return VK_ESCAPE;
    case KeyCode::Space:
      return VK_SPACE;
    case KeyCode::LeftArrow:
      return VK_LEFT;
    case KeyCode::UpArrow:
      return VK_UP;
    case KeyCode::RightArrow:
      return VK_RIGHT;
    case KeyCode::DownArrow:
      return VK_DOWN;
    case KeyCode::Delete:
      return VK_DELETE;
    case KeyCode::LeftShift:
      return VK_LSHIFT;
    case KeyCode::RightShift:
      return VK_RSHIFT;
    case KeyCode::LeftControl:
      return VK_LCONTROL;
    case KeyCode::RightControl:
      return VK_RCONTROL;
    case KeyCode::LeftAlt:
      return VK_LMENU;
    case KeyCode::RightAlt:
      return VK_RMENU;
    case KeyCode::Mouse0:
      return VK_LBUTTON;
    case KeyCode::Mouse1:
      return VK_RBUTTON;
    case KeyCode::Mouse2:
      return VK_MBUTTON;
    case KeyCode::Mouse3:
      return VK_XBUTTON1;
    case KeyCode::Mouse4:
      return VK_XBUTTON2;
    default:
      break;
  }

  if (key >= KeyCode::F1 && key <= KeyCode::F12) {
    return VK_F1 + (value - static_cast<int>(KeyCode::F1));
  }
#else
  (void)key;
#endif
  return 0;
}

input_binding::ModifierMask physical_held_modifier_mask()
{
  auto modifiers = input_binding::ModifierMask{};
#ifdef _WIN32
  if (win32_key_pressed(VK_LSHIFT)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::LeftShift));
  }
  if (win32_key_pressed(VK_RSHIFT)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::RightShift));
  }
  if (win32_key_pressed(VK_LCONTROL)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::LeftControl));
  }
  if (win32_key_pressed(VK_RCONTROL)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::RightControl));
  }
  if (win32_key_pressed(VK_LMENU)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::LeftAlt));
  }
  if (win32_key_pressed(VK_RMENU)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::RightAlt));
  }
  if (win32_key_pressed(VK_LWIN)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::LeftWindows));
  }
  if (win32_key_pressed(VK_RWIN)) {
    modifiers.Merge(input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::RightWindows));
  }
#endif
  return modifiers;
}
} // namespace hotkey_router_modifier_query
