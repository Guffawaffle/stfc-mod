#pragma once

// Production owner bindings and camera callback; native scene objects are controlled substitutes.
#include "game/hud.h"

namespace
{
struct HudOwnerCanvas {
  Il2CppObject  header;
  Il2CppObject* context = nullptr;
  Il2CppObject* object  = nullptr;
  bool          visible = true;
};
struct HudOwnerList {
  Il2CppObject                 header;
  std::vector<HudOwnerCanvas*> canvases;
};
struct HudOwnerManager {
  Il2CppObject  header;
  HudOwnerList* screens = nullptr;
};
struct HudOwnerObject {
  Il2CppObject  header;
  Il2CppObject* controller = nullptr;
};
HudOwnerManager* hud_owner_manager      = nullptr;
bool             hud_owner_count_throws = false;
int              hud_owner_searches     = 0;
Il2CppObject*    hud_owner_disabled     = nullptr;
bool             HudOwnerActive(Il2CppObject* controller, const MethodInfo*)
{ return controller != hud_owner_disabled; }
int32_t HudOwnerCount(HudOwnerList* list, const MethodInfo*)
{
  if (hud_owner_count_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return static_cast<int32_t>(list->canvases.size());
}
HudOwnerCanvas* HudOwnerItem(HudOwnerList* list, int32_t index, const MethodInfo*)
{ return list->canvases.at(index); }
Il2CppObject* HudOwnerContext(HudOwnerCanvas* canvas, const MethodInfo*)
{ return canvas->context; }
bool HudOwnerVisible(HudOwnerCanvas* canvas, const MethodInfo*)
{ return canvas->visible; }
Il2CppObject* HudOwnerGameObject(HudOwnerCanvas* canvas, const MethodInfo*)
{ return canvas->object; }
Il2CppObject* HudOwnerChildren(HudOwnerObject* object, Il2CppReflectionType*, bool inactive, const MethodInfo*)
{
  Require(!inactive, "owner search uses the non-generic Type,bool active-only overload");
  ++hud_owner_searches;
  return object->controller;
}
bool HudOwnerStatic(FieldInfo* field, void* value)
{
  if (std::string_view(field->name) != "s_instance")
    return false;
  std::memcpy(value, &hud_owner_manager, sizeof(hud_owner_manager));
  return true;
}

void HudOwnerCases()
{
  auto& behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  auto& active_getter =
      AddMethod(behaviour, "get_isActiveAndEnabled", reinterpret_cast<void*>(&HudOwnerActive), T(bool_class), {});
  AddProperty(behaviour, "isActiveAndEnabled", &active_getter);
  auto& manager = Define("Digit.Client.UI", "ScreenManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& canvas  = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &component_class);
  auto& list    = Define("HudFixture", "CanvasList", IL2CPP_TYPE_CLASS, &object_class);
  auto& nav = Define("Digit.Prime.Navigation.UI", "HudNavigationViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& drawer = Define("Digit.Prime.HUD", "HudAllianceAndNewsViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& nav_context = Define("Digit.Prime.Navigation", "NavigationViewData", IL2CPP_TYPE_CLASS, &object_class);
  auto& drawer_context =
      Define("Digit.Prime.HUD", "HudAllianceAndNewsViewControllerContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& go = Define("HudFixture", "OwnerObject", IL2CPP_TYPE_CLASS, &game_object_class);
  AddField(manager, "s_instance", manager, 0, true);
  AddField(manager, "_screens", list, offsetof(HudOwnerManager, screens));
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&HudOwnerCount), T(int_class), {});
  AddMethod(list, "get_Item", reinterpret_cast<void*>(&HudOwnerItem), T(canvas), {T(int_class)});
  auto& context_getter =
      AddMethod(canvas, "get_Context", reinterpret_cast<void*>(&HudOwnerContext), T(object_class), {});
  AddProperty(canvas, "Context", &context_getter);
  auto& visible_getter = AddMethod(canvas, "get_Visible", reinterpret_cast<void*>(&HudOwnerVisible), T(bool_class), {});
  AddProperty(canvas, "Visible", &visible_getter);
  // Component's accessor is resolved on Component, as in the real runtime.
  auto& object_getter = AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&HudOwnerGameObject),
                                  T(game_object_class), {});
  AddProperty(component_class, "gameObject", &object_getter);
  AddMethod(game_object_class, "GetComponentInChildren", reinterpret_cast<void*>(&HudOwnerChildren), T(component_class),
            {T(type_class), T(bool_class)});
  Il2CppObject navigation{{&nav.klass}}, retired_navigation{{&nav.klass}}, alliance{{&drawer.klass}},
      navigation_context{{&nav_context.klass}}, alliance_context{{&drawer_context.klass}};
  HudOwnerObject nav_object{{&go.klass}, &navigation}, retired_object{{&go.klass}, &retired_navigation},
      drawer_object{{&go.klass}, &alliance};
  HudOwnerCanvas overlay{{&canvas.klass}, &base_object.header, nullptr},
      hidden{{&canvas.klass}, &navigation_context, &nav_object.header, false};
  HudOwnerCanvas navigation_canvas{{&canvas.klass}, &navigation_context, &nav_object.header},
      retired_canvas{{&canvas.klass}, &navigation_context, &retired_object.header},
      alliance_canvas{{&canvas.klass}, &alliance_context, &drawer_object.header};
  HudOwnerList    screens{{&list.klass}, {&overlay, &hidden, &retired_canvas, &navigation_canvas, &alliance_canvas}};
  HudOwnerManager owner{{&manager.klass}, &screens};
  hud_owner_manager              = &owner;
  hud_owner_disabled             = &retired_navigation;
  tow_static_field_read_override = HudOwnerStatic;
  const auto roots               = warp_roots.size();
  auto*      found_navigation = ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>();
  if (!found_navigation)
    for (const auto& error : errors)
      std::cerr << error << '\n';
  Require(
      found_navigation == reinterpret_cast<HudNavigationViewController*>(&navigation),
      "navigation owner skips unrelated/hidden canvases and a disabled old controller before the active replacement");
  Require(
      ScreenManager::RegisteredController<HudAllianceAndNewsViewController, HudAllianceAndNewsViewControllerContext>()
          == reinterpret_cast<HudAllianceAndNewsViewController*>(&alliance),
      "drawer is found on a separate registered canvas");
  Require(hud_owner_searches == 3 && warp_roots.size() == roots,
          "only matching visible hierarchies are searched and temporary roots release");
  navigation_canvas.visible = false;
  Require(!ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>(),
          "hidden native owner never becomes a visible timer owner");
  navigation_canvas.visible = true;
  hud_owner_count_throws    = true;
  Require(!ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>(),
          "unavailable registry count does not authorize a hierarchy search");
  hud_owner_count_throws = false;
  hud_owner_manager      = nullptr;
  Require(!ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>(),
          "missing existing manager is not recreated");
  Require(warp_roots.size() == roots, "failure paths release all temporary registry roots");
  tow_static_field_read_override = nullptr;
  std::cout
      << "Registered HUD owner bindings: independent canvases, hidden/absent owners, exact Type,bool overload passed\n";
}

void StationRadiusCases()
{
  HavenFixtureGeometry scene("station-radius-native");
  auto&                hub      = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto&                sections = Define("Digit.Client.Sections", "SectionManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& getter = AddMethod(hub, "get_SectionManager", reinterpret_cast<void*>(&HavenFixtureSections), T(sections), {},
                           METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "SectionManager", &getter);
  auto& section_getter =
      AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&HavenFixtureSection), T(int_class), {});
  AddProperty(sections, "CurrentSection", &section_getter);
  haven_fixture_sections.klass        = &sections.klass;
  auto  replacement                   = scene.far_provider;
  auto* first                         = reinterpret_cast<OrbitFrameProvider*>(&scene.far_provider);
  auto* second                        = reinterpret_cast<OrbitFrameProvider*>(&replacement);
  auto& config                        = Config::Get();
  config.graphics.interior_zoom_scale = 2;
  bindings_config_override            = &config;
  interior_zoom_installed             = true;
  native_interior_radius.clear();
  int  originals        = 0;
  auto original         = [&](OrbitFrameProvider*, unity::Camera*) { ++originals; };
  haven_fixture_section = SectionID::Starbase_Interior;
  OrbitFrameProvider_UpdateConstrains_Hook(original, first, nullptr);
  Require(scene.far_range.minimum == 2000 && scene.far_range.maximum == 2000,
          "interior expansion starts from native radius");
  OrbitFrameProvider_UpdateConstrains_Hook(original, second, nullptr);
  Require(scene.far_range.maximum == 2000,
          "replacement provider cannot recapture the widened radius and compound scale");
  haven_fixture_section = SectionID::Starbase_Exterior;
  OrbitFrameProvider_UpdateConstrains_Hook(original, second, nullptr);
  Require(scene.far_range.minimum == 1000 && scene.far_range.maximum == 1000,
          "replacement exterior provider restores the shared native radius");
  haven_fixture_section = SectionID::Starbase_Interior;
  OrbitFrameProvider_UpdateConstrains_Hook(original, first, nullptr);
  config.graphics.interior_zoom_scale = 1;
  OrbitFrameProvider_UpdateConstrains_Hook(original, second, nullptr);
  Require(scene.far_range.maximum == 1000 && originals == 5,
          "live disable restores the shared range and every native original runs once");
  bindings_config_override = nullptr;
  native_interior_radius.clear();
  std::cout << "Station camera production callback: recreated providers, shared radius, native restoration and disable "
               "passed\n";
}
} // namespace
