#pragma once

// Native scene primitives used to extend a camera's ambient backdrop.
#include "ui.h"

#include <span>

namespace unity
{
struct Mesh : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Mesh">;
};
struct MeshFilter : Component {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "MeshFilter">;
};
struct MeshRenderer : Renderer {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "MeshRenderer">;
};
struct Texture : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Texture">;
};

template <typename T, class Element> il2cpp::CallResult<il2cpp::Array<T>*> CopyValueArray(std::span<const T> values)
{
  static_assert(std::is_trivially_copyable_v<T>);
  auto*    cls       = Element::Raw();
  uint32_t alignment = 0;
  if (!cls || il2cpp_class_value_size(cls, &alignment) != sizeof(T))
    return {};
  auto* array = il2cpp::Array<T>::New(cls, values.size());
  if (!array)
    return {};
  std::memcpy(array->data(), values.data(), values.size_bytes());
  return {true, array};
}

// Bounds is returned through the runtime invoker. Reject a same-size unrelated
// aggregate rather than assuming the generated struct-return ABI or layout.
inline il2cpp::CallResult<Bounds> TryBounds(Renderer* renderer)
{
  using Bound               = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Bounds">;
  static const auto* method = [] {
    const auto* candidate = il2cpp::detail::MethodCall<Bounds()>::Resolve(Renderer::Class::Raw(), "get_bounds", false);
    return candidate && candidate->invoker_method
                   && il2cpp::detail::ValueClassMatches(candidate->return_type, Bound::Raw(), sizeof(Bounds))
               ? candidate
               : nullptr;
  }();
  if (!renderer || !method)
    return {};
  const auto observed = il2cpp::detail::TryInvokeBoxed(method, renderer, nullptr);
  auto       box      = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  return box && box.Get()->klass
                 && il2cpp::detail::ValueClassMatches(&box.Get()->klass->byval_arg, Bound::Raw(), sizeof(Bounds))
             ? il2cpp::CallResult<Bounds>{true, il2cpp::detail::FromBoxed<Bounds>(box.Get())}
             : il2cpp::CallResult<Bounds>{};
}
} // namespace unity
