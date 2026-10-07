#include "config.h"
#include "errormsg.h"
#include "settings/camera_settings.h"
#include "patches/mapkey.h"
#include "patches/screen_update_hook.h"

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Camera.h>
#include <prime/Hub.h>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <cmath>
#include <cstring>

void ApplyHavenRoadDepthBias(bool enabled);
void ApplyHavenWaterVisibility(bool hidden);
bool HavenWaterVisibilityAvailable();

namespace
{
bool installed = false;
Il2CppClass      *planetary_provider_class = nullptr;
FieldInfo        *blend_source = nullptr, *blend_target = nullptr, *blend_curve = nullptr;
FieldInfo        *blend_minimum = nullptr, *blend_maximum = nullptr, *blend_ratio = nullptr;
FieldInfo        *blend_source_frame = nullptr, *blend_target_frame = nullptr, *blend_result_frame = nullptr;
FieldInfo        *provider_constraint = nullptr, *provider_radius = nullptr, *constraint_radius = nullptr;
FieldInfo        *provider_pivot = nullptr, *provider_look_target = nullptr;
FieldInfo        *radius_enabled = nullptr, *radius_minimum = nullptr, *radius_maximum = nullptr;
FieldInfo        *frame_position = nullptr, *frame_rotation = nullptr, *frame_fov = nullptr;
FieldInfo        *frame_far_clip = nullptr, *frame_orthographic = nullptr;
const MethodInfo *curve_evaluate = nullptr;
FieldInfo *orbit_elevation = nullptr, *orbit_rotation = nullptr;
FieldInfo *constraint_elevation = nullptr;
const MethodInfo *orbit_event_system = nullptr, *orbit_pointer_over_ui = nullptr;
const MethodInfo *orbit_manager_instance = nullptr, *orbit_placement = nullptr;
bool orbit_ready = false;
Il2CppObject *orbit_source = nullptr, *orbit_target = nullptr;

struct HavenOrbitState {
  Il2CppGCHandle blend = nullptr;
  Il2CppGCHandle source = nullptr, target = nullptr;
  int frame = -1;
  float yaw = 0.0f, tilt = 0.0f;
  float native_yaw = 0.0f, native_elevation = 0.0f;
  float x = 0.0f, y = 0.0f;
  bool dragging = false;
};
HavenOrbitState orbit_state;

void ClearHavenOrbit()
{
  for (auto handle : {orbit_state.source, orbit_state.target}) {
    if (handle != nullptr) {
      if (auto *provider = il2cpp_gchandle_get_target(handle); provider != nullptr) {
        il2cpp_field_set_value(provider, orbit_rotation, &orbit_state.native_yaw);
        il2cpp_field_set_value(provider, orbit_elevation, &orbit_state.native_elevation);
      }
      il2cpp_gchandle_free(handle);
    }
  }
  if (orbit_state.blend != nullptr)
    il2cpp_gchandle_free(orbit_state.blend);
  orbit_state = {};
}

void UpdateHavenOrbitLifetime()
{
  if (orbit_state.blend == nullptr)
    return;
  auto *sections = Hub::get_SectionManager();
  if (!orbit_ready || sections == nullptr || sections->CurrentSection != SectionID::Starbase_Planetary)
    ClearHavenOrbit();
}

struct HavenRotation {
  float x, y, z, w;
};

FieldInfo *HavenField(Il2CppClass *cls, const char *name, const char *type)
{
  auto *field = cls != nullptr ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field != nullptr && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
                 && method_contract::Type(field->type, type)
             ? field
             : nullptr;
}

FieldInfo *HavenReferenceField(Il2CppClass *cls, const char *name, Il2CppClass *expected)
{
  auto *field = cls != nullptr ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field != nullptr && field->type != nullptr && !field->type->byref && expected != nullptr
                 && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC) && !il2cpp_class_is_valuetype(expected)
                 && il2cpp_class_from_type(field->type) == expected
             ? field
             : nullptr;
}

template <typename T> T ReadHavenField(Il2CppObject *object, FieldInfo *field)
{
  T value{};
  il2cpp_field_get_value(object, field, &value);
  return value;
}

