#pragma once

// Game UI hit testing for the camera wheel guard. Observation only: the native
// scroll handler still receives its input, and owns the meaning of that input.
#include "game/navigation.h"
#include "game/ui_pointer.h"

#include <il2cpp/il2cpp.h>
#include <unity/input.h>
#include <unity/ui.h>

namespace scroll_input
{
using WheelRecognizer =
    il2cpp::Class<"Assembly-CSharp", "Digit.Client.CameraController", "MouseScrollAsPinchRecognizer">;

namespace detail
{
  using Handler = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "IScrollHandler">;

  struct Api {
    il2cpp::Method parents;
    FieldInfo*     pinch = nullptr;
    bool           ready = false;
  };

  template <typename Value> FieldInfo* Field(Il2CppClass* owner, const char* name, Il2CppTypeEnum type)
  {
    auto* field = owner ? il2cpp_class_get_field_from_name(owner, name) : nullptr;
    if (!field || !field->type || field->type->byref || field->type->type != type
        || (field->type->attrs & FIELD_ATTRIBUTE_STATIC) || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
      return nullptr;
    const auto size = il2cpp_class_instance_size(owner);
    return size >= sizeof(Value) && static_cast<uint32_t>(field->offset) <= size - sizeof(Value) ? field : nullptr;
  }

  inline const Api& Native()
  {
    static const Api api = [] {
      Api result;
      // Discovery only. These methods and Unity state are never invoked while
      // hook installation is running in the il2cpp_init callback.
      result.parents = unity::GameObject::Class::Method<il2cpp::ArrayObject*(
          Il2CppReflectionType*, bool, bool, bool, bool, Il2CppObject*)>("GetComponentsInternal");
      result.pinch   = Field<bool>(NavigationZoom::Class::Raw(), "_pinchStarted", IL2CPP_TYPE_BOOLEAN);
      auto* delta    = Field<float>(NavigationZoom::Class::Raw(), "_zoomDelta", IL2CPP_TYPE_R4);
      auto* last     = Field<float>(NavigationZoom::Class::Raw(), "_lastZoomDelta", IL2CPP_TYPE_R4);
      result.ready   = ui_pointer::Available() && result.parents && result.pinch && delta && last && Handler::Raw()
                       && unity::Input::PointerAvailable()
                       && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current")
                       && unity::Behaviour::Class::Method<bool()>("get_isActiveAndEnabled");
      return result;
    }();
    return api;
  }

} // namespace detail

inline bool Available()
{ return detail::Native().ready; }

inline il2cpp::CallResult<bool> PinchStarted(NavigationZoom* zoom)
{
  const auto& api = detail::Native();
  return api.ready && zoom ? il2cpp::CallResult<bool>{true, il2cpp::detail::ReadField<bool>(api.pinch, zoom)}
                           : il2cpp::CallResult<bool>{};
}

inline il2cpp::CallResult<bool> PointerOwnsScroll()
{
  const auto& api = detail::Native();
  if (!api.ready)
    return {};
  auto hit = ui_pointer::TopObject();
  if (!hit)
    return {};
  if (!hit.value)
    return {true, false};
  auto& object = hit.value;
  auto  type   = il2cpp::GcHandle::Strong(
      reinterpret_cast<Il2CppObject*>(il2cpp_type_get_object(il2cpp_class_get_type(detail::Handler::Raw()))));
  if (!type)
    return {};
  bool       typed_array = false, recursive = true, include_inactive = false, reverse = true;
  void*      parent_args[]{type.Get(), &typed_array, &recursive, &include_inactive, &reverse, nullptr};
  const auto parents = il2cpp::detail::TryInvokeBoxed(api.parents.Info(), object.Get(), parent_args);
  if (!parents)
    return {};
  auto owned_handlers = il2cpp::GcHandle::Strong(parents.value);
  if (parents.value && !owned_handlers)
    return {};
  const auto handlers = il2cpp::TryReferenceArray<Il2CppObject*>(owned_handlers.Get());
  if (!handlers)
    return {};
  if (!handlers.value)
    return {true, false};
  for (auto* handler : *handlers.value) {
    if (!handler)
      continue;
    auto held = il2cpp::GcHandle::Strong(handler);
    if (!held)
      return {};
    if (!il2cpp_class_is_assignable_from(unity::Behaviour::Class::Raw(), held.Get()->klass))
      return {true, true}; // Unity also accepts non-Behaviour IScrollHandlers.
    const auto active = reinterpret_cast<unity::Behaviour*>(held.Get())->TryCall<"get_isActiveAndEnabled", bool>();
    if (!active)
      return {};
    if (active.value)
      return {true, true};
  }
  return {true, false};
}
} // namespace scroll_input
