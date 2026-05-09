/**
 * @file Toast.h
 * @brief Toast notification data and state enumeration.
 *
 * Mirrors Digit.Prime.HUD.Toast — the in-game popup notifications for
 * battles, faction events, armada status, territory capture, etc.
 */
#pragma once

#include "toast_state.h"
#include <il2cpp/il2cpp_helper.h>

/**
 * @brief A single toast notification displayed in the HUD.
 *
 * Provides access to the notification's locale text context, attached data
 * payload, and display state (e.g. victory, defeat, incoming attack).
 */
struct Toast {
public:
  void* get_TextLocaleTextContext()
  {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(this) + 0x20);
  }

  Il2CppObject* get_Data()
  {
    return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(this) + 0x38);
  }

  int get_State()
  {
    static auto prop = get_class_helper().GetProperty("State");
    return *prop.Get<ToastState>((void *)this);
  }

private:
  static IL2CppClassHelper &get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "Toast");
    return class_helper;
  }
};