bool ReadHavenRadius(Il2CppObject *provider, float &radius)
{
  if (provider == nullptr || !il2cpp_class_is_assignable_from(planetary_provider_class, provider->klass))
    return false;
  auto *constraint = ReadHavenField<Il2CppObject *>(provider, provider_constraint);
  auto *data       = constraint != nullptr ? ReadHavenField<Il2CppObject *>(constraint, constraint_radius) : nullptr;
  if (data == nullptr || !ReadHavenField<bool>(data, radius_enabled))
    return false;
  const auto minimum = ReadHavenField<float>(data, radius_minimum);
  const auto maximum = ReadHavenField<float>(data, radius_maximum);
  radius             = ReadHavenField<float>(provider, provider_radius);
  return std::isfinite(radius) && std::isfinite(maximum) && maximum > 0.0f && minimum == maximum
         && std::abs(radius - maximum) < 0.01f;
}

Il2CppObject *InvokeHavenOrbit(const MethodInfo *method, void *object, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  if (!Il2CppRuntime::TryInvoke(method, object, args, &result)) {
    orbit_ready = false;
    ClearHavenOrbit();
    spdlog::warn("[HavenOrbit] input API changed; keeping native orientation");
    return nullptr;
  }
  return result;
}

bool HavenOrbitPointerAvailable()
{
  auto *system = InvokeHavenOrbit(orbit_event_system, nullptr);
  if (system == nullptr)
    return false;
  int pointer = -1;
  void *args[]{&pointer};
  auto *over_ui = InvokeHavenOrbit(orbit_pointer_over_ui, system, args);
  bool blocked = true;
  if (!Il2CppRuntime::TryBoolean(over_ui, blocked) || blocked)
    return false;
  auto *manager = InvokeHavenOrbit(orbit_manager_instance, nullptr);
  auto *placement = manager != nullptr ? InvokeHavenOrbit(orbit_placement, manager) : nullptr;
  return Il2CppRuntime::TryBoolean(placement, blocked) && !blocked;
}

bool ReadHavenFixedAngle(Il2CppObject *provider, FieldInfo *axis, float &angle)
{
  auto *constraint = ReadHavenField<Il2CppObject *>(provider, provider_constraint);
  auto *data = constraint != nullptr ? ReadHavenField<Il2CppObject *>(constraint, axis) : nullptr;
  if (data == nullptr || !ReadHavenField<bool>(data, radius_enabled))
    return false;
  angle = ReadHavenField<float>(data, radius_minimum);
  const auto maximum = ReadHavenField<float>(data, radius_maximum);
  return std::isfinite(angle) && std::isfinite(maximum) && std::abs(angle - maximum) < 0.0001f;
}

