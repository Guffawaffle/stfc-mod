#pragma once

// Production owner bindings and camera callback; native scene objects are controlled substitutes.
#include "game/hud.h"

#include <thread>

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

void ComponentLookupDiagnosticCases()
{
  const auto before = errors.size();
  has_input_field   = false;
  Require(!selected_object.GetComponent<unity::TMP_InputField>() && errors.size() == before,
          "successfully absent component is silent");
  component_lookup_throws = true;
  Require(!selected_object.GetComponent<unity::TMP_InputField>(), "managed lookup failure remains contained");
  Require(ErrorsMentioning("GetComponent<TMPro.TMP_InputField> failed at") == 1
              && ErrorsMentioning("hud_owner_cases.h:") == 1 && ErrorsMentioning("ComponentLookupDiagnosticCases") == 1,
          "failed lookup identifies requested component and actual native source caller");
  const auto after = errors.size();
  Require(!selected_object.GetComponent<unity::TMP_InputField>() && errors.size() == after,
          "repeated lookup failure cannot flood diagnostics");
  component_lookup_throws = false;
  has_input_field         = true;
  Require(selected_object.GetComponent<unity::TMP_InputField>() == &input_field,
          "diagnostic does not suppress recovery");
  std::cout << "Component lookup diagnostic: caller/type, contained failure, bounded logging and recovery passed\n";
}

