#include "patches/background_layer_science.h"
#include "config.h"
#include "errormsg.h"
#include "patches/key.h"
#include "patches/mapkey.h"
#include "patches/navigation_environment.h"
#include "patches/screen_update_hook.h"
#include <algorithm>
#include <cmath>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <limits>
#include <prime/Hub.h>
#include <prime/NavigationZoom.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <utility>

bool KeyboardPanAllowed();

namespace
{
bool              ready        = false;
FieldInfo        *scene_camera = nullptr, *distance_field = nullptr, *depth_field = nullptr;
FieldInfo        *zoom_location = nullptr, *world_point = nullptr;
const MethodInfo *get_transform = nullptr, *get_angles = nullptr, *set_angles = nullptr;
const MethodInfo *get_forward = nullptr, *set_position = nullptr, *mouse_world = nullptr;
const MethodInfo *clamp_pan_position = nullptr;
const MethodInfo *camera_fov = nullptr, *camera_height = nullptr;
const MethodInfo *get_world_position = nullptr, *get_far_clip = nullptr, *set_far_clip = nullptr;
FieldInfo        *pan_system_position = nullptr, *pan_view_radius = nullptr;
const MethodInfo *event_system = nullptr, *pointer_over_ui = nullptr, *can_move = nullptr;
FieldInfo        *pan_camera = nullptr, *pan_depth = nullptr, *pan_drag_delta = nullptr;
FieldInfo        *pan_tracking = nullptr, *pan_min_speed = nullptr, *normalized_zoom = nullptr;
int (*frame_count)()              = nullptr;
bool (*focused)()                 = nullptr;
void (*mouse_position)(Vector3 *) = nullptr;

struct OrbitState {
  Il2CppGCHandle zoom = nullptr, transform = nullptr, pan = nullptr;
  NodeDepth      depth = NodeDepth::Starbase;
  Vector3        native_angles{};
  float          distance = 0.0f;
  float          yaw = 0.0f, pitch = 0.0f, x = 0.0f, y = 0.0f;
  int            frame    = -1;
  bool           dragging = false, overridden = false;
};
OrbitState state;

bool Finite(Vector3 value)
{ return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

bool ReadVector(const MethodInfo *method, Il2CppObject *object, Vector3 &value, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  if (!Il2CppRuntime::TryInvoke(method, object, args, &result) || !result
      || !method_contract::Type(il2cpp_class_get_type(result->klass), "UnityEngine.Vector3"))
    return false;
  auto *data = static_cast<Vector3 *>(il2cpp_object_unbox(result));
  if (!data || !Finite(*data))
    return false;
  value = *data;
  return true;
}

bool SetView(Il2CppObject *transform, Vector3 angles, float distance)
{
  if (!transform || !Finite(angles) || !std::isfinite(distance) || distance <= 0.0f)
    return false;
  void   *angle_args[]{&angles};
  Vector3 forward{};
  if (!Il2CppRuntime::TryInvoke(set_angles, transform, angle_args) || !ReadVector(get_forward, transform, forward))
    return false;
  // Mirror NavigationZoom.UpdateCameraPosition: -forward * native distance.
  // Rotate the actual camera so projection, hit testing and pan share the view.
  Vector3 position{-forward.x * distance, -forward.y * distance, -forward.z * distance};
  if (!Finite(position))
    return false;
  void *position_args[]{&position};
  return Il2CppRuntime::TryInvoke(set_position, transform, position_args);
}

void Clear()
{
  navigation_environment::Clear();
  if (state.overridden && state.transform)
    SetView(il2cpp_gchandle_get_target(state.transform), state.native_angles, state.distance);
  if (state.zoom)
    il2cpp_gchandle_free(state.zoom);
  if (state.transform)
    il2cpp_gchandle_free(state.transform);
  if (state.pan)
    il2cpp_gchandle_free(state.pan);
  state = {};
}

void Fail(const char *operation)
{
  Clear();
  ready = false;
  spdlog::warn("[NavigationOrbit] {} failed; orbit disabled and native orientation restored", operation);
}

bool SectionMatches(NodeDepth depth)
{
  auto *sections = Hub::get_SectionManager();
  // Galaxy rendering assumes the native view; rotating it exposes layers that
  // are not intended to be visible. Keep its camera entirely native.
  return sections && depth == NodeDepth::SolarSystem && sections->CurrentSection == SectionID::Navigation_System;
}

bool Enabled()
{
  const auto &config = Config::Get();
  return ready && config.installNavigationOrbitHooks && config.hotkeys_enabled && !config.use_scopely_hotkeys
         && MapKey::HasBinding(GameFunction::NavigationOrbitDrag);
}

bool PointerAvailable(bool continuing)
{
  Il2CppObject *result = nullptr, *system = nullptr;
  bool          allowed = false;
  if (!Il2CppRuntime::TryInvoke(can_move, nullptr, nullptr, &result) || !Il2CppRuntime::TryBoolean(result, allowed)) {
    Fail("NavigationCamera.CanMoveCamera");
    return false;
  }
  if (!allowed || !KeyboardPanAllowed())
    return false;
  if (!Il2CppRuntime::TryInvoke(event_system, nullptr, nullptr, &system)) {
    Fail("EventSystem.get_current");
    return false;
  }
  if (!system)
    return false;
  int   pointer = -1;
  void *args[]{&pointer};
  bool  blocked = true;
  if (!Il2CppRuntime::TryInvoke(pointer_over_ui, system, args, &result)
      || !Il2CppRuntime::TryBoolean(result, blocked)) {
    Fail("EventSystem.IsPointerOverGameObject");
    return false;
  }
  return !blocked || continuing;
}

bool Reanchor(Il2CppObject *zoom)
{
  auto      *camera   = reinterpret_cast<Il2CppObject *>(reinterpret_cast<NavigationZoom *>(zoom)->_sceneCamera);
  const auto location = reinterpret_cast<NavigationZoom *>(zoom)->_zoomLocation;
  Vector3    screen{location.x, location.y, 0.0f}, world{};
  void      *args[]{camera, &screen};
  // The existing GetMouseWorldPos helper returns zero on failure, which is also
  // a valid map point. Require explicit success before replacing the anchor.
  if (!camera || !Finite(screen) || !ReadVector(mouse_world, nullptr, world, args))
    return false;
  il2cpp_field_set_value(zoom, world_point, &world);
  return true;
}

void Tick()
{
  if (!state.zoom)
    return;
  auto *zoom = il2cpp_gchandle_get_target(state.zoom);
  if (!Enabled() || !zoom || !SectionMatches(state.depth)
      || reinterpret_cast<NavigationZoom *>(zoom)->_depth != state.depth) {
    Clear();
    return;
  }
  if (state.frame == frame_count())
    return;
  state.frame              = frame_count();
  const bool input_allowed = focused() && !Key::IsInputFocused();
  const bool held          = input_allowed && MapKey::IsPressed(GameFunction::NavigationOrbitDrag);
  const bool reset         = input_allowed && MapKey::IsDown(GameFunction::NavigationOrbitReset);
  if (!held && !reset) {
    state.dragging = false;
    return;
  }
  if (!PointerAvailable(state.dragging && held && !reset)) {
    state.dragging = false;
    return;
  }
  auto *transform = il2cpp_gchandle_get_target(state.transform);
  bool  changed   = false;
  if (reset) {
    if (state.overridden && !SetView(transform, state.native_angles, state.distance)) {
      Fail("restore camera transform");
      return;
    }
    changed          = state.overridden;
    state.overridden = state.dragging = false;
    navigation_environment::Clear();
    state.yaw = 0.0f;
    spdlog::debug("[NavigationOrbit] reset native view");
  } else {
    Vector3 position{};
    mouse_position(&position);
    if (!Finite(position)) {
      state.dragging = false;
      return;
    }
    if (state.dragging) {
      constexpr float degrees_per_pixel = 0.15f;
      state.yaw        = std::remainder(state.yaw + (position.x - state.x) * degrees_per_pixel, 360.0f);
      state.pitch      = std::clamp(state.pitch - (position.y - state.y) * degrees_per_pixel, 5.0f, 90.0f);
      state.overridden = changed = true;
      if (!SetView(transform, {state.pitch, state.yaw, 0.0f}, state.distance)) {
        Fail("rotate camera transform");
        return;
      }
    } else if (MapKey::IsDown(GameFunction::NavigationOrbitDrag)) {
      if (!state.overridden)
        state.pitch = std::clamp(state.native_angles.x, 5.0f, 90.0f);
      state.dragging = true;
      spdlog::debug("[NavigationOrbit] drag started depth={} pitch={}", static_cast<int>(state.depth), state.pitch);
    }
    state.x = position.x;
    state.y = position.y;
  }
  // A gesture between native Update and LateUpdate must not look like camera
  // translation to the wheel handler. Native zoom keeps its own anchor.
  if (changed && !Reanchor(zoom))
    Fail("MathUtils.GetMouseWorldPos / zoom anchor");
}

bool FreePan(Il2CppObject *pan)
{
  auto *zoom = state.zoom ? il2cpp_gchandle_get_target(state.zoom) : nullptr;
  if (!pan || !zoom || !state.overridden || !Enabled() || !SectionMatches(state.depth)
      || reinterpret_cast<NavigationZoom *>(zoom)->_depth != state.depth)
    return false;
  NodeDepth     depth{};
  Il2CppObject *camera = nullptr;
  il2cpp_field_get_value(pan, pan_depth, &depth);
  il2cpp_field_get_value(pan, pan_camera, &camera);
  return depth == NodeDepth::SolarSystem
         && camera == reinterpret_cast<Il2CppObject *>(reinterpret_cast<NavigationZoom *>(zoom)->_sceneCamera)
         && !reinterpret_cast<NavigationPan *>(pan)->_trackingPOI;
}

Vector3 ClampPositionInside_Hook(auto original, Il2CppObject *pan, Vector3 position)
{
  // Native MoveCamera uses the clamp's overlap for BOTH outward damping and
  // automatic spring return. Zoom's Move path also consults it. Keep target
  // tracking and other map scopes native, but free user movement has no wall.
  return FreePan(pan) && Finite(position) ? position : original(pan, position);
}

vec2 ScreenPanDelta(vec2 pixels, float distance, float fov, int height, float yaw, float speed)
{
  constexpr double radians = 3.14159265358979323846 / 180.0;
  const double     scale   = 2.0 * distance * std::tan(fov * radians * 0.5) * speed / height;
  const double     x = pixels.x * scale, z = pixels.y * scale;
  const double     c = std::cos(yaw * radians), s = std::sin(yaw * radians);
  return {static_cast<float>(x * c + z * s), static_cast<float>(-x * s + z * c)};
}

void MoveCamera_Hook(auto original, Il2CppObject *pan, vec2 delta, bool momentum)
{
  // Retain the matching pan rig only to read the system's actual world center.
  // This is rendering metadata, never a movement boundary.
  auto         *zoom = state.zoom ? il2cpp_gchandle_get_target(state.zoom) : nullptr;
  NodeDepth     depth{};
  Il2CppObject *camera = nullptr;
  if (pan && Enabled()) {
    il2cpp_field_get_value(pan, pan_depth, &depth);
    il2cpp_field_get_value(pan, pan_camera, &camera);
  }
  if (pan && zoom && Enabled() && SectionMatches(state.depth) && depth == state.depth
      && camera == reinterpret_cast<Il2CppObject *>(reinterpret_cast<NavigationZoom *>(zoom)->_sceneCamera)
      && (!state.pan || il2cpp_gchandle_get_target(state.pan) != pan)) {
    if (state.pan)
      il2cpp_gchandle_free(state.pan);
    state.pan = il2cpp_gchandle_new(pan, false);
  }
  if (!FreePan(pan)) {
    original(pan, delta, momentum);
    return;
  }
  if (!momentum) {
    // Near the horizon, successive ground-ray intersections can be millions
    // of units apart. Use the actual gesture's pixel displacement at camera
    // focus distance instead. This changes sensitivity, never position bounds.
    Il2CppObject *fov_result = nullptr, *height_result = nullptr;
    vec2          pixels{};
    float         min_speed = 0.0f;
    il2cpp_field_get_value(pan, pan_drag_delta, &pixels);
    il2cpp_field_get_value(pan, pan_min_speed, &min_speed);
    const float normalized = reinterpret_cast<NavigationZoom *>(zoom)->NormalizedZoom;
    if (!Il2CppRuntime::TryInvoke(camera_fov, camera, nullptr, &fov_result) || !fov_result
        || !method_contract::Type(il2cpp_class_get_type(fov_result->klass), "System.Single")) {
      original(pan, vec2{}, momentum);
      return;
    }
    auto *fov_data = static_cast<float *>(il2cpp_object_unbox(fov_result));
    if (!fov_data) {
      original(pan, vec2{}, momentum);
      return;
    }
    const float fov = *fov_data;
    if (!Il2CppRuntime::TryInvoke(camera_height, camera, nullptr, &height_result) || !height_result
        || !method_contract::Type(il2cpp_class_get_type(height_result->klass), "System.Int32")) {
      original(pan, vec2{}, momentum);
      return;
    }
    auto *height_data = static_cast<int *>(il2cpp_object_unbox(height_result));
    if (!height_data) {
      original(pan, vec2{}, momentum);
      return;
    }
    const int height = *height_data;
    if (!std::isfinite(fov) || fov <= 0 || fov >= 179 || height <= 0 || !std::isfinite(normalized)
        || !std::isfinite(min_speed)) {
      original(pan, vec2{}, momentum);
      return;
    }
    delta = ScreenPanDelta(pixels, state.distance, fov, height, state.yaw, 1.0f - (1.0f - min_speed) * normalized);
  }
  // Preserve native movement events and momentum bookkeeping. Invalid input
  // is dropped, without relocating an already valid camera position.
  original(pan, std::isfinite(delta.x) && std::isfinite(delta.y) ? delta : vec2{}, momentum);
}

float FreeViewFarClip(float baseline, Vector3 camera, Vector3 center, float radius)
{
  if (!std::isfinite(baseline) || baseline <= 0 || !Finite(camera) || !Finite(center) || !std::isfinite(radius)
      || radius <= 0)
    return baseline;
  const double distance =
      std::hypot(double(camera.x) - center.x, double(camera.y) - center.y, double(camera.z) - center.z);
  const double required = distance + 4.0 * radius;
  return required <= std::numeric_limits<float>::max() ? std::max(baseline, float(required)) : baseline;
}

void ApplyDrawDistance(Il2CppObject *zoom)
{
  if (!zoom || !state.zoom || il2cpp_gchandle_get_target(state.zoom) != zoom || !state.pan || !state.overridden
      || !Enabled() || !SectionMatches(state.depth) || reinterpret_cast<NavigationZoom *>(zoom)->_depth != state.depth)
    return;
  auto *pan    = il2cpp_gchandle_get_target(state.pan);
  auto *camera = reinterpret_cast<Il2CppObject *>(reinterpret_cast<NavigationZoom *>(zoom)->_sceneCamera);
  if (!pan || !camera)
    return;
  NodeDepth     depth{};
  Il2CppObject *pan_scene_camera = nullptr;
  il2cpp_field_get_value(pan, pan_depth, &depth);
  il2cpp_field_get_value(pan, pan_camera, &pan_scene_camera);
  if (depth != state.depth || pan_scene_camera != camera)
    return;
  Il2CppObject *transform = nullptr, *boxed = nullptr;
  Vector3       position{};
  if (!Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &transform) || !transform
      || !ReadVector(get_world_position, transform, position)
      || !Il2CppRuntime::TryInvoke(get_far_clip, camera, nullptr, &boxed) || !boxed
      || !method_contract::Type(il2cpp_class_get_type(boxed->klass), "System.Single"))
    return;
  auto *value = static_cast<float *>(il2cpp_object_unbox(boxed));
  if (!value)
    return;
  const float baseline = *value;
  Vector3     center{};
  float       radius = 0.0f;
  il2cpp_field_get_value(pan, pan_system_position, &center);
  il2cpp_field_get_value(pan, pan_view_radius, &radius);
  float expanded = FreeViewFarClip(baseline, position, center, radius);
  if (expanded > baseline) {
    void *args[]{&expanded};
    Il2CppRuntime::TryInvoke(set_far_clip, camera, args);
  }
  navigation_environment::ApplyDrawDistance(camera);
}

void UpdateCameraPosition_Hook(auto original, Il2CppObject *zoom)
{
  if (state.zoom
      && (!Enabled() || !zoom || il2cpp_gchandle_get_target(state.zoom) != zoom
          || reinterpret_cast<NavigationZoom *>(zoom)->_depth != state.depth || !SectionMatches(state.depth)))
    Clear();
  original(zoom);
  if (!zoom || !Enabled() || !SectionMatches(reinterpret_cast<NavigationZoom *>(zoom)->_depth))
    return;
  const auto    depth     = reinterpret_cast<NavigationZoom *>(zoom)->_depth;
  auto         *camera    = reinterpret_cast<Il2CppObject *>(reinterpret_cast<NavigationZoom *>(zoom)->_sceneCamera);
  Il2CppObject *transform = nullptr, *camera_transform = nullptr;
  Vector3       angles{};
  const auto    distance = reinterpret_cast<NavigationZoom *>(zoom)->_actualDistance;
  if (!camera || !Il2CppRuntime::TryInvoke(get_transform, zoom, nullptr, &transform)
      || !Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &camera_transform) || !transform || !camera_transform
      || !ReadVector(get_angles, camera_transform, angles) || !std::isfinite(distance) || distance <= 0.0f) {
    Fail("camera transform / distance query");
    return;
  }
  // Qualify the rig instead of assuming a hierarchy or editing a shared parent.
  if (transform != camera_transform || std::abs(std::remainder(angles.y, 360.0f)) > 0.01f
      || std::abs(std::remainder(angles.z, 360.0f)) > 0.01f) {
    static bool warned = false;
    if (!warned) {
      spdlog::warn("[NavigationOrbit] native rig unqualified (same-transform={} pitch={} yaw={} roll={})",
                   transform == camera_transform, angles.x, angles.y, angles.z);
      warned = true;
    }
    Clear();
    return;
  }
  if (!state.zoom) {
    state.zoom      = il2cpp_gchandle_new(zoom, false);
    state.transform = il2cpp_gchandle_new(transform, false);
    if (!state.zoom || !state.transform) {
      Fail("retain camera rig");
      return;
    }
    state.depth = depth;
    spdlog::debug("[NavigationOrbit] qualified depth={} native-pitch={} distance={}", static_cast<int>(depth), angles.x,
                  distance);
  }
  state.native_angles = angles;
  state.distance      = distance;
  if (state.overridden && !SetView(transform, {state.pitch, state.yaw, 0.0f}, distance)) {
    Fail("reapply camera transform");
    return;
  }
  Tick();
  ApplyDrawDistance(zoom);
  if (state.overridden && Enabled() && background_layer_science::AmbientEnabled())
    navigation_environment::Update(camera);
  else
    navigation_environment::Clear();
}
} // namespace

