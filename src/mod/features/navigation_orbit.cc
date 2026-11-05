// System-only mouse orbit, free pan and clip extension. Galaxy stays native.
// Default bindings are MOUSE2 drag and ALT-MOUSE2 reset.
#include "features/navigation_orbit.h"
#include "config.h"
#include "features/features.h"
#include "features/navigation_environment.h"
#include "shortcuts.h"

#include "game/navigation_orbit.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
namespace system_orbit = navigation_orbit_binding;
using OrbitVector3     = unity::Vector3;

struct NavigationOrbitState {
  il2cpp::GcHandle zoom, transform, pan;
  NodeDepth        depth = NodeDepth::Starbase;
  OrbitVector3     native_angles{};
  float            distance = 0;
  float            yaw = 0, pitch = 0, x = 0, y = 0;
  int32_t          frame     = -1;
  bool             installed = false, dragging = false, overridden = false;
};
NavigationOrbitState& NavigationOrbit()
{
  // One active rig; roots are cleared on owner/section change while Unity lives.
  // Avoid GC calls from a static destructor after the IL2CPP runtime is gone.
  static auto* state = new NavigationOrbitState;
  return *state;
}
bool NavigationOrbitEnabled()
{
  const auto& config = Config::Get();
  return NavigationOrbit().installed && config.patches.navigationorbithooks && config.control.hotkeys_enabled
         && !config.control.use_scopely_hotkeys && Shortcut::HasBinding(Action::kNavigationOrbitDrag);
}
void NavigationOrbitClear()
{
  navigation_environment::Clear();
  auto& state     = NavigationOrbit();
  auto* transform = state.transform.Get<unity::Transform>();
  if (state.overridden && transform) {
    const auto alive = unity::TryAlive(transform);
    if (alive && alive.value)
      (void)system_orbit::SetView(transform, state.native_angles, state.distance);
  }
  state.zoom          = {};
  state.transform     = {};
  state.pan           = {};
  state.depth         = NodeDepth::Starbase;
  state.native_angles = {};
  state.distance = state.yaw = state.pitch = state.x = state.y = 0;
  state.frame                                                  = -1;
  state.dragging = state.overridden = false;
}
void NavigationOrbitFail(const char* operation)
{
  NavigationOrbitClear();
  NavigationOrbit().installed = false;
  spdlog::warn("[NavigationOrbit] {} unavailable; native orientation restored and orbit disabled", operation);
}
bool NavigationOrbitSection()
{
  auto&      state = NavigationOrbit();
  auto*      zoom  = state.zoom.Get<NavigationZoom>();
  const auto depth = system_orbit::Field<"_depth", NodeDepth>(zoom);
  return zoom && depth && depth.value == NodeDepth::SolarSystem && state.depth == depth.value
         && system_orbit::InSystem();
}
void NavigationOrbitTick()
{
  auto& state = NavigationOrbit();
  if (!state.zoom)
    return;
  if (!NavigationOrbitEnabled() || !NavigationOrbitSection()) {
    NavigationOrbitClear();
    return;
  }
  const auto frame = unity::Time::frameCount();
  if (state.frame == frame)
    return;
  state.frame        = frame;
  const auto focused = unity::Application::Class::TryCall<"get_isFocused", bool>();
  const bool input   = focused && focused.value && !unity::Key::IsInputFocused();
  const bool held    = input && Shortcut::IsPressed(Action::kNavigationOrbitDrag);
  const bool reset   = input && Shortcut::IsDown(Action::kNavigationOrbitReset);
  if (!held && !reset) {
    state.dragging = false;
    return;
  }
  const auto allowed = system_orbit::PointerAvailable(state.dragging && held && !reset);
  if (!allowed) {
    NavigationOrbitFail("native pointer gate");
    return;
  }
  if (!allowed.value) {
    state.dragging = false;
    return;
  }
  auto* transform = state.transform.Get<unity::Transform>();
  bool  changed   = false;
  if (reset) {
    if (state.overridden && !system_orbit::SetView(transform, state.native_angles, state.distance)) {
      NavigationOrbitFail("reset camera transform");
      return;
    }
    changed          = state.overridden;
    state.overridden = state.dragging = false;
    navigation_environment::Clear();
    state.yaw = 0;
  } else {
    const auto position = unity::Input::TryMousePosition();
    if (!position || !system_orbit::Finite(position.value)) {
      state.dragging = false;
      return;
    }
    if (state.dragging) {
      constexpr float degrees_per_pixel = 0.15f;
      state.yaw        = std::remainder(state.yaw + (position.value.x - state.x) * degrees_per_pixel, 360.0f);
      state.pitch      = std::clamp(state.pitch - (position.value.y - state.y) * degrees_per_pixel, 5.0f, 90.0f);
      state.overridden = changed = true;
      if (!system_orbit::SetView(transform, {state.pitch, state.yaw, 0}, state.distance)) {
        NavigationOrbitFail("rotate camera transform");
        return;
      }
    } else if (Shortcut::IsDown(Action::kNavigationOrbitDrag)) {
      if (!state.overridden)
        state.pitch = std::clamp(state.native_angles.x, 5.0f, 90.0f);
      state.dragging = true;
    }
    state.x = position.value.x;
    state.y = position.value.y;
  }
  // Prevent orbit motion from becoming a spurious wheel-zoom translation.
  if (changed && !system_orbit::Reanchor(state.zoom.Get<NavigationZoom>()))
    NavigationOrbitFail("mouse-world zoom anchor");
}
bool NavigationOrbitFreePan(NavigationPan* pan)
{
  auto& state = NavigationOrbit();
  if (!state.overridden || !NavigationOrbitEnabled() || !NavigationOrbitSection())
    return false;
  const auto camera      = system_orbit::Field<"_sceneCamera", unity::Camera*>(pan);
  const auto zoom_camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(state.zoom.Get<NavigationZoom>());
  const auto depth       = system_orbit::Field<"_nodeDepth", NodeDepth>(pan);
  const auto tracking    = system_orbit::Field<"_trackingPOI", system_orbit::Poi*>(pan);
  return camera && zoom_camera && camera.value && camera.value == zoom_camera.value && depth
         && depth.value == state.depth && tracking && !tracking.value;
}
OrbitVector3 NavigationPan_ClampPositionInside_OrbitHook(auto original, NavigationPan* _this, OrbitVector3 position)
{
  // The native clamp controls both outward damping and spring return. Only an
  // actively rotated, untracked system pan loses that boundary.
  return NavigationOrbitFreePan(_this) && system_orbit::Finite(position) ? position : original(_this, position);
}
unity::Vector2 NavigationOrbitScreenPan(unity::Vector2 pixels, float distance, float fov, int32_t height, float yaw,
                                        float speed)
{
  constexpr double radians = 3.14159265358979323846 / 180.0;
  const double     scale   = 2.0 * distance * std::tan(fov * radians * 0.5) * speed / height;
  const double     x = pixels.x * scale, z = pixels.y * scale;
  const double     c = std::cos(yaw * radians), s = std::sin(yaw * radians);
  return {static_cast<float>(x * c + z * s), static_cast<float>(-x * s + z * c)};
}
void NavigationPan_MoveCamera_OrbitHook(auto original, NavigationPan* _this, unity::Vector2 delta, bool momentum)
{
  auto& state = NavigationOrbit();
  auto* zoom  = state.zoom.Get<NavigationZoom>();
  if (zoom && NavigationOrbitEnabled() && NavigationOrbitSection()) {
    const auto depth       = system_orbit::Field<"_nodeDepth", NodeDepth>(_this);
    const auto camera      = system_orbit::Field<"_sceneCamera", unity::Camera*>(_this);
    const auto zoom_camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(zoom);
    if (depth && depth.value == state.depth && camera && zoom_camera && camera.value
        && camera.value == zoom_camera.value && state.pan.Get() != _this)
      state.pan = il2cpp::GcHandle::Strong(_this);
  }
  if (!NavigationOrbitFreePan(_this)) {
    original(_this, delta, momentum);
    return;
  }
  if (!momentum) {
    const auto camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(_this);
    auto       root   = il2cpp::GcHandle::Strong(camera ? camera.value : nullptr);
    const auto fov =
        root ? root.Get<unity::Camera>()->TryCall<"get_fieldOfView", float>() : il2cpp::CallResult<float>{};
    const auto height =
        root ? root.Get<unity::Camera>()->TryCall<"get_pixelHeight", int32_t>() : il2cpp::CallResult<int32_t>{};
    const auto pixels     = system_orbit::Field<"_dragDelta", unity::Vector2>(_this);
    const auto speed      = system_orbit::Field<"_minPanSpeed", float>(_this);
    const auto normalized = system_orbit::Field<"<NormalizedZoom>k__BackingField", float>(zoom);
    if (!fov || !height || !pixels || !speed || !normalized || !std::isfinite(fov.value) || fov.value <= 0
        || fov.value >= 179 || height.value <= 0 || !std::isfinite(speed.value) || !std::isfinite(normalized.value)) {
      original(_this, unity::Vector2{}, momentum);
      return;
    }
    delta = NavigationOrbitScreenPan(pixels.value, state.distance, fov.value, height.value, state.yaw,
                                     1 - (1 - speed.value) * normalized.value);
  }
  original(_this, std::isfinite(delta.x) && std::isfinite(delta.y) ? delta : unity::Vector2{}, momentum);
}
void NavigationOrbitDrawDistance(NavigationZoom* zoom)
{
  auto& state = NavigationOrbit();
  if (!zoom || state.zoom.Get() != zoom || !state.pan || !state.overridden || !NavigationOrbitEnabled()
      || !NavigationOrbitSection())
    return;
  auto*      pan        = state.pan.Get<NavigationPan>();
  const auto camera     = system_orbit::Field<"_sceneCamera", unity::Camera*>(zoom);
  const auto pan_camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(pan);
  const auto depth      = system_orbit::Field<"_nodeDepth", NodeDepth>(pan);
  if (!camera || !camera.value || !pan_camera || camera.value != pan_camera.value || !depth
      || depth.value != state.depth)
    return;
  auto       camera_root    = il2cpp::GcHandle::Strong(camera.value);
  const auto transform      = system_orbit::Transform(camera.value);
  auto       transform_root = il2cpp::GcHandle::Strong(transform ? transform.value : nullptr);
  const auto position       = system_orbit::Vector(system_orbit::VectorGetter<"get_position">(), transform_root.Get());
  const auto baseline       = camera.value->TryCall<"get_farClipPlane", float>();
  const auto center         = system_orbit::Field<"_systemPosition", OrbitVector3>(pan);
  const auto radius         = system_orbit::Field<"_viewRadius", float>(pan);
  if (!position || !baseline || !center || !radius || !system_orbit::Finite(center.value)
      || !std::isfinite(baseline.value) || baseline.value <= 0 || !std::isfinite(radius.value) || radius.value <= 0)
    return;
  const double distance =
      std::hypot(double(position.value.x) - center.value.x, double(position.value.y) - center.value.y,
                 double(position.value.z) - center.value.z);
  const double required = distance + 4.0 * radius.value;
  if (required > baseline.value && required <= std::numeric_limits<float>::max())
    (void)camera.value->TryCall<"set_farClipPlane">(float(required));
  navigation_environment::ApplyDrawDistance(camera.value);
}
void NavigationZoom_UpdateCameraPosition_OrbitHook(auto original, NavigationZoom* _this)
{
  unity::Key::Poll();
  auto& state = NavigationOrbit();
  if (state.zoom && (!NavigationOrbitEnabled() || state.zoom.Get() != _this || !NavigationOrbitSection()))
    NavigationOrbitClear();
  original(_this);
  const auto depth = system_orbit::Field<"_depth", NodeDepth>(_this);
  if (!NavigationOrbitEnabled() || !depth || depth.value != NodeDepth::SolarSystem || !system_orbit::InSystem())
    return;
  const auto camera                = system_orbit::Field<"_sceneCamera", unity::Camera*>(_this);
  auto       camera_root           = il2cpp::GcHandle::Strong(camera ? camera.value : nullptr);
  const auto transform             = system_orbit::Transform(_this);
  const auto camera_transform      = system_orbit::Transform(camera_root.Get());
  auto       transform_root        = il2cpp::GcHandle::Strong(transform ? transform.value : nullptr);
  auto       camera_transform_root = il2cpp::GcHandle::Strong(camera_transform ? camera_transform.value : nullptr);
  const auto angles =
      system_orbit::Vector(system_orbit::VectorGetter<"get_localEulerAngles">(), camera_transform_root.Get());
  const auto distance = system_orbit::Field<"_actualDistance", float>(_this);
  if (!camera_root || !transform_root || !camera_transform_root || !angles || !distance
      || !std::isfinite(distance.value) || distance.value <= 0) {
    NavigationOrbitFail("camera rig query");
    return;
  }
  if (transform_root.Get() != camera_transform_root.Get() || std::abs(std::remainder(angles.value.y, 360.0f)) > 0.01f
      || std::abs(std::remainder(angles.value.z, 360.0f)) > 0.01f) {
    static bool reported = false;
    if (!std::exchange(reported, true))
      spdlog::warn("[NavigationOrbit] native rig differs; native view retained (same-transform={} yaw={} roll={})",
                   transform_root.Get() == camera_transform_root.Get(), angles.value.y, angles.value.z);
    NavigationOrbitClear();
    return;
  }
  if (!state.zoom) {
    auto zoom_root = il2cpp::GcHandle::Strong(_this);
    if (!zoom_root || !transform_root) {
      NavigationOrbitFail("camera rig retention");
      return;
    }
    state.zoom      = std::move(zoom_root);
    state.transform = std::move(transform_root);
    state.depth     = depth.value;
  }
  state.native_angles = angles.value;
  state.distance      = distance.value;
  if (state.overridden
      && !system_orbit::SetView(state.transform.Get<unity::Transform>(), {state.pitch, state.yaw, 0}, state.distance)) {
    NavigationOrbitFail("reapply camera rotation");
    return;
  }
  NavigationOrbitTick();
  NavigationOrbitDrawDistance(_this);
  if (state.overridden && NavigationOrbitEnabled())
    navigation_environment::Update(camera_root.Get<unity::Camera>());
}
bool InstallNavigationOrbit()
{
  if (!system_orbit::Available())
    return false;
  il2cpp::HookSet hooks("NavigationOrbit");
  IL2CPP_STAGE(hooks, NavigationPan::Class::Method<OrbitVector3(OrbitVector3)>("ClampPositionInside"),
               NavigationPan_ClampPositionInside_OrbitHook);
  IL2CPP_STAGE(hooks, NavigationPan::Class::Method<void(unity::Vector2, bool)>("MoveCamera"),
               NavigationPan_MoveCamera_OrbitHook);
  IL2CPP_STAGE(hooks, NavigationZoom::Class::Method<void()>("UpdateCameraPosition"),
               NavigationZoom_UpdateCameraPosition_OrbitHook);
  NavigationOrbit().installed = hooks.Commit();
  return NavigationOrbit().installed;
}
} // namespace

void navigation_orbit::ApplyDrawDistance(NavigationZoom* zoom)
{ NavigationOrbitDrawDistance(zoom); }

const Feature kNavigationOrbit{"NavigationOrbit", &Config::Patches::navigationorbithooks, InstallNavigationOrbit,
                               NavigationOrbitTick};