void PrepareHavenOrbit(Il2CppObject *blend, Il2CppObject *source, Il2CppObject *target)
{
  const auto &config = Config::Get();
  auto *sections = Hub::get_SectionManager();
  float source_radius = 0.0f, target_radius = 0.0f;
  static auto frame_count = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
  static auto focused = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
  static auto mouse = il2cpp_resolve_icall_typed<void(Vector3 *)>(
      "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  if (!orbit_ready || !config.hotkeys_enabled || config.use_scopely_hotkeys
      || !MapKey::HasBinding(GameFunction::HavenOrbitDrag) || sections == nullptr
      || sections->CurrentSection != SectionID::Starbase_Planetary || frame_count == nullptr
      || focused == nullptr || mouse == nullptr || !ReadHavenRadius(source, source_radius)
      || !ReadHavenRadius(target, target_radius) || source_radius == target_radius) {
    ClearHavenOrbit();
    return;
  }
  auto *pivot = ReadHavenField<Il2CppObject *>(source, provider_pivot);
  float elevation = 0.0f, target_elevation = 0.0f, rotation = 0.0f, target_rotation = 0.0f;
  if (pivot == nullptr || ReadHavenField<Il2CppObject *>(target, provider_pivot) != pivot
      || ReadHavenField<Il2CppObject *>(source, provider_look_target) != pivot
      || ReadHavenField<Il2CppObject *>(target, provider_look_target) != pivot
      || !ReadHavenFixedAngle(source, constraint_elevation, elevation)
      || !ReadHavenFixedAngle(target, constraint_elevation, target_elevation)
      || std::abs(elevation - target_elevation) > 0.001f
      || elevation < 15.0f || elevation > 80.0f) {
    static bool warned = false;
    if (!warned) {
      spdlog::warn("[HavenOrbit] native view not qualified; keeping native orientation "
                   "(pitch={}/{} yaw={}/{})", elevation, target_elevation, rotation, target_rotation);
      warned = true;
    }
    ClearHavenOrbit();
    return;
  }
  const auto frame = frame_count();
  if (orbit_state.blend == nullptr || il2cpp_gchandle_get_target(orbit_state.blend) != blend
      || il2cpp_gchandle_get_target(orbit_state.source) != source
      || il2cpp_gchandle_get_target(orbit_state.target) != target
      || frame > orbit_state.frame + 1) {
    ClearHavenOrbit();
    rotation = ReadHavenField<float>(source, orbit_rotation);
    target_rotation = ReadHavenField<float>(target, orbit_rotation);
    if (!std::isfinite(rotation) || !std::isfinite(target_rotation)
        || std::abs(std::remainder(rotation - target_rotation, 360.0f)) > 0.001f)
      return;
    orbit_state.blend = il2cpp_gchandle_new(blend, false);
    orbit_state.source = il2cpp_gchandle_new(source, false);
    orbit_state.target = il2cpp_gchandle_new(target, false);
    orbit_state.native_yaw = rotation;
    orbit_state.native_elevation = elevation;
    spdlog::debug("[HavenOrbit] native pitch={} yaw={}; tilt limited to +/-15 degrees", elevation, rotation);
  }
  if (frame != orbit_state.frame) {
    orbit_state.frame = frame;
    const bool held = focused() && MapKey::IsPressed(GameFunction::HavenOrbitDrag);
    const bool reset = focused() && MapKey::IsDown(GameFunction::HavenOrbitReset);
    if (!held && !reset) {
      orbit_state.dragging = false;
    } else if (!HavenOrbitPointerAvailable()) {
      if (reset || MapKey::IsDown(GameFunction::HavenOrbitDrag))
        spdlog::debug("[HavenOrbit] gesture blocked by UI or active placement");
      // Require a fresh press after crossing UI or entering placement mode.
      orbit_state.dragging = false;
    } else if (reset) {
      spdlog::debug("[HavenOrbit] reset to native orientation");
      orbit_state.yaw = orbit_state.tilt = 0.0f;
      orbit_state.dragging = false;
    } else {
      Vector3 position{};
      mouse(&position);
      if (std::isfinite(position.x) && std::isfinite(position.y)) {
        if (orbit_state.dragging) {
          constexpr float degrees_per_pixel = 0.15f;
          orbit_state.yaw = std::remainder(orbit_state.yaw + (position.x - orbit_state.x) * degrees_per_pixel, 360.0f);
          // Keep a conservative tilt range around the game's measured native view.
          orbit_state.tilt = std::clamp(orbit_state.tilt - (position.y - orbit_state.y) * degrees_per_pixel,
                                       std::max(-15.0f, 15.0f - elevation), std::min(15.0f, 80.0f - elevation));
        }
        orbit_state.x = position.x;
        orbit_state.y = position.y;
        // A hold already in progress when entering Haven must not start a drag.
        const bool pressed = MapKey::IsDown(GameFunction::HavenOrbitDrag);
        if (pressed)
          spdlog::debug("[HavenOrbit] drag started at ({}, {})", position.x, position.y);
        orbit_state.dragging = orbit_state.dragging || pressed;
      } else {
        orbit_state.dragging = false;
      }
    }
  }
  if (orbit_ready) {
    orbit_source = source;
    orbit_target = target;
  }
}

