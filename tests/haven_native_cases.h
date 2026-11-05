#pragma once

// Actual Haven camera/history consumers against the existing controlled runtime.
// Orbit substitutes only shortcut input; scene/UI/constraint observations use the
// production typed adapters. No installed detour, rendered image or player claim.
#include "../src/mod/features/haven.cc"

namespace haven_fixture_shortcuts
{
inline bool bound = true, held = false, down = false, reset = false;
inline bool HasBinding(Action)
{ return bound; }
inline bool IsPressed(Action action)
{ return action == Action::kHavenOrbitDrag && held; }
inline bool IsDown(Action action)
{ return action == Action::kHavenOrbitDrag ? down : action == Action::kHavenOrbitReset && reset; }
} // namespace haven_fixture_shortcuts
namespace haven_fixture_orbit
{
void ApplyConstraints(OrbitFrameProvider* provider);
}
// The camera fixture retains its named Haven sink. Rename this export only so
// these cases can exercise the actual separate consumer in the same test TU.
#define haven_orbit haven_fixture_orbit
#define Shortcut haven_fixture_shortcuts
#include "../src/mod/features/haven_orbit.cc"
#undef Shortcut
#undef haven_orbit

namespace
{
bool HavenFixtureRooted(const void* object)
{
  return std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; });
}

struct HavenHistoryList {
  Il2CppObject               header;
  std::vector<Il2CppObject*> records;
};
std::vector<Il2CppObject*> haven_history_original;
int     haven_history_get_failure = -1, haven_history_set_failure = -1, haven_history_root_failure = -1;
int     haven_history_gets = 0, haven_history_sets = 0;
bool    haven_history_custody = true, haven_history_count_failure = false;
int32_t HavenHistoryCount(HavenHistoryList* list, const MethodInfo*)
{
  if (haven_history_count_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return static_cast<int32_t>(list->records.size());
}
Il2CppObject* HavenHistoryItem(HavenHistoryList* list, int32_t index, const MethodInfo*)
{
  ++haven_history_gets;
  if (index == haven_history_get_failure)
    throw Il2CppExceptionWrapper(&thrown);
  if (index == haven_history_root_failure)
    warp_root_failure = true;
  return list->records.at(index);
}
void HavenHistorySet(HavenHistoryList* list, int32_t index, Il2CppObject* record, const MethodInfo*)
{
  ++haven_history_sets;
  haven_history_custody &= HavenFixtureRooted(list) && haven_history_gets == list->records.size();
  for (auto* original : haven_history_original)
    haven_history_custody &= !original || HavenFixtureRooted(original);
  if (index == haven_history_set_failure)
    throw Il2CppExceptionWrapper(&thrown);
  list->records.at(index) = record;
}
void HavenHistoryCases()
{
  auto& list = Define("HavenFixture", "Records", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&HavenHistoryCount), T(int_class), {});
  AddMethod(list, "get_Item", reinterpret_cast<void*>(&HavenHistoryItem), T(object_class), {T(int_class)});
  AddMethod(list, "set_Item", reinterpret_cast<void*>(&HavenHistorySet), T(void_class),
            {T(int_class), T(object_class)});
  Il2CppObject     a{&object_class.klass}, b{&object_class.klass}, c{&object_class.klass};
  HavenHistoryList records{{&list.klass}};
  int              originals = 0;
  auto*            observed  = reinterpret_cast<il2cpp::List<Il2CppObject*>*>(&records);
  auto             run       = [&](std::vector<Il2CppObject*> items) {
    records.records = haven_history_original = std::move(items);
    haven_history_gets = haven_history_sets = 0;
    haven_history_custody                   = true;
    PlanetaryHelpHistoryContext_ApplyRecords_Hook(
        [&](auto*, auto* passed) {
          ++originals;
          Require(passed == observed, "history delivers the original list identity exactly once");
        },
        static_cast<PlanetaryHelpHistoryContext*>(nullptr), observed);
    Require(haven_history_custody && warp_roots.empty(), "history roots all original rows across every setter");
  };
  Config::Get().ui.reverse_haven_history = true;
  run({});
  run({&a});
  Require(haven_history_sets == 0, "empty and singleton history need no managed writes");
  run({&a, nullptr, &b, &c});
  Require(records.records == std::vector<Il2CppObject*>{&c, &b, nullptr, &a} && haven_history_sets == 4,
          "actual history consumer reverses many rows and retains a known null");
  Config::Get().ui.reverse_haven_history = false;
  run({&a, &b, &c});
  Require(records.records == haven_history_original && haven_history_gets == 0 && haven_history_sets == 0,
          "disabled history preserves native order without reading records");
  Config::Get().ui.reverse_haven_history = true;
  haven_history_get_failure              = 1;
  run({&a, &b, &c});
  Require(records.records == haven_history_original && haven_history_sets == 0,
          "failed history acquisition cannot publish a null or a partially acquired reversal");
  haven_history_get_failure   = -1;
  haven_history_count_failure = true;
  run({&a, &b, &c});
  Require(haven_history_gets == 0 && haven_history_sets == 0, "unavailable count preserves native delivery");
  haven_history_count_failure = false;
  haven_history_root_failure  = 1;
  run({&a, &b, &c});
  warp_root_failure = false;
  Require(records.records == haven_history_original && haven_history_sets == 0,
          "failed row custody cannot start history mutation");
  haven_history_root_failure = -1;
  haven_history_set_failure  = 1;
  run({&a, &b, &c});
  Require(records.records == std::vector<Il2CppObject*>{&c, &b, &c} && haven_history_sets == 2,
          "setter failure stops the source's forward reversal and still calls native once");
  haven_history_set_failure = -1;
  auto many                 = std::vector<Il2CppObject*>(2001, &a);
  many.back()               = &c;
  run(many);
  Require(records.records.front() == &c && records.records.back() == &a && haven_history_sets == 2001 && originals == 9,
          "valid game history is not silently truncated at the source's arbitrary admission cap");
  std::cout << "Haven production history: snapshot custody, known null, failure and disabled paths passed\n";
}

