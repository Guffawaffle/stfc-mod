#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "NavigationInteractionUIContext.h"
#include "SetCourseWidget.h"
#include "ViewController.h"

struct NavigationInteractionUIViewController
    : public ViewController<NavigationInteractionUIContext, NavigationInteractionUIViewController> {
public:
  __declspec(property(get = __get__setCourseWidget)) SetCourseWidget* _setCourseWidget;

  void OnSetCourseButtonClick()
  {
    static auto OnSetCourseButtonClick =
        get_class_helper().GetMethod<void(NavigationInteractionUIViewController*)>("OnSetCourseButtonClick");
    OnSetCourseButtonClick(this);
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController");
    return class_helper;
  }

  SetCourseWidget* __get__setCourseWidget()
  {
    static auto field = get_class_helper().GetField("_setCourseWidget").offset();
    return *(SetCourseWidget**)((char*)this + field);
  }

private:
  friend struct ViewController<NavigationInteractionUIContext, NavigationInteractionUIViewController>;
  friend class ObjectFinder<NavigationInteractionUIViewController>;
};