void HavenOrbit_UpdateConstraints_Hook(auto original, Il2CppObject *provider, Camera *camera)
{
  original(provider, camera);
  if (!orbit_ready || (provider != orbit_source && provider != orbit_target))
    return;
  float elevation = 0.0f;
  if (!ReadHavenFixedAngle(provider, constraint_elevation, elevation))
    return;
  elevation += orbit_state.tilt;
  auto rotation = orbit_state.native_yaw + orbit_state.yaw;
  // Modify this update's runtime angles after native constraints, never the
  // shared constraint assets. Native frame generation then also uses the new
  // orientation when converting pan input into world movement.
  il2cpp_field_set_value(provider, orbit_elevation, &elevation);
  il2cpp_field_set_value(provider, orbit_rotation, &rotation);
}

void HavenOrbit_UpdateInputData_Hook(auto original, Il2CppObject *provider, Camera *camera)
{
  struct OrbitScope {
    Il2CppObject *source = orbit_source, *target = orbit_target;
    ~OrbitScope() { orbit_source = source; orbit_target = target; }
  } scope;
  orbit_source = orbit_target = nullptr;
  if (provider != nullptr)
    PrepareHavenOrbit(provider, ReadHavenField<Il2CppObject *>(provider, blend_source),
                      ReadHavenField<Il2CppObject *>(provider, blend_target));
  // Blend input updates both endpoints through UpdateProvider. Keep the scope
  // active through their constraints and frame generation, before interpolation.
  original(provider, camera);
}

