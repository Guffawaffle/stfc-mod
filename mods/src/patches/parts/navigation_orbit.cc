#include "config.h"
#include "errormsg.h"
#include "patches/key.h"
#include "patches/mapkey.h"
#include "patches/screen_update_hook.h"
#include <algorithm>
#include <cmath>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Hub.h>
#include <prime/NavigationZoom.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

bool KeyboardPanAllowed();

namespace
{
bool              ready        = false;
FieldInfo        *scene_camera = nullptr, *distance_field = nullptr, *depth_field = nullptr;
FieldInfo        *zoom_location = nullptr, *world_point = nullptr;
const MethodInfo *get_transform = nullptr, *get_angles = nullptr, *set_angles = nullptr;
const MethodInfo *get_forward = nullptr, *set_position = nullptr, *mouse_world = nullptr;
const MethodInfo *get_local_position = nullptr;
const MethodInfo *clamp_pan_position = nullptr;
const MethodInfo *event_system = nullptr, *pointer_over_ui = nullptr, *can_move = nullptr;
FieldInfo *pan_camera = nullptr, *pan_depth = nullptr, *pan_radius = nullptr, *pan_soft_range = nullptr;
FieldInfo *pan_far_normal = nullptr, *pan_far_extended = nullptr, *pan_near_normal = nullptr, *pan_near_extended = nullptr;
FieldInfo *pan_return_coeff = nullptr;
int (*frame_count)()              = nullptr;
bool (*focused)()                 = nullptr;
void (*mouse_position)(Vector3 *) = nullptr;

struct OrbitState {
  Il2CppGCHandle zoom = nullptr, transform = nullptr;
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

FieldInfo *Field(Il2CppClass *cls, const char *name, const char *type)
{
  auto *field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
                 && field->offset >= static_cast<int>(sizeof(Il2CppObject)) && method_contract::Type(field->type, type)
             ? field
             : nullptr;
}

template <typename T> T Read(Il2CppObject *object, FieldInfo *field)
{
  T value{};
  il2cpp_field_get_value(object, field, &value);
  return value;
}

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
  if (state.overridden && state.transform)
    SetView(il2cpp_gchandle_get_target(state.transform), state.native_angles, state.distance);
  if (state.zoom)
    il2cpp_gchandle_free(state.zoom);
  if (state.transform)
    il2cpp_gchandle_free(state.transform);
  state = {};
}

void Fail()
{
  Clear();
  ready = false;
  spdlog::warn("[NavigationOrbit] camera/input API changed; keeping native orientation");
}

bool SectionMatches(NodeDepth depth)
{
  auto *sections = Hub::get_SectionManager();
  // Galaxy rendering assumes the native view; rotating it exposes layers that
  // are not intended to be visible. Keep its camera entirely native.
  return sections && depth == NodeDepth::SolarSystem
         && sections->CurrentSection == SectionID::Navigation_System;
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
    Fail();
    return false;
  }
  if (!allowed || !KeyboardPanAllowed())
    return false;
  if (!Il2CppRuntime::TryInvoke(event_system, nullptr, nullptr, &system)) {
    Fail();
    return false;
  }
  if (!system)
    return false;
  int   pointer = -1;
  void *args[]{&pointer};
  bool  blocked = true;
  if (!Il2CppRuntime::TryInvoke(pointer_over_ui, system, args, &result)
      || !Il2CppRuntime::TryBoolean(result, blocked)) {
    Fail();
    return false;
  }
  return !blocked || continuing;
}

bool Reanchor(Il2CppObject *zoom)
{
  auto      *camera   = Read<Il2CppObject *>(zoom, scene_camera);
  const auto location = Read<vec2>(zoom, zoom_location);
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
  if (!Enabled() || !zoom || !SectionMatches(state.depth) || Read<NodeDepth>(zoom, depth_field) != state.depth) {
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
      Fail();
      return;
    }
    changed          = state.overridden;
    state.overridden = state.dragging = false;
    state.yaw                         = 0.0f;
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
        Fail();
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
    Fail();
}

vec2 LimitPanDelta(Vector3 position, vec2 delta, double radius)
{
  if (!std::isfinite(delta.x) || !std::isfinite(delta.y))
    return {};
  // Native MoveCamera subtracts (delta.x, delta.y) from local (x, z).
  // Use doubles so a finite but enormous ray intersection cannot overflow.
  const double x = static_cast<double>(position.x) - delta.x;
  const double z = static_cast<double>(position.z) - delta.y;
  const double distance = std::hypot(x, z);
  // A target may have positioned the native camera outside our user-pan limit.
  // Permit its existing smooth return; do not snap it back on a zero/inward move.
  const double limit = std::max(radius, std::hypot(static_cast<double>(position.x), static_cast<double>(position.z)));
  if (distance <= limit)
    return delta;
  const double scale = limit / distance;
  vec2 bounded{static_cast<float>(position.x - x * scale), static_cast<float>(position.z - z * scale)};
  return std::isfinite(bounded.x) && std::isfinite(bounded.y) ? bounded : vec2{};
}

