#pragma once

#include "NavigationInteractionUIContext.h"
#include "SemaphoreButtonListener.h"
#include "Widget.h"

#include <il2cpp/il2cpp_helper.h>

struct MissionLocationWidget : public Widget<NavigationInteractionUIContext, MissionLocationWidget> {
public:
  __declspec(property(get = __get_SetCourseButton)) SemaphoreButtonListener* SetCourseButton;
  __declspec(property(get = __get__isVisible)) bool                          _isVisible;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "MissionLocationWidget");
    return class_helper;
  }

public:
  SemaphoreButtonListener* __get_SetCourseButton()
  {
    static auto prop = get_class_helper().GetProperty("SetCourseButton");
    return prop.GetRaw<SemaphoreButtonListener>(this);
  }

  bool __get__isVisible()
  {
    static auto field = get_class_helper().GetField("_isVisible").offset();
    return *(bool*)((char*)this + field);
  }

  friend struct Widget<NavigationInteractionUIContext, MissionLocationWidget>;
};
