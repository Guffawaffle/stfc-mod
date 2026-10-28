#pragma once

// Alliance Games store bindings. The study's native research path, with checked
// metadata and operation-local custody; no diagnostic dispatcher or shop model.
#include "inventory.h"

#include <array>
#include <vector>

struct StoreTooltipTrigger : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Tooltip", "TooltipTrigger">;
};
struct StoreTooltipAction : StoreTooltipTrigger {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Tooltip", "TooltipActionTrigger">;
};
struct StoreTooltipLoader : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Tooltip", "TooltipLoadAndShow">;
};
struct StoreActionLocale : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Actions", "ActionLocaleSetting">;
};
struct StoreActionLocales : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Actions", "ActionLocaleSettingsList">;
};
struct StoreInventoryTooltip : Widget<InventoryItem> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryTooltipTriggerWidget">;
};
struct AllianceStoreContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Alliances.Games", "AllianceGamesSectionContext">;
};
struct AllianceStoreCurrencies : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "GenericHudResourcesData">;
};
struct AllianceStoreScroller : il2cpp::Object {
  using Class =
      il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Alliances.Games", "AllianceGamesStoreScrollerViewController">;
};
struct StoreSmartScroller : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase">;
};
struct StoreResearchManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Research", "ResearchManager">;
};

namespace alliance_store_game
{
enum class ResearchTree : int32_t { Standard = 0 };

// Every nonnull acquired object must have successful custody before a later
// allocating native call. Roots expire with this one reconciliation operation.
struct Roots {
  std::vector<il2cpp::GcHandle> values;
  bool                          healthy = true;
  template <typename T> T*      Keep(T* value)
  {
    if (!value)
      return nullptr;
    auto root = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(value));
    if (!root || root.Get() != reinterpret_cast<Il2CppObject*>(value)) {
      healthy = false;
      return nullptr;
    }
    values.push_back(std::move(root));
    return value;
  }
};

template <typename T> struct Field {
  FieldInfo* field = nullptr;
  bool       Resolve(Il2CppClass* owner, std::string_view name)
  {
    field = il2cpp::detail::ResolveField(owner, name, il2cpp::detail::SlotOf<T>(), false);
    return field != nullptr;
  }
  T Read(const Il2CppObject* object) const
  { return il2cpp::detail::ReadField<T>(field, object); }
  bool Write(Il2CppObject* object, T value) const
  {
    if (!field || !object)
      return false;
    il2cpp::detail::WriteField(field, object, value);
    if constexpr (std::is_pointer_v<T> || std::is_scalar_v<T>)
      return Read(object) == value;
    else
      return std::memcmp(&value, reinterpret_cast<const char*>(object) + field->offset, sizeof(T)) == 0;
  }
};

// These three bytes are storage only, never a by-value native-call signature.
struct OverridableBool {
  bool serialized, current, modified;
};
static_assert(sizeof(OverridableBool) == 3);

struct Fields {
  Field<StoreTooltipTrigger*>             trigger;
  Field<StoreTooltipAction*>              action;
  Field<StoreActionLocale*>               search, locale;
  Field<StoreActionLocales*>              settings;
  Field<OverridableBool>                  show_header, show_description;
  Field<LocaleTextContext*>               header, description;
  Field<bool>                             override_locale, interactable;
  Field<int32_t>                          anchor;
  Field<unity::Vector2>                   size, offset;
  Field<unity::Transform*>                target;
  Field<Il2CppString*>                    description_override;
  Field<StoreTooltipLoader*>              loader;
  Field<StoreSmartScroller*>              scroller;
  Field<il2cpp::List<unity::Transform*>*> spawned;
  Field<StoreInventoryTooltip*>           bundle_tooltip;

  bool Resolve()
  {
    auto* widget  = StoreInventoryTooltip::Class::Raw();
    auto* tooltip = StoreTooltipTrigger::Class::Raw();
    bool  ok      = true;
    ok &= trigger.Resolve(widget, "_tooltipTrigger");
    ok &= action.Resolve(widget, "_tooltipActionTrigger");
    ok &= search.Resolve(widget, "_searchActionLocaleSetting");
    ok &= locale.Resolve(widget, "_actionLocaleSetting");
    ok &= settings.Resolve(widget, "_settings");
    ok &= show_header.Resolve(tooltip, "_showHeader");
    ok &= show_description.Resolve(tooltip, "_showDescription");
    // A same-sized unrelated struct must not become a serialized override.
    for (auto* field : {show_header.field, show_description.field})
      ok &= field
            && il2cpp::ClassName(il2cpp_class_from_type(field->type)) == "Digit.Client.Serialization.OverridableBool";
    ok &= header.Resolve(tooltip, "_header");
    ok &= description.Resolve(tooltip, "_description");
    ok &= override_locale.Resolve(tooltip, "_overrideLocale");
    ok &= anchor.Resolve(tooltip, "_anchorOverride");
    ok &= size.Resolve(tooltip, "_sizeOverride");
    ok &= target.Resolve(tooltip, "_positionOverrideTarget");
    ok &= offset.Resolve(tooltip, "_positionOverrideOffset");
    ok &= interactable.Resolve(tooltip, "_isInteractable");
    ok &= description_override.Resolve(tooltip, "<DescriptionOverrideText>k__BackingField");
    ok &= loader.Resolve(tooltip, "_loader");
    ok &= scroller.Resolve(ShopListScrollerViewController::Class::Raw(), "_smartScroller");
    ok &= spawned.Resolve(StoreSmartScroller::Class::Raw(), "_spawnedItems");
    ok &= bundle_tooltip.Resolve(BundleDataWidget::Class::Raw(), "_invTooltip");
    return ok;
  }
};