// The zoom patch owns the ordinary clip distance. Reapply after its writes so
// both Update and LateUpdate cover a freely translated system camera.
void ApplyNavigationOrbitDrawDistance(NavigationZoom *zoom)
{ ApplyDrawDistance(reinterpret_cast<Il2CppObject *>(zoom)); }

void InstallNavigationOrbitHooks()
{
  auto        zoom       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom");
  auto        navigation = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationCamera");
  auto        pan        = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPan");
  auto        component  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  auto        transform  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  auto        camera_cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Camera");
  auto        events     = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
  auto        math       = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils");
  const auto *update     = method_contract::Resolve(zoom.get_cls(), "UpdateCameraPosition", false, "System.Void", {});
  const auto *move       = method_contract::Resolve(pan.get_cls(), "MoveCamera", false, "System.Void",
                                                    {"UnityEngine.Vector2", "System.Boolean"});
  get_transform = method_contract::Resolve(component.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
  get_angles = method_contract::Resolve(transform.get_cls(), "get_localEulerAngles", false, "UnityEngine.Vector3", {});
  clamp_pan_position = method_contract::Resolve(pan.get_cls(), "ClampPositionInside", false, "UnityEngine.Vector3",
                                                {"UnityEngine.Vector3"});
  get_world_position = method_contract::Resolve(transform.get_cls(), "get_position", false, "UnityEngine.Vector3", {});
  get_far_clip       = method_contract::Resolve(camera_cls.get_cls(), "get_farClipPlane", false, "System.Single", {});
  set_far_clip =
      method_contract::Resolve(camera_cls.get_cls(), "set_farClipPlane", false, "System.Void", {"System.Single"});
  camera_fov    = method_contract::Resolve(camera_cls.get_cls(), "get_fieldOfView", false, "System.Single", {});
  camera_height = method_contract::Resolve(camera_cls.get_cls(), "get_pixelHeight", false, "System.Int32", {});
  set_angles    = method_contract::Resolve(transform.get_cls(), "set_localEulerAngles", false, "System.Void",
                                           {"UnityEngine.Vector3"});
  get_forward   = method_contract::Resolve(transform.get_cls(), "get_forward", false, "UnityEngine.Vector3", {});
  set_position =
      method_contract::Resolve(transform.get_cls(), "set_localPosition", false, "System.Void", {"UnityEngine.Vector3"});
  event_system =
      method_contract::Resolve(events.get_cls(), "get_current", true, "UnityEngine.EventSystems.EventSystem", {});
  pointer_over_ui =
      method_contract::Resolve(events.get_cls(), "IsPointerOverGameObject", false, "System.Boolean", {"System.Int32"});
  can_move    = method_contract::Resolve(navigation.get_cls(), "CanMoveCamera", true, "System.Boolean", {});
  mouse_world = method_contract::Resolve(math.get_cls(), "GetMouseWorldPos", true, "UnityEngine.Vector3",
                                         {"UnityEngine.Camera", "UnityEngine.Vector3"});
  struct FieldBinding {
    FieldInfo        **destination;
    IL2CppClassHelper *owner;
    const char        *name, *type;
  };
  const FieldBinding fields[] = {
      {&pan_system_position, &pan, "_systemPosition", "UnityEngine.Vector3"},
      {&pan_view_radius, &pan, "_viewRadius", "System.Single"},
      {&scene_camera, &zoom, "_sceneCamera", "UnityEngine.Camera"},
      {&distance_field, &zoom, "_actualDistance", "System.Single"},
      {&depth_field, &zoom, "_depth", "Digit.PrimeServer.Models.NodeDepth"},
      {&zoom_location, &zoom, "_zoomLocation", "UnityEngine.Vector2"},
      {&world_point, &zoom, "_worldPoint", "UnityEngine.Vector3"},
      {&pan_camera, &pan, "_sceneCamera", "UnityEngine.Camera"},
      {&pan_depth, &pan, "_nodeDepth", "Digit.PrimeServer.Models.NodeDepth"},
      {&pan_drag_delta, &pan, "_dragDelta", "UnityEngine.Vector2"},
      {&pan_tracking, &pan, "_trackingPOI", "Digit.Prime.Navigation.POI"},
      {&pan_min_speed, &pan, "_minPanSpeed", "System.Single"},
      {&normalized_zoom, &zoom, "<NormalizedZoom>k__BackingField", "System.Single"},
  };
  for (const auto &binding : fields) {
    auto *field = binding.owner->get_cls() ? binding.owner->GetField(binding.name).get_info() : nullptr;
    if (!field || (il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
        || !method_contract::Type(field->type, binding.type)) {
      auto *actual = field && field->type ? il2cpp_type_get_name(field->type) : nullptr;
      spdlog::warn("[NavigationOrbit] {}.{} unavailable: expected instance {}; actual={}; orbit hooks skipped",
                   binding.owner->get_cls() ? binding.owner->get_cls()->name : "<missing class>", binding.name,
                   binding.type, actual ? actual : "<missing field>");
      il2cpp_free(actual);
      return;
    }
    *binding.destination = field;
  }
  frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
  focused        = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
  mouse_position = il2cpp_resolve_icall_typed<void(Vector3 *)>(
      "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  const std::pair<const MethodInfo *, const char *> methods[] = {
      {update, "NavigationZoom.UpdateCameraPosition"},
      {move, "NavigationPan.MoveCamera"},
      {clamp_pan_position, "NavigationPan.ClampPositionInside"},
      {get_transform, "Component.get_transform"},
      {get_angles, "Transform.get_localEulerAngles"},
      {get_world_position, "Transform.get_position"},
      {get_far_clip, "Camera.get_farClipPlane"},
      {set_far_clip, "Camera.set_farClipPlane"},
      {camera_fov, "Camera.get_fieldOfView"},
      {camera_height, "Camera.get_pixelHeight"},
      {set_angles, "Transform.set_localEulerAngles"},
      {get_forward, "Transform.get_forward"},
      {set_position, "Transform.set_localPosition"},
      {event_system, "EventSystem.get_current"},
      {pointer_over_ui, "EventSystem.IsPointerOverGameObject"},
      {can_move, "NavigationCamera.CanMoveCamera"},
      {mouse_world, "MathUtils.GetMouseWorldPos"},
  };
  for (const auto &[method, name] : methods) {
    if (!method || method->has_full_generic_sharing_signature) {
      spdlog::warn("[NavigationOrbit] {} signature unavailable; orbit hooks skipped", name);
      return;
    }
  }
  if (!frame_count || !focused || !mouse_position) {
    spdlog::warn("[NavigationOrbit] input icalls unavailable: Time.get_frameCount={} Application.get_isFocused={} "
                 "Input.get_mousePosition={}; orbit hooks skipped",
                 frame_count != nullptr, focused != nullptr, mouse_position != nullptr);
    return;
  }
  if (!install_screen_manager_update_hook() || !register_screen_manager_update_callback(Tick)) {
    spdlog::warn("[NavigationOrbit] ScreenManager update callback unavailable; orbit hooks skipped");
    return;
  }
  if (SPUD_STATIC_DETOUR(clamp_pan_position->methodPointer, ClampPositionInside_Hook)
      && SPUD_STATIC_DETOUR(move->methodPointer, MoveCamera_Hook))
    ready = SPUD_STATIC_DETOUR(update->methodPointer, UpdateCameraPosition_Hook) != nullptr;
  spdlog::info("[NavigationOrbit] installed={} drag={} reset={}", ready,
               MapKey::GetShortcuts(GameFunction::NavigationOrbitDrag),
               MapKey::GetShortcuts(GameFunction::NavigationOrbitReset));
}
