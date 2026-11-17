#pragma once

// Executes the production donor acquisition policy and typed slot readers.
// Native Unity liveness, serialized widgets and tracker contents are supplied;
// this does not render, instantiate an indicator or qualify real GC behavior.
#include "../src/mod/features/ship_tech_indicators.cc"

bool EnsureShipTileBinding()
{ return true; } // The shared detour installer is outside this donor fixture.

namespace
{
struct TechNativeOwner {
  Il2CppObject                       header;
  ship_tech_binding::TechSlotWidget* slots[4]{};
};
struct TechNativeSlot {
  Il2CppObject header;
  Selector*    image = nullptr;
};
struct TechNativeTile {
  Il2CppObject header;
  Selector*    image = nullptr;
};
struct TechNativeVector {
  Il2CppObject        header;
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t size   = 1;
  Il2CppObject*       items[1]{};
};
TechNativeVector tech_native_vector;
Il2CppObject     tech_native_object;
Il2CppObject*    tech_native_dead  = nullptr;
int              tech_native_scans = 0;
bool             TechNativeAlive(Il2CppObject* object, const MethodInfo*)
{ return object && object != tech_native_dead; }
unity::GameObject* TechNativeObject(il2cpp::Object*, const MethodInfo*)
{ return reinterpret_cast<unity::GameObject*>(&tech_native_object); }
il2cpp::ArrayObject* TechNativeFind(Il2CppReflectionType*, const MethodInfo*)
{
  ++tech_native_scans;
  return reinterpret_cast<il2cpp::ArrayObject*>(&tech_native_vector);
}

void ShipTechNativeCases()
{
  auto& owner    = Define("Digit.Prime.Ships", "ShipManagementViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& context  = Define("Digit.Prime.Ships", "ShipManagementViewContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& tile     = Define("Digit.Prime.Ships", "ShipTileWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& slot     = Define("Digit.Prime.ForbiddenTechs", "ForbiddenTechSlotWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& selector = Define("Digit.Client.UI", "ImageSelector", IL2CPP_TYPE_CLASS, &component_class);
  auto& resources                  = Define("UnityEngine", "Resources", IL2CPP_TYPE_CLASS, &object_class);
  auto& vector                     = Define("UnityEngine", "Object[]", IL2CPP_TYPE_SZARRAY, &array_class);
  vector.klass.byval_arg.data.type = T(unity_object_class);
  owner.klass.instance_size        = sizeof(TechNativeOwner);
  slot.klass.instance_size         = sizeof(TechNativeSlot);
  tile.klass.instance_size         = sizeof(TechNativeTile);
  context.klass.instance_size = selector.klass.instance_size = sizeof(Il2CppObject);
  const char* fields[]{"_forbiddenTechSlot", "_forbiddenTechStatus", "_chaosTechSlot", "_chaosTechStatus"};
  for (size_t index = 0; index < std::size(fields); ++index)
    AddField(owner, fields[index], slot,
             static_cast<int32_t>(offsetof(TechNativeOwner, slots) + sizeof(void*) * index));
  AddField(slot, "_tokenImage", selector, offsetof(TechNativeSlot, image));
  AddField(tile, "_shipImage", selector, offsetof(TechNativeTile, image));
  auto& game_object = AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&TechNativeObject),
                                T(game_object_class), {});
  AddProperty(component_class, "gameObject", &game_object);
  AddMethod(resources, "FindObjectsOfTypeAll", reinterpret_cast<void*>(&TechNativeFind), T(vector), {T(type_class)},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  for (const auto* method : unity_object_class.methods)
    if (std::string_view(method->name) == "op_Implicit") {
      auto* entry          = const_cast<MethodInfo*>(method);
      entry->methodPointer = entry->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(&TechNativeAlive);
    }
  Il2CppObject    primary{{&selector.klass}}, late{{&selector.klass}}, fallback{{&selector.klass}};
  Il2CppObject    first_context{{&context.klass}}, second_context{{&context.klass}};
  TechNativeSlot  native_slot{{&slot.klass}, reinterpret_cast<Selector*>(&primary)};
  TechNativeOwner native_owner{{&owner.klass}};
  TechNativeTile  native_tile{{&tile.klass}, reinterpret_cast<Selector*>(&fallback)};
  tech_native_object.klass        = &game_object_class.klass;
  tech_native_vector.header.klass = &vector.klass;
  tech_native_vector.items[0]     = &native_slot.header;
  auto*      view                 = reinterpret_cast<ShipTileWidget*>(&native_tile);
  auto*      selection            = reinterpret_cast<ShipManagementViewContext*>(&first_context);
  const auto now                  = std::chrono::steady_clock::time_point{};
  auto&      state                = ShipTech();
  state.donor_retry               = {};
  for (size_t index = 0; index < std::size(fields); ++index) {
    for (auto& value : native_owner.slots)
      value = nullptr;
    native_owner.slots[index] = reinterpret_cast<ship_tech_binding::TechSlotWidget*>(&native_slot);
    Require(ship_tech_binding::OwnedTokenSource(reinterpret_cast<ShipManagementViewController*>(&native_owner))
                == reinterpret_cast<Selector*>(&primary),
            "all four native owner slots provide qualified token art");
  }
  fleet_native_widgets[&owner.klass] = {&native_owner.header};
  Require(TechDonor(view, selection, now) == reinterpret_cast<Selector*>(&primary) && tech_native_scans == 0,
          "loaded native owner supplies token art without a global resource scan");
  fleet_native_widgets.clear();
  Require(TechDonor(view, selection, now) == reinterpret_cast<Selector*>(&primary) && tech_native_scans == 0,
          "weak cached live template survives the owner's disappearance");
  tech_native_dead = &primary;
  Require(TechDonor(view, selection, now + std::chrono::seconds(1)) == reinterpret_cast<Selector*>(&fallback),
          "dead donor falls back to native ship art during the retry window");
  Require(tech_native_scans == 0, "unavailable donor does not scan repeatedly during binding");
  tech_native_vector.items[0] = nullptr;
  (void)TechDonor(view, selection, now + std::chrono::seconds(5));
  for (int i = 6; i < 30; ++i)
    (void)TechDonor(view, selection, now + std::chrono::seconds(i));
  Require(tech_native_scans == 1, "global fallback is attempted at most once in the same selection context");
  auto* other = reinterpret_cast<ShipManagementViewContext*>(&second_context);
  (void)TechDonor(view, other, now + std::chrono::seconds(30));
  Require(tech_native_scans == 2, "a new selection context admits one fresh fallback");
  native_slot.image                  = reinterpret_cast<Selector*>(&late);
  fleet_native_widgets[&owner.klass] = {&native_owner.header};
  Require(TechDonor(view, other, now + std::chrono::seconds(35)) == reinterpret_cast<Selector*>(&late)
              && tech_native_scans == 2,
          "late native widgets remain discoverable through the cheap throttled owner path");
  state.donor.Reset();
  state.donor_context.Reset();
  fleet_native_widgets.clear();
  warp_root_failure = true;
  Require(TechDonor(view, selection, now + std::chrono::seconds(40)) == reinterpret_cast<Selector*>(&fallback)
              && tech_native_scans == 2,
          "failed context custody never starts an unowned global lookup");
  warp_root_failure = false;
  tech_native_dead  = nullptr;
  Require(warp_roots.empty(), "donor operations release temporary and retained handles on retirement");
  std::cout << "Ship-tech production donor policy and owned serialized sources passed\n";
}
} // namespace