void StationRadiusCases(std::string_view mode)
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
  haven_fixture_sections.klass = &sections.klass;
  auto  replacement            = scene.far_provider;
  auto* first                  = reinterpret_cast<OrbitFrameProvider*>(&scene.far_provider);
  auto* second                 = reinterpret_cast<OrbitFrameProvider*>(&replacement);
  if (mode != "station-radius-native") {
    auto&      fields = mode == "station-radius-missing-provider"
                            ? Fake(scene.far_provider.header.klass->parent->parent).fields
                        : mode == "station-radius-missing-radius" ? Fake(scene.far_constraint.header.klass).fields
                                                                  : Fake(scene.far_range.header.klass).fields;
    const auto name   = mode == "station-radius-missing-provider" ? "_constraint"
                        : mode == "station-radius-missing-radius" ? "_radius"
                        : mode == "station-radius-missing-min"    ? "_min"
                                                                  : "_max";
    for (auto& field : fields)
      if (std::string_view(field.name) == name)
        field.name = "_unavailable";
    Require(!InstallInteriorZoom() && !interior_zoom_installed,
            "missing critical field refuses interior mutation before shared hook installation");
    int originals = 0;
    OrbitFrameProvider_UpdateConstrains_Hook([&](auto*, auto*) { ++originals; }, first, nullptr);
    Require(scene.far_range.maximum == 1000 && originals == 1 && native_interior_radius.empty(),
            "unavailable fields preserve native callback and range");
    return;
  }
  Require(OrbitFrameProvider::ConstraintRadiusFieldsAvailable(), "all critical radius fields are qualified");
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

  const auto roots = warp_roots.size();
  auto       run   = [&](HavenFixtureProvider& provider) {
    const auto before = originals;
    OrbitFrameProvider_UpdateConstrains_Hook(original, reinterpret_cast<OrbitFrameProvider*>(&provider), nullptr);
    Require(originals == before + 1, "each station callback delivers native original exactly once");
  };
  config.graphics.interior_zoom_scale = 2;
  run(scene.far_provider);
  run(scene.close_provider);
  Require(native_interior_radius.size() == 2 && warp_roots.size() == roots + 6,
          "distinct ranges retain bounded weak owner triples, not strong permanent roots");
  haven_fixture_section = SectionID::Starbase_Exterior;
  run(replacement);
  Require(scene.far_range.maximum == 1000 && scene.close_range.maximum == 100 && native_interior_radius.empty()
              && warp_roots.size() == roots,
          "exit from another provider restores every no-longer-ticking range and releases handles");

  haven_fixture_section = SectionID::Starbase_Interior;
  run(scene.far_provider);
  scene.far_range.minimum = scene.far_range.maximum = 1500;
  run(scene.far_provider);
  Require(scene.far_range.maximum == 3000, "native replacement range becomes the new baseline");
  scene.far_range.minimum = scene.far_range.maximum = 1700;
  haven_fixture_section                             = SectionID::Starbase_Exterior;
  run(scene.close_provider);
  Require(scene.far_range.maximum == 1700, "exit never overwrites a later native or feature write");

  haven_fixture_section = SectionID::Starbase_Interior;
  run(scene.far_provider);
  scene.far_provider.constraint = &scene.close_constraint;
  run(scene.far_provider);
  Require(scene.far_range.maximum == 1700 && scene.close_range.maximum == 200,
          "replaced constraint restores retired radius before capturing the new one");
  // Model managed-address reuse by expiring the stored weak provider identity, then reusing its native address.
  for (auto& [handle, target] : warp_roots)
    if (target == &scene.far_provider.header)
      target = nullptr;
  scene.close_range.minimum = scene.close_range.maximum = 350;
  run(scene.far_provider);
  Require(scene.close_range.maximum == 700
              && native_interior_radius.at(first).provider.Get() == &scene.far_provider.header,
          "expired weak identity at a reused address cannot reuse the old baseline");
  haven_fixture_section = SectionID::Starbase_Exterior;
  run(replacement);
  scene.far_provider.constraint = &scene.far_constraint;
  Require(scene.close_range.maximum == 350 && warp_roots.size() == roots, "replacement exit releases all handles");

  haven_fixture_section = SectionID::Starbase_Interior;
  run(scene.far_provider);
  scene.far_constraint.radius = &scene.close_range;
  run(scene.far_provider);
  Require(scene.far_range.maximum == 1700 && scene.close_range.maximum == 700,
          "radius replacement within the same constraint restores the prior range");
  haven_fixture_section = SectionID::Starbase_Exterior;
  run(replacement);
  scene.far_constraint.radius = &scene.far_range;

  haven_fixture_section   = SectionID::Starbase_Interior;
  scene.far_range.minimum = scene.far_range.maximum = 1000;
  run(scene.far_provider);
  config.graphics.interior_zoom_scale = std::numeric_limits<float>::max();
  run(scene.far_provider);
  Require(scene.far_range.maximum == 1000 && native_interior_radius.empty(),
          "overflow restores our last finite write and retires the entry");
  config.graphics.interior_zoom_scale = 2;
  warp_root_failure                   = true;
  run(scene.far_provider);
  warp_root_failure = false;
  Require(scene.far_range.maximum == 1000 && native_interior_radius.empty(), "failed strong custody cannot mutate");
  weak_root_refusal = [](Il2CppObject*) { return true; };
  run(scene.far_provider);
  weak_root_refusal = nullptr;
  Require(scene.far_range.maximum == 1000 && native_interior_radius.empty() && warp_roots.size() == roots,
          "failed weak custody cannot capture or mutate");

  run(scene.far_provider);
  for (auto& [handle, target] : warp_roots)
    if (target == &scene.far_provider.header)
      target = nullptr;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  run(scene.close_provider);
  Require(scene.far_range.maximum == 1000 && scene.close_range.maximum == 700 && native_interior_radius.size() == 1
              && warp_roots.size() == roots + 3,
          "periodic prune restores a surviving radius and releases an expired provider's weak handles");
  haven_fixture_section = SectionID::Starbase_Exterior;
  run(replacement);
  haven_fixture_section = SectionID::Starbase_Interior;

  std::array<HavenFixtureRange, kInteriorProviderLimit + 1>      ranges;
  std::array<HavenFixtureConstraint, kInteriorProviderLimit + 1> constraints;
  std::array<HavenFixtureProvider, kInteriorProviderLimit + 1>   providers;
  for (size_t i = 0; i < providers.size(); ++i) {
    ranges[i]               = scene.far_range;
    constraints[i]          = scene.far_constraint;
    constraints[i].radius   = &ranges[i];
    providers[i]            = scene.far_provider;
    providers[i].constraint = &constraints[i];
    run(providers[i]);
  }
  Require(native_interior_radius.size() == kInteriorProviderLimit && ranges.back().maximum == 1000,
          "bounded live owner admission leaves the seventeenth native range untouched");
  config.graphics.interior_zoom_scale = std::numeric_limits<float>::quiet_NaN();
  run(replacement);
  Require(std::ranges::all_of(ranges, [](const auto& range) { return range.maximum == 1000; })
              && native_interior_radius.empty() && warp_roots.size() == roots,
          "invalid live scale restores every owned range and releases all weak handles");
  bindings_config_override = nullptr;
  native_interior_radius.clear();
  std::cout << "Station camera production callback: recreated providers, shared radius, native restoration and disable "
               "passed\n";
}
} // namespace
