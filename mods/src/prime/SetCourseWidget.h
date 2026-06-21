#pragma once

#include "NavigationInteractionUIContext.h"
#include "SemaphoreButtonListener.h"
#include "VisibilityController.h"
#include "Widget.h"

#include <il2cpp/il2cpp_helper.h>

struct SetCourseWidget : public Widget<NavigationInteractionUIContext, SetCourseWidget> {
public:
  __declspec(property(get = __get_SetCourseButton)) SemaphoreButtonListener*    SetCourseButton;
  __declspec(property(get = __get__visibilityController)) VisibilityController* _visibilityController;
  __declspec(property(get = __get__isVisible)) bool                             _isVisible;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "SetCourseWidget");
    return class_helper;
  }

public:
  SemaphoreButtonListener* __get_SetCourseButton()
  {
    static auto prop = get_class_helper().GetProperty("SetCourseButton");
    return prop.GetRaw<SemaphoreButtonListener>(this);
  }

  VisibilityController* __get__visibilityController()
  {
    static auto field = get_class_helper().GetField("_visibilityController").offset();
    return *(VisibilityController**)((char*)this + field);
  }

  bool __get__isVisible()
  {
    static auto field = get_class_helper().GetField("_isVisible").offset();
    return *(bool*)((char*)this + field);
  }

  friend struct Widget<NavigationInteractionUIContext, SetCourseWidget>;
};