struct HavenFixtureRange {
  Il2CppObject header;
  bool         enabled = true;
  float        minimum = 0, maximum = 0;
};
struct HavenFixtureConstraint {
  Il2CppObject       header;
  HavenFixtureRange* radius    = nullptr;
  HavenFixtureRange* elevation = nullptr;
};
struct HavenFixtureProvider {
  Il2CppObject            header;
  HavenFixtureConstraint* constraint = nullptr;
  float                   radius = 0, yaw = 20, pitch = 45;
  TargetController*       pivot = nullptr;
  TargetController*       look  = nullptr;
};
struct HavenFixtureFrame {
  Il2CppObject      header;
  unity::Vector3    position{10, 20, 30};
  unity::Quaternion rotation{0, 0, 0, 1};
  float             fov = 60, clip = 500;
  bool              orthographic = false;
};
struct HavenFixtureBlend {
  Il2CppObject           header;
  FrameProvider*         source       = nullptr;
  FrameProvider*         target       = nullptr;
  CameraFrame*           source_frame = nullptr;
  CameraFrame*           target_frame = nullptr;
  EditableCameraFrame*   result       = nullptr;
  unity::AnimationCurve* curve        = nullptr;
  float                  minimum = 0, maximum = 1, ratio = 1;
};
bool  haven_curve_failure = false, haven_curve_flat = false;
float HavenFixtureEvaluate(Il2CppObject*, float value, const MethodInfo*)
{
  if (haven_curve_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return haven_curve_flat ? 0.5f : value;
}

struct HavenFixtureGeometry {
  HavenFixtureRange      close_range{}, far_range{}, pitch_range{};
  HavenFixtureConstraint close_constraint{}, far_constraint{};
  HavenFixtureProvider   close_provider{}, far_provider{};
  HavenFixtureFrame      source_frame{}, target_frame{}, result{};
  HavenFixtureBlend      blend{};
  Il2CppObject           pivot{}, curve{};

  explicit HavenFixtureGeometry(std::string_view mode)
  {
    auto& provider = Define("Digit.Client.CameraController", "FrameProvider", IL2CPP_TYPE_CLASS, &component_class);
    auto& orbit = Define("Digit.Client.CameraController", "AbstractOrbitFrameProvider", IL2CPP_TYPE_CLASS, &provider);
    auto& concrete = Define("Digit.Client.CameraController", "OrbitFrameProvider", IL2CPP_TYPE_CLASS, &orbit);
    auto& planetary =
        Define("Digit.Client.CameraController", "PlanetaryOrbitFrameProvider", IL2CPP_TYPE_CLASS, &concrete);
    auto& target =
        Define("Digit.Client.CameraController", "AbstractTargetController", IL2CPP_TYPE_CLASS, &component_class);
    auto& constraint = Define("Digit.Client.CameraController", "OrbitConstraint", IL2CPP_TYPE_CLASS, &object_class);
    auto& base_constraint     = Define("Digit.Client.CameraController", "Constraint", IL2CPP_TYPE_CLASS, &object_class);
    auto& range               = Define("", "FloatData", IL2CPP_TYPE_CLASS, &object_class);
    range.klass.declaringType = &base_constraint.klass;
    base_constraint.nested.push_back(&range.klass);
    auto& frame       = Define("Digit.Client.CameraController", "CameraFrame", IL2CPP_TYPE_CLASS, &object_class);
    auto& editable    = Define("Digit.Client.CameraController", "EditableCameraFrame", IL2CPP_TYPE_CLASS, &frame);
    auto& blend_class = Define("Digit.Client.CameraController", "BlendFrameProvider", IL2CPP_TYPE_CLASS, &provider);
    auto& animation   = Define("UnityEngine", "AnimationCurve", IL2CPP_TYPE_CLASS, &object_class);
    auto& rotation    = Define("UnityEngine", "Quaternion", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Quaternion));
    auto& foreign_position =
        Define("HavenFixture", "ForeignPosition", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector3));
    auto& foreign_rotation =
        Define("HavenFixture", "ForeignRotation", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Quaternion));
    for (auto* item : {&provider, &orbit, &concrete, &planetary})
      item->klass.instance_size = sizeof(HavenFixtureProvider);
    for (auto* item : {&frame, &editable})
      item->klass.instance_size = sizeof(HavenFixtureFrame);
    range.klass.instance_size       = sizeof(HavenFixtureRange);
    constraint.klass.instance_size  = sizeof(HavenFixtureConstraint);
    blend_class.klass.instance_size = sizeof(HavenFixtureBlend);
    AddField(range, "_enabled", bool_class, offsetof(HavenFixtureRange, enabled));
    AddField(range, "_min", float_class, offsetof(HavenFixtureRange, minimum));
    AddField(range, "_max", float_class, offsetof(HavenFixtureRange, maximum));
    AddField(constraint, "_radius", range, offsetof(HavenFixtureConstraint, radius));
    AddField(constraint, "_elevation", range, offsetof(HavenFixtureConstraint, elevation));
    AddField(orbit, "_constraint", constraint, offsetof(HavenFixtureProvider, constraint));
    AddField(orbit, "_radius", float_class, offsetof(HavenFixtureProvider, radius));
    AddField(orbit, "_rotationAngle", float_class, offsetof(HavenFixtureProvider, yaw));
    AddField(orbit, "_elevationAngle", float_class, offsetof(HavenFixtureProvider, pitch));
    AddField(orbit, "_orbitPivot", target, offsetof(HavenFixtureProvider, pivot));
    AddField(orbit, "_lookTarget", target, offsetof(HavenFixtureProvider, look));
    auto& position_type = mode == "haven-zoom-missing-position"   ? int_class
                          : mode == "haven-zoom-foreign-position" ? foreign_position
                                                                  : vec3_class;
    AddField(frame, "_position", position_type, offsetof(HavenFixtureFrame, position));
    AddField(frame, "_rotation", mode == "haven-zoom-foreign-rotation" ? foreign_rotation : rotation,
             offsetof(HavenFixtureFrame, rotation));
    AddField(frame, "_FOV", float_class, offsetof(HavenFixtureFrame, fov));
    AddField(frame, "_farPlane", float_class, offsetof(HavenFixtureFrame, clip));
    AddField(frame, "_orthographic", mode == "haven-zoom-missing-orthographic" ? int_class : bool_class,
             offsetof(HavenFixtureFrame, orthographic));
    AddField(blend_class, "_sourceFrameProvider", mode == "haven-zoom-widened-provider" ? object_class : provider,
             offsetof(HavenFixtureBlend, source));
    AddField(blend_class, "_targetFrameProvider", provider, offsetof(HavenFixtureBlend, target));
    AddField(blend_class, "_sourceFrame", mode == "haven-zoom-widened-frame" ? object_class : frame,
             offsetof(HavenFixtureBlend, source_frame));
    AddField(blend_class, "_targetFrame", frame, offsetof(HavenFixtureBlend, target_frame));
    AddField(blend_class, "_blendedCameraFrame", mode == "haven-zoom-widened-output-frame" ? frame : editable,
             offsetof(HavenFixtureBlend, result));
    AddField(blend_class, "_blendCurve", animation, offsetof(HavenFixtureBlend, curve));
    AddField(blend_class, "_softLimitMin", float_class, offsetof(HavenFixtureBlend, minimum));
    AddField(blend_class, "_softLimitMax", float_class, offsetof(HavenFixtureBlend, maximum));
    AddField(blend_class, "_blendRatio", float_class, offsetof(HavenFixtureBlend, ratio));
    AddMethod(animation, "Evaluate", reinterpret_cast<void*>(&HavenFixtureEvaluate), T(float_class), {T(float_class)});
    close_range               = {{&range.klass}, true, 100, 100};
    far_range                 = {{&range.klass}, true, 1000, 1000};
    pitch_range               = {{&range.klass}, true, 45, 45};
    close_constraint          = {{&constraint.klass}, &close_range, &pitch_range};
    far_constraint            = {{&constraint.klass}, &far_range, &pitch_range};
    pivot.klass               = &target.klass;
    curve.klass               = &animation.klass;
    auto* pivot_object        = reinterpret_cast<TargetController*>(&pivot);
    close_provider            = {{&planetary.klass}, &close_constraint, 100, 20, 45, pivot_object, pivot_object};
    far_provider              = {{&planetary.klass}, &far_constraint, 1000, 20, 45, pivot_object, pivot_object};
    source_frame.header.klass = target_frame.header.klass = &frame.klass;
    result.header.klass                                   = &editable.klass;
    blend                                                 = {{&blend_class.klass},
                                                             reinterpret_cast<FrameProvider*>(&close_provider),
                                                             reinterpret_cast<FrameProvider*>(&far_provider),
                                                             reinterpret_cast<CameraFrame*>(&source_frame),
                                                             reinterpret_cast<CameraFrame*>(&target_frame),
                                                             reinterpret_cast<EditableCameraFrame*>(&result),
                                                             reinterpret_cast<unity::AnimationCurve*>(&curve)};
  }
};

