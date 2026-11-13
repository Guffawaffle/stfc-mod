// System-only mouse orbit, free pan and clip extension. Galaxy stays native.
// Default bindings are MOUSE2 drag and ALT-MOUSE2 reset.
#include "features/navigation_orbit.h"
#include "config.h"
#include "features/features.h"
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
  il2cpp::GcHandle zoom, camera, transform, pan, clip_camera;
  NodeDepth        depth = NodeDepth::Starbase;
  OrbitVector3     native_angles{};
  float            distance    = 0;
  float            native_clip = 0, applied_clip = 0;
  float            yaw = 0, pitch = 0, x = 0, y = 0;
  int32_t          frame         = -1;
  uint64_t         view_revision = 0;
  bool             installed = false, dragging = false, overridden = false, clearing = false;
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
  return NavigationOrbit().installed && !NavigationOrbit().clearing && config.graphics.system_camera_orbit
         && config.control.hotkeys_enabled && !config.control.use_scopely_hotkeys
         && Shortcut::HasBinding(Action::kNavigationOrbitDrag);
}
bool NavigationOrbitRestoreClip()
{
  auto& state  = NavigationOrbit();
  auto* camera = state.clip_camera.Get<unity::Camera>();
  if (camera) {
    const auto alive = unity::TryAlive(camera);
    if (!alive)
      return false;
    if (alive.value) {
      const auto clip = camera->TryCall<"get_farClipPlane", float>();
      if (!clip)
        return false;
      // Restore only a value still owned by orbit. A later zoom/native owner wins.
      if (clip.value == state.applied_clip && !camera->TryCall<"set_farClipPlane">(state.native_clip))
        return false;
    }
  }
  state.clip_camera = {};
  state.native_clip = state.applied_clip = 0;
  return true;
}
void NavigationOrbitClear(bool restore_view = true)
{
  auto& state = NavigationOrbit();
  if (state.clearing)
    return;
  struct ClearScope {
    bool& active;
    explicit ClearScope(bool& flag)
        : active(flag)
    { active = true; }
    ~ClearScope()
    { active = false; }
  } clearing(state.clearing);
  auto* transform = state.transform.Get<unity::Transform>();
  if (restore_view && state.overridden && transform) {
    const auto alive = unity::TryAlive(transform);
    if (alive && alive.value)
      (void)system_orbit::SetView(transform, state.native_angles, state.distance);
  }
  NavigationOrbitRestoreClip();
  state.zoom          = {};
  state.camera        = {};
  state.transform     = {};
  state.pan           = {};
  state.depth         = NodeDepth::Starbase;
  state.native_angles = {};
  state.distance = state.yaw = state.pitch = state.x = state.y = 0;
  state.frame                                                  = -1;
  state.dragging = state.overridden = false;
}
void NavigationOrbitFail(const char* operation, bool restore_view = true)
{
  NavigationOrbitClear(restore_view);
  NavigationOrbit().installed = false;
  spdlog::warn("[NavigationOrbit] {} unavailable; orbit disabled after camera cleanup", operation);
}
bool NavigationOrbitSection()
{
  auto&      state = NavigationOrbit();
  auto*      zoom  = state.zoom.Get<NavigationZoom>();
  const auto depth = system_orbit::Field<"_depth", NodeDepth>(zoom);
  return zoom && depth && depth.value == NodeDepth::SolarSystem && state.depth == depth.value
         && system_orbit::InSystem();
}
void NavigationOrbitRequestFlush(NavigationZoom* zoom);
void NavigationOrbitTick()
{
  auto& state = NavigationOrbit();
  // Failed observations do not release ownership. Finish pending cleanup even after the zoom rig was cleared.
  if (!state.overridden)
    NavigationOrbitRestoreClip();
  if (!state.zoom)
    return;
  if (!NavigationOrbitSection()) {
    NavigationOrbitClear(false);
    return;
  }
  if (!NavigationOrbitEnabled()) {
    NavigationOrbitClear();
    return;
  }
  const auto frame = unity::Time::frameCount();
  if (state.frame == frame)
    return;
  state.frame      = frame;
  const bool held  = Shortcut::IsPressed(Action::kNavigationOrbitDrag);
  const bool reset = Shortcut::IsDown(Action::kNavigationOrbitReset);
  if (!held && !reset) {
    state.dragging = false;
    return;
  }
  const auto focused = unity::Application::Class::TryCall<"get_isFocused", bool>();
  if (!focused || !focused.value || unity::Key::IsInputFocused()) {
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
    NavigationOrbitRestoreClip();
    state.yaw = 0;
  } else {
    const auto position = unity::Input::TryMousePosition();
    if (!position || !system_orbit::Finite(position.value)) {
      state.dragging = false;
      return;
    }
    if (state.dragging && (position.value.x != state.x || position.value.y != state.y)) {
      constexpr float degrees_per_pixel = 0.15f;
      state.yaw        = std::remainder(state.yaw + (position.value.x - state.x) * degrees_per_pixel, 360.0f);
      state.pitch      = std::clamp(state.pitch - (position.value.y - state.y) * degrees_per_pixel, 5.0f, 90.0f);
      state.overridden = changed = true;
      if (!system_orbit::SetView(transform, {state.pitch, state.yaw, 0}, state.distance)) {
        NavigationOrbitFail("rotate camera transform");
        return;
      }
      ++state.view_revision;
    } else if (Shortcut::IsDown(Action::kNavigationOrbitDrag)) {
      if (!state.overridden)
        state.pitch = std::clamp(state.native_angles.x, 5.0f, 90.0f);
      state.dragging = true;
    }
    state.x = position.value.x;
    state.y = position.value.y;
  }
  // Prevent orbit motion from becoming a spurious wheel-zoom translation.
  if (changed) {
    if (!system_orbit::Reanchor(state.zoom.Get<NavigationZoom>()))
      NavigationOrbitFail("mouse-world zoom anchor");
    else
      NavigationOrbitRequestFlush(state.zoom.Get<NavigationZoom>());
  }
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
  // Only fresh pointer movement is scaled here. Keys are scaled by LateUpdate; cached glide is already in
  // world units. The shared detour also serves NavigationOrbit when the Pan hook group is unavailable.
  const bool  pan_installed = std::ranges::any_of(FeatureInstallations(), [](const FeatureInstallation& feature) {
    return feature.name && std::string_view(feature.name) == kPan.name && feature.state == InstallState::kInstalled;
  });
  const float multiplier    = pan_installed && !momentum && NavigationPan::FieldsAvailable() && _this->panningStarted()
                                  ? Config::Get().graphics.system_pan_speed
                                  : 1.0f;
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
    original(_this, delta * multiplier, momentum);
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
  delta = delta * multiplier;
  original(_this, std::isfinite(delta.x) && std::isfinite(delta.y) ? delta : unity::Vector2{}, momentum);
}
bool NavigationOrbitExtendClip(unity::Camera* camera, float baseline, float expanded)
{
  auto& state = NavigationOrbit();
  if (state.clip_camera && state.clip_camera.Get() != camera && !NavigationOrbitRestoreClip())
    return false;
  const bool owned = state.clip_camera.Get() == camera && baseline == state.applied_clip;
  auto       owner = state.clip_camera.Get() == camera ? il2cpp::GcHandle{} : il2cpp::GcHandle::Strong(camera);
  if ((!state.clip_camera && !owner) || !camera->TryCall<"set_farClipPlane">(expanded))
    return false;
  if (!owned)
    state.native_clip = baseline;
  state.applied_clip = expanded;
  if (owner)
    state.clip_camera = std::move(owner);
  return true;
}
void NavigationOrbitDrawDistance(NavigationZoom* zoom)
{
  auto& state = NavigationOrbit();
  if (!zoom || state.zoom.Get() != zoom || !state.overridden || !NavigationOrbitEnabled() || !NavigationOrbitSection())
    return;
  auto*      pan        = state.pan.Get<NavigationPan>();
  const auto camera     = system_orbit::Field<"_sceneCamera", unity::Camera*>(zoom);
  const auto pan_camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(pan);
  const auto depth      = system_orbit::Field<"_nodeDepth", NodeDepth>(pan);
  const auto center     = system_orbit::Field<"_systemPosition", OrbitVector3>(pan);
  const auto radius     = system_orbit::Field<"_viewRadius", float>(pan);
  if (!camera || !camera.value || state.camera.Get() != camera.value || !pan_camera
      || pan_camera.value != camera.value || !depth || depth.value != state.depth || !center || !radius
      || !system_orbit::Finite(center.value) || !std::isfinite(radius.value) || radius.value <= 0)
    return;
  const auto position = system_orbit::Vector(system_orbit::VectorGetter<"get_position">(), state.transform.Get());
  const auto baseline = camera.value->TryCall<"get_farClipPlane", float>();
  if (!position || !baseline || !std::isfinite(baseline.value) || baseline.value <= 0)
    return;
  const double distance =
      std::hypot(double(position.value.x) - center.value.x, double(position.value.y) - center.value.y,
                 double(position.value.z) - center.value.z);
  const double required = distance + 4.0 * radius.value;
  if (required > baseline.value && required <= std::numeric_limits<float>::max())
    NavigationOrbitExtendClip(camera.value, baseline.value, float(required));
}
navigation_orbit::ZoomUpdateScope*& NavigationOrbitUpdates()
{
  static navigation_orbit::ZoomUpdateScope* current = nullptr;
  return current;
}
void NavigationOrbitFlush(NavigationZoom* zoom)
{ NavigationOrbitDrawDistance(zoom); }
void NavigationOrbitRequestFlush(NavigationZoom* zoom)
{
  for (auto* scope = NavigationOrbitUpdates(); scope; scope = scope->previous)
    if (scope->zoom == zoom) {
      scope->pending = true;
      return;
    }
  navigation_orbit::ZoomUpdateScope scope(zoom);
  scope.pending = true;
  scope.Complete();
}
void NavigationZoom_UpdateCameraPosition_OrbitHook(auto original, NavigationZoom* _this)
{
  unity::Key::Poll();
  navigation_orbit::ZoomUpdateScope update(_this);
  auto&                             state = NavigationOrbit();
  if (state.zoom) {
    if (state.zoom.Get() != _this || !NavigationOrbitSection())
      NavigationOrbitClear(false);
    else if (!NavigationOrbitEnabled())
      NavigationOrbitClear();
  }
  original(_this);
  // A quiet native camera needs no rig observations, boxed vectors or temporary roots.
  // Keep owner/section cleanup above this gate, including after reset or a cancelled gesture.
  if (!state.overridden && !state.dragging && !Shortcut::IsDown(Action::kNavigationOrbitDrag))
    return;
  const auto depth = system_orbit::Field<"_depth", NodeDepth>(_this);
  if (!NavigationOrbitEnabled() || !depth || depth.value != NodeDepth::SolarSystem || !system_orbit::InSystem())
    return;
  const auto camera = system_orbit::Field<"_sceneCamera", unity::Camera*>(_this);
  auto       camera_root =
      camera && camera.value != state.camera.Get() ? il2cpp::GcHandle::Strong(camera.value) : il2cpp::GcHandle{};
  if (!camera || !camera.value || (camera.value != state.camera.Get() && !camera_root)) {
    NavigationOrbitFail("camera retention", false);
    return;
  }
  auto*      retained_camera = camera.value;
  const auto transform       = system_orbit::Transform(_this);
  auto       transform_root  = transform && transform.value != state.transform.Get()
                                   ? il2cpp::GcHandle::Strong(transform.value)
                                   : il2cpp::GcHandle{};
  if (!transform || !transform.value || (transform.value != state.transform.Get() && !transform_root)) {
    NavigationOrbitFail("camera transform retention", false);
    return;
  }
  auto*      retained_transform = transform.value;
  const auto camera_transform   = system_orbit::Transform(retained_camera);
  const auto angles =
      camera_transform && camera_transform.value == retained_transform
          ? system_orbit::Vector(system_orbit::VectorGetter<"get_localEulerAngles">(), retained_transform)
          : il2cpp::CallResult<OrbitVector3>{};
  const auto distance = system_orbit::Field<"_actualDistance", float>(_this);
  if (!camera || !camera.value || !transform || !transform.value || !camera_transform || !retained_camera
      || !retained_transform || (camera_transform.value == retained_transform && !angles) || !distance
      || !std::isfinite(distance.value) || distance.value <= 0) {
    NavigationOrbitFail("camera rig query", false);
    return;
  }
  if (state.zoom && (state.camera.Get() != camera.value || state.transform.Get() != transform.value)) {
    // A native rig replacement is a new owner, even when NavigationZoom itself is reused.
    if (!camera_root)
      camera_root = il2cpp::GcHandle::Strong(camera.value);
    if (!transform_root)
      transform_root = il2cpp::GcHandle::Strong(transform.value);
    NavigationOrbitClear(false);
  }
  if (retained_transform != camera_transform.value || !angles
      || std::abs(std::remainder(angles.value.y, 360.0f)) > 0.01f
      || std::abs(std::remainder(angles.value.z, 360.0f)) > 0.01f) {
    static bool reported = false;
    if (!std::exchange(reported, true))
      spdlog::warn("[NavigationOrbit] native rig differs; native view retained (same-transform={} yaw={} roll={})",
                   retained_transform == camera_transform.value, angles.value.y, angles.value.z);
    // original() already authored the new native rig; do not overwrite it with our previous orientation.
    NavigationOrbitClear(false);
    return;
  }
  if (!state.zoom || state.camera.Get() != camera.value || state.transform.Get() != transform.value) {
    auto zoom_root = il2cpp::GcHandle::Strong(_this);
    if (!zoom_root || (!state.transform && !transform_root) || (!state.camera && !camera_root)) {
      NavigationOrbitFail("camera rig retention");
      return;
    }
    state.zoom = std::move(zoom_root);
    if (transform_root)
      state.transform = std::move(transform_root);
    if (camera_root)
      state.camera = std::move(camera_root);
    state.depth = depth.value;
  }
  state.native_angles      = angles.value;
  state.distance           = distance.value;
  const auto view_revision = state.view_revision;
  NavigationOrbitTick();
  // Native work can rewrite the view more than once in one frame. Input is sampled once,
  // but every original call still needs its final override reapplied unless Tick just did it.
  if (state.overridden && state.view_revision == view_revision
      && !system_orbit::SetView(state.transform.Get<unity::Transform>(), {state.pitch, state.yaw, 0}, state.distance)) {
    NavigationOrbitFail("reapply camera rotation");
    return;
  }
  NavigationOrbitRequestFlush(_this);
  update.Complete();
}
void NavigationZoom_LateUpdate_OrbitHook(auto original, NavigationZoom* _this)
{
  // LateUpdate can author the native pose twice through ZoomCamera. Correct each
  // write immediately, then observe and extend the final camera clip once.
  navigation_orbit::ZoomUpdateScope update(_this);
  original(_this);
  update.Complete();
}
bool InstallNavigationOrbit()
{
  if (!system_orbit::Available() || !navigation_orbit::EnsurePanMovement())
    return false;
  il2cpp::HookSet hooks("NavigationOrbit");
  IL2CPP_STAGE(hooks, NavigationPan::Class::Method<OrbitVector3(OrbitVector3)>("ClampPositionInside"),
               NavigationPan_ClampPositionInside_OrbitHook);
  IL2CPP_STAGE(hooks, NavigationZoom::Class::Method<void()>("UpdateCameraPosition"),
               NavigationZoom_UpdateCameraPosition_OrbitHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationZoom::Class::Method<void()>("LateUpdate")),
                  NavigationZoom_LateUpdate_OrbitHook);
  NavigationOrbit().installed = hooks.Commit();
  return NavigationOrbit().installed;
}
} // namespace

