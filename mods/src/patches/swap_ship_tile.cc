#include "swap_ship_tile.h"

#include "prime/ShipTileWidget.h"
#include "prime/Transform.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp-tabledefs.h>

#include <cstring>

namespace swap_ship_tile
{
Il2CppObject* SelectionContext(ShipTileWidget* widget)
{
  if (!widget)
    return nullptr;
  static auto selection =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "ShipSelectionViewController");
  static auto game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto get_component_in_parent = game_object.GetMethodInfo("GetComponentInParent", 2);
  auto* get_transform = ShipTileWidget::get_class_helper().GetMethodInfo("get_transform", 0);
  if (!selection.isValidHelper() || !get_component_in_parent || !get_transform)
    return nullptr;

  Il2CppException* exception = nullptr;
  auto* transform = reinterpret_cast<Transform*>(il2cpp_runtime_invoke(get_transform, widget, nullptr, &exception));
  if (exception || !transform || !transform->gameObject)
    return nullptr;

  void* type             = selection.GetType();
  bool  include_inactive = true;
  void* args[]{type, &include_inactive};
  exception = nullptr;
  auto* owner = il2cpp_runtime_invoke(get_component_in_parent, transform->gameObject, args, &exception);
  if (exception || !owner)
    return nullptr;

  for (auto* cls = owner->klass; cls; cls = il2cpp_class_get_parent(cls)) {
    void* iterator = nullptr;
    while (auto* field = il2cpp_class_get_fields(cls, &iterator)) {
      if (std::strcmp(il2cpp_field_get_name(field), "m_context") != 0 || !field->type
          || field->type->byref || (field->type->attrs & FIELD_ATTRIBUTE_STATIC))
        continue;
      Il2CppObject* context = nullptr;
      il2cpp_field_get_value(owner, field, &context);
      if (context && std::strcmp(il2cpp_class_get_name(context->klass), "ShipManagementViewContext") == 0
          && std::strcmp(il2cpp_class_get_namespace(context->klass), "Digit.Prime.Ships") == 0)
        return context;
      return nullptr;
    }
  }
  return nullptr;
}

bool IsInSelection(ShipTileWidget* widget)
{ return SelectionContext(widget) != nullptr; }
} // namespace swap_ship_tile
