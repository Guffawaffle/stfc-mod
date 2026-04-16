/**
 * @file Widget.h
 * @brief Base widget template.
 *
 * Mirrors the generic Digit.Client.UI.Widget<T> base class. Exposes the
 * widget context, enabled state, and IsActive() check. Concrete widgets
 * inherit from this via CRTP.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

/**
 * @brief Generic base for game UI widgets.
 *
 * Resolves the Widget`1 parent class from the concrete subclass Y's
 * class helper, and exposes Context, enabled, and isActiveAndEnabled.
 *
 * @tparam T The context/data-model type.
 * @tparam Y The concrete derived class (CRTP), must expose get_class_helper().
 */
template <typename T, typename Y> struct Widget {
public:
  __declspec(property(get = __get_Context)) T* Context;
  __declspec(property(get = __get_enabled)) bool enabled;
  __declspec(property(get = __get_isActiveAndEnabled)) bool isActiveAndEnabled;

  bool IsActive()
  {
    auto IsActive = get_class_helper().GetMethod<bool(Widget*)>("IsActive");
    return IsActive(this);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static IL2CppClassHelper class_helper = Y::get_class_helper().GetParent("Widget`1");
    return class_helper;
  }

public:
  bool __get_enabled()
  {
    static auto field = get_class_helper().GetProperty("enabled");
    return field.Get<bool>(this);
  }

  T* __get_Context()
  {
    static auto field = get_class_helper().GetProperty("Context");
    return field.GetRaw<T>(this);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto field = get_class_helper().GetProperty("isActiveAndEnabled");
    return *field.Get<bool>(this);
  }
};
