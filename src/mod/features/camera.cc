// The camera in system view and at the station: the extended zoom range, keyboard zoom and its presets, planet
// sprite scaling, keyboard panning, pan momentum and the target camera lock, keyboard rotation around the station,
// held touches that keep panning, and [graphics] interior_zoom_scale inside the station. Fleet label detail, which
// follows the zoom, is in fleet_labels.cc; Haven's camera is in haven.cc.

#include "config.h"
#include "features/features.h"
#include "features/galaxy.h"
#include "features/haven_orbit.h"
#include "features/navigation_orbit.h"
#include "runtime_config.h"
#include "settings/basic_gameplay_settings.h"
#include "shortcuts.h"

#include "game/app.h"
#include "game/camera.h"
#include "game/navigation.h"
#include "game/scroll_input.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <utility>

namespace
{
// --- Zoom ---
bool camera_zoom_installed = false;

// One pending system-entry default belongs to that camera. Other native camera
// instances must not consume it. Never release a GC handle after IL2CPP teardown.
il2cpp::GcHandle& PendingDefaultZoom()
{
  static auto* owner = new il2cpp::GcHandle;
  return *owner;
}
uint64_t zoom_default_generation = 0;

bool TakeDefaultZoom(NavigationZoom* zoom)
{
  auto& owner   = PendingDefaultZoom();
  auto* pending = owner.Get<NavigationZoom>();
  if (!pending) {
    owner.Reset();
    return false;
  }
  if (pending != zoom)
    return false;
  owner.Reset();
  const auto depth = zoom->TryDepth();
  return depth && depth.value == NodeDepth::SolarSystem;
}

// Native camera wheel consumption is scoped separately from mod keyboard zoom.
// UI ownership leaves the UI's wheel action intact and never blocks a touch pinch.
NavigationZoom* scroll_native_update     = nullptr;
bool            scroll_queries_available = false;
struct NativeZoomUpdateScope {
  NavigationZoom* previous;
  explicit NativeZoomUpdateScope(NavigationZoom* zoom)
      : previous(std::exchange(scroll_native_update, zoom))
  {
  }
  ~NativeZoomUpdateScope()
  { scroll_native_update = previous; }
};

bool ScrollPointerOwned()
{
  if (!scroll_queries_available)
    return false;
  try {
    const auto observed = scroll_input::PointerOwnsScroll();
    if (observed)
      return observed.value;
  } catch (...) {
  }
  scroll_queries_available = false;
  try {
    spdlog::warn("[ScrollZoomGuard] UI scroll ownership unavailable; native wheel input retained");
  } catch (...) {
  }
  return false;
}

void NavigationZoom_ZoomCameraAtWorldPoint_Hook(auto original, NavigationZoom* _this)
{
  if (_this == scroll_native_update) {
    const auto pinch = scroll_input::PinchStarted(_this);
    if (pinch && !pinch.value && ScrollPointerOwned()) {
      _this->SetZoomDelta(0.0f);
      _this->SetLastZoomDelta(0.0f);
      return;
    }
  }
  original(_this);
}

float MouseScrollAsPinchRecognizer_ReadInput_Hook(auto original, il2cpp::Object* _this)
{
  const auto delta = original(_this);
  return delta != 0.0f && ScrollPointerOwned() ? 0.0f : delta;
}

// Presets and default_system_zoom are in zoom units: the camera distance's place in [minimum, maximum] mapped onto
// [0, graphics.zoom], so they keep their meaning when the game moves the range. ZoomAtMouse(absolute) is the inverse.
std::optional<float> CurrentZoom(NavigationZoom* zoom)
{
  auto owner = il2cpp::GcHandle::Strong(zoom);
  if (!owner)
    return {};
  const auto distance = zoom->TryDistance();
  const auto minimum  = zoom->TryMinimum();
  const auto maximum  = zoom->TryMaximum();
  if (!distance || !minimum || !maximum)
    return {};
  return gameplay_settings::QualifiedCurrentZoom(distance.value, minimum.value, maximum.value,
                                                 Config::Get().graphics.zoom);
}

bool StorePreset(const char* label, float Config::Graphics::* member, NavigationZoom* zoom,
                 gameplay_settings::Zoom identity)
{
  const auto observed = CurrentZoom(zoom);
  if (!observed)
    return false;
  // Managed measurement can reenter configuration activation. Resolve the
  // destination only afterward, rather than retaining a Config member reference.
  auto&      preset = Config::Get().graphics.*member;
  const auto old    = preset;
  gameplay_settings::StoreZoom(preset, *observed, identity, [](const gameplay_settings::Key& key, double value) {
    runtime_config::SaveSetting(key.section, key.name, value, key.delay);
  });
  spdlog::info("[Zoom] {} set from {} to {}", label, old, preset);
  return true;
}

struct ZoomPreset {
  Action store;
  Action go;
  float Config::Graphics::* value;
  const char*               label;
  gameplay_settings::Zoom   identity;
};

constexpr ZoomPreset kZoomPresets[] = {
    {Action::kSetZoomPreset1, Action::kZoomPreset1, &Config::Graphics::system_zoom_preset_1, "System Preset 1",
     gameplay_settings::Zoom::Preset1},
    {Action::kSetZoomPreset2, Action::kZoomPreset2, &Config::Graphics::system_zoom_preset_2, "System Preset 2",
     gameplay_settings::Zoom::Preset2},
    {Action::kSetZoomPreset3, Action::kZoomPreset3, &Config::Graphics::system_zoom_preset_3, "System Preset 3",
     gameplay_settings::Zoom::Preset3},
    {Action::kSetZoomPreset4, Action::kZoomPreset4, &Config::Graphics::system_zoom_preset_4, "System Preset 4",
     gameplay_settings::Zoom::Preset4},
    {Action::kSetZoomPreset5, Action::kZoomPreset5, &Config::Graphics::system_zoom_preset_5, "System Preset 5",
     gameplay_settings::Zoom::Preset5},
};

// Widens the system view's zoom range to the configured maximum and pushes the far clip plane out with it. A radius of
// 0 leaves the far ratios alone and only reapplies the camera settings.
bool ApplyZoomRange(NavigationZoom* zoom, float radius)
{
  auto owner = il2cpp::GcHandle::Strong(zoom);
  if (!owner)
    return false;
  const auto max_zoom = Config::Get().graphics.zoom;
  const auto clip     = max_zoom * 3.75f;
  if (!zoom->RangeFieldsAvailable() || !std::isfinite(max_zoom) || max_zoom <= 0.0f || !std::isfinite(clip)
      || !std::isfinite(radius) || radius < 0.0f)
    return false;
  const auto scene_camera = zoom->TrySceneCamera();
  if (!scene_camera)
    return false;
  auto camera = scene_camera.value ? il2cpp::GcHandle::Strong(scene_camera.value) : il2cpp::GcHandle{};
  if (scene_camera.value && !camera)
    return false;
  // The far ratios are multiples of the system's view radius, so max_zoom / radius puts the extended stop at max_zoom.
  // The normal stop at 0.55 of that is carried over from the original mod.
  if (radius > 0.0f) {
    const auto ratio = max_zoom / radius;
    if (!std::isfinite(ratio))
      return false;
    zoom->SetFarRatioSystemNormal(0.55f * max_zoom / radius);
    zoom->SetFarRatioSystemExtended(ratio);
  }
  // At max_zoom the camera still has to reach the far side of the system and its backdrop, hence the 3.75x clip.
  // Clearing to black instead of the skybox hides the void past the backdrop's edge.
  if (camera) {
    camera.Get<unity::Camera>()->SetFarClipPlane(clip);
    camera.Get<unity::Camera>()->SetClearFlags(unity::Camera::ClearFlags::SolidColor);
    camera.Get<unity::Camera>()->SetBackgroundColor({0, 0, 0, 0});
  }
  navigation_orbit::ApplyDrawDistance(zoom);
  return true;
}

// The game resets the range now and then; reapply it around every update. _zoomtotal caches maximum - minimum and
// must follow the widened maximum.
void KeepZoomRange(NavigationZoom* zoom)
{
  const auto max_zoom = Config::Get().graphics.zoom;
  const auto depth    = zoom->TryDepth();
  const auto radius   = zoom->TryViewRadius();
  const auto minimum  = zoom->TryMinimum();
  const auto maximum  = zoom->TryMaximum();
  if (!depth || depth.value != NodeDepth::SolarSystem || !radius || !minimum || !maximum
      || !zoom->RangeFieldsAvailable() || !std::isfinite(radius.value) || radius.value < 0.0f
      || !std::isfinite(max_zoom * 3.75f)
      || !gameplay_settings::QualifiedZoomDistance(0.0f, minimum.value, maximum.value, max_zoom))
    return;
  const auto widened = std::max(maximum.value, max_zoom);
  const auto total   = widened - minimum.value;
  if (!std::isfinite(total) || total <= 0.0f)
    return;
  if (!ApplyZoomRange(zoom, radius.value))
    return;
  const auto current_depth   = zoom->TryDepth();
  const auto current_minimum = zoom->TryMinimum();
  const auto current_maximum = zoom->TryMaximum();
  if (!current_depth || current_depth.value != depth.value || !current_minimum || !current_maximum
      || current_minimum.value != minimum.value || current_maximum.value != maximum.value
      || Config::Get().graphics.zoom != max_zoom)
    return;
  if (maximum.value < max_zoom)
    zoom->SetMaximum(max_zoom);
  zoom->SetZoomtotal(total);
}

// Zooms around the mouse pointer, either by a step of `value` or, with `absolute`, to `value` in zoom units.
bool ZoomAtMouse(NavigationZoom* zoom, float value, bool absolute)
{
  auto owner = il2cpp::GcHandle::Strong(zoom);
  if (!owner || !zoom->ZoomInputFieldsAvailable() || !std::isfinite(value))
    return false;
  const auto mouse           = unity::Input::TryMousePosition();
  const auto observed_camera = zoom->TrySceneCamera();
  auto       camera =
      observed_camera && observed_camera.value ? il2cpp::GcHandle::Strong(observed_camera.value) : il2cpp::GcHandle{};
  if (!mouse || !std::isfinite(mouse.value.x) || !std::isfinite(mouse.value.y) || !std::isfinite(mouse.value.z)
      || !camera)
    return false;
  const auto minimum    = zoom->TryMinimum();
  const auto maximum    = zoom->TryMaximum();
  const auto configured = Config::Get().graphics.zoom;
  const auto distance = minimum && maximum
                            ? gameplay_settings::QualifiedZoomDistance(value, minimum.value, maximum.value, configured)
                            : std::optional<float>{};
  if (absolute && !distance)
    return false;
  const auto world          = MathUtils::TryGetMouseWorldPos(camera.Get<unity::Camera>(), mouse.value);
  const auto current_camera = zoom->TrySceneCamera();
  if (!world || !current_camera || current_camera.value != camera.Get<unity::Camera>())
    return false;
  if (absolute) {
    const auto current_minimum = zoom->TryMinimum();
    const auto current_maximum = zoom->TryMaximum();
    if (!current_minimum || !current_maximum || current_minimum.value != minimum.value
        || current_maximum.value != maximum.value || Config::Get().graphics.zoom != configured
        || !zoom->TrySetDistance(*distance))
      return false;
    // The native setter can synchronously replace its scene/range or reload configuration.
    // Its success does not authorize publishing pointer data acquired for the previous inputs.
    const auto applied_camera  = zoom->TrySceneCamera();
    const auto applied_minimum = zoom->TryMinimum();
    const auto applied_maximum = zoom->TryMaximum();
    if (!applied_camera || applied_camera.value != camera.Get<unity::Camera>() || !applied_minimum || !applied_maximum
        || applied_minimum.value != minimum.value || applied_maximum.value != maximum.value
        || Config::Get().graphics.zoom != configured)
      return false;
  }
  zoom->SetZoomLocation({mouse.value.x, mouse.value.y});
  if (!absolute) {
    zoom->SetZoomDelta(value);
    zoom->SetLastZoomDelta(value);
  }
  zoom->SetWorldPoint(world.value);
  return zoom->TryCall<"ZoomCameraAtWorldPoint", void>().succeeded;
}

// Keyboard zoom: Q/E zoom by keyboard_zoom_speed per second, presets jump, SHIFT-F1.. store the current zoom.
// ZoomMin goes all the way out (graphics.zoom) and ZoomMax close in. Returns false when a preset was stored, which
// skips the game's update for that frame.
bool KeyboardZoom(NavigationZoom* zoom)
{
  if (!Config::Get().control.hotkeys_enabled || Config::Get().control.use_scopely_hotkeys)
    return true;

  for (const auto& preset : kZoomPresets)
    if (Shortcut::IsDown(preset.store)) {
      return !StorePreset(preset.label, preset.value, zoom, preset.identity);
    }
  if (Shortcut::IsDown(Action::kSetZoomDefault)) {
    return !StorePreset("System Default", &Config::Graphics::default_system_zoom, zoom,
                        gameplay_settings::Zoom::Default);
  }

  // An extended shortcut held with a preset wins the jump, but the preset still counts for use_presets_as_default.
  const auto* preset = std::ranges::find_if(kZoomPresets, [](const ZoomPreset& p) { return Shortcut::IsDown(p.go); });
  const bool  from_preset = preset != std::ranges::end(kZoomPresets);
  float       target      = from_preset ? Config::Get().graphics.*preset->value : 0.0f;
  if (Config::Get().control.hotkeys_extended) {
    if (Shortcut::IsDown(Action::kZoomReset))
      target = Config::Get().graphics.default_system_zoom;
    else if (Shortcut::IsDown(Action::kZoomMin))
      target = Config::Get().graphics.zoom;
    else if (Shortcut::IsDown(Action::kZoomMax))
      target = 100.0f;
  }

  const float dt      = unity::Time::deltaTime();
  const float step    = Config::Get().graphics.keyboard_zoom_speed * dt;
  bool        applied = false;
  if (target != 0.0f)
    applied = ZoomAtMouse(zoom, target, true);
  else if (std::isfinite(dt) && dt > 0.0f && std::isfinite(step) && step != 0.0f
           && Shortcut::IsPressed(Action::kZoomIn))
    applied = ZoomAtMouse(zoom, step, false);
  else if (std::isfinite(dt) && dt > 0.0f && std::isfinite(step) && step != 0.0f
           && Shortcut::IsPressed(Action::kZoomOut))
    applied = ZoomAtMouse(zoom, -step, false);

  // With use_presets_as_default, jumping to a preset also makes it the zoom used on entering a system.
  const auto observed = applied && from_preset && target > 0.0f && Config::Get().graphics.use_presets_as_default
                            ? CurrentZoom(zoom)
                            : std::optional<float>{};
  if (observed && Config::Get().graphics.use_presets_as_default) {
    auto&      default_zoom = Config::Get().graphics.default_system_zoom;
    const auto previous     = default_zoom;
    gameplay_settings::UsePresetAsDefault(default_zoom, *observed, [](const gameplay_settings::Key& key, double value) {
      runtime_config::RetainSessionIntent(key.section, key.name, value);
    });
    spdlog::info("[Zoom] System Preset Default from Preset set from {} to {}", previous, default_zoom);
  }
  return true;
}

void NavigationZoom_Update_Hook(auto original, NavigationZoom* _this)
{
  unity::Key::Poll();
  auto owner = il2cpp::GcHandle::Strong(_this);
  if (!owner) {
    NativeZoomUpdateScope native_update(_this);
    original(_this);
    return;
  }
  navigation_orbit::ZoomUpdateScope camera_update(_this);
  FleetLabelsFollowDepth(_this, _this->depth());
  if (!camera_zoom_installed) {
    NativeZoomUpdateScope native_update(_this);
    original(_this);
    camera_update.Complete();
    FleetLabelsAfterZoom(_this);
    galaxy_controls::AfterZoom(_this);
    return;
  }
  KeepZoomRange(_this);

  // The jump on entering a system is not keyboard input, so it happens even while a text field has focus.
  bool run_game_update = true;
  if (TakeDefaultZoom(_this)) {
    // Zero preserves the game's entry zoom. Consume this camera's pending entry even when no jump is requested.
    const auto default_zoom = Config::Get().graphics.default_system_zoom;
    if (default_zoom != 0.0f)
      ZoomAtMouse(_this, default_zoom, true);
  } else if (!unity::Key::IsInputFocused())
    run_game_update = KeyboardZoom(_this);
  if (!run_game_update) {
    camera_update.Complete();
    return;
  }

  {
    NativeZoomUpdateScope native_update(_this);
    original(_this);
  }
  KeepZoomRange(_this);
  camera_update.Complete();
  FleetLabelsAfterZoom(_this);
  galaxy_controls::AfterZoom(_this);
}

// Entering a system: widen the range before and after the game sets it up, and zoom to the default next update.
template <typename Original, typename... Args>
void EnterDepth(Original original, NavigationZoom* zoom, NodeDepth depth, float radius, Args... args)
{
  const auto generation = ++zoom_default_generation;
  auto       owner      = il2cpp::GcHandle::Strong(zoom);
  if (!owner)
    return original(zoom, args...);
  FleetLabelsFollowDepth(zoom, depth);
  if (depth != NodeDepth::SolarSystem) {
    if (PendingDefaultZoom().Get<NavigationZoom>() == zoom)
      PendingDefaultZoom().Reset();
    return original(zoom, args...);
  }
  navigation_orbit::ZoomUpdateScope camera_update(zoom);
  ApplyZoomRange(zoom, radius);
  original(zoom, args...);
  ApplyZoomRange(zoom, 0.0f);
  camera_update.Complete();
  if (generation == zoom_default_generation)
    PendingDefaultZoom() = il2cpp::GcHandle::Weak(zoom);
}

void NavigationZoom_SetViewParameters_Hook(auto original, NavigationZoom* _this, float radius, NodeDepth depth)
{ EnterDepth(original, _this, depth, radius, radius, depth); }

void NavigationZoom_SetDepth_Hook(auto original, NavigationZoom* _this, NodeDepth depth)
{
  const auto radius = _this->TryViewRadius();
  EnterDepth(original, _this, depth, radius ? radius.value : 0.0f, depth);
}

// Planet flat renderables (the 2D planet sprites) are scaled by fr_scale. The game reuses them and asks for them on
// every zoom, so each transform remembers the size it was scaled to and is left alone while it still has it.
// Planets differ in base size, so this is per transform: one shared value made two planets rescale each other.
//
// A backdrop that was just created or reloaded has its authored scale again and is always scaled. The getter is
// hooked, so ScaleAuthoredBackdrop asks for the renderable with authored_backdrop set; backdrop_revision counts
// CreateBackdrop calls so Populate can tell whether one ran.
bool     authored_backdrop = false;
uint64_t backdrop_revision = 0;

struct BackdropScale {
  il2cpp::GcHandle transform;
  float            applied;
};
auto& BackdropScales()
{
  // Retire expired identities instead of forgetting still-live transforms. Do not release handles after runtime
  // teardown; weak custody does not keep scenes alive and distinguishes a recycled managed address.
  static auto* scaled = new std::unordered_map<unity::Transform*, BackdropScale>;
  return *scaled;
}

void ScaleFlatRenderable(unity::Component* renderable, bool authored)
{
  const float factor = Config::Get().graphics.fr_scale;
  // Written so a NaN fr_scale, which the reader accepts, skips scaling too.
  if (!renderable || !std::isfinite(factor) || !(factor > 0.0f) || factor == 1.0f)
    return;
  auto component = il2cpp::GcHandle::Strong(renderable);
  if (!component)
    return;
  auto* transform = renderable->transform();
  if (!transform)
    return;
  auto owner = il2cpp::GcHandle::Strong(transform);
  if (!owner)
    return;
  const auto observed = transform->TryLocalScale();
  if (!observed || !std::isfinite(observed.value.x) || !std::isfinite(observed.value.y)
      || !std::isfinite(observed.value.z) || Config::Get().graphics.fr_scale != factor)
    return;
  auto& scaled = BackdropScales();
  std::erase_if(scaled, [](const auto& item) { return item.second.transform.Get() != item.first; });
  if (const auto it = scaled.find(transform);
      !authored && it != scaled.end() && std::fabs(observed.value.x - it->second.applied) < 0.1f)
    return;
  const auto new_scale = observed.value * factor;
  if (!std::isfinite(new_scale.x) || !std::isfinite(new_scale.y) || !std::isfinite(new_scale.z))
    return;
  auto identity = il2cpp::GcHandle::Weak(transform);
  if (identity && transform->TrySetLocalScale(new_scale))
    scaled.insert_or_assign(transform, BackdropScale{std::move(identity), new_scale.x});
}

unity::Component* PlanetViewUtils_get_FlatRenderable_Hook(auto original, PlanetViewUtils* _this)
{
  auto* renderable = original(_this);
  ScaleFlatRenderable(renderable, authored_backdrop);
  return renderable;
}

// The game mostly reads the renderable field directly; asking through the getter routes it via the hook above.
void PlanetViewUtils_CameraZoomedEventHandler_Hook(auto original, PlanetViewUtils* _this, float distance,
                                                   float normalized)
{
  original(_this, distance, normalized);
  _this->FlatRenderable();
}

void ScaleAuthoredBackdrop(PlanetViewUtils* view)
{
  const auto previous = std::exchange(authored_backdrop, true);
  view->FlatRenderable();
  authored_backdrop = previous;
}

void PlanetViewUtils_CreateBackdrop_Hook(auto original, PlanetViewUtils* _this, Il2CppObject* backdrop,
                                         Il2CppObject* resource, bool using_default)
{
  original(_this, backdrop, resource, using_default);
  if (resource) {
    ++backdrop_revision;
    ScaleAuthoredBackdrop(_this);
  }
}

void PlanetViewUtils_Populate_Hook(auto original, PlanetViewUtils* _this, Il2CppObject* system, int64_t node_id,
                                   NodeDepth depth)
{
  auto*      previous = _this->popData();
  const auto revision = backdrop_revision;
  original(_this, system, node_id, depth);
  auto* population = _this->popData();
  // A cached variation backdrop reloads its authored scale without calling CreateBackdrop.
  if (depth == NodeDepth::SolarSystem && population && population != previous && population->VariationCollection()
      && revision == backdrop_revision)
    ScaleAuthoredBackdrop(_this);
}

// --- Panning and rotation ---

// Whether the mod, not the game, moves the camera with the Move* shortcuts.
bool ModMovesCamera()
{
  const auto& config = Config::Get();
  return config.control.hotkeys_enabled && !config.control.use_scopely_hotkeys && !config.ui.disable_move_keys;
}

struct PanSettings {
  bool  hotkeys, native_hotkeys, move_disabled, hotkey_hooks, unlock;
  float falloff;
  bool  operator==(const PanSettings&) const = default;
};
PanSettings ReadPanSettings()
{
  const auto& config = Config::Get();
  return {config.control.hotkeys_enabled,
          config.control.use_scopely_hotkeys,
          config.ui.disable_move_keys,
          config.patches.hotkeyhooks,
          config.graphics.disable_target_camera_lock,
          config.graphics.system_pan_momentum_falloff};
}
bool KeyboardPanActive(const PanSettings& settings)
{
  constexpr std::array kArrows{unity::KeyCode::LeftArrow, unity::KeyCode::RightArrow, unity::KeyCode::UpArrow,
                               unity::KeyCode::DownArrow};
  return settings.hotkeys && !settings.move_disabled && !Shortcut::CaptureBlocksShortcuts()
         && !std::ranges::any_of(kArrows, Shortcut::Owns);
}

// The game's own tutorial and modal/text-input gates for keyboard panning.
bool KeyboardPanAllowed()
{
  auto* manager = Hub::Tutorial();
  return manager && manager->IsSceneInputAllowed() && ShortcutsManager::CanUseShortcuts()
         && !unity::Key::IsInputFocused();
}

// Replaces the game's LateUpdate: the keyboard source is the mod's shortcuts, and the pan keeps gliding after the
// drag or key is released. Each frame moves the camera once: by the keys while one is held, otherwise by the
// glide.
bool NavigationPan_LateUpdate_Hook(auto original, NavigationPan* _this)
{
  unity::Key::Poll();
  const auto settings = ReadPanSettings();
  const bool keyboard = KeyboardPanActive(settings);
  const bool fields   = NavigationPan::FieldsAvailable();

  // With the game's own hotkeys, its LateUpdate pans and glides by itself; only the extended range is ours.
  if (keyboard && (settings.native_hotkeys || !settings.hotkey_hooks)) {
    if (fields && settings.unlock && _this->trackingPOI())
      _this->SetTrackingPOI(nullptr);
    original(_this);
    if (fields)
      _this->SetFarMagRadiusRatioSystemExtended(_this->farMagRadiusRatioSystemNormal());
    return true;
  }

  auto       owner         = il2cpp::GcHandle::Strong(_this);
  const auto pointer_state = unity::Input::TryPointerHeld();
  if (!fields || !owner || !pointer_state || ReadPanSettings() != settings)
    return true;
  // Dropping the tracked target before either pan path runs lets the player pan away from it.
  if (settings.unlock && _this->trackingPOI())
    _this->SetTrackingPOI(nullptr);
  auto       d       = _this->lastDelta();
  const auto tracked = _this->trackingPOI();
  const bool blocked = _this->BlockPan() || (tracked && !settings.unlock);
  const bool pointer = pointer_state.value;
  bool       by_keys = false;
  const bool allowed = keyboard && !blocked && !pointer && KeyboardPanAllowed();
  if (ReadPanSettings() != settings)
    return true;
  const auto current_pointer = unity::Input::TryPointerHeld();
  if (!current_pointer || current_pointer.value != pointer || _this->trackingPOI() != tracked
      || (_this->BlockPan() || (tracked && !settings.unlock)) != blocked || !std::isfinite(d.x) || !std::isfinite(d.y)
      || !std::isfinite(settings.falloff))
    return true;
  if (allowed && KeyboardPanActive(settings)) {
    // Opposite bindings cancel; normalize diagonals like the native 2D composite. MoveCamera takes a drag, and
    // dragging left moves the view right, hence the negation.
    const float x = float(Shortcut::IsPressed(Action::kMoveRight)) - float(Shortcut::IsPressed(Action::kMoveLeft));
    const float y = float(Shortcut::IsPressed(Action::kMoveUp)) - float(Shortcut::IsPressed(Action::kMoveDown));
    if (const float length = std::sqrt(x * x + y * y); length > 0.0f) {
      const float speed = _this->keyboardDeltaScaler();
      const float scale = _this->deltaScaler();
      if (!std::isfinite(speed) || !std::isfinite(scale))
        return true;
      d                   = {-x / length * speed, -y / length * speed};
      by_keys             = true;
      const auto movement = d * scale;
      if (!std::isfinite(movement.x) || !std::isfinite(movement.y))
        return true;
      _this->MoveCamera(movement, true);
    }
  }

  if (blocked) {
    d = {};
  } else if (!by_keys && !pointer) {
    // Glide: the last drag or key delta shrinks by the falloff every frame.
    d = d * settings.falloff;
    if (!std::isfinite(d.x) || !std::isfinite(d.y))
      return true;
    _this->MoveCamera(d, true);
  }
  if (ReadPanSettings() != settings || _this->trackingPOI() != tracked)
    return true;
  _this->SetLastDelta(d);
  _this->SetFarMagRadiusRatioSystemExtended(_this->farMagRadiusRatioSystemNormal());
  return true;
}

// Move left and right rotate the camera around the station.
void OrbitFrameProvider_UpdateInputData_Hook(auto original, OrbitFrameProvider* _this, unity::Camera* primary_camera)
{
  original(_this, primary_camera);
  unity::Key::Poll();

  auto* sections = Hub::Sections();
  if (!ModMovesCamera() || !sections || sections->CurrentSection() != SectionID::Starbase_Exterior
      || unity::Key::IsInputFocused())
    return;

  constexpr float kDegreesPerSecond = 45.0f;
  const float     step              = kDegreesPerSecond * unity::Time::deltaTime();
  if (Shortcut::IsPressed(Action::kMoveLeft))
    _this->SetRotationAngleDelta(_this->rotationAngleDelta() - step);
  if (Shortcut::IsPressed(Action::kMoveRight))
    _this->SetRotationAngleDelta(_this->rotationAngleDelta() + step);
}

// A held touch reports Stationary, which the game's pan ignores; report it as moving so drags keep panning.
TKTouch* TKTouch_populateWithPosition_Hook(auto original, TKTouch* _this, uintptr_t pos, unity::TouchPhase phase)
{
  auto* touch = original(_this, pos, phase);
  if (touch && touch->phase() == unity::TouchPhase::Stationary)
    touch->SetPhase(unity::TouchPhase::Moved);
  return touch;
}

// --- Station interior zoom ---
//
// The interior view blends from a fixed close frame out to an orbit pinned at one radius, so its far stop is that
// orbit's radius. Widening the orbit's [min, max] lets the zoom go further. The same orbit is the exterior station
// camera, so it is widened only inside the interior and put back everywhere else.

struct InteriorRadiusRange {
  float min, max;
  bool  operator==(const InteriorRadiusRange&) const = default;
};
struct InteriorRadiusEntry {
  il2cpp::GcHandle    provider, constraint, radius;
  InteriorRadiusRange native, applied;
  bool                owns_write = false;
};
// Weak identities distinguish reused addresses and replaced constraints. The process-lifetime owner avoids
// releasing handles after IL2CPP teardown. As in the frozen implementation, admit at most 16 live owners.
auto&            native_interior_radius  = *new std::unordered_map<OrbitFrameProvider*, InteriorRadiusEntry>;
constexpr size_t kInteriorProviderLimit  = 16;
bool             interior_zoom_installed = false;

InteriorRadiusRange ReadInteriorRadius(FloatConstraint* radius)
{ return {radius->min(), radius->max()}; }

void WriteInteriorRadius(FloatConstraint* radius, InteriorRadiusRange range)
{
  radius->SetMin(range.min);
  radius->SetMax(range.max);
}

void RestoreInteriorRadius(InteriorRadiusEntry& entry)
{
  if (!entry.owns_write)
    return;
  auto radius = il2cpp::GcHandle::Strong(entry.radius.Get());
  // A later native/other-feature write supersedes ours; never restore over it.
  if (radius && ReadInteriorRadius(radius.Get<FloatConstraint>()) == entry.applied)
    WriteInteriorRadius(radius.Get<FloatConstraint>(), entry.native);
}

void PruneInteriorRadii()
{
  static auto next = std::chrono::steady_clock::time_point{};
  const auto  now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::seconds(1);
  for (auto it = native_interior_radius.begin(); it != native_interior_radius.end();) {
    auto& entry = it->second;
    if (!entry.provider.Get() || !entry.constraint.Get() || !entry.radius.Get()) {
      RestoreInteriorRadius(entry);
      it = native_interior_radius.erase(it);
    } else {
      ++it;
    }
  }
}

void ApplyInteriorRadius(OrbitFrameProvider* provider)
{
  if (!interior_zoom_installed)
    return;
  PruneInteriorRadii();
  auto*      sections = Hub::Sections();
  const bool interior = sections && sections->CurrentSection() == SectionID::Starbase_Interior;
  const auto scale    = Config::Get().graphics.interior_zoom_scale;
  if (!interior || !std::isfinite(scale) || scale <= 1.0f) {
    // Exit/disable restores even retired providers which no longer receive callbacks.
    for (auto& [_, entry] : native_interior_radius)
      RestoreInteriorRadius(entry);
    native_interior_radius.clear();
    return;
  }

  auto provider_owner = il2cpp::GcHandle::Strong(provider);
  if (!provider_owner)
    return;
  auto* constraint       = provider->constraint();
  auto  constraint_owner = il2cpp::GcHandle::Strong(constraint);
  auto* radius           = constraint_owner ? constraint->radius() : nullptr;
  auto  radius_owner     = il2cpp::GcHandle::Strong(radius);
  auto  cached           = native_interior_radius.find(provider);
  if (cached != native_interior_radius.end()
      && (cached->second.provider.Get() != provider || cached->second.constraint.Get() != constraint
          || cached->second.radius.Get() != radius)) {
    RestoreInteriorRadius(cached->second);
    native_interior_radius.erase(cached);
    cached = native_interior_radius.end();
  }
  if (!radius_owner)
    return;

  auto it = cached;
  if (it == native_interior_radius.end()) {
    // Different providers can alias one FloatData; its already widened range is never a new baseline.
    it = std::ranges::find_if(native_interior_radius,
                              [radius](const auto& item) { return item.second.radius.Get() == radius; });
  }
  if (it == native_interior_radius.end()) {
    if (native_interior_radius.size() >= kInteriorProviderLimit)
      return;
    const auto          native = ReadInteriorRadius(radius);
    InteriorRadiusEntry entry{il2cpp::GcHandle::Weak(provider), il2cpp::GcHandle::Weak(constraint),
                              il2cpp::GcHandle::Weak(radius), native, native};
    if (!entry.provider.Get() || !entry.constraint.Get() || !entry.radius.Get())
      return;
    it = native_interior_radius.emplace(provider, std::move(entry)).first;
  }
  auto&      entry    = it->second;
  const auto observed = ReadInteriorRadius(radius);
  if (observed != entry.applied)
    entry.native = observed;
  const InteriorRadiusRange widened{entry.native.min * scale, entry.native.max * scale};
  if (!std::isfinite(widened.min) || !std::isfinite(widened.max)) {
    RestoreInteriorRadius(entry);
    native_interior_radius.erase(it);
    return;
  }
  if (observed != widened)
    WriteInteriorRadius(radius, widened);
  entry.applied    = widened;
  entry.owns_write = widened != entry.native;
}

void OrbitFrameProvider_UpdateConstrains_Hook(auto original, OrbitFrameProvider* _this, unity::Camera* primary_camera)
{
  ApplyInteriorRadius(_this);
  // The game clamps the camera against the widened or restored range.
  original(_this, primary_camera);
  haven_orbit::ApplyConstraints(_this);
}

bool InstallZoom()
{
  if (!EnsureNavigationZoomUpdates())
    return false;
  il2cpp::HookSet hooks("Zoom");
  IL2CPP_STAGE(hooks, PlanetViewUtils::Class::Method("Populate", 3), PlanetViewUtils_Populate_Hook);
  IL2CPP_STAGE(hooks,
               PlanetViewUtils::Class::Method(
                   "CreateBackdrop", {"Digit.PrimeServer.Models.ITreeNode", "UnityEngine.Object", "System.Boolean"}),
               PlanetViewUtils_CreateBackdrop_Hook);
  IL2CPP_STAGE(hooks,
               PlanetViewUtils::Class::Method("CreateBackdrop", {"Digit.PrimeServer.Models.ITreeNode",
                                                                 "SystemVariationConfigCollection", "System.Boolean"}),
               PlanetViewUtils_CreateBackdrop_Hook);
  IL2CPP_STAGE(hooks, PlanetViewUtils::Class::Method<void(float, float)>("CameraZoomedEventHandler"),
               PlanetViewUtils_CameraZoomedEventHandler_Hook);
  IL2CPP_STAGE(hooks, PlanetViewUtils::Class::Method("get_FlatRenderable"), PlanetViewUtils_get_FlatRenderable_Hook);
  // TODO: SetDepth has only ever been detoured on Windows and the reason was not recorded; check its prologue
  // against SPUD's overwrite size before enabling it on macOS.
#if _WIN32
  IL2CPP_STAGE(hooks, NavigationZoom::Class::Method<void(NodeDepth)>("SetDepth"), NavigationZoom_SetDepth_Hook);
#endif
  IL2CPP_STAGE(hooks, NavigationZoom::Class::Method<void(float, NodeDepth)>("SetViewParameters"),
               NavigationZoom_SetViewParameters_Hook);
  camera_zoom_installed = hooks.Commit();
  return camera_zoom_installed;
}

bool InstallPan()
{
  il2cpp::HookSet hooks("Pan");
  IL2CPP_STAGE(hooks, OrbitFrameProvider::Class::Method("UpdateInputData"), OrbitFrameProvider_UpdateInputData_Hook);
  IL2CPP_STAGE(hooks, TKTouch::Class::Method("populateWithPosition"), TKTouch_populateWithPosition_Hook);
  IL2CPP_STAGE(hooks, NavigationPan::Class::Method("LateUpdate"), NavigationPan_LateUpdate_Hook);
  return hooks.Commit();
}

bool InstallScrollZoomGuard()
{
  if (!scroll_input::Available()) {
    spdlog::warn("[ScrollZoomGuard] native UI scroll contract unavailable; camera input retained");
    return false;
  }
  il2cpp::HookSet hooks("ScrollZoomGuard");
  IL2CPP_STAGE(hooks, NavigationZoom::Class::Method<void()>("ZoomCameraAtWorldPoint"),
               NavigationZoom_ZoomCameraAtWorldPoint_Hook);
  IL2CPP_STAGE(hooks, scroll_input::WheelRecognizer::Method<float()>("ReadInput"),
               MouseScrollAsPinchRecognizer_ReadInput_Hook);
  scroll_queries_available = hooks.Commit();
  return scroll_queries_available;
}

bool InstallInteriorZoom()
{
  if (!OrbitFrameProvider::ConstraintRadiusFieldsAvailable()) {
    spdlog::warn("[InteriorZoom] orbit radius fields unavailable; native behavior retained");
    return false;
  }
  interior_zoom_installed = EnsureOrbitConstraints();
  return interior_zoom_installed;
}
} // namespace

bool EnsureNavigationZoomUpdates()
{
  static const bool installed = [] {
    il2cpp::HookSet hooks("NavigationZoomUpdates");
    IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationZoom::Class::Method<void()>("Update")),
                    NavigationZoom_Update_Hook);
    return hooks.Commit();
  }();
  return installed;
}

bool EnsureOrbitConstraints()
{
  static const bool installed = [] {
    il2cpp::HookSet hooks("OrbitConstraints");
    IL2CPP_STAGE(hooks, OrbitFrameProvider::Class::Method<void(unity::Camera*)>("UpdateConstrains"),
                 OrbitFrameProvider_UpdateConstrains_Hook);
    return hooks.Commit();
  }();
  return installed;
}

const Feature kZoom{"Zoom", &Config::Patches::zoomhooks, InstallZoom};
const Feature kScrollZoomGuard{"ScrollZoomGuard", &Config::Patches::zoomhooks, InstallScrollZoomGuard};
const Feature kPan{"Pan", &Config::Patches::panhooks, InstallPan};
const Feature kInteriorZoom{"InteriorZoom", &Config::Patches::interiorzoomhooks, InstallInteriorZoom};