void MoveCamera_Hook(auto original, Il2CppObject *pan, vec2 delta, bool momentum)
{
  auto *zoom = state.zoom ? il2cpp_gchandle_get_target(state.zoom) : nullptr;
  if (!pan || !zoom || !state.overridden || !Enabled() || !SectionMatches(state.depth)
      || Read<NodeDepth>(zoom, depth_field) != state.depth
      || Read<NodeDepth>(pan, pan_depth) != NodeDepth::SolarSystem
      || Read<Il2CppObject *>(pan, pan_camera) != Read<Il2CppObject *>(zoom, scene_camera)) {
    original(pan, delta, momentum);
    return;
  }
  const double radius = Read<float>(pan, pan_radius);
  const double soft = Read<float>(pan, pan_soft_range);
  const double far_normal = Read<float>(pan, pan_far_normal), far_extended = Read<float>(pan, pan_far_extended);
  const double near_normal = Read<float>(pan, pan_near_normal), near_extended = Read<float>(pan, pan_near_extended);
  // Conservative outer bound for either native zoom mode. The native near
  // contribution shrinks with normalized zoom; retain its full maximum here.
  const double limit = radius * (std::max(far_normal, far_extended) + std::max(near_normal, near_extended)) + soft;
  Il2CppObject *transform = nullptr;
  Vector3 position{};
  if (!std::isfinite(radius) || radius <= 0.0 || !std::isfinite(soft) || soft < 0.0
      || !std::isfinite(far_normal) || far_normal < 0.0 || !std::isfinite(far_extended) || far_extended < 0.0
      || !std::isfinite(near_normal) || near_normal < 0.0 || !std::isfinite(near_extended) || near_extended < 0.0
      || !std::isfinite(limit) || limit <= 0.0
      || !Il2CppRuntime::TryInvoke(get_transform, pan, nullptr, &transform)
      || !transform || !ReadVector(get_local_position, transform, position)) {
    Fail();
    original(pan, vec2{}, false);
    return;
  }
  const double outer = std::max(limit, std::hypot(static_cast<double>(position.x), static_cast<double>(position.z)));
  if (momentum) {
    // Native MoveCamera adds its inward boundary return AFTER damping delta.
    // Project from the position after that return, keeping native bookkeeping.
    const double coefficient = Read<float>(pan, pan_return_coeff);
    Vector3 clamped{};
    void *args[]{&position};
    if (!std::isfinite(coefficient) || coefficient < 0.0 || coefficient > 1.0
        || !ReadVector(clamp_pan_position, pan, clamped, args)) {
      Fail();
      original(pan, vec2{}, false);
      return;
    }
    position.x = static_cast<float>(position.x - (static_cast<double>(position.x) - clamped.x) * coefficient);
    position.z = static_cast<float>(position.z - (static_cast<double>(position.z) - clamped.z) * coefficient);
  }
  const auto bounded = LimitPanDelta(position, delta, outer);
#ifdef _MODDBG
  static unsigned reports = 0;
  if ((bounded.x != delta.x || bounded.y != delta.y) && reports < 10) {
    ++reports;
    spdlog::debug("[NavigationOrbit] pan limited delta=({}, {}) bounded=({}, {}) radius={}",
                  delta.x, delta.y, bounded.x, bounded.y, limit);
  }
#endif
  original(pan, bounded, momentum);
}

void UpdateCameraPosition_Hook(auto original, Il2CppObject *zoom)
{
  if (state.zoom
      && (!Enabled() || !zoom || il2cpp_gchandle_get_target(state.zoom) != zoom
          || Read<NodeDepth>(zoom, depth_field) != state.depth || !SectionMatches(state.depth)))
    Clear();
  original(zoom);
  if (!zoom || !Enabled() || !SectionMatches(Read<NodeDepth>(zoom, depth_field)))
    return;
  const auto    depth     = Read<NodeDepth>(zoom, depth_field);
  auto         *camera    = Read<Il2CppObject *>(zoom, scene_camera);
  Il2CppObject *transform = nullptr, *camera_transform = nullptr;
  Vector3       angles{};
  const auto    distance = Read<float>(zoom, distance_field);
  if (!camera || !Il2CppRuntime::TryInvoke(get_transform, zoom, nullptr, &transform)
      || !Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &camera_transform) || !transform || !camera_transform
      || !ReadVector(get_angles, camera_transform, angles) || !std::isfinite(distance) || distance <= 0.0f) {
    Fail();
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
      Fail();
      return;
    }
    state.depth = depth;
    spdlog::debug("[NavigationOrbit] qualified depth={} native-pitch={} distance={}", static_cast<int>(depth), angles.x,
                  distance);
  }
  state.native_angles = angles;
  state.distance      = distance;
  if (state.overridden && !SetView(transform, {state.pitch, state.yaw, 0.0f}, distance)) {
    Fail();
    return;
  }
  Tick();
}
} // namespace