void HavenZoomCases(std::string_view mode)
{
  HavenFixtureGeometry scene(mode);
  auto*                blend              = reinterpret_cast<BlendFrameProvider*>(&scene.blend);
  Config::Get().graphics.haven_zoom       = 4000;
  Config::Get().graphics.hide_haven_water = false;
  int  originals                          = 0;
  auto run                                = [&] {
    scene.result.position = {10, 20, 30};
    scene.result.clip     = 500;
    BlendFrameProvider_UpdateCameraFrame_Hook([&](auto*, auto*) { ++originals; }, blend, nullptr);
  };
  run();
  if (mode != "haven-zoom-native") {
    Require(scene.result.position.z == 30 && scene.result.clip == 500 && originals == 1,
            "unavailable or foreign geometry declarations cannot authorize camera geometry writes");
    return;
  }
  Require(scene.result.position.x == 10 && scene.result.position.y == 20 && scene.result.position.z == -2970
              && scene.result.clip == 3500 && scene.far_provider.radius == 1000 && scene.far_range.maximum == 1000,
          "actual camera consumer extends only blended output and far plane at the far stop");
  scene.blend.source = reinterpret_cast<FrameProvider*>(&scene.far_provider);
  scene.blend.target = reinterpret_cast<FrameProvider*>(&scene.close_provider);
  scene.blend.ratio  = 0;
  run();
  Require(scene.result.position.z == -2970 && scene.result.clip == 3500,
          "opposite blend direction preserves the same outer-distance formula");
  scene.blend.ratio = 1;
  run();
  Require(scene.result.position.z == 30 && scene.result.clip == 500, "the native inner stop stays native");
  scene.blend.ratio   = 0;
  haven_curve_failure = true;
  run();
  Require(scene.result.position.z == 30 && scene.result.clip == 500,
          "failed curve evaluation stays unavailable rather than becoming an outer-stop zero");
  haven_curve_failure = false;
  haven_curve_flat    = true;
  run();
  Require(scene.result.position.z == 30 && scene.result.clip == 500, "flat curves retain native output");
  haven_curve_flat       = false;
  scene.target_frame.fov = 59;
  run();
  Require(scene.result.position.z == 30, "mismatched endpoint optics keep the game's output");
  scene.target_frame.fov      = 60;
  scene.target_frame.rotation = {1, 0, 0, 1};
  run();
  Require(scene.result.position.z == 30 && scene.result.clip == 500,
          "nonunit target quaternion cannot pass merely because its dot product with the source is one");
  scene.target_frame.rotation = {0, 0, 0, 1};
  scene.far_provider.look     = nullptr;
  run();
  Require(scene.result.position.z == 30, "unqualified look target cannot widen an unrelated camera");
  scene.far_provider.look           = scene.close_provider.look;
  Config::Get().graphics.haven_zoom = 0;
  run();
  Require(scene.result.position.z == 30 && scene.result.clip == 500 && originals == 9 && warp_roots.empty(),
          "native-zoom setting leaves geometry intact and every path delivers native exactly once");
  std::cout << "Haven production zoom: both blend directions, native geometry and checked fallback passed\n";
}

