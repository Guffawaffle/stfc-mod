#pragma once

// Game UI hit testing for the camera wheel guard. Observation only: the native
// scroll handler still receives its input, and owns the meaning of that input.
#include "game/navigation.h"

#include <il2cpp/il2cpp.h>
#include <unity/input.h>
#include <unity/ui.h>

namespace scroll_input
{
using WheelRecognizer =
    il2cpp::Class<"Assembly-CSharp", "Digit.Client.CameraController", "MouseScrollAsPinchRecognizer">;

namespace detail
{
  using Raycasts = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "RaycastExtensions">;
  using Raycast  = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "RaycastResult">;
  using Handler  = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "IScrollHandler">;

  struct Api {
    il2cpp::Method    raycast, parents;
    const MethodInfo *count = nullptr, *item = nullptr;
    FieldInfo *       target = nullptr, *pinch = nullptr;
    bool              ready = false;
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
      result.raycast    = Raycasts::StaticMethod<Il2CppObject*(unity::EventSystem*, unity::Vector2)>("CachedRaycast");
      result.parents    = unity::GameObject::Class::Method<il2cpp::ArrayObject*(
          Il2CppReflectionType*, bool, bool, bool, bool, Il2CppObject*)>("GetComponentsInternal");
      auto* raycast     = Raycast::Raw();
      auto* list        = result.raycast ? il2cpp_class_from_type(result.raycast.Info()->return_type) : nullptr;
      result.count      = list ? il2cpp_class_get_method_from_name(list, "get_Count", 0) : nullptr;
      result.item       = list ? il2cpp_class_get_method_from_name(list, "get_Item", 1) : nullptr;
      const auto* count = result.count;
      const auto* item  = result.item;
      const bool  count_valid = count && count->invoker_method && !(count->flags & METHOD_ATTRIBUTE_STATIC)
                                && count->parameters_count == 0
                                && il2cpp::detail::ExactScalarMatches(count->return_type, IL2CPP_TYPE_I4);
      const bool  item_valid  = item && item->invoker_method && !(item->flags & METHOD_ATTRIBUTE_STATIC)
                                && item->parameters_count == 1 && item->parameters && item->parameters[0]
                                && il2cpp::detail::ExactScalarMatches(item->parameters[0], IL2CPP_TYPE_I4)
                                && item->return_type && !item->return_type->byref
                                && item->return_type->type == IL2CPP_TYPE_VALUETYPE && raycast
                                && il2cpp_class_from_type(item->return_type) == raycast;
      result.target           = Field<unity::GameObject*>(raycast, "m_GameObject", IL2CPP_TYPE_CLASS);
      result.pinch            = Field<bool>(NavigationZoom::Class::Raw(), "_pinchStarted", IL2CPP_TYPE_BOOLEAN);
      auto* delta             = Field<float>(NavigationZoom::Class::Raw(), "_zoomDelta", IL2CPP_TYPE_R4);
      auto* last              = Field<float>(NavigationZoom::Class::Raw(), "_lastZoomDelta", IL2CPP_TYPE_R4);
      result.ready = result.raycast && result.parents && count_valid && item_valid && result.target && result.pinch
                     && delta && last && Handler::Raw() && unity::Input::PointerAvailable()
                     && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current")
                     && unity::Behaviour::Class::Method<bool()>("get_isActiveAndEnabled")
                     && il2cpp_class_from_type(result.target->type) == unity::GameObject::Class::Raw();
      return result;
    }();
    return api;
  }

  inline il2cpp::CallResult<int32_t> Count(const Api& api, Il2CppObject* list)
  {
    const auto observed = il2cpp::detail::TryInvokeBoxed(api.count, list, nullptr);
    if (!observed || !observed.value
        || !il2cpp::detail::ExactScalarMatches(il2cpp_class_get_type(observed.value->klass), IL2CPP_TYPE_I4))
      return {};
    auto  root  = il2cpp::GcHandle::Strong(observed.value);
    auto* value = root ? il2cpp_object_unbox(root.Get()) : nullptr;
    if (!value)
      return {};
    const auto count = *static_cast<int32_t*>(value);
    return count >= 0 ? il2cpp::CallResult<int32_t>{true, count} : il2cpp::CallResult<int32_t>{};
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
  const auto mouse   = unity::Input::TryMousePosition();
  const auto current = unity::EventSystem::Class::TryCall<"get_current", unity::EventSystem*>();
  if (!mouse || !current)
    return {};
  if (!current.value)
    return {true, false};
  auto system = il2cpp::GcHandle::Strong(current.value);
  if (!system)
    return {};
  const unity::Vector2 position{mouse.value.x, mouse.value.y};
  void*                raycast_args[]{system.Get(), const_cast<unity::Vector2*>(&position)};
  const auto           observed = il2cpp::detail::TryInvokeBoxed(api.raycast.Info(), nullptr, raycast_args);
  if (!observed)
    return {};
  if (!observed.value)
    return {true, false};
  auto hits = il2cpp::GcHandle::Strong(observed.value);
  if (!hits)
    return {};
  const auto count = detail::Count(api, hits.Get());
  if (!count)
    return {};
  if (count.value == 0)
    return {true, false};
  // Unity dispatches to the first sorted hit. Never let an occluded scroller
  // underneath another window claim the wheel.
  int32_t    first = 0;
  void*      item_args[]{&first};
  const auto item = il2cpp::detail::TryInvokeBoxed(api.item, hits.Get(), item_args);
  if (!item || !item.value || item.value->klass != detail::Raycast::Raw())
    return {};
  auto hit = il2cpp::GcHandle::Strong(item.value);
  if (!hit)
    return {};
  auto* target = il2cpp::detail::ReadField<unity::GameObject*>(api.target, hit.Get());
  auto  object = il2cpp::GcHandle::Strong(target);
  if (!object)
    return target ? il2cpp::CallResult<bool>{} : il2cpp::CallResult<bool>{true, false};
  auto type = il2cpp::GcHandle::Strong(
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
