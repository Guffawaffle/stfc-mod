#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "CanvasController.h"
#include "Hub.h"
#include "InputFieldWidget.h"

struct InventoryListViewController {
public:
  __declspec(property(get = __get__inputField)) InputFieldWidget*      _inputField;
  __declspec(property(get = __get__targetSection)) SectionID           _targetSection;
  __declspec(property(get = __get_isActiveAndEnabled)) bool            isActiveAndEnabled;
  __declspec(property(get = __get_canvasController)) CanvasController* canvasController;

private:
  friend class ObjectFinder<InventoryListViewController>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryListViewController");
    return class_helper;
  }

  InputFieldWidget* __get__inputField()
  {
    static auto offset = get_class_helper().GetField("_inputField").offset();
    return *reinterpret_cast<InputFieldWidget**>(reinterpret_cast<uintptr_t>(this) + offset);
  }

  SectionID __get__targetSection()
  {
    static auto offset = get_class_helper().GetField("_targetSection").offset();
    return *reinterpret_cast<SectionID*>(reinterpret_cast<uintptr_t>(this) + offset);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto property = get_class_helper().GetProperty("isActiveAndEnabled");
    return property.Get<bool>(this);
  }

  CanvasController* __get_canvasController()
  { return GetCanvasControllerFromComponent(this); }
};
