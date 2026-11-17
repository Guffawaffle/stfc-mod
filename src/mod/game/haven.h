#pragma once

// Haven (the planetary base): help history and exact camera-field observations.
// The generic typed library permits related reference declarations and same-size
// value layouts. Haven's measured camera geometry requires the frozen declared
// classes too; a widened Object field or foreign vector is not that contract.

#include "camera.h"
#include <il2cpp/il2cpp.h>

namespace haven_binding
{
template <class T> bool DeclaredType(const Il2CppType* type)
{
  if constexpr (std::is_pointer_v<T>) {
    using Value = std::remove_cv_t<std::remove_pointer_t<T>>;
    static_assert(il2cpp::detail::Bound<Value>, "Haven reference fields need a named managed class");
    auto* expected = Value::Class::Raw();
    return type && !type->byref && expected && il2cpp_class_from_type(type) == expected;
  } else if constexpr (std::is_same_v<T, unity::Vector3>) {
    return il2cpp::detail::ValueClassMatches(type, unity::Vector3Class(), sizeof(T));
  } else if constexpr (std::is_same_v<T, unity::Quaternion>) {
    using Rotation = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Quaternion">;
    return il2cpp::detail::ValueClassMatches(type, Rotation::Raw(), sizeof(T));
  } else {
    // ResolveField already establishes the exact Boolean/Single scalar kind.
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, float>);
    return true;
  }
}

template <il2cpp::Name N, class T, class B> il2cpp::CallResult<T> Field(B* object)
{
  static auto* field = [] {
    auto* found = il2cpp::detail::ResolveField(B::Class::Raw(), N.View(), il2cpp::detail::SlotOf<T>(), false);
    if (found && !DeclaredType<T>(found->type)) {
      il2cpp::Report("[IL2CPP] Haven field " + std::string(N.View()) + " has an incompatible declared type");
      return static_cast<FieldInfo*>(nullptr);
    }
    return found;
  }();
  return object && field ? il2cpp::CallResult<T>{true, il2cpp::detail::ReadField<T>(field, object)}
                         : il2cpp::CallResult<T>{};
}
} // namespace haven_binding

struct PlanetaryHelpHistoryContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlanetaryBase", "PlanetaryHelpHistoryContext">;
};

// The substantive post-refresh endpoint is shared by native mesh arrival and
// material-load completion. Its receiver is used only by the installed detour.
using HavenAssetLodStreaming = il2cpp::Class<"Assembly-CSharp", "", "AssetLODStreaming">;
