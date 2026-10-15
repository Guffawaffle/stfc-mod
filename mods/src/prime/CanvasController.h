#pragma once

#include "Canvas.h"
#include "Transform.h"

#include <il2cpp/il2cpp.h>

struct CanvasController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "CanvasController">;

  bool ResetFocus()
  { return Call<"ResetFocus", bool>(); }
  bool Visible()
  { return Get<"Visible", bool>(); }
  bool get_enabled()
  { return Get<"enabled", bool>(); }
  bool m_Visible()
  { return Field<"m_visible", bool>(); }

  Il2CppString* name()
  { return Get<"name", Il2CppString*>(); }
  Transform* transform()
  { return Get<"transform", Transform*>(); }
  Canvas* canvas()
  { return Field<"m_canvas", Canvas*>(); }
};

inline CanvasController* GetCanvasControllerFromComponent(il2cpp::Object* component)
{
  if (!component)
    return nullptr;
  // Passing System.Type selects GetComponentInParent(Type) over the generic overload.
  return component->Call<"GetComponentInParent", CanvasController*>(CanvasController::Class::TypeObject());
}
