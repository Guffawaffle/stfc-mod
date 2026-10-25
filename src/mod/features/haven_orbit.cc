// Haven's native planetary orbit: MOUSE2 drag and ALT-MOUSE2 reset by default.
// Shared constraint assets, Haven zoom expansion and water ownership stay native.
#include "features/haven_orbit.h"
#include "config.h"
#include "features/features.h"
#include "shortcuts.h"

#include "game/haven_orbit.h"

#include <il2cpp/hook.h>
#include <unity/input.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace
{
struct HavenOrbitEndpoint {
  il2cpp::GcHandle provider;
  float            original_yaw   = 0;
  float            original_pitch = 0;
  float            applied_yaw    = 0;
  float            applied_pitch  = 0;
  bool             yaw_owned      = false;
  bool             pitch_owned    = false;
};
struct HavenOrbitState {
  bool                installed = false;
  il2cpp::GcHandle    blend;
  HavenOrbitEndpoint  source;
  HavenOrbitEndpoint  target;
  int32_t             frame         = -1;
  float               yaw           = 0;
  float               tilt          = 0;
  float               x             = 0;
  float               y             = 0;
  bool                dragging      = false;
  OrbitFrameProvider* scoped_source = nullptr;
  OrbitFrameProvider* scoped_target = nullptr;
};
HavenOrbitState& HavenOrbit()
{
  // At most one live blend and its two qualified endpoints. No GC API at teardown.
  static auto* state = new HavenOrbitState;
  return *state;
}
bool HavenOrbitEnabled()
{
  const auto& config = Config::Get();
  return HavenOrbit().installed && config.patches.havenzoomhooks && config.control.hotkeys_enabled
         && !config.control.use_scopely_hotkeys && Shortcut::HasBinding(Action::kHavenOrbitDrag);
}
bool HavenOrbitNear(float a, float b)
{ return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) < 0.0001f; }

bool HavenOrbitRestore(HavenOrbitEndpoint& endpoint)
{
  auto* provider = endpoint.provider.Get<AbstractOrbitFrameProvider>();
  if (!provider)
    return true;
  if (endpoint.yaw_owned) {
    const auto current = haven_orbit_binding::Field<"_rotationAngle", float>(provider);
    if (current
        && (!HavenOrbitNear(current.value, endpoint.applied_yaw)
            || haven_orbit_binding::Write<"_rotationAngle">(provider, endpoint.original_yaw)))
      endpoint.yaw_owned = false;
  }
  if (endpoint.pitch_owned) {
    const auto current = haven_orbit_binding::Field<"_elevationAngle", float>(provider);
    if (current
        && (!HavenOrbitNear(current.value, endpoint.applied_pitch)
            || haven_orbit_binding::Write<"_elevationAngle">(provider, endpoint.original_pitch)))
      endpoint.pitch_owned = false;
  }
  if (!endpoint.yaw_owned && !endpoint.pitch_owned) {
    endpoint.provider = {};
    return true;
  }
  return false;
}
bool HavenOrbitClear()
{
  auto& state       = HavenOrbit();
  state.dragging    = false;
  const bool source = HavenOrbitRestore(state.source);
  const bool target = HavenOrbitRestore(state.target);
  if (!source || !target)
    return false;
  state.blend = {};
  state.frame = -1;
  state.yaw = state.tilt = 0;
  return true;
}
void HavenOrbitFrame()
{
  if (HavenOrbit().blend && (!HavenOrbitEnabled() || !haven_orbit_binding::InHaven()))
    (void)HavenOrbitClear();
}

