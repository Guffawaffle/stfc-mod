#pragma once

// Read the game's current player-wide automatic buffs. No service creation,
// scene discovery, network requests, or combat-history reconstruction.
#include "fleet.h"
#include "settings.h"

#include <il2cpp/il2cpp.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace hyperthermic_decay_binding
{
struct BuffData;
using BuffList = il2cpp::List<BuffData*>;

using DateTimeClass = il2cpp::Class<"mscorlib", "System", "DateTime">;
struct NativeDateTime {
  uint64_t date_data = 0;

  int64_t Ticks() const
  { return static_cast<int64_t>(date_data & uint64_t{0x3fffffffffffffff}); }
};
static_assert(sizeof(NativeDateTime) == 8);

struct DateTimeUtils {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Networking.Core", "DateTimeUtils">;
};

// Native HasExpired uses this server-adjusted clock, rather than the PC clock.
inline il2cpp::CallResult<int64_t> TryServerTicks()
{
  static const auto* getter = [] {
    const auto* method =
        il2cpp::detail::MethodCall<NativeDateTime()>::Resolve(DateTimeUtils::Class::Raw(), "get_UtcNow", true);
    return method
                   && il2cpp::detail::ValueClassMatches(method->return_type, DateTimeClass::Raw(),
                                                        sizeof(NativeDateTime))
               ? method
               : nullptr;
  }();
  if (!getter)
    return {};
  const auto value = il2cpp::detail::TryInvoke<NativeDateTime>(getter, nullptr);
  return value ? il2cpp::CallResult<int64_t>{true, value.value.Ticks()} : il2cpp::CallResult<int64_t>{};
}

struct BuffData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BuffData">;

  il2cpp::CallResult<int64_t> TryId()
  { return TryCall<"get_Id", int64_t>(); }
  il2cpp::CallResult<int32_t> TryRank()
  { return TryCall<"get_Rank", int32_t>(); }
  il2cpp::CallResult<double> TryValue()
  { return TryCall<"get_Value", double>(); }
  il2cpp::CallResult<bool> TryHasExpired()
  { return TryCall<"get_HasExpired", bool>(); }

  il2cpp::CallResult<std::optional<int64_t>> TryExpiryTicks()
  {
    using Expiry       = il2cpp::Nullable<NativeDateTime>;
    static auto* field = [] {
      auto* candidate = il2cpp::detail::ResolveField(Class::Raw(), "<ExpiresAt>k__BackingField",
                                                     il2cpp::detail::SlotOf<Expiry>(), false);
      return candidate
                     && il2cpp::detail::NullableLayoutMatches(candidate->type, DateTimeClass::Raw(), sizeof(Expiry),
                                                              alignof(Expiry), offsetof(Expiry, has_value),
                                                              offsetof(Expiry, value))
                 ? candidate
                 : nullptr;
    }();
    if (!field)
      return {};
    // Client 275's generated getter only reads this backing field. Qualify the
    // exact Nullable<DateTime> storage before copying it; no box or aggregate
    // return ABI is needed. Metadata drift remains an unavailable observation.
    const auto expiry = il2cpp::detail::ReadField<Expiry>(field, this);
    if (!expiry.has_value)
      return {true, std::nullopt};
    return {true, expiry.value.Ticks()};
  }
};

// The game's complete Manage Ship buff widget. Context binding/release must use
// the virtual object overload: Widget<BuffData> also exposes a typed overload.
struct AutoBuffWidget : Widget<BuffData> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ShipManagement", "ShipAutoBuffWidget">;

  unity::GameObject* gameObject()
  {
    auto* component = As<unity::Component>();
    return component ? component->gameObject() : nullptr;
  }
  unity::RectTransform* Rect()
  {
    auto* component = As<unity::Component>();
    auto* transform = component ? component->transform() : nullptr;
    return transform ? transform->As<unity::RectTransform>() : nullptr;
  }
  il2cpp::CallResult<void> TryBind(BuffData* data)
  {
    auto* widget = As<UntypedWidget>();
    if (!widget || !data)
      return {};
    const auto bound = widget->TryBindContext(nullptr, data);
    return bound && Context() == data ? il2cpp::CallResult<void>{true} : il2cpp::CallResult<void>{};
  }
  il2cpp::CallResult<void> TryRelease()
  {
    auto* widget = As<UntypedWidget>();
    return widget ? widget->TryReleaseContext() : il2cpp::CallResult<void>{};
  }
};

struct AutoBuffList : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "DynamicListContainer">;
};

struct AutoBuffTemplate {
  unity::GameObject* object = nullptr;
  // Zero means unavailable geometry, not an absent prefab. The consumer can
  // choose a layout fallback without assigning a gameplay meaning to it.
  float width = 0;
};