void InstallNavigationOrbitHooks()
{
  auto        zoom       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom");
  auto        navigation = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationCamera");
  auto        pan        = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPan");
  auto        component  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  auto        transform  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  auto        events     = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
  auto        math       = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils");
  const auto *update     = method_contract::Resolve(zoom.get_cls(), "UpdateCameraPosition", false, "System.Void", {});
  const auto *move = method_contract::Resolve(pan.get_cls(), "MoveCamera", false, "System.Void",
                                              {"UnityEngine.Vector2", "System.Boolean"});
  get_transform = method_contract::Resolve(component.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
  get_angles  = method_contract::Resolve(transform.get_cls(), "get_localEulerAngles", false, "UnityEngine.Vector3", {});
  get_local_position = method_contract::Resolve(transform.get_cls(), "get_localPosition", false, "UnityEngine.Vector3", {});
  clamp_pan_position = method_contract::Resolve(pan.get_cls(), "ClampPositionInside", false, "UnityEngine.Vector3",
                                                {"UnityEngine.Vector3"});
  set_angles  = method_contract::Resolve(transform.get_cls(), "set_localEulerAngles", false, "System.Void",
                                         {"UnityEngine.Vector3"});
  get_forward = method_contract::Resolve(transform.get_cls(), "get_forward", false, "UnityEngine.Vector3", {});
  set_position =
      method_contract::Resolve(transform.get_cls(), "set_localPosition", false, "System.Void", {"UnityEngine.Vector3"});
  event_system =
      method_contract::Resolve(events.get_cls(), "get_current", true, "UnityEngine.EventSystems.EventSystem", {});
  pointer_over_ui =
      method_contract::Resolve(events.get_cls(), "IsPointerOverGameObject", false, "System.Boolean", {"System.Int32"});
  can_move       = method_contract::Resolve(navigation.get_cls(), "CanMoveCamera", true, "System.Boolean", {});
  mouse_world    = method_contract::Resolve(math.get_cls(), "GetMouseWorldPos", true, "UnityEngine.Vector3",
                                            {"UnityEngine.Camera", "UnityEngine.Vector3"});
  scene_camera   = Field(zoom.get_cls(), "_sceneCamera", "UnityEngine.Camera");
  distance_field = Field(zoom.get_cls(), "_actualDistance", "System.Single");
  depth_field    = Field(zoom.get_cls(), "_depth", "Digit.PrimeServer.Models.NodeDepth");
  zoom_location  = Field(zoom.get_cls(), "_zoomLocation", "UnityEngine.Vector2");
  world_point    = Field(zoom.get_cls(), "_worldPoint", "UnityEngine.Vector3");
  pan_camera = Field(pan.get_cls(), "_sceneCamera", "UnityEngine.Camera");
  pan_depth = Field(pan.get_cls(), "_nodeDepth", "Digit.PrimeServer.Models.NodeDepth");
  pan_radius = Field(pan.get_cls(), "_viewRadius", "System.Single");
  pan_soft_range = Field(pan.get_cls(), "_softClampRange", "System.Single");
  pan_far_normal = Field(pan.get_cls(), "_farMagRadiusRatioSystemNormal", "System.Single");
  pan_far_extended = Field(pan.get_cls(), "_farMagRadiusRatioSystemExtended", "System.Single");
  pan_near_normal = Field(pan.get_cls(), "_nearMagRadiusRatioSystemNormal", "System.Single");
  pan_near_extended = Field(pan.get_cls(), "_nearMagRadiusRatioSystemExtended", "System.Single");
  pan_return_coeff = Field(pan.get_cls(), "_outOfBoundsReturnCoeff", "System.Single");
  frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
  focused        = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
  mouse_position = il2cpp_resolve_icall_typed<void(Vector3 *)>(
      "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  if (!method_contract::Pointer(update) || update->has_full_generic_sharing_signature || !method_contract::Pointer(move)
      || move->has_full_generic_sharing_signature || !get_transform || !get_angles || !get_local_position || !clamp_pan_position
      || !set_angles || !get_forward || !set_position || !event_system || !pointer_over_ui || !can_move || !mouse_world
      || !scene_camera || !distance_field || !depth_field || !zoom_location || !world_point || !frame_count || !focused
      || !pan_camera || !pan_depth || !pan_radius || !pan_soft_range || !pan_far_normal || !pan_far_extended
      || !pan_near_normal || !pan_near_extended || !pan_return_coeff
      || !mouse_position || !install_screen_manager_update_hook() || !register_screen_manager_update_callback(Tick)) {
    spdlog::warn("[NavigationOrbit] camera/input API unavailable; keeping native orientation");
    return;
  }
  if (SPUD_STATIC_DETOUR(move->methodPointer, MoveCamera_Hook))
    ready = SPUD_STATIC_DETOUR(update->methodPointer, UpdateCameraPosition_Hook) != nullptr;
  spdlog::info("[NavigationOrbit] installed={} drag={} reset={}", ready,
               MapKey::GetShortcuts(GameFunction::NavigationOrbitDrag),
               MapKey::GetShortcuts(GameFunction::NavigationOrbitReset));
}