Il2CppObject  haven_fixture_sections{}, haven_fixture_starbase{};
SectionID     haven_fixture_section = SectionID::Starbase_Planetary;
bool          haven_fixture_focused = true, haven_fixture_ui = false, haven_fixture_placement = false;
bool          haven_fixture_pointer_failure = false;
Il2CppObject* HavenFixtureSections(const MethodInfo*)
{ return &haven_fixture_sections; }
SectionID HavenFixtureSection(Il2CppObject*, const MethodInfo*)
{ return haven_fixture_section; }
Il2CppObject* HavenFixtureStarbase(const MethodInfo*)
{ return &haven_fixture_starbase; }
bool HavenFixtureFocused(const MethodInfo*)
{ return haven_fixture_focused; }
bool HavenFixturePlacement(Il2CppObject*, const MethodInfo*)
{ return haven_fixture_placement; }
bool HavenFixtureOverUI(Il2CppObject*, int32_t pointer, const MethodInfo*)
{
  Require(pointer == -1, "orbit queries the native mouse pointer identity");
  if (haven_fixture_pointer_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return haven_fixture_ui;
}
void HavenOrbitCases()
{
  HavenFixtureGeometry scene("haven-orbit-native");
  auto&                hub      = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto&                sections = Define("Digit.Client.Sections", "SectionManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& starbase = Define("Digit.Prime.PlanetaryBase.PlanetaryStarbase", "PlanetaryStarbaseManager", IL2CPP_TYPE_CLASS,
                          &object_class);
  auto& application  = Define("UnityEngine", "Application", IL2CPP_TYPE_CLASS, &object_class);
  auto& get_sections = AddMethod(hub, "get_SectionManager", reinterpret_cast<void*>(&HavenFixtureSections), T(sections),
                                 {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "SectionManager", &get_sections);
  AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&HavenFixtureSection), T(int_class), {});
  AddMethod(starbase, "get_Instance", reinterpret_cast<void*>(&HavenFixtureStarbase), T(starbase), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(starbase, "get_HasActivePlacement", reinterpret_cast<void*>(&HavenFixturePlacement), T(bool_class), {});
  AddMethod(application, "get_isFocused", reinterpret_cast<void*>(&HavenFixtureFocused), T(bool_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(event_system_class, "IsPointerOverGameObject", reinterpret_cast<void*>(&HavenFixtureOverUI), T(bool_class),
            {T(int_class)});
  haven_fixture_sections.klass       = &sections.klass;
  haven_fixture_starbase.klass       = &starbase.klass;
  auto* blend                        = reinterpret_cast<BlendFrameProvider*>(&scene.blend);
  auto* close_provider               = reinterpret_cast<OrbitFrameProvider*>(&scene.close_provider);
  auto* far_provider                 = reinterpret_cast<OrbitFrameProvider*>(&scene.far_provider);
  auto& config                       = Config::Get();
  config.control.hotkeys_enabled     = true;
  config.control.use_scopely_hotkeys = false;
  config.patches.havenzoomhooks      = true;
  HavenOrbit().installed             = true; // Consumer qualification only, never an installer claim.
  selected                           = nullptr;
  has_event_system                   = true;
  input_frame                        = 100;
  camera_mouse                       = {10, 20, 0};
  int  originals                     = 0;
  auto run                           = [&] {
    ++input_frame;
    BlendFrameProvider_UpdateInputData_HavenOrbitHook(
        [&](auto*, auto*) {
          ++originals;
          haven_fixture_orbit::ApplyConstraints(close_provider);
          haven_fixture_orbit::ApplyConstraints(far_provider);
        },
        blend, nullptr);
    haven_fixture_shortcuts::down = haven_fixture_shortcuts::reset = false;
  };
  haven_fixture_shortcuts::held = true;
  run();
  camera_mouse = {110, 120, 0};
  run();
  Require(scene.close_provider.yaw == 20 && scene.close_provider.pitch == 45 && !HavenOrbit().dragging,
          "a hold already active on Haven entry cannot start rotation");
  haven_fixture_shortcuts::down = true;
  run();
  camera_mouse     = {210, 220, 0};
  haven_fixture_ui = true;
  run();
  Require(HavenOrbitNear(scene.close_provider.yaw, 35) && HavenOrbitNear(scene.close_provider.pitch, 30)
              && scene.far_provider.yaw == scene.close_provider.yaw
              && scene.far_provider.pitch == scene.close_provider.pitch,
          "actual orbit consumer retains a map-start drag across UI badges and updates both endpoints");
  haven_fixture_placement = true;
  camera_mouse            = {310, 320, 0};
  run();
  Require(!HavenOrbit().dragging && HavenOrbitNear(scene.close_provider.yaw, 35),
          "placement cancels ongoing gesture deltas");
  haven_fixture_placement = false;
  haven_fixture_ui        = false;
  run();
  Require(!HavenOrbit().dragging && HavenOrbitNear(scene.close_provider.yaw, 35),
          "cancelled drag requires a fresh press");
  haven_fixture_shortcuts::reset = true;
  run();
  Require(scene.close_provider.yaw == 20 && scene.close_provider.pitch == 45 && !HavenOrbit().dragging,
          "modifier reset wins a simultaneous drag hold and keeps native pitch/yaw");
  haven_fixture_shortcuts::down = true;
  run();
  camera_mouse = {310, -10000, 0};
  run();
  Require(scene.close_provider.pitch == 90, "tilt clamps at straight down without inverting the map");
  haven_fixture_pointer_failure = true;
  run();
  Require(!HavenOrbit().dragging && scene.close_provider.pitch == 90,
          "unknown pointer ownership cannot continue a gesture");
  haven_fixture_pointer_failure  = false;
  scene.close_provider.yaw       = 777; // Another owner superseded one axis after our last write.
  config.control.hotkeys_enabled = false;
  HavenOrbitFrame();
  Require(scene.close_provider.yaw == 777 && scene.close_provider.pitch == 45 && scene.far_provider.yaw == 20
              && scene.far_provider.pitch == 45 && !HavenOrbit().blend && warp_roots.empty(),
          "disable restores only still-owned axes and releases the exact blend/endpoints");
  config.control.hotkeys_enabled = true;
  scene.close_provider.yaw       = 20;
  haven_fixture_shortcuts::held = haven_fixture_shortcuts::down = true;
  run();
  camera_mouse.x += 100;
  run();
  haven_fixture_section = SectionID::Navigation_System;
  HavenOrbitFrame();
  Require(scene.close_provider.yaw == 20 && scene.far_provider.yaw == 20 && !HavenOrbit().blend && warp_roots.empty(),
          "leaving Haven drains orientation ownership without needing another camera callback");
  Require(!HavenOrbit().scoped_source && !HavenOrbit().scoped_target && originals == 12,
          "orbit scope clears after every native original call");
  std::cout << "Haven production orbit: gesture start/continue/reset gates and exact-owner restoration passed\n";
}

void HavenNativeCases(std::string_view mode)
{
  if (mode == "haven-history-native")
    HavenHistoryCases();
  else if (mode == "haven-orbit-native")
    HavenOrbitCases();
  else
    HavenZoomCases(mode);
}
} // namespace
