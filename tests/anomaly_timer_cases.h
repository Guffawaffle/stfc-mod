#pragma once

// Execute the real countdown owner against the existing controlled Unity scene and HUD registry.
// The clock is explicit so a target-card hide can straddle discovery without sleeps or a game client.
#include "../src/mod/features/anomaly_timer.cc"

namespace
{
Il2CppObject *anomaly_fixture_context = nullptr, *anomaly_fixture_address = nullptr, *anomaly_fixture_data = nullptr,
             *anomaly_fixture_timer = nullptr, *anomaly_fixture_manager = nullptr;
struct AnomalyFontList {
  Il2CppObject  header;
  Il2CppObject* label;
};
Il2CppObject* AnomalyContext(Il2CppObject*, const MethodInfo*)
{ return anomaly_fixture_context; }
Il2CppObject* AnomalyAddress(Il2CppObject*, const MethodInfo*)
{ return anomaly_fixture_address; }
Il2CppObject* AnomalyData(Il2CppObject*, int64_t system, const MethodInfo*)
{
  Require(system == 37, "countdown observes the viewed system");
  return anomaly_fixture_data;
}
Il2CppObject* AnomalyTimerContext(Il2CppObject*, const MethodInfo*)
{ return anomaly_fixture_timer; }
int64_t AnomalySystem(Il2CppObject*, const MethodInfo*)
{ return 37; }
bool AnomalyActive(Il2CppObject*, const MethodInfo*)
{ return true; }
unity::TimeSpan AnomalyTime(Il2CppObject*, const MethodInfo*)
{ return {120'000'000}; }
int32_t AnomalyFontCount(AnomalyFontList*, const MethodInfo*)
{ return 1; }
Il2CppObject* AnomalyFontItem(AnomalyFontList* list, int32_t index, const MethodInfo*)
{
  Require(index == 0, "countdown borrows the native system-name font");
  return list->label;
}
bool AnomalyStatic(FieldInfo* field, void* value)
{
  if (std::string_view(field->parent->name) == "GalacticAnomaliesManager") {
    std::memcpy(value, &anomaly_fixture_manager, sizeof(anomaly_fixture_manager));
    return true;
  }
  return HudOwnerStatic(field, value);
}
Il2CppObject* AnomalyGameObject(Il2CppObject* object, const MethodInfo* method)
{
  return std::string_view(object->klass->name) == "CanvasController"
             ? HudOwnerGameObject(reinterpret_cast<HudOwnerCanvas*>(object), method)
             : LoadingGameObject(object, method);
}

void AnomalyTimerLifecycleCases()
{
  // Reuse the qualified Unity allocator, parenting, native font and text adapters.
  LoadingArtworkConsumerCases("loading-login-consumer");
  tow_object_new_override = &LoadingAllocate;
  type_object_override    = &LoadingType;
  auto& behaviour         = Fake(loading_image_class->klass.parent);
  auto& hub               = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto& sections          = Define("Digit.Client.Sections", "SectionManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& section_manager   = AddMethod(hub, "get_SectionManager", reinterpret_cast<void*>(&HavenFixtureSections),
                                      T(sections), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "SectionManager", &section_manager);
  auto& section =
      AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&HavenFixtureSection), T(int_class), {});
  AddProperty(sections, "CurrentSection", &section);
  haven_fixture_sections.klass = &sections.klass;
  haven_fixture_section        = SectionID::Navigation_System;

  auto& manager = Define("Digit.Client.UI", "ScreenManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& canvas  = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &component_class);
  auto& list    = Define("AnomalyFixture", "CanvasList", IL2CPP_TYPE_CLASS, &object_class);
  auto& nav     = Define("Digit.Prime.Navigation.UI", "HudNavigationViewController", IL2CPP_TYPE_CLASS, &behaviour);
  auto& drawer  = Define("Digit.Prime.HUD", "HudAllianceAndNewsViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& context = Define("Digit.Prime.Navigation", "NavigationViewData", IL2CPP_TYPE_CLASS, &object_class);
  auto& drawer_context =
      Define("Digit.Prime.HUD", "HudAllianceAndNewsViewControllerContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& faction = Define("Digit.Prime.HUD", "FactionInformation", IL2CPP_TYPE_CLASS, &object_class);
  auto& names   = Define("AnomalyFixture", "SystemNames", IL2CPP_TYPE_CLASS, &object_class);
  auto& address = Define("Digit.PrimeServer.Models", "NodeAddress", IL2CPP_TYPE_CLASS, &object_class);
  auto& anomaly_manager =
      Define("Digit.Prime.GalacticAnomalies", "GalacticAnomaliesManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& anomaly    = Define("AnomalyFixture", "Anomaly", IL2CPP_TYPE_CLASS, &object_class);
  auto& timer_data = Define("AnomalyFixture", "Timer", IL2CPP_TYPE_CLASS, &object_class);
  auto& time       = Define("System", "TimeSpan", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::TimeSpan));
  auto  property   = [](FakeClass& cls, const char* name, auto function, const Il2CppType* result) {
    for (auto* entry : cls.methods)
      if (std::string_view(entry->name) == name) {
        auto* method          = const_cast<MethodInfo*>(entry);
        method->methodPointer = method->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(function);
        AddProperty(cls, name + 4, method);
        return;
      }
    auto& method = AddMethod(cls, name, reinterpret_cast<void*>(function), result, {});
    AddProperty(cls, name + 4, &method);
  };
  property(behaviour, "get_isActiveAndEnabled", &HudOwnerActive, T(bool_class));
  property(component_class, "get_gameObject", &AnomalyGameObject, T(game_object_class));
  property(canvas, "get_Context", &HudOwnerContext, T(object_class));
  property(canvas, "get_Visible", &HudOwnerVisible, T(bool_class));
  property(nav, "get_CanvasContext", &AnomalyContext, T(context));
  property(context, "get_ViewingAddress", &AnomalyAddress, T(address));
  property(address, "get_System", &AnomalySystem, T(long_class));
  property(anomaly, "get_IsActive", &AnomalyActive, T(bool_class));
  property(anomaly, "get_EndTimeTimerDataContext", &AnomalyTimerContext, T(timer_data));
  property(timer_data, "get_RemainingTime", &AnomalyTime, T(time));
  AddField(manager, "s_instance", manager, 0, true);
  AddField(manager, "_screens", list, offsetof(HudOwnerManager, screens));
  AddField(anomaly_manager, "s_instance", anomaly_manager, 0, true);
  AddField(nav, "_factionInformationHelper", faction, offsetof(LoadingFixtureNode, image));
  AddField(nav, "_galacticAnomaliesButtonWidget", component_class, offsetof(LoadingFixtureNode, background));
  AddField(drawer, "_chestPromotion", component_class, offsetof(LoadingFixtureNode, canvas));
  AddField(faction, "_systemName", names, offsetof(LoadingFixtureNode, image));
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&HudOwnerCount), T(int_class), {});
  AddMethod(list, "get_Item", reinterpret_cast<void*>(&HudOwnerItem), T(canvas), {T(int_class)});
  AddMethod(names, "get_Count", reinterpret_cast<void*>(&AnomalyFontCount), T(int_class), {});
  AddMethod(names, "get_Item", reinterpret_cast<void*>(&AnomalyFontItem), T(component_class), {T(int_class)});
  AddMethod(game_object_class, "GetComponentInChildren", reinterpret_cast<void*>(&HudOwnerChildren), T(component_class),
            {T(type_class), T(bool_class)});
  AddMethod(nav, "CanShowGalacticAnomaly", reinterpret_cast<void*>(&AnomalyActive), T(bool_class), {});
  AddMethod(anomaly_manager, "GetSystemGalacticAnomalies", reinterpret_cast<void*>(&AnomalyData), T(anomaly),
            {T(long_class)});
  Il2CppObject navigation_context{{&context.klass}}, alliance_context{{&drawer_context.klass}},
      manager_data{{&anomaly_manager.klass}}, anomaly_data{{&anomaly.klass}}, clock_data{{&timer_data.klass}},
      address_data{{&address.klass}};
  anomaly_fixture_context = &navigation_context;
  anomaly_fixture_address = &address_data;
  anomaly_fixture_data    = &anomaly_data;
  anomaly_fixture_timer   = &clock_data;
  anomaly_fixture_manager = &manager_data;
  auto* native_nav        = LoadingNode(&nav.klass);
  auto* native_drawer     = LoadingNode(&drawer.klass);
  auto* native_names      = LoadingNode(&faction.klass);
  auto* font_owner        = LoadingGo("System name");
  auto* font_label        = LoadingNode(&loading_text_ui_class->klass);
  font_label->font        = &base_object.header;
  font_owner->label       = font_label;
  AnomalyFontList fonts{{&names.klass}, &font_owner->header};
  native_names->image          = reinterpret_cast<LoadingFixtureNode*>(&fonts);
  native_nav->image            = native_names;
  auto* mount                  = LoadingGo("Alliance drawer");
  auto* promotion              = LoadingNode(&component_class.klass);
  promotion->transform         = LoadingGo("Chest promotion")->transform;
  promotion->transform->parent = mount->transform;
  native_drawer->canvas        = promotion;
  HudOwnerObject nav_object{{&game_object_class.klass}, &native_nav->header},
      drawer_object{{&game_object_class.klass}, &native_drawer->header};
  HudOwnerCanvas nav_canvas{{&canvas.klass}, &navigation_context, &nav_object.header},
      drawer_canvas{{&canvas.klass}, &alliance_context, &drawer_object.header};
  HudOwnerList    screens{{&list.klass}, {&nav_canvas, &drawer_canvas}};
  HudOwnerManager native_manager{{&manager.klass}, &screens};
  hud_owner_manager               = &native_manager;
  tow_static_field_read_override  = &AnomalyStatic;
  auto& graphics                  = Config::Get().graphics;
  graphics.galactic_anomaly_timer = true;
  graphics.mirror_space_timer     = false;
  const auto start                = AnomalyClock::time_point{} + std::chrono::seconds(10);
  auto       panel                = []() -> LoadingFixtureNode* {
    for (auto& node : loading_nodes)
      if (node.name == "CommunityMod_AnomalyTimer" && !loading_destroyed.contains(&node.header))
        return &node;
    return nullptr;
  };
  AnomalyTimer countdown;
  countdown.Update(start);
  auto* first = panel();
  Require(first && first->active, "countdown is mounted and active before opening a target card");
  const auto created = loading_created;
  nav_canvas.visible = drawer_canvas.visible = false;
  hud_owner_disabled                         = &native_nav->header;
  mount->active                              = false;
  countdown.Update(start + std::chrono::milliseconds(1100));
  Require(panel() == first && first->active && loading_created == created,
          "hidden native owners preserve the mounted panel across a discovery tick");
  nav_canvas.visible = drawer_canvas.visible = true;
  hud_owner_disabled                         = nullptr;
  mount->active                              = true;
  countdown.Update(start + std::chrono::milliseconds(1101));
  Require(panel() == first && first->active && mount->active && loading_created == created,
          "dismissal restores the same panel with its native parent before either timer deadline");
  drawer_canvas.visible = false;
  countdown.Update(start + std::chrono::milliseconds(2200));
  Require(panel() == first && first->active, "a hidden drawer alone is not a replaced mount");
  drawer_canvas.visible        = true;
  auto* replacement_mount      = LoadingGo("Replacement drawer");
  promotion->transform->parent = replacement_mount->transform;
  countdown.Update(start + std::chrono::milliseconds(3300));
  Require(panel() && panel() != first && loading_destroyed.contains(&first->header)
              && panel()->transform->parent == replacement_mount->transform,
          "a changed native mount retires the old panel and creates one under the current drawer");
  haven_fixture_section = SectionID::Navigation_Galaxy;
  countdown.Update(start + std::chrono::milliseconds(3600));
  Require(!panel(), "leaving the system destroys the countdown despite preserved hidden-owner custody");
  haven_fixture_section = SectionID::Navigation_System;
  countdown.Update(start + std::chrono::milliseconds(3900));
  Require(panel(), "reentry discovers and mounts the countdown again");
  graphics.galactic_anomaly_timer = false;
  countdown.Update(start + std::chrono::milliseconds(4200));
  Require(!panel(), "disabling both timers removes the panel");
  tow_static_field_read_override = nullptr;
  hud_owner_manager              = nullptr;
  std::cout << "Countdown lifecycle: target hide/dismiss across discovery, native mount replacement, exit and disable "
               "passed\n";
}
} // namespace
