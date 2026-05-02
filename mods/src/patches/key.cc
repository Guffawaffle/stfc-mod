#include "key.h"
#include "prime/EventSystem.h"
#include <prime/TMP_InputField.h>

int Key::cacheInputFocused  = 0;
int Key::cacheInputModified = 0;

std::array<int, (int)KeyCode::Max> Key::cacheKeyPressed = {};
std::array<int, (int)KeyCode::Max> Key::cacheKeyDown    = {};

bool Key::IsModified()
{
  if (cacheInputModified == 0) {
    cacheInputModified = -1;
    if (Key::Pressed(KeyCode::LeftAlt) || Key::Pressed(KeyCode::LeftControl) || Key::Pressed(KeyCode::LeftShift)
        || Key::Pressed(KeyCode::RightAlt) || Key::Pressed(KeyCode::RightControl) || Key::Pressed(KeyCode::RightShift)
        || Key::Pressed(KeyCode::AltGr) || Key::Pressed(KeyCode::LeftCommand) || Key::Pressed(KeyCode::LeftWindows)
        || Key::Pressed(KeyCode::RightCommand) || Key::Pressed(KeyCode::RightWindows)) {
      cacheInputModified = 1;
    }
  }

  return cacheInputModified == 1;
}

void Key::ResetCache()
{
  Key::cacheInputFocused  = 0;
  Key::cacheInputModified = 0;
  for (int i = 0; i < (int)KeyCode::Max; i++) {
    Key::cacheKeyDown[i]    = 0;
    Key::cacheKeyPressed[i] = 0;
  }
}
bool Key::Down(KeyCode key)
{
  static auto GetKeyDownInt =
      il2cpp_resolve_icall_typed<bool(KeyCode)>("UnityEngine.Input::GetKeyDownInt(UnityEngine.KeyCode)");

  if (cacheKeyDown[(int)key] == 0) {
    cacheKeyDown[(int)key] = GetKeyDownInt(key) ? 1 : -1;
  }

  return cacheKeyDown[(int)key] == 1;
}

bool Key::Pressed(KeyCode key)
{
  static auto GetKeyInt =
      il2cpp_resolve_icall_typed<bool(KeyCode)>("UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)");

  if (cacheKeyPressed[(int)key] == 0) {
    cacheKeyPressed[(int)key] = GetKeyInt(key) ? 1 : -1;
  }

  return cacheKeyPressed[(int)key] == 1;
}

bool Key::IsInputFocused()
{
  if (cacheInputFocused == 0) {
    cacheInputFocused = -1;

    auto eventSystem = EventSystem::current();
    if (eventSystem) {
      auto n = eventSystem->currentSelectedGameObject;
      try {
        if (n) {
          auto n2 = n->GetComponentFastPath2<TMP_InputField>();
          if (n2 && n2->isFocused) {
            cacheInputFocused = 1;
          }
        }
      } catch (...) {
      }
    }
  }

  return cacheInputFocused == 1;
}

bool Key::HasShift()
{
  return Key::Pressed(KeyCode::LeftShift) || Key::Pressed(KeyCode::RightShift);
}

bool Key::HasAlt()
{
  return Key::Pressed(KeyCode::LeftAlt) || Key::Pressed(KeyCode::RightShift);
}

bool Key::HasCtrl()
{
  return Key::Pressed(KeyCode::LeftControl) || Key::Pressed(KeyCode::RightControl);
}

void Key::ClearInputFocus()
{
  try {
    if (auto eventSystem = EventSystem::current(); eventSystem) {
      if (auto n = eventSystem->currentSelectedGameObject; n) {
        auto n2 = n->GetComponentFastPath2<TMP_InputField>();
        if (n2 && n2->isFocused) {
          eventSystem->SetSelectedGameObject(nullptr);
          return;
        }
      }
    }
  } catch (...) {
    //
  }
}