struct SectionDirector : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Sections", "SectionDirectorBase">;
};
struct ShipDetailsDirector : SectionDirector {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementDetailsSectionDirector">;
};
struct SectionRegistry : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Sections", "SectionDirectorMemoryManager">;
};
struct ScreenLoader : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "GenericLoadAndShowUI">;
};
struct ScreenResource : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Engine.AssetBundle.Runtime", "Digit.Client.AssetBundles", "AssetBundleResource">;
};
struct SectionDictionary : il2cpp::Object {
};

inline bool SectionDictionaryClass(const il2cpp::Object& object)
{
  return il2cpp::detail::Lookup<SectionDictionary>(object, [](Il2CppClass* klass) {
    using IdClass = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Sections", "SectionID">;
    if (!klass || il2cpp_class_is_valuetype(klass)
        || std::strcmp(il2cpp_class_get_namespace(klass), "System.Collections.Generic")
        || std::strcmp(il2cpp_class_get_name(klass), "Dictionary`2"))
      return false;
    const auto* contains = il2cpp::detail::MethodCall<bool(SectionID)>::Resolve(klass, "ContainsKey", false);
    const auto* item     = il2cpp::detail::MethodCall<SectionDirector*(SectionID)>::Resolve(klass, "get_Item", false);
    return contains && item && il2cpp::detail::ExactScalarMatches(contains->return_type, IL2CPP_TYPE_BOOLEAN)
           && !contains->parameters[0]->byref && !item->parameters[0]->byref
           && il2cpp_class_from_type(contains->parameters[0]) == IdClass::Raw()
           && il2cpp_class_from_type(item->parameters[0]) == IdClass::Raw()
           && il2cpp_class_from_type(item->return_type) == SectionDirector::Class::Raw();
  });
}

// Reads the exact Manage Ship section's already-loaded prefab, including its
// inactive components. Never opens a screen, loads an asset, creates a director,
// scans scene objects, or refreshes a native list. A cold/unloaded section is
// unavailable, not proof that its prefab or native buff widget is absent.
// The returned prefab is borrowed. Root it before retaining or cloning it.
inline il2cpp::CallResult<AutoBuffTemplate> TryAutoBuffTemplate()
{
  static auto* instance = [] {
    auto* klass = SectionRegistry::Class::Raw();
    auto* field = klass ? il2cpp_class_get_field_from_name(klass, "_instance") : nullptr;
    return field && settings_binding::Reference(field->type) && (field->type->attrs & FIELD_ATTRIBUTE_STATIC)
                   && il2cpp_class_from_type(field->type) == klass
               ? field
               : nullptr;
  }();
  if (!instance)
    return {};
  // get_Instance constructs the registry when absent. Read the existing static
  // reference without calling that allocating Instance getter.
  SectionRegistry* existing = nullptr;
  il2cpp_field_static_get_value(instance, &existing);
  auto  registry_root = il2cpp::GcHandle::Strong(existing);
  auto* registry      = registry_root.Get<SectionRegistry>();
  if (!registry)
    return {};
  const auto sections        = registry->TryCall<"get_SectionIDToBase", settings_binding::ReferenceObject*>();
  auto       dictionary_root = il2cpp::GcHandle::Strong(sections ? sections.value : nullptr);
  auto*      dictionary      = dictionary_root.Get<SectionDictionary>();
  if (!dictionary || !SectionDictionaryClass(*dictionary))
    return {};
  const auto contains = dictionary->TryCall<"ContainsKey", bool(SectionID)>(SectionID::ShipManagement_Details);
  if (!contains || !contains.value)
    return {};
  const auto section = dictionary->TryCall<"get_Item", SectionDirector*(SectionID)>(SectionID::ShipManagement_Details);
  auto       director_root = il2cpp::GcHandle::Strong(section ? section.value : nullptr);
  auto*      base_director = director_root.Get<SectionDirector>();
  auto*      director      = base_director ? base_director->As<ShipDetailsDirector>() : nullptr;
  if (!unity::Alive(director))
    return {};
  auto  loader_root = il2cpp::GcHandle::Strong(director->Field<"_loadAndShow", ScreenLoader*>());
  auto* loader      = loader_root.Get<ScreenLoader>();
  if (!unity::Alive(loader))
    return {};
  const auto resource         = loader->TryCall<"get_Resource", ScreenResource*>();
  auto       resource_root    = il2cpp::GcHandle::Strong(resource ? resource.value : nullptr);
  auto*      current_resource = resource_root.Get<ScreenResource>();
  if (!current_resource)
    return {};
  const auto loaded        = current_resource->TryCall<"get_LoadedResource", unity::GameObject*>();
  auto       loaded_root   = il2cpp::GcHandle::Strong(loaded ? loaded.value : nullptr);
  auto*      loaded_object = loaded_root.Get<il2cpp::Object>();
  auto*      loaded_prefab = loaded_object ? loaded_object->As<unity::GameObject>() : nullptr;
  if (!unity::Alive(loaded_prefab))
    return {};
  auto* owner_type = ShipManagementViewController::Class::TypeObject();
  if (!owner_type)
    return {};
  const auto controller =
      loaded_prefab->TryCall<"GetComponentInChildren", ShipManagementViewController*>(owner_type, true);
  auto  owner_root = il2cpp::GcHandle::Strong(controller ? controller.value : nullptr);
  auto* owner      = owner_root.Get<ShipManagementViewController>();
  if (!unity::Alive(owner))
    return {};
  auto  list_root = il2cpp::GcHandle::Strong(owner->Field<"_autoBuffsList", AutoBuffList*>());
  auto* list      = list_root.Get<AutoBuffList>();
  if (!unity::Alive(list))
    return {};
  const auto acquired = list->TryCall<"get_Template", unity::GameObject*>();
  if (!acquired)
    return {};
  auto       prefab_root = il2cpp::GcHandle::Strong(acquired.value);
  auto*      prefab      = prefab_root.Get<unity::GameObject>();
  const auto width       = prefab ? list->TryCall<"get_TemplateWidth", float>() : il2cpp::CallResult<float>{};
  return {true, {prefab, width ? width.value : 0.0f}};
}

