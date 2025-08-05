#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "ToastState.h"

struct ToastObserver {
public:
  __declspec(property(get = get_State)) ToastState State;

private:
  static IL2CppClassHelper &get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "Toast");
    return class_helper;
  }

public:
  ToastState get_State()
  {
    static auto prop = get_class_helper().GetProperty("State");
    return *prop.Get<ToastState>((void *)this);
  }
};
