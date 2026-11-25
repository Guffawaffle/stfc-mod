#pragma once

#include "server.h"
#include "settings.h"

namespace spec_service
{
struct SpecManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.Managers", "SpecManager">;
};

struct CachedSpecService {
  SpecService* service = nullptr;
};

// Observe a populated CachedService without creating a manager or a server session.
template <typename Manager, il2cpp::Name Field> SpecService* ExistingCachedService()
{
  const auto existing = settings_binding::ExistingSingleton<Manager>();
  auto       owner    = il2cpp::GcHandle::Strong(existing ? existing.value : nullptr);
  if (!owner)
    return nullptr;
  auto*             manager   = owner.template Get<Manager>();
  static auto*      field     = il2cpp::detail::ResolveField(Manager::Class::Raw(), Field.View(),
                                                             il2cpp::detail::SlotOf<CachedSpecService>(), false);
  static const bool qualified = [] {
    auto* type = field && field->type && !field->type->byref ? il2cpp_class_from_type(field->type) : nullptr;
    if (!type || !il2cpp_class_is_valuetype(type) || il2cpp::ClassName(type) != "Digit.Client.UI.CachedService`1")
      return false;
    uint32_t alignment = 0;
    if (il2cpp_class_value_size(type, &alignment) != sizeof(CachedSpecService))
      return false;
    auto* cached = il2cpp::detail::ResolveField(type, "_cachedService", il2cpp::detail::SlotOf<SpecService*>(), false);
    return cached && cached->offset == sizeof(Il2CppObject);
  }();
  if (!qualified)
    return nullptr;
  auto* service = il2cpp::detail::ReadField<CachedSpecService>(field, manager).service;
  return service && reinterpret_cast<il2cpp::Object*>(service)->As<SpecService>() ? service : nullptr;
}

inline SpecService* ExistingSpecService()
{ return ExistingCachedService<SpecManager, "_specServiceCached">(); }
} // namespace spec_service
