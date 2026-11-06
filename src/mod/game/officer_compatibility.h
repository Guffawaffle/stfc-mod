#pragma once

// Scoped officer-filter access. Static catalog binding reads an already populated
// CachedService value; it never creates a singleton or requests account inventory.
#include "server.h"
#include "settings.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace officer_compatibility_binding
{
struct Officer : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Officer">;
};
struct InventoryItem : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "InventoryItem">;
};
struct FilterFunction : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "FilterFunction">;
};
struct ToggleableFilters : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "ToggleableFilters">;
};
struct SortComparer : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Sorting", "SortComparer">;
};
struct Context : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Officers", "OfficerFilterContext">;

  SettingsContainerContext* AbilityOptions()
  { return Get<"AbilityOptionContext", SettingsContainerContext*>(); }
  static il2cpp::Method AddToggleMethod()
  {
    return Class::Method<void(SettingsContainerContext*, Il2CppString*, il2cpp::Object*, il2cpp::Object*)>("AddToggle");
  }
  static Il2CppObject* ToggleDelegate(int32_t argument, Context* owner, const MethodInfo* callback)
  {
    const auto add = AddToggleMethod();
    return add && (argument == 2 || argument == 3)
               ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(add.Info()->parameters[argument]), owner,
                                           il2cpp::Method(callback))
               : nullptr;
  }
  bool RefreshTemporaryFilters()
  {
    // CreateStandardFilter reads committed OfficerManager flags. These controls
    // preview the current temporary native flags, just like native toggles do.
    if (!Field<"SortComparer", SortComparer*>()
        || !Field<"_updateFiltersDelegate", settings_binding::ReferenceDelegate*>())
      return false;
    auto availability = il2cpp::GcHandle::Strong(Field<"_availabilityFilters", ToggleableFilters*>());
    auto abilities    = il2cpp::GcHandle::Strong(Field<"_abilityFilters", ToggleableFilters*>());
    if (!availability || !abilities)
      return false;
    const auto a      = availability.Get<ToggleableFilters>()->TryCall<"GetEnabledFilters", il2cpp::ArrayObject*>();
    auto       a_root = il2cpp::GcHandle::Strong(a ? a.value : nullptr);
    const auto b      = abilities.Get<ToggleableFilters>()->TryCall<"GetEnabledFilters", il2cpp::ArrayObject*>();
    auto       b_root = il2cpp::GcHandle::Strong(b ? b.value : nullptr);
    if (!a || !b || (a.value && !a_root) || (b.value && !b_root)
        || !il2cpp::TryReferenceArray<settings_binding::ReferenceDelegate*>(a_root.Get())
        || !il2cpp::TryReferenceArray<settings_binding::ReferenceDelegate*>(b_root.Get()))
      return false;
    return TryCall<"UpdateSortComparerFilters", void(il2cpp::ArrayObject*, il2cpp::ArrayObject*)>(
               a_root.Get<il2cpp::ArrayObject>(), b_root.Get<il2cpp::ArrayObject>())
        .succeeded;
  }
};

inline std::optional<std::int64_t> OfficerId(il2cpp::Object* argument)
{
  if (!argument)
    return std::nullopt;
  auto root = il2cpp::GcHandle::Strong(argument);
  if (!root)
    return std::nullopt;
  auto*            officer = root.Get<il2cpp::Object>()->As<Officer>();
  il2cpp::GcHandle officer_root;
  if (!officer) {
    auto* item = root.Get<il2cpp::Object>()->As<InventoryItem>();
    if (!item)
      return std::nullopt;
    const auto type = item->TryCall<"get_Type", int32_t>();
    if (!type || (type.value != 9 && type.value != 11))
      return std::nullopt;
    const auto value = item->TryCall<"get_Officer", Officer*>();
    officer_root     = il2cpp::GcHandle::Strong(value ? value.value : nullptr);
    officer          = officer_root.Get<Officer>();
    if (!value || !officer)
      return std::nullopt;
  }
  const auto id = officer->TryCall<"get_Id", std::int64_t>();
  return id && id.value > 0 ? std::optional<std::int64_t>(id.value) : std::nullopt;
}