inline Fields& Members()
{
  static Fields value;
  return value;
}

inline bool IsStore(Il2CppObject* object)
{
  auto* type = AllianceStoreScroller::Class::Raw();
  return object && type && il2cpp_class_is_assignable_from(type, object->klass);
}

inline il2cpp::CallResult<bool> Eligible(InventoryItem* item)
{
  Roots roots;
  if (!roots.Keep(item))
    return {};
  const auto resource = item->TryCall<"get_ResourceSpec", ResourceSpec*>();
  auto*      spec     = resource ? roots.Keep(resource.value) : nullptr;
  const auto id       = spec ? spec->TryCall<"get_Id", int64_t>() : il2cpp::CallResult<int64_t>{};
  if (!id || id.value <= 0)
    return {};
  using HubClass      = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Core", "Hub">;
  const auto observed = HubClass::TryCall<"get_ResearchManager", StoreResearchManager*>();
  auto*      manager  = observed ? roots.Keep(observed.value) : nullptr;
  return manager ? manager->TryCall<"IsResourceRequiredByVisibleResearches", bool(int64_t, ResearchTree)>(
                       id.value, ResearchTree::Standard)
                 : il2cpp::CallResult<bool>{};
}

inline bool Enabled(StoreTooltipTrigger* trigger, bool value)
{
  // Match the qualified study's base setter. TooltipTrigger hides this member
  // with an additional sibling-controller operation; it is not equivalent.
  return trigger && reinterpret_cast<unity::Behaviour*>(trigger)->TryCall<"set_enabled", void(bool)>(value).succeeded;
}

inline il2cpp::CallResult<int32_t> CurrencyCount(AllianceStoreContext* context, Roots& roots)
{
  const auto observed   = context->TryCall<"get_StoreCurrency", AllianceStoreCurrencies*>();
  auto*      currencies = observed ? roots.Keep(observed.value) : nullptr;
  const auto soft       = currencies ? currencies->TryCall<"get_SoftResources", il2cpp::List<il2cpp::Object*>*>()
                                     : il2cpp::CallResult<il2cpp::List<il2cpp::Object*>*>{};
  auto*      list       = soft ? roots.Keep(soft.value) : nullptr;
  return list ? list->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
}

inline StoreTooltipAction* ActionComponent(StoreInventoryTooltip* widget, bool create, Roots& roots)
{
  const auto object      = widget->TryCall<"get_gameObject", unity::GameObject*>();
  auto*      game_object = object ? roots.Keep(object.value) : nullptr;
  auto*      type        = StoreTooltipAction::Class::TypeObject();
  if (!game_object || !type)
    return nullptr;
  const auto found = game_object->TryCall<"GetComponent", StoreTooltipAction*>(type);
  if (!found)
    return nullptr;
  if (found.value)
    return roots.Keep(found.value);
  if (!create)
    return nullptr;
  const auto added = game_object->TryCall<"AddComponent", StoreTooltipAction*>(type);
  return added ? roots.Keep(added.value) : nullptr;
}

inline StoreTooltipTrigger* PlainComponent(StoreInventoryTooltip* widget, Roots& roots)
{
  auto* type = StoreTooltipTrigger::Class::TypeObject();
  if (!type)
    return nullptr;
  const auto plain = widget->TryCall<"GetComponent", StoreTooltipTrigger*>(type);
  auto*      value = plain ? roots.Keep(plain.value) : nullptr;
  // A native action subclass is not our original plain-plus-added arrangement.
  return value && value->klass == StoreTooltipTrigger::Class::Raw() ? value : nullptr;
}

inline StoreInventoryTooltip* Template(Roots& roots)
{
  using Resources  = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Resources">;
  auto*      type  = StoreInventoryTooltip::Class::TypeObject();
  const auto found = type ? Resources::TryCall<"FindObjectsOfTypeAll", il2cpp::ArrayObject*>(type)
                          : il2cpp::CallResult<il2cpp::ArrayObject*>{};
  auto*      owned = found ? roots.Keep(found.value) : nullptr;
  const auto array = il2cpp::TryReferenceArray<il2cpp::Object*>(owned);
  if (!array || !array.value)
    return nullptr;
  auto& fields = Members();
  for (auto* object : *array.value) {
    auto* widget = static_cast<StoreInventoryTooltip*>(object);
    if (widget && il2cpp_class_is_assignable_from(StoreInventoryTooltip::Class::Raw(), widget->klass)
        && fields.action.Read(widget) && fields.search.Read(widget))
      return roots.Keep(widget);
  }
  return nullptr;
}

// Complete the study's field preflight before making the new component usable.
inline bool CopyAppearance(StoreTooltipTrigger* plain, StoreTooltipAction* action, Roots& roots)
{
  auto& f    = Members();
  auto  copy = [&]<typename T>(const Field<T>& field) {
    auto value = field.Read(plain);
    if constexpr (std::is_pointer_v<T>)
      if (value && !roots.Keep(value))
        return false;
    return field.Write(action, value);
  };
  return copy(f.show_header) && copy(f.header) && copy(f.show_description) && copy(f.description)
         && copy(f.override_locale) && copy(f.anchor) && copy(f.size) && copy(f.target) && copy(f.offset)
         && copy(f.interactable) && copy(f.description_override) && copy(f.loader);
}
} // namespace alliance_store_game
