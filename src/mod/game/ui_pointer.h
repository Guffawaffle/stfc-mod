#pragma once

// The game's cached, sorted UI raycast. Both mouse actions and wheel ownership must use the first hit,
// rather than an occluded UI object below it. Observation only; native pointer dispatch remains untouched.
#include <il2cpp/il2cpp.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <cmath>

namespace ui_pointer
{
namespace detail
{
  using Raycasts = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "RaycastExtensions">;
  using Raycast  = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "RaycastResult">;
  struct Api {
    il2cpp::Method    raycast;
    const MethodInfo *count = nullptr, *item = nullptr;
    FieldInfo*        target = nullptr;
    bool              ready  = false;
  };
  inline const Api& Native()
  {
    static const Api api = [] {
      Api result;
      result.raycast    = Raycasts::StaticMethod<Il2CppObject*(unity::EventSystem*, unity::Vector2)>("CachedRaycast");
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
      auto*       field       = raycast ? il2cpp_class_get_field_from_name(raycast, "m_GameObject") : nullptr;
      const auto  size        = raycast ? il2cpp_class_instance_size(raycast) : 0;
      if (field && field->type && !field->type->byref && field->type->type == IL2CPP_TYPE_CLASS
          && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC)
          && field->offset >= static_cast<int32_t>(sizeof(Il2CppObject)) && size >= sizeof(unity::GameObject*)
          && static_cast<uint32_t>(field->offset) <= size - sizeof(unity::GameObject*)
          && il2cpp_class_from_type(field->type) == unity::GameObject::Class::Raw())
        result.target = field;
      result.ready = result.raycast && count_valid && item_valid && result.target && unity::Input::PointerAvailable()
                     && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current");
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

// Successful empty custody means no hit. Failed observation remains distinct, never a fabricated empty scene.
inline il2cpp::CallResult<il2cpp::GcHandle> TopObject()
{
  const auto& api = detail::Native();
  if (!api.ready)
    return {};
  const auto mouse   = unity::Input::TryMousePosition();
  const auto current = unity::EventSystem::Class::TryCall<"get_current", unity::EventSystem*>();
  if (!mouse || !current || !std::isfinite(mouse.value.x) || !std::isfinite(mouse.value.y))
    return {};
  if (!current.value)
    return {true, {}};
  auto system = il2cpp::GcHandle::Strong(current.value);
  if (!system)
    return {};
  unity::Vector2 position{mouse.value.x, mouse.value.y};
  void*          args[]{system.Get(), &position};
  const auto     observed = il2cpp::detail::TryInvokeBoxed(api.raycast.Info(), nullptr, args);
  if (!observed)
    return {};
  if (!observed.value)
    return {true, {}};
  auto hits = il2cpp::GcHandle::Strong(observed.value);
  if (!hits)
    return {};
  const auto count = detail::Count(api, hits.Get());
  if (!count)
    return {};
  if (count.value == 0)
    return {true, {}};
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
  if (target && !object)
    return {};
  return {true, std::move(object)};
}
} // namespace ui_pointer
