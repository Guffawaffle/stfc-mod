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

Il2CppObject   station_zoom_controller{};
FrameProvider* station_zoom_current_provider = nullptr;
bool           station_zoom_instance_failure = false, station_zoom_provider_failure = false;
Il2CppObject*  StationZoomInstance(const MethodInfo*)
{
  if (station_zoom_instance_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return &station_zoom_controller;
}
FrameProvider* StationZoomCurrentProvider(Il2CppObject*, const MethodInfo*)
{
  if (station_zoom_provider_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return station_zoom_current_provider;
}

void StationRadiusCases(std::string_view mode)
{
  HavenFixtureGeometry scene("station-zoom-native");
  auto&                hub      = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto&                sections = Define("Digit.Client.Sections", "SectionManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& getter = AddMethod(hub, "get_SectionManager", reinterpret_cast<void*>(&HavenFixtureSections), T(sections), {},
                           METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "SectionManager", &getter);
  auto& section_getter =
      AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&HavenFixtureSection), T(int_class), {});
  AddProperty(sections, "CurrentSection", &section_getter);
  haven_fixture_sections.klass = &sections.klass;
  auto& controller = Define("Digit.Client.CameraController", "CameraController", IL2CPP_TYPE_CLASS, &component_class);
  auto& provider   = Fake(scene.far_provider.header.klass->parent->parent->parent);
  auto& instance   = AddMethod(controller, "get_Instance", reinterpret_cast<void*>(&StationZoomInstance), T(controller),
                               {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(controller, "Instance", &instance);
  auto& current = AddMethod(controller, "get_CurrentProvider", reinterpret_cast<void*>(&StationZoomCurrentProvider),
                            T(provider), {});
  AddProperty(controller, "CurrentProvider", &current);
  station_zoom_controller.klass = &controller.klass;
  auto* blend                   = reinterpret_cast<BlendFrameProvider*>(&scene.blend);
  station_zoom_current_provider = reinterpret_cast<FrameProvider*>(blend);
  // The measured station endpoints are regular orbits, not Planetary orbits.
  auto* concrete                    = scene.far_provider.header.klass->parent;
  scene.close_provider.header.klass = scene.far_provider.header.klass = concrete;
  scene.close_provider.radius                                         = 600;
  scene.far_provider.radius                                           = 1000;
  Il2CppObject close_pivot{scene.pivot.klass};
  scene.close_provider.pivot = scene.close_provider.look = reinterpret_cast<TargetController*>(&close_pivot);
  Require(scene.close_provider.pivot != scene.far_provider.pivot, "station uses distinct native pivot controllers");
  // Radius limits can be disabled and need not pin the current endpoint radius.
  scene.close_range      = {scene.close_range.header, false, 600, 600};
  scene.far_range        = {scene.far_range.header, false, 1000, 1000};
  scene.source_frame.fov = scene.target_frame.fov = 43;
  scene.blend.source                              = reinterpret_cast<FrameProvider*>(&scene.far_provider);
  scene.blend.target                              = reinterpret_cast<FrameProvider*>(&scene.close_provider);
  scene.blend.minimum                             = .1f;
  scene.blend.maximum                             = .9f;
  scene.blend.ratio                               = .1f;
  haven_fixture_section                           = SectionID::Starbase_Interior;
  auto& config                                    = Config::Get();
  config.graphics.interior_zoom_scale             = 4;
  config.graphics.haven_zoom                      = 4000;
  config.graphics.hide_haven_water                = false;
  bindings_config_override                        = &config;
  interior_zoom_installed                         = true;
  haven_zoom_installed                            = false;
  if (!mode.ends_with("-native")) {
    auto&      fields = mode.ends_with("missing-provider") ? Fake(scene.blend.header.klass).fields
                        : mode.ends_with("missing-radius") ? Fake(concrete->parent).fields
                                                           : Fake(scene.blend.header.klass).fields;
    const auto name   = mode.ends_with("missing-provider") ? "_sourceFrameProvider"
                        : mode.ends_with("missing-radius") ? "_radius"
                        : mode.ends_with("missing-min")    ? "_softLimitMin"
                                                           : "_softLimitMax";
    for (auto& field : fields)
      if (std::string_view(field.name) == name)
        field.name = "_unavailable";
  }
  int                     originals = 0;
  unity::Vector3          native_position{};
  float                   native_clip  = 0;
  const auto              close_enough = [](float a, float b) { return std::abs(a - b) < .03f; };
  const unity::Vector3    far_position{-14.999944f, 766.04443f, 772.7875f};
  const unity::Vector3    close_position{-14.999967f, 459.62665f, 515.6725f};
  const unity::Quaternion far_rotation{-1.8457913e-08f, .9063078f, -.42261833f, -3.9648835e-08f};
  const unity::Quaternion close_rotation{-1.8544275e-08f, .9063078f, -.4226183f, -3.9463625e-08f};
  unity::Vector3          pan{}, close_offset{}, far_offset{};
  const auto              pose = [&](bool is_outer) {
    const auto p      = is_outer ? far_position : close_position;
    const auto offset = is_outer ? far_offset : close_offset;
    return unity::Vector3{p.x + pan.x + offset.x, p.y + pan.y + offset.y, p.z + pan.z + offset.z};
  };
  const auto distance = [&] {
    const auto p = scene.result.position;
    const auto x = double(p.x) - (-15.0 + pan.x), y = double(p.y) - pan.y, z = double(p.z) - (130.0 + pan.z);
    return static_cast<float>(std::sqrt(x * x + y * y + z * z));
  };
  auto run = [&] {
    const auto before = originals;
    BlendFrameProvider_UpdateCameraFrame_Hook(
        [&](BlendFrameProvider* passed, unity::Camera*) {
          Require(passed == blend, "station keeps the native blend receiver");
          ++originals;
          const bool source_far       = scene.blend.source == reinterpret_cast<FrameProvider*>(&scene.far_provider);
          const bool target_far       = scene.blend.target == reinterpret_cast<FrameProvider*>(&scene.far_provider);
          scene.source_frame.position = pose(source_far);
          scene.target_frame.position = pose(target_far);
          scene.source_frame.rotation = source_far ? far_rotation : close_rotation;
          scene.target_frame.rotation = target_far ? far_rotation : close_rotation;
          scene.source_frame.clip     = source_far ? 10217.f : 15010.f;
          scene.target_frame.clip     = target_far ? 10217.f : 15010.f;
          const auto curved           = haven_curve_flat      ? .5f
                                        : haven_curve_squared ? scene.blend.ratio * scene.blend.ratio
                                                              : scene.blend.ratio;
          const auto weight           = std::clamp(curved, 0.f, 1.f);
          // Actual client position math: endpoint lerp after curve evaluation,
          // not a synthetic request that assumes the orbit's radius is driven.
          const auto a = scene.source_frame.position, b = scene.target_frame.position;
          native_position = {a.x + (b.x - a.x) * weight, a.y + (b.y - a.y) * weight, a.z + (b.z - a.z) * weight};
          native_clip     = scene.source_frame.clip + (scene.target_frame.clip - scene.source_frame.clip) * weight;
          scene.result.position = native_position;
          scene.result.clip     = native_clip;
          scene.result.rotation = scene.source_frame.rotation;
          scene.result.fov      = scene.source_frame.fov + (scene.target_frame.fov - scene.source_frame.fov) * weight;
        },
        blend, nullptr);
    Require(originals == before + 1, "every station path calls native generation exactly once");
    Require(scene.close_range.minimum == 600 && scene.close_range.maximum == 600 && !scene.close_range.enabled
                && scene.far_range.minimum == 1000 && scene.far_range.maximum == 1000 && !scene.far_range.enabled
                && scene.close_provider.radius == 600 && scene.far_provider.radius == 1000,
            "station extension never mutates endpoint radii or constraint assets");
    const auto expected_source = pose(scene.blend.source == reinterpret_cast<FrameProvider*>(&scene.far_provider));
    const auto expected_target = pose(scene.blend.target == reinterpret_cast<FrameProvider*>(&scene.far_provider));
    Require(
        scene.source_frame.position.x == expected_source.x && scene.source_frame.position.y == expected_source.y
            && scene.source_frame.position.z == expected_source.z && scene.target_frame.position.x == expected_target.x
            && scene.target_frame.position.y == expected_target.y && scene.target_frame.position.z == expected_target.z,
        "station extension leaves both freshly generated endpoint frames native");
  };
  const auto native = [&] {
    Require(scene.result.position.x == native_position.x && scene.result.position.y == native_position.y
                && scene.result.position.z == native_position.z && scene.result.clip == native_clip,
            "unavailable, disabled or unrelated station geometry retains native output");
  };
  run();
  if (!mode.ends_with("-native")) {
    native();
    bindings_config_override = nullptr;
    return;
  }
  Require(close_enough(distance(), 4000) && close_enough(scene.result.clip - native_clip, 3040),
          "far soft stop actually reaches the requested scaled distance instead of widening an unused bound");
  run();
  Require(close_enough(distance(), 4000) && close_enough(scene.result.clip - native_clip, 3040),
          "fresh native output prevents repeated callbacks from compounding zoom");
  scene.blend.ratio = .9f;
  run();
  native();
  Require(close_enough(distance(), 640), "native closest soft stop remains reachable at nonzero soft limits");
  scene.blend.ratio = 1;
  run();
  native();
  Require(close_enough(distance(), 600), "native hard closest stop remains reachable");
  scene.blend.ratio = 0;
  run();
  Require(close_enough(distance(), 4000) && close_enough(scene.result.clip - native_clip, 3000),
          "native hard far stop extends to the same requested distance");
  scene.blend.ratio = .5f;
  run();
  Require(close_enough(distance(), 2320) && close_enough(scene.result.clip - native_clip, 1520),
          "midpoint scales continuously between native near and the extended far stop");
  scene.blend.source = reinterpret_cast<FrameProvider*>(&scene.close_provider);
  scene.blend.target = reinterpret_cast<FrameProvider*>(&scene.far_provider);
  scene.blend.ratio  = .1f;
  run();
  native();
  scene.blend.ratio = .9f;
  run();
  Require(close_enough(distance(), 4000) && close_enough(scene.result.clip - native_clip, 3040),
          "reversed endpoint order reaches the same extended far distance");
  scene.blend.ratio = .5f;
  run();
  Require(close_enough(distance(), 2320), "reversed midpoint has the same continuous expansion");
  scene.blend.ratio = 0;
  run();
  native();
  Require(close_enough(distance(), 600), "reversed hard closest stop remains native");
  scene.blend.ratio = 1;
  run();
  Require(close_enough(distance(), 4000), "reversed hard far stop reaches the requested distance");
  const auto unpanned = scene.result.position;
  pan                 = {355, 0, 295};
  run();
  Require(close_enough(distance(), 4000) && close_enough(scene.result.position.x, unpanned.x + 355)
              && close_enough(scene.result.position.y, unpanned.y)
              && close_enough(scene.result.position.z, unpanned.z + 295),
          "translated station pivots preserve the zoom reach and native pan displacement");
  pan = {};
  const std::array endpoint_pans{std::pair{unity::Vector3{50, 15, -35}, unity::Vector3{}},
                                 std::pair{unity::Vector3{}, unity::Vector3{-80, -25, 60}},
                                 std::pair{unity::Vector3{50, 15, -35}, unity::Vector3{-80, -25, 60}}};
  for (const bool source_is_outer : {true, false}) {
    scene.blend.source =
        reinterpret_cast<FrameProvider*>(source_is_outer ? &scene.far_provider : &scene.close_provider);
    scene.blend.target =
        reinterpret_cast<FrameProvider*>(source_is_outer ? &scene.close_provider : &scene.far_provider);
    const float inner_ratio = source_is_outer ? .9f : .1f, outer_ratio = source_is_outer ? .1f : .9f;
    const float inner_hard = source_is_outer ? 1.f : 0.f, outer_hard = source_is_outer ? 0.f : 1.f;
    for (const float ratio : {inner_hard, inner_ratio, .5f, outer_ratio, outer_hard}) {
      scene.blend.ratio = ratio;
      close_offset = far_offset = {};
      run();
      // Establish the physical zoom reach only before applying independent
      // anchors. Once they move, one common world pivot is no longer the model.
      Require(close_enough(distance(), ratio == inner_hard    ? 600.f
                                       : ratio == inner_ratio ? 640.f
                                       : ratio == .5f         ? 2320.f
                                                              : 4000.f),
              "unpanned near/mid/far baseline remains qualified in either endpoint order");
      if (ratio == inner_ratio || ratio == inner_hard)
        native();
      const auto baseline_output = scene.result.position, baseline_native = native_position;
      const auto baseline_clip = scene.result.clip;
      for (const auto& [inner_pan, outer_pan] : endpoint_pans) {
        close_offset = inner_pan;
        far_offset   = outer_pan;
        run();
        const auto translation =
            unity::Vector3{native_position.x - baseline_native.x, native_position.y - baseline_native.y,
                           native_position.z - baseline_native.z};
        Require(close_enough(scene.result.position.x - baseline_output.x, translation.x)
                    && close_enough(scene.result.position.y - baseline_output.y, translation.y)
                    && close_enough(scene.result.position.z - baseline_output.z, translation.z)
                    && close_enough(scene.result.clip, baseline_clip),
                "one or both independently panned endpoints preserve the exact native blended translation and zoom");
        if (ratio == inner_ratio || ratio == inner_hard)
          native();
        const auto first_output = scene.result.position;
        run();
        Require(close_enough(scene.result.position.x, first_output.x)
                    && close_enough(scene.result.position.y, first_output.y)
                    && close_enough(scene.result.position.z, first_output.z)
                    && close_enough(scene.result.clip, baseline_clip),
                "independent native pan cannot compound zoom on repeated callbacks");
        close_offset = far_offset = {};
        run();
        Require(close_enough(scene.result.position.x, baseline_output.x)
                    && close_enough(scene.result.position.y, baseline_output.y)
                    && close_enough(scene.result.position.z, baseline_output.z)
                    && close_enough(scene.result.clip, baseline_clip),
                "clearing independent pan returns to the verified zoom baseline without recovery toggles");
      }
    }
  }
  haven_curve_squared = true;
  scene.blend.ratio   = .1f;
  run();
  native();
  scene.blend.ratio = .9f;
  run();
  Require(close_enough(distance(), 4000) && close_enough(scene.result.clip - native_clip, 3076),
          "nonlinear native curve uses its actual soft-stop weights and still reaches the requested far distance");
  haven_curve_squared                 = false;
  config.graphics.interior_zoom_scale = 1;
  run();
  native();
  config.graphics.interior_zoom_scale = 4;
  haven_fixture_section               = SectionID::Starbase_Exterior;
  run();
  native();
  haven_fixture_section = SectionID::Navigation_System;
  run();
  native();
  haven_fixture_section         = SectionID::Starbase_Interior;
  station_zoom_current_provider = nullptr;
  run();
  native();
  station_zoom_current_provider = reinterpret_cast<FrameProvider*>(&scene.close_provider);
  run();
  native();
  station_zoom_current_provider = reinterpret_cast<FrameProvider*>(blend);
  station_zoom_instance_failure = true;
  run();
  native();
  station_zoom_instance_failure = false;
  station_zoom_provider_failure = true;
  run();
  native();
  station_zoom_provider_failure = false;
  haven_curve_failure           = true;
  run();
  native();
  haven_curve_failure = false;
  haven_curve_flat    = true;
  run();
  native();
  haven_curve_flat                    = false;
  config.graphics.interior_zoom_scale = std::numeric_limits<float>::quiet_NaN();
  run();
  native();
  config.graphics.interior_zoom_scale = 4;
  scene.target_frame.fov              = 42;
  run();
  native();
  scene.target_frame.fov  = 43;
  scene.far_provider.look = nullptr;
  run();
  native();
  scene.far_provider.look = scene.far_provider.pivot;
  interior_zoom_installed = false;
  run();
  native();
  // Exercise the shared output hook with real input state transitions and the measured station endpoints.
  auto& application = Define("UnityEngine", "Application", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(application, "get_isFocused", reinterpret_cast<void*>(&HavenFixtureFocused), T(bool_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(event_system_class, "IsPointerOverGameObject", reinterpret_cast<void*>(&HavenFixtureOverUI), T(bool_class),
            {T(int_class)});
  auto&               tutorial = Define("Digit.Prime.Tutorial", "TutorialManager", IL2CPP_TYPE_CLASS, &object_class);
  static Il2CppObject station_tutorial{};
  station_tutorial.klass = &tutorial.klass;
  auto* get_tutorial     = +[](const MethodInfo*) { return &station_tutorial; };
  auto& tutorial_getter  = AddMethod(hub, "get_TutorialManager", reinterpret_cast<void*>(get_tutorial), T(tutorial), {},
                                     METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "TutorialManager", &tutorial_getter);
  auto* allow_scene = +[](Il2CppObject*, const MethodInfo*) { return true; };
  AddMethod(tutorial, "IsSceneInputAllowed", reinterpret_cast<void*>(allow_scene), T(bool_class), {});
  auto& shortcuts   = Define("Digit.Prime.GameInput", "ShortcutsManager", IL2CPP_TYPE_CLASS, &component_class);
  auto* allow_keys  = +[](const MethodInfo*) { return true; };
  auto& keys_getter = AddMethod(shortcuts, "get_CanUseShortcuts", reinterpret_cast<void*>(allow_keys), T(bool_class),
                                {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(shortcuts, "CanUseShortcuts", &keys_getter);
  selected                             = nullptr;
  has_event_system                     = true;
  interior_zoom_installed              = true;
  config.patches.interiorzoomhooks     = true;
  config.control.hotkeys_enabled       = true;
  config.control.use_scopely_hotkeys   = false;
  config.graphics.station_camera_orbit = true;
  scene.blend.ratio                    = 1;
  input_frame                          = 100;
  auto gesture                         = [&](bool down, float x, float y) {
    ++input_frame;
    haven_fixture_shortcuts::held = true;
    haven_fixture_shortcuts::down = down;
    camera_mouse                  = {x, y, 0};
    run();
  };
  haven_fixture_ui = true;
  gesture(true, 120, 100);
  haven_fixture_ui = false;
  gesture(false, 220, 100);
  Require(!StationCamera().view.Active(), "station orbit cannot start on UI or continue a blocked press");
  gesture(true, 220, 100);
  gesture(false, 500, 150);
  const auto orbit_eye  = scene.result.position;
  const auto orbit_look = scene.result.rotation;
  Require(StationCamera().view.Active() && StationCamera().view.yaw != 0 && close_enough(distance(), 4000)
              && close_enough(scene.result.clip - native_clip, 3000),
          "orbit keeps the verified outer zoom and expanded far clip");

  struct PanFixture {
    Il2CppObject    header;
    PanDriver::Plane plane = PanDriver::Plane::XZ;
  };
  auto& pan_class = Define("Digit.Client.CameraController", "PanDriver", IL2CPP_TYPE_CLASS, &component_class);
  pan_class.klass.instance_size = sizeof(PanFixture);
  AddField(pan_class, "_plane", int_class, offsetof(PanFixture, plane));
  AddField(Fake(concrete->parent), "_targetOffsetDriver", pan_class, offsetof(HavenFixtureProvider, pan));
  AddField(Fake(concrete->parent), "_targetOffsetDelta", vec3_class, offsetof(HavenFixtureProvider, pan_delta));
  PanFixture driver{{&pan_class.klass}};
  auto& view = StationCamera().view;
  const auto native_yaw = view.yaw;
  const auto pan_input = [&](HavenFixtureProvider& endpoint) {
    endpoint.pan_delta = {180, 2, 110};
    ApplyStationInteriorPan(reinterpret_cast<OrbitFrameProvider*>(&endpoint));
    return endpoint.pan_delta;
  };
  scene.far_provider.pan = scene.close_provider.pan = reinterpret_cast<PanDriver*>(&driver);
  view.yaw = 90;
  const auto quarter = pan_input(scene.far_provider);
  Require(close_enough(quarter.x, 110) && quarter.y == 2 && close_enough(quarter.z, -180),
          "quarter turn remaps the fresh pan delta to the heading on both horizontal axes");
  view.yaw = 180;
  const auto half = pan_input(scene.close_provider);
  Require(close_enough(half.x, -180) && half.y == 2 && close_enough(half.z, -110),
          "half turn reverses both native horizontal pan axes");
  const auto unchanged = [](unity::Vector3 delta) { return delta.x == 180 && delta.y == 2 && delta.z == 110; };
  driver.plane = PanDriver::Plane::XY;
  Require(unchanged(pan_input(scene.close_provider)), "other pan planes stay native");
  driver.plane = PanDriver::Plane::XZ;
  auto* active_provider = station_zoom_current_provider;
  station_zoom_current_provider = nullptr;
  Require(unchanged(pan_input(scene.close_provider)), "an inactive station blend cannot redirect pan input");
  station_zoom_current_provider = active_provider;
  view.yaw = 0;
  Require(unchanged(pan_input(scene.close_provider)), "reset heading preserves native pan");
  view.yaw = native_yaw;

  haven_fixture_shortcuts::held = false;
  pan                           = {180, 0, 110};
  ++input_frame;
  run();
  Require(close_enough(scene.result.position.y, orbit_eye.y)
              && close_enough(scene.result.position.x - orbit_eye.x, pan.x)
              && close_enough(scene.result.position.z - orbit_eye.z, pan.z) && close_enough(distance(), 4000)
              && scene.result.rotation.y == orbit_look.y,
          "native pan translates the rotated view without moving its zoom or orientation");
  scene.blend.ratio = 0;
  ++input_frame;
  run();
  Require(close_enough(distance(), 600) && scene.result.clip == native_clip,
          "close-up distance and clipping survive combined orbit and pan without extending the inner endpoint");
  haven_fixture_shortcuts::reset = true;
  ++input_frame;
  run();
  native();
  Require(!StationCamera().view.Active() && close_enough(distance(), 600),
          "reset clears orbit at current zoom and preserves native pan");
  haven_fixture_shortcuts::reset      = false;
  config.graphics.interior_zoom_scale = 1;
  scene.blend.ratio                   = 1;
  gesture(true, 750, 240);
  gesture(false, 820, 245);
  Require(StationCamera().view.Active() && close_enough(distance(), 1000) && scene.result.clip == native_clip,
          "station orbit also works with the native zoom range");
  haven_fixture_section = SectionID::Starbase_Exterior;
  UpdateStationInteriorCamera();
  run();
  native();
  Require(!StationCamera().blend && !StationCamera().view.Active(), "leaving the station forgets the view and gesture");
  haven_fixture_section = SectionID::Starbase_Interior;
  gesture(false, 850, 245);
  native();
  ResetStationInteriorCamera();
  bindings_config_override = nullptr;
  Require(warp_roots.empty(), "stateless zoom retains no managed endpoint or constraint owners");
  std::cout << "Station production blend zoom: native near, actual far reach, both directions, nonlinear soft stops, "
               "active-provider gating and stateless fallback passed\n";
}
} // namespace