inline std::optional<std::vector<il2cpp::GcHandle>> Children(SettingsContainerContext* parent)
{
  if (!parent)
    return std::nullopt;
  auto       root  = il2cpp::GcHandle::Strong(parent->Children());
  auto*      list  = root.Get<il2cpp::List<SettingsOptionContext*>>();
  const auto count = list ? list->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return std::nullopt;
  std::vector<il2cpp::GcHandle> result;
  result.reserve(static_cast<size_t>(count.value));
  for (int32_t i = 0; i < count.value; ++i) {
    const auto child = list->TryCall<"get_Item", SettingsOptionContext*(int32_t)>(i);
    if (!child)
      return std::nullopt;
    auto handle = il2cpp::GcHandle::Strong(child.value);
    if (child.value && !handle)
      return std::nullopt;
    result.push_back(std::move(handle));
  }
  return result;
}
inline std::optional<bool> Contains(SettingsContainerContext* parent, Il2CppObject* row)
{
  auto children = Children(parent);
  if (!children)
    return std::nullopt;
  return std::ranges::any_of(*children, [&](const auto& child) { return child.Get() == row; });
}
inline Context* CallbackOwner(Il2CppObject* callback)
{
  if (!callback || !reinterpret_cast<il2cpp::Object*>(callback)->As<settings_binding::ReferenceDelegate>())
    return nullptr;
  auto* target = reinterpret_cast<Il2CppDelegate*>(callback)->target;
  return target ? reinterpret_cast<il2cpp::Object*>(target)->As<Context>() : nullptr;
}
inline bool ConfigureRow(ToggleOptionContext* row, Il2CppObject* query, std::string_view label)
{
  const auto set_query =
      row->TryCall<"set_QueryOptionState", void(il2cpp::Object*)>(reinterpret_cast<il2cpp::Object*>(query));
  if (!set_query || row->Query() != query)
    return false;
  auto  context = il2cpp::GcHandle::Strong(row->Label());
  auto* text    = context.Get<LocaleTextContext>();
  if (!text || !il2cpp::Equals(text->Get<"Identifier", Il2CppString*>(), label))
    return false;
  // Empty category is the native localizer's explicit literal-identifier path.
  const auto set_category = text->TryCall<"set_Category", void(Il2CppString*)>(il2cpp::NewString(""));
  return set_category && il2cpp::Equals(text->Get<"Category", Il2CppString*>(), "");
}

struct SpecManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.Managers", "SpecManager">;
};
struct OfficerManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Officers", "OfficerManager">;
};
struct CachedSpecService {
  SpecService* service = nullptr;
};
template <typename Manager, il2cpp::Name Field> SpecService* ExistingCachedService()
{
  const auto existing = settings_binding::ExistingSingleton<Manager>();
  auto       owner    = il2cpp::GcHandle::Strong(existing ? existing.value : nullptr);
  if (!owner)
    return nullptr;
  auto*             manager   = owner.Get<Manager>();
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
{
  if (auto* service = ExistingCachedService<SpecManager, "_specServiceCached">())
    return service;
  return ExistingCachedService<OfficerManager, "_specServiceCache">();
}
inline bool Available()
{
  return Context::AddToggleMethod() && Context::Class::HasProperty<"AbilityOptionContext", SettingsContainerContext*>()
         && Context::Class::HasField<"SortComparer", SortComparer*>()
         && Context::Class::HasField<"_updateFiltersDelegate", settings_binding::ReferenceDelegate*>()
         && Context::Class::HasField<"_availabilityFilters", ToggleableFilters*>()
         && Context::Class::HasField<"_abilityFilters", ToggleableFilters*>()
         && Context::Class::Method<void(il2cpp::ArrayObject*, il2cpp::ArrayObject*)>("UpdateSortComparerFilters")
         && ToggleableFilters::Class::Method<il2cpp::ArrayObject*()>("GetEnabledFilters")
         && SettingsContainerContext::Class::Method<void(SettingsOptionContext*)>("RemoveChild")
         && LocaleTextContext::Class::Method<void(Il2CppString*)>("set_Category");
}
} // namespace officer_compatibility_binding