void HavenCamera_UpdateCameraFrame_Hook(auto original, Il2CppObject *provider, Camera *camera)
{
  // Native input, constraints and endpoint updates run first. Only this blend's
  // newly generated output is extended; endpoint assets and normalized LOD stay native.
  original(provider, camera);
  if (provider == nullptr)
    return;

  auto *source        = ReadHavenField<Il2CppObject *>(provider, blend_source);
  auto *target        = ReadHavenField<Il2CppObject *>(provider, blend_target);
  float source_radius = 0.0f, target_radius = 0.0f;
  if (!ReadHavenRadius(source, source_radius) || !ReadHavenRadius(target, target_radius)
      || source_radius == target_radius)
    return;

  auto *pivot = ReadHavenField<Il2CppObject *>(source, provider_pivot);
  if (pivot == nullptr || ReadHavenField<Il2CppObject *>(target, provider_pivot) != pivot
      || ReadHavenField<Il2CppObject *>(source, provider_look_target) != pivot
      || ReadHavenField<Il2CppObject *>(target, provider_look_target) != pivot)
    return;
  auto      *source_frame = ReadHavenField<Il2CppObject *>(provider, blend_source_frame);
  auto      *target_frame = ReadHavenField<Il2CppObject *>(provider, blend_target_frame);
  auto      *result       = ReadHavenField<Il2CppObject *>(provider, blend_result_frame);
  auto      *curve        = ReadHavenField<Il2CppObject *>(provider, blend_curve);
  const auto minimum      = ReadHavenField<float>(provider, blend_minimum);
  const auto maximum      = ReadHavenField<float>(provider, blend_maximum);
  const auto ratio        = ReadHavenField<float>(provider, blend_ratio);
  if (source_frame == nullptr || target_frame == nullptr || result == nullptr || curve == nullptr
      || source_frame == target_frame || result == source_frame || result == target_frame || !std::isfinite(minimum)
      || !std::isfinite(maximum) || !std::isfinite(ratio) || minimum < 0.0f || maximum > 1.0f || maximum <= minimum)
    return;

  auto       position        = ReadHavenField<Vector3>(result, frame_position);
  const auto ar              = ReadHavenField<HavenRotation>(source_frame, frame_rotation);
  const auto br              = ReadHavenField<HavenRotation>(target_frame, frame_rotation);
  const auto af              = ReadHavenField<float>(source_frame, frame_fov);
  const auto bf              = ReadHavenField<float>(target_frame, frame_fov);
  const auto rotation_dot    = ar.x * br.x + ar.y * br.y + ar.z * br.z + ar.w * br.w;
  const auto gap             = std::abs(source_radius - target_radius);
  const auto rotation_length = ar.x * ar.x + ar.y * ar.y + ar.z * ar.z + ar.w * ar.w;
  // Qualify the common-pivot, same-facing perspective orbit measured in Haven.
  // Preserve native output when a client changes that geometry or camera mode.
  if (!std::isfinite(rotation_length) || std::abs(rotation_length - 1.0f) > 0.0001f || !std::isfinite(rotation_dot)
      || std::abs(std::abs(rotation_dot) - 1.0f) > 0.0001f || !std::isfinite(af) || !std::isfinite(bf) || af <= 0.0f
      || std::abs(af - bf) > 0.001f || ReadHavenField<bool>(source_frame, frame_orthographic)
      || ReadHavenField<bool>(target_frame, frame_orthographic) || ReadHavenField<bool>(result, frame_orthographic))
    return;

  ApplyHavenWaterVisibility(Config::Get().hide_haven_water);
  const auto max_zoom = Config::Get().haven_zoom;
  const bool expanded = std::isfinite(max_zoom) && max_zoom > std::max(source_radius, target_radius);
  ApplyHavenRoadDepthBias(expanded);
  if (!expanded)
    return;

  const auto evaluate =
      reinterpret_cast<float (*)(Il2CppObject *, float, const MethodInfo *)>(curve_evaluate->methodPointer);
  const auto at_ratio   = evaluate(curve, ratio, curve_evaluate);
  const auto at_minimum = evaluate(curve, minimum, curve_evaluate);
  const auto at_maximum = evaluate(curve, maximum, curve_evaluate);
  if (!std::isfinite(at_ratio) || !std::isfinite(at_minimum) || !std::isfinite(at_maximum))
    return;
  const bool source_is_far = source_radius > target_radius;
  const auto outer_weight =
      source_is_far ? 1.0f - std::clamp(at_minimum, 0.0f, 1.0f) : std::clamp(at_maximum, 0.0f, 1.0f);
  const auto inner_weight =
      source_is_far ? 1.0f - std::clamp(at_maximum, 0.0f, 1.0f) : std::clamp(at_minimum, 0.0f, 1.0f);
  if (outer_weight <= inner_weight)
    return;
  const auto weight       = source_is_far ? 1.0 - std::clamp(at_ratio, 0.0f, 1.0f) : std::clamp(at_ratio, 0.0f, 1.0f);
  const auto inner_radius = std::min(source_radius, target_radius);
  const auto native_outer = inner_radius + gap * outer_weight;
  const auto native_distance = inner_radius + gap * weight;
  const auto progress        = std::clamp((weight - inner_weight) / (outer_weight - inner_weight), 0.0, 1.0);
  const auto extra           = std::min(max_zoom - native_distance, (max_zoom - native_outer) * progress);
  if (!std::isfinite(extra))
    return;
  if (extra <= 0.0)
    return;

  // Sequential endpoint updates can cache different positions of their shared
  // pivot during pan. Use the camera back axis so expansion remains steady.
  position.x += static_cast<float>(-2.0 * (ar.x * ar.z + ar.w * ar.y) * extra);
  position.y += static_cast<float>(-2.0 * (ar.y * ar.z - ar.w * ar.x) * extra);
  position.z += static_cast<float>((2.0 * (ar.x * ar.x + ar.y * ar.y) - 1.0) * extra);
  auto clip = ReadHavenField<float>(result, frame_far_clip);
  if (!std::isfinite(clip) || clip <= 0.0f)
    return;
  clip += static_cast<float>(extra);
  if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(clip)
      || clip <= 0.0f)
    return;
  il2cpp_field_set_value(result, frame_position, &position);
  il2cpp_field_set_value(result, frame_far_clip, &clip);
  static bool reported = false;
  if (!reported) {
    spdlog::info("[HavenZoom] expanded native outer distance {} to maximum {}", native_outer, max_zoom);
    reported = true;
  }
}
} // namespace

namespace mod_settings
{
bool HavenCameraControlAvailable()
{ return installed && Config::Get().installHavenZoomHooks; }
bool HavenWaterControlAvailable()
{ return HavenCameraControlAvailable() && HavenWaterVisibilityAvailable(); }
} // namespace mod_settings

