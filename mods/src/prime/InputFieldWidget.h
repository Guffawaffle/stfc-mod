#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "TMP_InputField.h"

struct InputFieldWidget {
public:
  __declspec(property(get = __get__input)) TMP_InputField*  _input;
  __declspec(property(get = __get_isActiveAndEnabled)) bool isActiveAndEnabled;

  void Focus()
  {
    static auto focus_method = get_class_helper().GetMethod<void(InputFieldWidget*)>("Focus");
    if (focus_method) {
      focus_method(this);
    }

    if (auto* input = _input; input) {
      input->ActivateInputField();
    }
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "InputFieldWidget");
    return class_helper;
  }

public:
  TMP_InputField* __get__input()
  {
    static auto offset = get_class_helper().GetField("_input").offset();
    return *reinterpret_cast<TMP_InputField**>(reinterpret_cast<uintptr_t>(this) + offset);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto property = get_class_helper().GetProperty("isActiveAndEnabled");
    return property.Get<bool>(this);
  }
};