bool navigation_orbit::Available()
{ return NavigationOrbit().installed; }

bool navigation_orbit::EnsurePanMovement()
{
  static const bool installed = [] {
    il2cpp::HookSet hooks("PanMovement");
    IL2CPP_STAGE(hooks, NavigationPan::Class::Method<void(unity::Vector2, bool)>("MoveCamera"),
                 NavigationPan_MoveCamera_OrbitHook);
    return hooks.Commit();
  }();
  return installed;
}

navigation_orbit::ZoomUpdateScope::ZoomUpdateScope(NavigationZoom* value)
    : zoom(value)
    , previous(NavigationOrbitUpdates())
{ NavigationOrbitUpdates() = this; }

navigation_orbit::ZoomUpdateScope::~ZoomUpdateScope() noexcept
{
  if (active)
    NavigationOrbitUpdates() = previous;
}

void navigation_orbit::ZoomUpdateScope::Complete()
{
  if (!active || flushing)
    return;
  auto* outer = previous;
  while (outer && outer->zoom != zoom)
    outer = outer->previous;
  if (outer)
    outer->pending |= pending;
  else if (pending) {
    pending  = false;
    flushing = true;
    NavigationOrbitFlush(zoom);
    flushing = false;
  }
  NavigationOrbitUpdates() = previous;
  active                   = false;
}

void navigation_orbit::ApplyDrawDistance(NavigationZoom* zoom)
{ NavigationOrbitRequestFlush(zoom); }

const Feature kNavigationOrbit{"NavigationOrbit", &Config::Patches::navigationorbithooks, InstallNavigationOrbit,
                               NavigationOrbitTick};