void InstallHavenZoomHooks()
{
  auto blend = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "BlendFrameProvider");
  auto frame_provider = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "FrameProvider");
  auto orbit =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "AbstractOrbitFrameProvider");
  auto target = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "AbstractTargetController");
  auto planetary =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "PlanetaryOrbitFrameProvider");
  auto orbit_constraint =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitConstraint");
  auto constraint = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "Constraint");
  auto frame      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "CameraFrame");
  auto editable   = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "EditableCameraFrame");
  auto curve      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "AnimationCurve");
  Il2CppClass *radius_class = nullptr;
  if (constraint.get_cls() != nullptr) {
    void *iterator = nullptr;
    while (auto *nested = il2cpp_class_get_nested_types(constraint.get_cls(), &iterator)) {
      if (std::strcmp(il2cpp_class_get_name(nested), "FloatData") == 0) {
        radius_class = nested;
        break;
      }
    }
  }
  const auto *update =
      method_contract::Resolve(blend.get_cls(), "UpdateCameraFrame", false, "System.Void", {"UnityEngine.Camera"});
  curve_evaluate = method_contract::Resolve(curve.get_cls(), "Evaluate", false, "System.Single", {"System.Single"});
  planetary_provider_class = planetary.get_cls();
  blend_source             = HavenReferenceField(blend.get_cls(), "_sourceFrameProvider", frame_provider.get_cls());
  blend_target             = HavenReferenceField(blend.get_cls(), "_targetFrameProvider", frame_provider.get_cls());
  blend_curve              = HavenReferenceField(blend.get_cls(), "_blendCurve", curve.get_cls());
  blend_minimum            = HavenField(blend.get_cls(), "_softLimitMin", "System.Single");
  blend_maximum            = HavenField(blend.get_cls(), "_softLimitMax", "System.Single");
  blend_ratio              = HavenField(blend.get_cls(), "_blendRatio", "System.Single");
  blend_source_frame       = HavenReferenceField(blend.get_cls(), "_sourceFrame", frame.get_cls());
  blend_target_frame       = HavenReferenceField(blend.get_cls(), "_targetFrame", frame.get_cls());
  blend_result_frame       = HavenReferenceField(blend.get_cls(), "_blendedCameraFrame", editable.get_cls());
  provider_constraint      = HavenReferenceField(orbit.get_cls(), "_constraint", orbit_constraint.get_cls());
  provider_radius          = HavenField(orbit.get_cls(), "_radius", "System.Single");
  provider_pivot           = HavenReferenceField(orbit.get_cls(), "_orbitPivot", target.get_cls());
  provider_look_target     = HavenReferenceField(orbit.get_cls(), "_lookTarget", target.get_cls());
  constraint_radius        = HavenReferenceField(orbit_constraint.get_cls(), "_radius", radius_class);
  radius_enabled           = HavenField(radius_class, "_enabled", "System.Boolean");
  radius_minimum           = HavenField(radius_class, "_min", "System.Single");
  radius_maximum           = HavenField(radius_class, "_max", "System.Single");
  frame_position           = HavenField(frame.get_cls(), "_position", "UnityEngine.Vector3");
  frame_rotation           = HavenField(frame.get_cls(), "_rotation", "UnityEngine.Quaternion");
  frame_fov                = HavenField(frame.get_cls(), "_FOV", "System.Single");
  frame_far_clip           = HavenField(frame.get_cls(), "_farPlane", "System.Single");
  frame_orthographic       = HavenField(frame.get_cls(), "_orthographic", "System.Boolean");
  if (update == nullptr || update->has_full_generic_sharing_signature || curve_evaluate == nullptr
      || curve_evaluate->has_full_generic_sharing_signature || planetary_provider_class == nullptr
      || orbit.get_cls() == nullptr || !il2cpp_class_is_assignable_from(orbit.get_cls(), planetary_provider_class)) {
    spdlog::warn("[HavenZoom] camera API unavailable; keeping native zoom range");
    return;
  }
  for (auto *field :
       {blend_source,       blend_target,        blend_curve,          blend_minimum,      blend_maximum,
        blend_ratio,        provider_pivot,      provider_look_target, blend_source_frame, blend_target_frame,
        blend_result_frame, provider_constraint, provider_radius,      constraint_radius,  radius_enabled,
        radius_minimum,     radius_maximum,      frame_position,       frame_rotation,     frame_fov,
        frame_far_clip,     frame_orthographic}) {
    if (field == nullptr) {
      spdlog::warn("[HavenZoom] camera fields unavailable; keeping native zoom range");
      return;
    }
  }
  // [patches].havenzoomhooks controls installation independently of haven_zoom.
  if (SPUD_STATIC_DETOUR(update->methodPointer, HavenCamera_UpdateCameraFrame_Hook)) {
    installed = true;
    spdlog::info("[HavenZoom] installed planetary blend camera hook (maximum distance {})", Config::Get().haven_zoom);
    auto orbital = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitFrameProvider");
    auto events = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
    auto starbase = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlanetaryBase.PlanetaryStarbase",
                                         "PlanetaryStarbaseManager");
    auto *parent = starbase.get_cls() != nullptr ? il2cpp_class_get_parent(starbase.get_cls()) : nullptr;
    orbit_manager_instance = parent != nullptr ? il2cpp_class_get_method_from_name(parent, "get_Instance", 0) : nullptr;
    const auto singleton_valid = orbit_manager_instance != nullptr && orbit_manager_instance->methodPointer
        && (orbit_manager_instance->flags & METHOD_ATTRIBUTE_STATIC) && orbit_manager_instance->parameters_count == 0
        && method_contract::Type(orbit_manager_instance->return_type,
                                 "Digit.Prime.PlanetaryBase.PlanetaryStarbase.PlanetaryStarbaseManager");
    orbit_placement = method_contract::Resolve(starbase.get_cls(), "get_HasActivePlacement", false, "System.Boolean", {});
    orbit_event_system = method_contract::Resolve(events.get_cls(), "get_current", true,
                                                 "UnityEngine.EventSystems.EventSystem", {});
    orbit_pointer_over_ui = method_contract::Resolve(events.get_cls(), "IsPointerOverGameObject", false,
                                                    "System.Boolean", {"System.Int32"});
    orbit_elevation = HavenField(orbit.get_cls(), "_elevationAngle", "System.Single");
    orbit_rotation = HavenField(orbit.get_cls(), "_rotationAngle", "System.Single");
    constraint_elevation = HavenReferenceField(orbit_constraint.get_cls(), "_elevation", radius_class);
    const auto *constrain = method_contract::Resolve(orbital.get_cls(), "UpdateConstrains", false,
                                                   "System.Void", {"UnityEngine.Camera"});
    const auto *input = method_contract::Resolve(blend.get_cls(), "UpdateInputData", false,
                                               "System.Void", {"UnityEngine.Camera"});
    if (singleton_valid && orbit_placement != nullptr && orbit_event_system != nullptr && orbit_pointer_over_ui != nullptr
        && orbit_elevation != nullptr && orbit_rotation != nullptr && constraint_elevation != nullptr
        && constrain != nullptr && !constrain->has_full_generic_sharing_signature
        && input != nullptr && !input->has_full_generic_sharing_signature
        && install_screen_manager_update_hook() && register_screen_manager_update_callback(UpdateHavenOrbitLifetime)) {
      orbit_ready = SPUD_STATIC_DETOUR(constrain->methodPointer, HavenOrbit_UpdateConstraints_Hook)
                    && SPUD_STATIC_DETOUR(input->methodPointer, HavenOrbit_UpdateInputData_Hook);
      spdlog::info("[HavenOrbit] installed={} drag={} reset={}", orbit_ready,
                   MapKey::GetShortcuts(GameFunction::HavenOrbitDrag), MapKey::GetShortcuts(GameFunction::HavenOrbitReset));
    } else {
      spdlog::warn("[HavenOrbit] camera/input API unavailable; keeping native orientation "
                   "(singleton={} placement={} event={} pointer={} elevation={} rotation={} constraints={} update={} input={})",
                   singleton_valid, orbit_placement != nullptr, orbit_event_system != nullptr,
                   orbit_pointer_over_ui != nullptr, orbit_elevation != nullptr, orbit_rotation != nullptr,
                   constraint_elevation != nullptr, constrain != nullptr, input != nullptr);
    }
  } else {
    spdlog::warn("[HavenZoom] camera hook was not installed; keeping native zoom range");
  }
}
