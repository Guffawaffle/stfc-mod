#pragma once

#include <il2cpp/il2cpp.h>
#include "VisibilityController.h"

struct SetCourseWidget : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "SetCourseWidget">;
  VisibilityController* visibilityController()
  { return Field<"_visibilityController", VisibilityController*>(); }
  bool isActiveAndEnabled()
  { return Get<"isActiveAndEnabled", bool>(); }
};

struct NavigationInteractionUIViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController">;

  bool IsSetCourseVisible()
  {
    auto* widget = Field<"_setCourseWidget", SetCourseWidget*>();
    auto* visibility = widget ? widget->visibilityController() : nullptr;
    return visibility && (visibility->state() == VisibilityState::Visible || visibility->state() == VisibilityState::Show)
           && widget->isActiveAndEnabled();
  }
  void OnSetCourseButtonClick()
  { Call<"OnSetCourseButtonClick">(); }
};