struct Service : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService">;
};

inline bool BuffListClass(const il2cpp::Object& object)
{
  return il2cpp::detail::Lookup<BuffList>(object, [](Il2CppClass* klass) {
    if (!klass || il2cpp_class_is_valuetype(klass)
        || std::strcmp(il2cpp_class_get_namespace(klass), "System.Collections.Generic")
        || std::strcmp(il2cpp_class_get_name(klass), "List`1"))
      return false;
    auto* field = il2cpp::detail::ResolveField(klass, "_items", il2cpp::detail::SlotOf<il2cpp::ArrayObject*>(), false);
    const auto* type    = field ? field->type : nullptr;
    const auto* element = type && !type->byref && type->type == IL2CPP_TYPE_SZARRAY ? type->data.type : nullptr;
    return element && !element->byref && il2cpp_class_from_type(element) == BuffData::Class::Raw();
  });
}

struct Manager : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Buffs", "BuffsManager">;

  il2cpp::CallResult<Service*> TryExistingService()
  {
    struct CachedService {
      Service* service = nullptr;
    };
    static auto* field =
        il2cpp::detail::ResolveField(Class::Raw(), "_cachedService", il2cpp::detail::SlotOf<CachedService>(), false);
    static const bool qualified = [] {
      auto* type = field && field->type && !field->type->byref ? il2cpp_class_from_type(field->type) : nullptr;
      if (!type || !il2cpp_class_is_valuetype(type) || il2cpp::ClassName(type) != "Digit.Client.UI.CachedService`1")
        return false;
      uint32_t alignment = 0;
      if (il2cpp_class_value_size(type, &alignment) != sizeof(CachedService))
        return false;
      auto* cached = il2cpp::detail::ResolveField(type, "_cachedService", il2cpp::detail::SlotOf<Service*>(), false);
      return cached && cached->offset == sizeof(Il2CppObject);
    }();
    if (!qualified)
      return {};
    auto* service = il2cpp::detail::ReadField<CachedService>(field, this).service;
    return !service || service->As<Service>() ? il2cpp::CallResult<Service*>{true, service}
                                              : il2cpp::CallResult<Service*>{};
  }

  // The returned list is borrowed. Root it while traversing; any retained buff
  // needs its own root, held through the native widget's unbind lifecycle.
  // A missing baseline is unavailable, not an observed empty buff set.
  il2cpp::CallResult<BuffList*> TryActiveBuffs()
  {
    // The native facade looks up an existing registry service and warms its
    // own cache. It never constructs services or requests server data.
    const auto value = TryCall<"GetAllActiveUserBuffs", settings_binding::ReferenceObject*>();
    if (!value || !value.value || !BuffListClass(*value.value))
      return {};
    auto list_root = il2cpp::GcHandle::Strong(value.value);
    if (!list_root)
      return {};
    const auto service = TryExistingService();
    auto       root    = il2cpp::GcHandle::Strong(service ? service.value : nullptr);
    if (!root)
      return {};
    auto*      current = root.Get<Service>();
    const auto ready   = current->TryCall<"HasData", bool>();
    if (!ready || !ready.value)
      return {};
    return {true, reinterpret_cast<BuffList*>(value.value)};
  }
};

inline il2cpp::CallResult<Manager*> TryCurrentManager()
{ return settings_binding::ExistingSingleton<Manager>(); }
} // namespace hyperthermic_decay_binding
