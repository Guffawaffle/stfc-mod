/**
 * @file ViewController.h
 * @brief Base view controller template.
 *
 * Mirrors the generic Digit.Client.UI.ViewController<T> base class.
 * Provides access to the canvas context (the data model backing the view).
 * Concrete view controllers inherit from this via CRTP.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

/**
 * @brief Generic base for game view controllers.
 *
 * Resolves the ViewController`1 parent class from the concrete subclass Y's
 * class helper, and exposes the CanvasContext property typed as T*.
 *
 * @tparam T The context/data-model type for this view.
 * @tparam Y The concrete derived class (CRTP), must expose get_class_helper().
 */
template <typename T, typename Y> struct ViewController {
public:
  __declspec(property(get = __get_CanvasContext)) T* CanvasContext;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static IL2CppClassHelper class_helper = Y::get_class_helper().GetParent("ViewController`1");
    return class_helper;
  }

public:
  T* __get_CanvasContext()
  {
    static auto field = get_class_helper().GetProperty("CanvasContext");
    return field.GetRaw<T>(this);
  }
};