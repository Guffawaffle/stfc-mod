#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Button.h"

struct ElementSelectorViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.UI", "ElementSelectorViewController");
    return class_helper;
  }

  bool isActiveAndEnabled()
  {
    static auto get_active = il2cpp_resolve_icall_typed<bool(ElementSelectorViewController*)>(
        "UnityEngine.Behaviour::get_isActiveAndEnabled()");
    return get_active ? get_active(this) : false;
  }

  void PressDecrement()
  {
    static auto offset = get_class_helper().GetField("_decrementButton").offset();
    if (auto* button = *reinterpret_cast<Button**>(reinterpret_cast<uintptr_t>(this) + offset); button) {
      button->Press();
    }
  }

  void PressIncrement()
  {
    static auto offset = get_class_helper().GetField("_incrementButton").offset();
    if (auto* button = *reinterpret_cast<Button**>(reinterpret_cast<uintptr_t>(this) + offset); button) {
      button->Press();
    }
  }
};
