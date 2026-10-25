#pragma once

// Native equipment/art observations used by Swap Ship decorations. No combat interpretation.
#include "fleet.h"
#include "server.h"
#include "ui.h"

#include <unity/ui.h>

#include <array>
#include <cstddef>
#include <optional>

namespace ship_tech_binding
{
struct TechSpec : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ForbiddenTechSpec">;
};
struct Tech : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ForbiddenTech">;
};
struct ImageSelector : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "ImageSelector">;

  unity::Image* Target()
  {
    const auto observed = TryCall<"get_Target", il2cpp::Object*>();
    return observed && observed.value ? observed.value->As<unity::Image>() : nullptr;
  }
  bool SetArt(int64_t id)
  {
    auto       root   = il2cpp::GcHandle::Strong(il2cpp::Box(id));
    const auto method = ArtSetter();
    if (!root || !method)
      return false;
    void* args[] = {root.Get()};
    return il2cpp::detail::TryInvokeBoxed(method.Info(), this, args).succeeded;
  }
  static il2cpp::Method ArtSetter()
  {
    // Object and object[] overloads are not interchangeable. Preserve the exact
    // existing native scalar-identifier contract instead of an untyped ref match.
    static const auto method = Class::Method("SetIdentifierParams", {"System.Object"});
    const auto*       info   = method.Info();
    return info && !(info->flags & METHOD_ATTRIBUTE_STATIC) && info->return_type && !info->return_type->byref
                   && info->return_type->type == IL2CPP_TYPE_VOID
               ? method
               : il2cpp::Method{};
  }
};
struct TechSlotWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ForbiddenTechs", "ForbiddenTechSlotWidget">;

  ImageSelector* TokenImage()
  { return Field<"_tokenImage", ImageSelector*>(); }
};

inline ImageSelector* ShipImage(ShipTileWidget* tile)
{ return tile ? tile->Field<"_shipImage", ImageSelector*>() : nullptr; }

inline const MethodInfo* SlotItemGetter()
{
  using Value               = il2cpp::Nullable<int64_t>;
  static const auto* method = [] {
    const auto* value = il2cpp::detail::MethodCall<Value()>::Resolve(EntitySlot::Class::Raw(), "get_SlotItemId", false);
    return value
                   && il2cpp::detail::NullableLayoutMatches(value->return_type, il2cpp::PrimitiveClass<int64_t>(),
                                                            sizeof(Value), alignof(Value), offsetof(Value, has_value),
                                                            offsetof(Value, value))
               ? value
               : nullptr;
  }();
  return method;
}

inline il2cpp::CallResult<int64_t> SlotItem(EntitySlot* slot)
{
  const auto* getter = SlotItemGetter();
  if (!slot || !getter)
    return {};
  const auto boxed = il2cpp::detail::TryInvokeBoxed(getter, slot, nullptr);
  if (!boxed)
    return {};
  if (!boxed.value)
    return {true, 0}; // An observed empty Nullable has no equipped item.
  int64_t id = 0;
  return il2cpp::Unbox(boxed.value, id) ? il2cpp::CallResult<int64_t>{true, id} : il2cpp::CallResult<int64_t>{};
}

inline il2cpp::CallResult<int64_t> Art(ForbiddenTechManager* manager, int64_t id)
{
  if (id == 0)
    return {true, 0};
  const auto tech =
      manager ? manager->TryCall<"GetForbiddenTechById", Tech*(int64_t)>(id) : il2cpp::CallResult<Tech*>{};
  auto       root = il2cpp::GcHandle::Strong(tech ? tech.value : nullptr);
  const auto spec =
      root ? root.Get<Tech>()->TryCall<"get_ForbiddenTechSpec", TechSpec*>() : il2cpp::CallResult<TechSpec*>{};
  auto       spec_root = il2cpp::GcHandle::Strong(spec ? spec.value : nullptr);
  const auto refs =
      spec_root ? spec_root.Get<TechSpec>()->TryCall<"get_IdRefs", IdRefs*>() : il2cpp::CallResult<IdRefs*>{};
  auto refs_root = il2cpp::GcHandle::Strong(refs ? refs.value : nullptr);
  return refs_root ? refs_root.Get<IdRefs>()->TryCall<"get_ArtId", int64_t>() : il2cpp::CallResult<int64_t>{};
}

inline il2cpp::CallResult<std::array<int64_t, 2>> EquippedArt(int64_t ship)
{
  auto                       root    = il2cpp::GcHandle::Strong(ForbiddenTechManager::Instance());
  auto*                      manager = root.Get<ForbiddenTechManager>();
  il2cpp::List<EntitySlot*>* list    = nullptr;
  const auto                 found =
      manager
          ? manager->TryCall<"TryGetActiveForbiddenTechSlot", bool(int64_t, il2cpp::List<EntitySlot*>*&)>(ship, list)
          : il2cpp::CallResult<bool>{};
  if (!found)
    return {};
  if (!found.value)
    return {true, {0, 0}};
  auto       list_root = il2cpp::GcHandle::Strong(list);
  const auto count     = list_root ? list->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return {};
  std::array<int64_t, 2> result{};
  for (int32_t index = 0; index < count.value; ++index) {
    const auto entry     = list->TryCall<"get_Item", EntitySlot*(int32_t)>(index);
    auto       slot_root = il2cpp::GcHandle::Strong(entry ? entry.value : nullptr);
    auto*      slot      = slot_root.Get<EntitySlot>();
    if (!slot)
      continue;
    const auto spec = slot->TryCall<"get_SlotSpecId", int64_t>();
    if (!spec)
      return {};
    const int target = spec.value == EntitySlot::kForbiddenTechSlot ? 0
                       : spec.value == EntitySlot::kChaosTechSlot   ? 1
                                                                    : -1;
    if (target < 0)
      continue;
    const auto id  = SlotItem(slot);
    const auto art = id ? Art(manager, id.value) : il2cpp::CallResult<int64_t>{};
    if (!art)
      return {};
    result[static_cast<size_t>(target)] = art.value;
  }
  return {true, result};
}

inline ImageSelector* TokenSource()
{
  using Resources = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Resources">;
  const auto values =
      Resources::TryCall<"FindObjectsOfTypeAll", il2cpp::ArrayObject*>(TechSlotWidget::Class::TypeObject());
  const auto array = values ? il2cpp::TryReferenceArray<il2cpp::Object*>(values.value)
                            : il2cpp::CallResult<il2cpp::Array<il2cpp::Object*>*>{};
  auto       root  = il2cpp::GcHandle::Strong(array ? array.value : nullptr);
  if (!root)
    return nullptr;
  for (auto* object : il2cpp::Items(array.value)) {
    auto* slot = object ? object->As<TechSlotWidget>() : nullptr;
    if (auto* image = slot ? slot->TokenImage() : nullptr; unity::Alive(image))
      return image;
  }
  return nullptr;
}

inline bool Available()
{
  return ShipTileWidget::Class::HasField<"_shipImage", ImageSelector*>()
         && TechSlotWidget::Class::HasField<"_tokenImage", ImageSelector*>()
         && ForbiddenTechManager::Class::Method<Tech*(int64_t)>("GetForbiddenTechById")
         && Tech::Class::Method<TechSpec*()>("get_ForbiddenTechSpec")
         && TechSpec::Class::Method<IdRefs*()>("get_IdRefs") && IdRefs::Class::Method<int64_t()>("get_ArtId")
         && SlotItemGetter() && ImageSelector::ArtSetter();
}
} // namespace ship_tech_binding
