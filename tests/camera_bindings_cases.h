#pragma once

// The camera callbacks below are production code. Only the native UI ownership
// query is substituted; this does not qualify Unity raycast sorting or handlers.
#include "game/scroll_input.h"
namespace camera_fixture_scroll
{
using WheelRecognizer = scroll_input::WheelRecognizer;
inline il2cpp::CallResult<bool> owned{true, false}, pinch{true, false};
inline int                      queries = 0;
inline bool                     Available()
{ return true; }
inline il2cpp::CallResult<bool> PointerOwnsScroll()
{
  ++queries;
  return owned;
}
inline il2cpp::CallResult<bool> PinchStarted(NavigationZoom*)
{ return pinch; }
} // namespace camera_fixture_scroll
#define scroll_input camera_fixture_scroll
#include "../src/mod/features/camera.cc"
#undef scroll_input

namespace
{
struct CameraNativeObject {
  Il2CppObject   header;
  float          distance = 30.0f, minimum = 10.0f, maximum = 110.0f, radius = 500.0f;
  NodeDepth      depth  = NodeDepth::SolarSystem;
  unity::Camera* camera = nullptr;
  float          delta = 3.0f, last = 3.0f, normal = 0.0f, extended = 0.0f, total = 100.0f;
  unity::Vector2 location{};
  unity::Vector3 world{};
};
struct CameraNativeScene {
  Il2CppObject              header;
  float                     clip  = 0.0f;
  unity::Camera::ClearFlags flags = unity::Camera::ClearFlags::Skybox;
  unity::Color              background{1, 2, 3, 4};
};
int     camera_native_distance_sets = 0, camera_native_zoom_calls = 0;
int     camera_native_clip_gets = 0, camera_native_clip_sets = 0, camera_native_flags_sets = 0,
        camera_native_background_gets = 0, camera_native_background_sets = 0, camera_native_boxed_calls = 0;
bool    camera_native_distance_throws = false, camera_native_set_throws = false, camera_native_zoom_throws = false;
bool    camera_native_clip_get_throws = false, camera_native_flags_throws = false, camera_native_background_throws = false;
bool    camera_native_flags_replace_config = false;
NavigationZoom* camera_native_flags_rebind_owner = nullptr;
bool    camera_native_background_get_throws = false, camera_native_background_get_replace_config = false;
NavigationZoom* camera_native_background_get_rebind_owner = nullptr;
bool    camera_native_replace_config      = false;
Config* camera_replacement_config         = nullptr;
unity::Camera* camera_native_rebind_scene = nullptr;
float          CameraNativeDistance(NavigationZoom* zoom, const MethodInfo*)
{
  if (camera_native_distance_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (camera_native_replace_config) {
    camera_native_replace_config = false;
    bindings_replace_config      = true;
    camera_replacement_config    = &Config::Get();
  }
  return reinterpret_cast<CameraNativeObject*>(zoom)->distance;
}
void CameraNativeSetDistance(NavigationZoom* zoom, float value, const MethodInfo*)
{
  if (camera_native_set_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++camera_native_distance_sets;
  reinterpret_cast<CameraNativeObject*>(zoom)->distance = value;
  if (camera_native_rebind_scene)
    reinterpret_cast<CameraNativeObject*>(zoom)->camera = camera_native_rebind_scene;
}
void CameraNativeZoom(NavigationZoom*, const MethodInfo*)
{
  if (camera_native_zoom_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++camera_native_zoom_calls;
}
float CameraNativeGetClip(unity::Camera* scene, const MethodInfo*)
{
  ++camera_native_clip_gets;
  if (camera_native_clip_get_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<CameraNativeScene*>(scene)->clip;
}
void CameraNativeClip(unity::Camera* scene, float value, const MethodInfo*)
{
  ++camera_native_clip_sets;
  reinterpret_cast<CameraNativeScene*>(scene)->clip = value;
}
void CameraNativeFlags(unity::Camera* scene, unity::Camera::ClearFlags value, const MethodInfo*)
{
  ++camera_native_flags_sets;
  if (camera_native_flags_throws)
    throw Il2CppExceptionWrapper(&thrown);
  reinterpret_cast<CameraNativeScene*>(scene)->flags = value;
  if (camera_native_flags_rebind_owner)
    reinterpret_cast<CameraNativeObject*>(camera_native_flags_rebind_owner)->camera = camera_native_rebind_scene;
  if (std::exchange(camera_native_flags_replace_config, false)) {
    bindings_replace_config   = true;
    camera_replacement_config = &Config::Get();
    camera_replacement_config->graphics.zoom = 1200.0f;
  }
}
unity::Camera::ClearFlags CameraNativeGetFlags(unity::Camera* scene, const MethodInfo*)
{ return reinterpret_cast<CameraNativeScene*>(scene)->flags; }
unity::Color CameraNativeGetBackground(unity::Camera* scene, const MethodInfo*)
{
  ++camera_native_background_gets;
  if (camera_native_background_get_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (camera_native_background_get_rebind_owner)
    reinterpret_cast<CameraNativeObject*>(camera_native_background_get_rebind_owner)->camera = camera_native_rebind_scene;
  if (std::exchange(camera_native_background_get_replace_config, false)) {
    bindings_replace_config = true;
    camera_replacement_config = &Config::Get();
    camera_replacement_config->graphics.zoom = 1200.0f;
  }
  return reinterpret_cast<CameraNativeScene*>(scene)->background;
}
Il2CppObject* CameraNativeObserveBoxed(const MethodInfo*, void*, void**, Il2CppException**, bool& handled)
{
  ++camera_native_boxed_calls;
  handled = false;
  return nullptr;
}
void CameraNativeBackground(unity::Camera* scene, unity::Color value, const MethodInfo*)
{
  ++camera_native_background_sets;
  if (camera_native_background_throws)
    throw Il2CppExceptionWrapper(&thrown);
  reinterpret_cast<CameraNativeScene*>(scene)->background = value;
}
unity::Vector3 CameraNativeWorld(unity::Camera*, unity::Vector3, const MethodInfo*)
{ return {}; } // The controlled runtime's boxed route validates arguments.

void CameraNativeCases(std::string_view mode)
{
  auto& zoom_class  = Define("Digit.Prime.Navigation", "NavigationZoom", IL2CPP_TYPE_CLASS, &object_class);
  auto& scene_class = Define("UnityEngine", "Camera", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& vector2     = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& color       = Define("UnityEngine", "Color", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Color));
  auto& depth       = Define("Digit.Prime.Navigation", "NodeDepth", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(NodeDepth));
  auto& flags =
      Define("UnityEngine", "CameraClearFlags", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Camera::ClearFlags));
  auto& maths   = Define("Digit.Client.Core", "MathUtils", IL2CPP_TYPE_CLASS, &object_class);
  depth.is_enum = flags.is_enum   = true;
  zoom_class.klass.instance_size  = sizeof(CameraNativeObject);
  scene_class.klass.instance_size = sizeof(CameraNativeScene);
  AddField(zoom_class, "_minimum", mode == "camera-missing-minimum" ? int_class : float_class,
           offsetof(CameraNativeObject, minimum));
  AddField(zoom_class, "_maximum", float_class, offsetof(CameraNativeObject, maximum));
  AddField(zoom_class, "_viewRadius", float_class, offsetof(CameraNativeObject, radius));
  AddField(zoom_class, "_depth", depth, offsetof(CameraNativeObject, depth));
  AddField(zoom_class, "_sceneCamera", scene_class, offsetof(CameraNativeObject, camera));
  AddField(zoom_class, "_zoomDelta", float_class, offsetof(CameraNativeObject, delta));
  AddField(zoom_class, "_lastZoomDelta", float_class, offsetof(CameraNativeObject, last));
  AddField(zoom_class, "_zoomLocation", vector2, offsetof(CameraNativeObject, location));
  AddField(zoom_class, "_worldPoint", vec3_class, offsetof(CameraNativeObject, world));
  AddField(zoom_class, "_farRatioSystemNormal", float_class, offsetof(CameraNativeObject, normal));
  AddField(zoom_class, "_farRatioSystemExtended", float_class, offsetof(CameraNativeObject, extended));
  AddField(zoom_class, "_zoomtotal", float_class, offsetof(CameraNativeObject, total));
  auto& get_distance =
      AddMethod(zoom_class, "get_Distance", reinterpret_cast<void*>(&CameraNativeDistance), T(float_class), {});
  auto& set_distance = AddMethod(zoom_class, "set_Distance", reinterpret_cast<void*>(&CameraNativeSetDistance),
                                 T(void_class), {T(float_class)});
  AddProperty(zoom_class, "Distance", &get_distance, &set_distance);
  AddMethod(zoom_class, "ZoomCameraAtWorldPoint", reinterpret_cast<void*>(&CameraNativeZoom), T(void_class), {});
  auto& clip_getter = AddMethod(scene_class, "get_farClipPlane", reinterpret_cast<void*>(&CameraNativeGetClip),
                                T(float_class), {});
  auto& clip_setter = AddMethod(scene_class, "set_farClipPlane", reinterpret_cast<void*>(&CameraNativeClip),
                                T(void_class), {T(float_class)});
  AddProperty(scene_class, "farClipPlane", &clip_getter, &clip_setter);
  auto& flags_setter =
      AddMethod(scene_class, "set_clearFlags", reinterpret_cast<void*>(&CameraNativeFlags), T(void_class), {T(flags)});
  auto& flags_getter =
      AddMethod(scene_class, "get_clearFlags", reinterpret_cast<void*>(&CameraNativeGetFlags), T(flags), {});
  AddProperty(scene_class, "clearFlags", &flags_getter, &flags_setter);
  auto& color_setter = AddMethod(scene_class, "set_backgroundColor", reinterpret_cast<void*>(&CameraNativeBackground),
                                 T(void_class), {T(color)});
  const MethodInfo* color_getter = nullptr;
  if (mode != "camera-missing-minimum")
    color_getter = &AddMethod(scene_class, "get_backgroundColor", reinterpret_cast<void*>(&CameraNativeGetBackground),
                              T(color), {});
  AddProperty(scene_class, "backgroundColor", color_getter, &color_setter);
  camera_world_method =
      &AddMethod(maths, "GetMouseWorldPos", reinterpret_cast<void*>(&CameraNativeWorld), T(vec3_class),
                 {T(scene_class), T(vec3_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  CameraNativeScene  scene{{&scene_class.klass}}, rebound_scene{{&scene_class.klass}};
  CameraNativeObject object{{&zoom_class.klass}}, other{{&zoom_class.klass}};
  object.camera = other.camera   = reinterpret_cast<unity::Camera*>(&scene);
  auto* zoom                     = reinterpret_cast<NavigationZoom*>(&object);
  auto* other_zoom               = reinterpret_cast<NavigationZoom*>(&other);
  camera_expected_scene          = &scene.header;
  camera_mouse                   = {40.0f, 50.0f, 0.0f};
  camera_world                   = {4.0f, 5.0f, 6.0f};
  camera_delta_time              = 0.25f;
  auto& config                   = Config::Get();
  config.graphics.zoom           = 1000.0f;
  config.control.hotkeys_enabled = config.control.hotkeys_extended = true;
  config.control.use_scopely_hotkeys                               = false;
  config.graphics.use_presets_as_default                           = false;
  selected                                                         = nullptr;
  has_event_system                                                 = true;
  ++input_frame;
  const auto roots = warp_roots.size();
  const auto saves = fleet_native_save_calls;
  if (mode == "camera-missing-minimum") {
    auto& preset = config.graphics.system_zoom_preset_1;
    preset       = 42.0f;
    Require(
        !CurrentZoom(zoom)
            && !StorePreset("missing", &Config::Graphics::system_zoom_preset_1, zoom, gameplay_settings::Zoom::Preset1)
            && preset == 42.0f && fleet_native_save_calls == saves && !ZoomAtMouse(zoom, 100.0f, true),
        "unavailable native range never becomes zero, a saved preset or an absolute zoom");
    KeepZoomRange(zoom);
    Require(object.maximum == 110.0f && object.normal == 0.0f && object.extended == 0.0f,
            "unavailable range leaves native geometry unchanged");
    Require(ApplyZoomRange(zoom, 500.0f) && camera_native_background_sets == 1 && scene.background.r == 0
                && scene.background.g == 0 && scene.background.b == 0 && scene.background.a == 0,
            "a missing background getter conservatively invokes the native setter");
    return;
  }
  Require(mode == "camera-native", "unknown camera fixture mode");
  Require(CurrentZoom(zoom) == 200.0f, "actual native range preserves original normalized zoom formula");
  auto& preset = config.graphics.system_zoom_preset_1;
  preset       = 42.0f;
  for (const auto value :
       {10.0f, 9.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    object.maximum = value;
    Require(!StorePreset("invalid", &Config::Graphics::system_zoom_preset_1, zoom, gameplay_settings::Zoom::Preset1)
                && preset == 42.0f && fleet_native_save_calls == saves,
            "zero, reversed and nonfinite ranges cannot persist a fabricated preset");
  }
  object.maximum                = 110.0f;
  camera_native_distance_throws = true;
  Require(!CurrentZoom(zoom), "failed native distance invocation stays unavailable");
  camera_native_distance_throws = false;
  for (std::size_t i = 0; i < std::size(kZoomPresets); ++i) {
    fleet_native_keys = {kZoomPresets[i].store};
    Require(!KeyboardZoom(zoom), "successful explicit preset store retains source native-update skip");
    Require(config.graphics.*kZoomPresets[i].value == 200.0f
                && fleet_native_saved_key == gameplay_settings::Identity(kZoomPresets[i].identity).name
                && std::get<double>(fleet_native_saved_any) == 200.0,
            "each preset uses its registered canonical save key and exact observed value");
  }
  fleet_native_keys = {Action::kSetZoomDefault};
  Require(!KeyboardZoom(zoom) && fleet_native_saved_key == "default_system_zoom",
          "explicit default is a durable save request");
  fleet_native_keys.clear();
  const auto before_zoom = camera_native_zoom_calls;
  Require(ZoomAtMouse(zoom, 300.0f, true) && object.distance == 40.0f && object.location.x == 40.0f
              && object.location.y == 50.0f && object.world.z == 6.0f && camera_native_zoom_calls == before_zoom + 1,
          "absolute zoom applies qualified distance and measured pointer/world coordinates once");
  object.location            = {-40.0f, -50.0f};
  object.world               = {-4.0f, -5.0f, -6.0f};
  camera_native_rebind_scene = reinterpret_cast<unity::Camera*>(&rebound_scene);
  Require(
      !ZoomAtMouse(zoom, 500.0f, true) && object.distance == 60.0f && object.camera == camera_native_rebind_scene
          && object.location.x == -40.0f && object.location.y == -50.0f && object.world.x == -4.0f
          && object.world.y == -5.0f && object.world.z == -6.0f && camera_native_zoom_calls == before_zoom + 1,
      "successful distance setter that rebinds the scene cannot publish old pointer/world data or zoom the new scene");
  camera_native_rebind_scene = nullptr;
  object.camera              = reinterpret_cast<unity::Camera*>(&scene);
  camera_world_failure       = true;
  const auto distance_sets   = camera_native_distance_sets;
  Require(!ZoomAtMouse(zoom, 500.0f, true) && camera_native_distance_sets == distance_sets,
          "unavailable mouse world point cannot write a distance or invent origin coordinates");
  camera_world_failure     = false;
  camera_native_set_throws = true;
  Require(!ZoomAtMouse(zoom, 500.0f, true) && camera_native_zoom_calls == before_zoom + 1,
          "failed native distance setter is not reported as a successful zoom");
  camera_native_set_throws            = false;
  config.graphics.keyboard_zoom_speed = 40.0f;
  fleet_native_keys                   = {Action::kZoomIn};
  KeyboardZoom(zoom);
  Require(object.delta == 10.0f && object.last == 10.0f, "held zoom-in uses configured rate times current frame delta");
  fleet_native_keys = {Action::kZoomOut};
  KeyboardZoom(zoom);
  Require(object.delta == -10.0f && object.last == -10.0f, "held zoom-out preserves signed native delta");
  const auto before_capture = camera_native_zoom_calls;
  fleet_native_capture      = true;
  KeyboardZoom(zoom);
  fleet_native_capture               = false;
  config.control.use_scopely_hotkeys = true;
  KeyboardZoom(zoom);
  config.control.use_scopely_hotkeys = false;
  Require(camera_native_zoom_calls == before_capture, "capture and native-hotkey mode do not inject mod keyboard zoom");
  config.graphics.system_zoom_preset_1   = 600.0f;
  config.graphics.default_system_zoom    = 800.0f;
  config.graphics.use_presets_as_default = true;
  fleet_native_keys                      = {Action::kZoomPreset1, Action::kZoomReset};
  const auto prior_session = fleet_native_session_calls, prior_saves = fleet_native_save_calls;
  KeyboardZoom(zoom);
  Require(object.distance == 90.0f && config.graphics.default_system_zoom == 800.0f
              && fleet_native_session_calls == prior_session + 1 && fleet_native_save_calls == prior_saves,
          "extended reset wins the jump but simultaneous preset retains only a session-default intent");
  fleet_native_keys.clear();
  object.maximum = 110.0f;
  KeepZoomRange(zoom);
  Require(object.maximum == 1000.0f && object.total == 990.0f && object.extended == 2.0f && scene.clip == 3750.0f
              && scene.flags == unity::Camera::ClearFlags::SolidColor,
          "system range and legacy pinch field retain the original finite ratios and multiplier");
  Require(camera_native_clip_sets == 1 && camera_native_flags_sets == 1 && camera_native_background_sets == 1,
          "first camera observation applies clip and appearance exactly once");
  tow_runtime_invoke_override = &CameraNativeObserveBoxed;
  const auto idle_roots = warp_roots.size();
  KeepZoomRange(zoom);
  KeepZoomRange(zoom);
  tow_runtime_invoke_override = nullptr;
  Require(camera_native_clip_gets == 3 && camera_native_clip_sets == 1 && camera_native_flags_sets == 1
              && camera_native_background_gets == 3 && camera_native_background_sets == 1
              && camera_native_boxed_calls == 0 && warp_roots.size() == idle_roots,
          "idle pre/post camera observations use no boxing or persistent roots and rewrite no unchanged properties");
  object.maximum = 110.0f;
  object.normal = object.extended = object.total = 0.0f;
  scene.clip = 20.0f;
  KeepZoomRange(zoom);
  Require(object.maximum == 1000.0f && object.total == 990.0f && object.normal == 1.1f && object.extended == 2.0f
              && scene.clip == 3750.0f && camera_native_clip_sets == 2 && camera_native_flags_sets == 1,
          "native range and clip resets are repaired without repeating constant appearance writes");
  scene.clip = 5000.0f;
  KeepZoomRange(zoom);
  Require(scene.clip == 3750.0f && camera_native_clip_sets == 3,
          "orbit expansion still receives the configured base before its final draw-distance reapply");
  scene.flags = unity::Camera::ClearFlags::Depth;
  KeepZoomRange(zoom);
  Require(scene.flags == unity::Camera::ClearFlags::SolidColor && camera_native_flags_sets == 2
              && camera_native_background_sets == 1,
          "an external CameraController clear-flags write is repaired without rewriting background color");
  object.camera = reinterpret_cast<unity::Camera*>(&rebound_scene);
  KeepZoomRange(zoom);
  Require(rebound_scene.clip == 3750.0f && camera_native_flags_sets == 3 && camera_native_background_sets == 2,
          "a replacement camera gets its own clip and appearance");
  config.graphics.zoom = 1200.0f;
  KeepZoomRange(zoom);
  Require(rebound_scene.clip == 4500.0f && object.extended == 2.4f && camera_native_flags_sets == 3
              && camera_native_background_sets == 2,
          "changed configuration repairs clip and ratios without rewriting already correct appearance");
  config.graphics.zoom = 1000.0f;
  object.camera = reinterpret_cast<unity::Camera*>(&scene);
  KeepZoomRange(zoom);
  const auto before_failed_flags = camera_native_flags_sets;
  scene.flags = unity::Camera::ClearFlags::Depth;
  camera_native_flags_throws = true;
  ApplyZoomRange(zoom, 500.0f);
  Require(scene.flags == unity::Camera::ClearFlags::Depth, "failed flags setter retains the observed native value");
  camera_native_flags_throws = false;
  KeepZoomRange(zoom);
  KeepZoomRange(zoom);
  Require(camera_native_flags_sets == before_failed_flags + 2,
          "failed flags setter retries on the next update and then becomes quiet");
  const auto before_failed_background = camera_native_background_sets;
  scene.background = {1, 0, 0, 0};
  camera_native_background_throws = true;
  ApplyZoomRange(zoom, 500.0f);
  camera_native_background_throws = false;
  KeepZoomRange(zoom);
  KeepZoomRange(zoom);
  Require(camera_native_background_sets == before_failed_background + 2,
          "failed background setter retries from the next native observation");
  for (const auto changed : {unity::Color{1, 0, 0, 0}, unity::Color{0, 1, 0, 0}, unity::Color{0, 0, 1, 0},
                             unity::Color{0, 0, 0, 1}}) {
    scene.background = changed;
    const auto before_external_background = camera_native_background_sets;
    KeepZoomRange(zoom);
    Require(scene.background.r == 0 && scene.background.g == 0 && scene.background.b == 0 && scene.background.a == 0
                && camera_native_background_sets == before_external_background + 1,
            "external writes to every background component are repaired without an entry callback");
  }
  const auto before_unknown_background = camera_native_background_sets;
  camera_native_background_get_throws = true;
  KeepZoomRange(zoom);
  camera_native_background_get_throws = false;
  Require(camera_native_background_sets == before_unknown_background + 1,
          "a throwing background getter preserves conservative native repair");
  const auto before_unknown_clip = camera_native_clip_sets;
  camera_native_clip_get_throws = true;
  KeepZoomRange(zoom);
  camera_native_clip_get_throws = false;
  Require(camera_native_clip_sets == before_unknown_clip + 1,
          "an unavailable clip observation preserves conservative native repair");
  camera_native_flags_rebind_owner = zoom;
  camera_native_rebind_scene = reinterpret_cast<unity::Camera*>(&rebound_scene);
  scene.flags = unity::Camera::ClearFlags::Depth;
  const auto before_rebind_background = camera_native_background_sets;
  Require(!ApplyZoomRange(zoom, 500.0f) && camera_native_background_sets == before_rebind_background,
          "camera replacement during flags setter cannot continue the old appearance refresh");
  camera_native_flags_rebind_owner = nullptr;
  camera_native_rebind_scene = nullptr;
  KeepZoomRange(zoom);
  Require(rebound_scene.clip == 3750.0f && rebound_scene.flags == unity::Camera::ClearFlags::SolidColor,
          "replacement camera is retried after a reentrant setter changed ownership");
  rebound_scene.flags = unity::Camera::ClearFlags::Depth;
  camera_native_flags_replace_config = true;
  Require(!ApplyZoomRange(zoom, 500.0f),
          "configuration replacement during flags setter cannot continue with old configuration inputs");
  KeepZoomRange(zoom);
  Require(object.extended == 2.4f && rebound_scene.clip == 4500.0f,
          "the replacement configuration is applied on the next observation");
  bindings_config_override = nullptr;
  bindings_replacement_config.reset();
  camera_replacement_config = nullptr;
  object.camera = reinterpret_cast<unity::Camera*>(&scene);
  KeepZoomRange(zoom);
  camera_native_background_get_rebind_owner = zoom;
  camera_native_rebind_scene = reinterpret_cast<unity::Camera*>(&rebound_scene);
  scene.background = {1, 0, 0, 0};
  const auto before_getter_rebind = camera_native_background_sets;
  Require(!ApplyZoomRange(zoom, 500.0f) && camera_native_background_sets == before_getter_rebind,
          "camera replacement during background getter cannot write to the old camera");
  camera_native_background_get_rebind_owner = nullptr;
  camera_native_rebind_scene = nullptr;
  KeepZoomRange(zoom);
  rebound_scene.background = {0, 1, 0, 0};
  camera_native_background_get_replace_config = true;
  const auto before_getter_config = camera_native_background_sets;
  Require(!ApplyZoomRange(zoom, 500.0f) && camera_native_background_sets == before_getter_config,
          "configuration replacement during background getter cannot apply old inputs");
  KeepZoomRange(zoom);
  Require(rebound_scene.clip == 4500.0f && rebound_scene.background.g == 0
              && camera_native_background_sets == before_getter_config + 1,
          "new configuration and background are repaired after getter reentry");
  bindings_config_override = nullptr;
  bindings_replacement_config.reset();
  camera_replacement_config = nullptr;
  object.camera = reinterpret_cast<unity::Camera*>(&scene);
  KeepZoomRange(zoom);
  object.depth   = NodeDepth::Galaxy;
  object.maximum = 110.0f;
  KeepZoomRange(zoom);
  Require(object.maximum == 110.0f, "extended range is restricted to system depth");
  object.depth         = NodeDepth::SolarSystem;
  PendingDefaultZoom() = il2cpp::GcHandle::Weak(zoom);
  Require(!TakeDefaultZoom(other_zoom) && PendingDefaultZoom().Get<NavigationZoom>() == zoom && TakeDefaultZoom(zoom)
              && !PendingDefaultZoom(),
          "another camera cannot consume a pending system-entry default");
  EnterDepth(
      [&](auto*, NodeDepth) {
        EnterDepth([](auto*, NodeDepth) {}, other_zoom, NodeDepth::SolarSystem, 500.0f, NodeDepth::SolarSystem);
      },
      zoom, NodeDepth::SolarSystem, 500.0f, NodeDepth::SolarSystem);
  Require(PendingDefaultZoom().Get<NavigationZoom>() == other_zoom,
          "reentrant newer camera entry cannot be overwritten by the outer completion");
  EnterDepth([](auto*, NodeDepth) {}, other_zoom, NodeDepth::Galaxy, 0.0f, NodeDepth::Galaxy);
  Require(!PendingDefaultZoom(), "leaving system clears only that camera's deferred default");
  camera_zoom_installed = true;
  selected              = &selected_object;
  has_input_field       = true;
  ++input_frame;
  const auto before_focus = camera_native_zoom_calls;
  fleet_native_keys       = {Action::kZoomIn};
  int originals           = 0;
  NavigationZoom_Update_Hook([&](auto*) { ++originals; }, zoom);
  Require(camera_native_zoom_calls == before_focus && originals == 1,
          "text focus retains the ordinary native update without key zoom");
  const auto native_entry_distance    = object.distance;
  const auto before_entry_sets        = camera_native_distance_sets;
  const auto before_entry_saves       = fleet_native_save_calls;
  config.graphics.default_system_zoom = 0.0f;
  PendingDefaultZoom()                = il2cpp::GcHandle::Weak(zoom);
  NavigationZoom_Update_Hook([&](auto*) { ++originals; }, other_zoom);
  Require(PendingDefaultZoom().Get<NavigationZoom>() == zoom && camera_native_zoom_calls == before_focus
              && camera_native_distance_sets == before_entry_sets && originals == 2,
          "another camera's update cannot consume or apply a zero system-entry default");
  NavigationZoom_Update_Hook([&](auto*) { ++originals; }, zoom);
  Require(!PendingDefaultZoom() && object.distance == native_entry_distance && camera_native_zoom_calls == before_focus
              && camera_native_distance_sets == before_entry_sets && originals == 3
              && config.graphics.default_system_zoom == 0.0f && fleet_native_save_calls == before_entry_saves,
          "zero entry default preserves native distance and saved preference while consuming its pending owner");
  config.graphics.default_system_zoom = 800.0f;
  NavigationZoom_Update_Hook([&](auto*) { ++originals; }, zoom);
  Require(object.distance == native_entry_distance && camera_native_zoom_calls == before_focus && originals == 4,
          "a consumed zero entry cannot apply a later positive default without a new system entry");
  PendingDefaultZoom() = il2cpp::GcHandle::Weak(zoom);
  NavigationZoom_Update_Hook([&](auto*) { ++originals; }, zoom);
  Require(!PendingDefaultZoom() && object.distance == 802.0f && camera_native_zoom_calls == before_focus + 1
              && camera_native_distance_sets == before_entry_sets + 1 && originals == 5,
          "positive system-entry default still applies once while text has focus");
  selected = nullptr;
  fleet_native_keys.clear();
  ++input_frame;
  scroll_queries_available     = true;
  camera_fixture_scroll::owned = {true, true};
  camera_fixture_scroll::pinch = {true, false};
  int        wheel_originals   = 0;
  const auto wheel             = [&](auto*) { ++wheel_originals; };
  NavigationZoom_ZoomCameraAtWorldPoint_Hook(wheel, zoom);
  Require(wheel_originals == 1, "UI ownership cannot block mod keyboard zoom outside the native-update scope");
  object.delta = object.last = 4.0f;
  {
    NativeZoomUpdateScope outer(zoom);
    NavigationZoom_ZoomCameraAtWorldPoint_Hook(wheel, zoom);
    Require(wheel_originals == 1 && object.delta == 0.0f && object.last == 0.0f,
            "only scoped native wheel consumption is suppressed over owned UI");
    camera_fixture_scroll::pinch = {true, true};
    NavigationZoom_ZoomCameraAtWorldPoint_Hook(wheel, zoom);
    Require(wheel_originals == 2, "native touch pinch remains intact over UI");
    camera_fixture_scroll::pinch = {};
    NavigationZoom_ZoomCameraAtWorldPoint_Hook(wheel, zoom);
    Require(wheel_originals == 3, "unknown pinch state is not an observed mouse-wheel input");
    {
      NativeZoomUpdateScope nested(other_zoom);
      Require(scroll_native_update == other_zoom, "nested native update binds its own camera");
    }
    Require(scroll_native_update == zoom, "nested native update restores the previous camera");
  }
  Require(!scroll_native_update, "native update scope is cleared after the callback");
  int recognizer_originals = 0;
  Require(MouseScrollAsPinchRecognizer_ReadInput_Hook(
              [&](auto*) {
                ++recognizer_originals;
                return 2.0f;
              },
              nullptr)
                  == 0.0f
              && recognizer_originals == 1,
          "station/Haven recognizer consumes only its camera wheel result, calling native input once");
  camera_fixture_scroll::owned = {};
  Require(MouseScrollAsPinchRecognizer_ReadInput_Hook([](auto*) { return 2.0f; }, nullptr) == 2.0f
              && !scroll_queries_available,
          "failed UI ownership query retains native input instead of blocking unknown state");
  PendingDefaultZoom().Reset();
  object.distance                      = 30.0f;
  object.minimum                       = 10.0f;
  object.maximum                       = 110.0f;
  config.graphics.zoom                 = 1000.0f;
  config.graphics.system_zoom_preset_5 = -123.0f;
  const auto before_replacement_save   = fleet_native_save_calls;
  camera_native_replace_config         = true;
  Require(StorePreset("replaced", &Config::Graphics::system_zoom_preset_5, zoom, gameplay_settings::Zoom::Preset5)
              && bindings_config_override == camera_replacement_config
              && camera_replacement_config->graphics.system_zoom_preset_5 == 200.0f
              && config.graphics.system_zoom_preset_5 == -123.0f
              && fleet_native_save_calls == before_replacement_save + 1,
          "managed measurement replacing Config writes only the current preset owner");
  bindings_config_override = nullptr;
  bindings_replacement_config.reset();
  camera_replacement_config = nullptr;
  Require(warp_roots.size() == roots, "camera paths release temporary roots and pending weak ownership");
  std::cout << "Camera production callbacks: exact presets, qualified ranges, scoped input, owner-bound defaults and "
               "wheel/pinch passed\n";
}
} // namespace