bool HavenOrbitPrepare(BlendFrameProvider* blend)
{
  auto&      state = HavenOrbit();
  const auto frame = unity::Time::frameCount();
  if (!HavenOrbitEnabled() || frame < 0 || !haven_orbit_binding::InHaven()) {
    (void)HavenOrbitClear();
    return false;
  }
  const auto source_observed = haven_orbit_binding::Field<"_sourceFrameProvider", FrameProvider*>(blend);
  const auto target_observed = haven_orbit_binding::Field<"_targetFrameProvider", FrameProvider*>(blend);
  auto       source_root     = il2cpp::GcHandle::Strong(source_observed ? source_observed.value : nullptr);
  auto       target_root     = il2cpp::GcHandle::Strong(target_observed ? target_observed.value : nullptr);
  auto*      source = source_root ? source_root.Get<il2cpp::Object>()->As<PlanetaryOrbitFrameProvider>() : nullptr;
  auto*      target = target_root ? target_root.Get<il2cpp::Object>()->As<PlanetaryOrbitFrameProvider>() : nullptr;
  auto*      source_orbit = source ? source->As<AbstractOrbitFrameProvider>() : nullptr;
  auto*      target_orbit = target ? target->As<AbstractOrbitFrameProvider>() : nullptr;
  const auto a            = haven_orbit_binding::Radius(source_orbit);
  const auto b            = haven_orbit_binding::Radius(target_orbit);
  if (!a || !b || *a == *b) {
    (void)HavenOrbitClear();
    return false;
  }
  const auto pivot_a = haven_orbit_binding::Field<"_orbitPivot", TargetController*>(source_orbit);
  const auto pivot_b = haven_orbit_binding::Field<"_orbitPivot", TargetController*>(target_orbit);
  const auto look_a  = haven_orbit_binding::Field<"_lookTarget", TargetController*>(source_orbit);
  const auto look_b  = haven_orbit_binding::Field<"_lookTarget", TargetController*>(target_orbit);
  const auto pitch_a = haven_orbit_binding::FixedElevation(source_orbit);
  const auto pitch_b = haven_orbit_binding::FixedElevation(target_orbit);
  if (!pivot_a || !pivot_b || !look_a || !look_b || !pivot_a.value || pivot_b.value != pivot_a.value
      || look_a.value != pivot_a.value || look_b.value != pivot_a.value || !pitch_a || !pitch_b
      || std::abs(*pitch_a - *pitch_b) > 0.001f || *pitch_a < 15 || *pitch_a > 80) {
    (void)HavenOrbitClear();
    return false;
  }
  if (!state.blend || state.blend.Get() != blend || state.source.provider.Get() != source
      || state.target.provider.Get() != target || frame > state.frame + 1) {
    if (!HavenOrbitClear())
      return false;
    const auto yaw_a = haven_orbit_binding::Field<"_rotationAngle", float>(source_orbit);
    const auto yaw_b = haven_orbit_binding::Field<"_rotationAngle", float>(target_orbit);
    if (!yaw_a || !yaw_b || !std::isfinite(yaw_a.value) || !std::isfinite(yaw_b.value)
        || std::abs(std::remainder(yaw_a.value - yaw_b.value, 360.0f)) > 0.001f)
      return false;
    auto blend_root = il2cpp::GcHandle::Strong(blend);
    if (!blend_root || !source_root || !target_root)
      return false;
    state.blend  = std::move(blend_root);
    state.source = {std::move(source_root), yaw_a.value, *pitch_a};
    state.target = {std::move(target_root), yaw_b.value, *pitch_b};
  }
  if (frame != state.frame) {
    state.frame        = frame;
    const auto focused = unity::Application::Class::TryCall<"get_isFocused", bool>();
    const bool input   = focused && focused.value && !unity::Key::IsInputFocused();
    const bool held    = input && Shortcut::IsPressed(Action::kHavenOrbitDrag);
    const bool reset   = input && Shortcut::IsDown(Action::kHavenOrbitReset);
    if (!held && !reset) {
      state.dragging = false;
    } else if (!haven_orbit_binding::PointerAvailable(state.dragging && held && !reset)) {
      state.dragging = false; // A blocked UI start must wait for a fresh press.
    } else if (reset) {
      state.yaw = state.tilt = 0;
      state.dragging         = false;
    } else {
      const auto position = unity::Input::TryMousePosition();
      if (!position || !std::isfinite(position.value.x) || !std::isfinite(position.value.y)) {
        state.dragging = false;
      } else {
        if (state.dragging) {
          constexpr float degrees_per_pixel = 0.15f;
          state.yaw  = std::remainder(state.yaw + (position.value.x - state.x) * degrees_per_pixel, 360.0f);
          state.tilt = std::clamp(state.tilt - (position.value.y - state.y) * degrees_per_pixel, 5.0f - *pitch_a,
                                  90.0f - *pitch_a);
        }
        state.x        = position.value.x;
        state.y        = position.value.y;
        state.dragging = state.dragging || Shortcut::IsDown(Action::kHavenOrbitDrag);
      }
    }
  }
  state.scoped_source = source;
  state.scoped_target = target;
  return true;
}

void BlendFrameProvider_UpdateInputData_HavenOrbitHook(auto original, BlendFrameProvider* _this, unity::Camera* camera)
{
  auto& state = HavenOrbit();
  struct Scope {
    HavenOrbitState&    state;
    OrbitFrameProvider* source = state.scoped_source;
    OrbitFrameProvider* target = state.scoped_target;
    ~Scope()
    {
      state.scoped_source = source;
      state.scoped_target = target;
    }
  } scope{state};
  state.scoped_source = state.scoped_target = nullptr;
  unity::Key::Poll();
  (void)HavenOrbitPrepare(_this);
  // Endpoint constraints and frame generation occur inside this native update.
  original(_this, camera);
}
bool InstallHavenOrbit()
{
  if (!haven_orbit_binding::Available() || !EnsureOrbitConstraints())
    return false;
  il2cpp::HookSet hooks("HavenOrbit");
  IL2CPP_STAGE(hooks, BlendFrameProvider::Class::Method<void(unity::Camera*)>("UpdateInputData"),
               BlendFrameProvider_UpdateInputData_HavenOrbitHook);
  HavenOrbit().installed = hooks.Commit();
  return HavenOrbit().installed;
}
} // namespace

void haven_orbit::ApplyConstraints(OrbitFrameProvider* provider)
{
  auto& state = HavenOrbit();
  if (!state.installed || (provider != state.scoped_source && provider != state.scoped_target))
    return;
  auto& endpoint = provider == state.scoped_source ? state.source : state.target;
  if (endpoint.provider.Get() != provider)
    return; // A nested native callback may already have released this scope.
  auto*      orbit = provider->As<AbstractOrbitFrameProvider>();
  const auto pitch = haven_orbit_binding::FixedElevation(orbit);
  if (!pitch)
    return;
  const auto yaw       = endpoint.original_yaw + state.yaw;
  const auto elevation = *pitch + state.tilt;
  if (haven_orbit_binding::Write<"_rotationAngle">(orbit, yaw)) {
    endpoint.applied_yaw = yaw;
    endpoint.yaw_owned   = true;
  }
  if (haven_orbit_binding::Write<"_elevationAngle">(orbit, elevation)) {
    endpoint.applied_pitch = elevation;
    endpoint.pitch_owned   = true;
  }
}

const Feature kHavenOrbit{"HavenOrbit", &Config::Patches::havenzoomhooks, InstallHavenOrbit, HavenOrbitFrame};
