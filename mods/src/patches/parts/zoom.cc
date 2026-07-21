/**
 * @file zoom.cc
 * @brief Camera zoom controls — presets, keyboard zoom, and far-clip extension.
 *
 * Hooks NavigationZoom methods to provide:
 *  - Five user-assignable zoom presets (store/recall via hotkeys)
 *  - Smooth keyboard-driven zoom in/out with configurable speed
 *  - Extended maximum zoom range (controlled by Config::Get().zoom)
 *  - Automatic far-clip plane adjustment so distant objects remain visible
 *  - Optional "use preset as default" behavior
 *
 * Config keys:
 *  - zoom:                   maximum zoom distance
 *  - keyboard_zoom_speed:    zoom delta per second for keyboard zoom
 *  - system_zoom_preset_1–5: stored zoom preset values
 *  - default_system_zoom:    zoom level applied on system entry / reset
 *  - hotkeys_extended:       enables ZoomReset / ZoomMin / ZoomMax keys
 *  - use_presets_as_default: automatically updates default zoom after preset recall
 */

#include "config.h"
#include "errormsg.h"
#include "patches/hook_registry.h"
#include "patches/mod_impact_monitor.h"

#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "patches/key.h"
#include "testable_functions.h"

#include <il2cpp/il2cpp_helper.h>

#include <prime/NavigationPan.h>
#include <prime/NavigationZoom.h>
#include <prime/PlanetViewUtils.h>
#include <prime/Transform.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// ─── Helpers ────────────────────────────────────────────────────────────────

namespace
{
constexpr HookDescriptor kPlanetViewUtilsCameraZoomedEventHandlerHook{
    "PlanetViewUtils.CameraZoomedEventHandler",
    "Re-apply backdrop presentation and trigger flat-renderable scaling after system zoom events.",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils", "CameraZoomedEventHandler"},
    "System-view border suppression can regress after zoom changes."};

constexpr HookDescriptor kPlanetViewUtilsGetFlatRenderableHook{
    "PlanetViewUtils.get_FlatRenderable",
    "Scale the flat renderable to hide edge void at extreme system zoom.",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils", "get_FlatRenderable"},
    "Extreme system zoom can expose backdrop void at the edges."};

constexpr HookDescriptor kNavigationZoomUpdateHook{
    "NavigationZoom.Update", "Route configured zoom bindings and preserve the configured system zoom range.",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom", "Update"},
    "Keyboard zoom and the extended range stop working."};

constexpr HookDescriptor kNavigationZoomSetDepthHook{
    "NavigationZoom.SetDepth", "Apply system zoom presentation during Windows depth transitions.",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom", "SetDepth"},
    "The default zoom or background scaling can regress on system entry."};

constexpr HookDescriptor kNavigationZoomSetViewParametersHook{
    "NavigationZoom.SetViewParameters", "Apply extended zoom ratios while system view parameters are initialized.",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom", "SetViewParameters"},
    "The configured maximum zoom can be capped by the game."};

struct ZoomRuntimeDispatchCache {
  uint64_t                                     generation = 0;
  std::vector<KeyCode>                         watched_keys;
  std::vector<input_binding::DispatchKeyState> key_states;
  input_binding::DispatchPlan                  plan;
};

constexpr std::array kZoomActions{
    input_binding::InputActionId::ZoomIn,         input_binding::InputActionId::ZoomOut,
    input_binding::InputActionId::ZoomPreset1,    input_binding::InputActionId::ZoomPreset2,
    input_binding::InputActionId::ZoomPreset3,    input_binding::InputActionId::ZoomPreset4,
    input_binding::InputActionId::ZoomPreset5,    input_binding::InputActionId::ZoomMin,
    input_binding::InputActionId::ZoomMax,        input_binding::InputActionId::ZoomReset,
    input_binding::InputActionId::SetZoomPreset1, input_binding::InputActionId::SetZoomPreset2,
    input_binding::InputActionId::SetZoomPreset3, input_binding::InputActionId::SetZoomPreset4,
    input_binding::InputActionId::SetZoomPreset5, input_binding::InputActionId::SetZoomDefault,
};

ZoomRuntimeDispatchCache &zoom_runtime_dispatch_cache()
{
  static auto cache = ZoomRuntimeDispatchCache{};
  return cache;
}

input_binding::ModifierMask zoom_held_modifier_mask()
{
  input_binding::ModifierMask modifiers;
  for (const auto modifier_key : {KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                                  KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::LeftWindows, KeyCode::RightWindows,
                                  KeyCode::LeftCommand, KeyCode::RightCommand, KeyCode::AltGr}) {
    if (Key::Pressed(modifier_key)) {
      modifiers.Merge(input_binding::ModifierMask::FromPressedKey(modifier_key));
    }
  }

  return modifiers;
}

void rebuild_zoom_watched_keys(ZoomRuntimeDispatchCache &cache, const input_binding::CompileResult &runtime_bindings)
{
  const auto generation = input_binding::RuntimeBindingGeneration();
  if (cache.generation == generation) {
    return;
  }

  cache.watched_keys = input_binding::WatchedKeysForActions(
      runtime_bindings, input_binding::InputPhase::NavigationZoomUpdate, kZoomActions);
  cache.generation = generation;
}

const input_binding::DispatchPlan &navigation_zoom_dispatch_plan()
{
  auto       &cache            = zoom_runtime_dispatch_cache();
  const auto &runtime_bindings = input_binding::RuntimeBindingModel();
  rebuild_zoom_watched_keys(cache, runtime_bindings);

  cache.key_states.clear();
  cache.key_states.reserve(cache.watched_keys.size());

  const auto modifiers = zoom_held_modifier_mask();
  for (const auto key : cache.watched_keys) {
    cache.key_states.push_back({key, modifiers, Key::Down(key), Key::Pressed(key)});
  }

  input_binding::PlanDispatchSnapshot(runtime_bindings, input_binding::InputPhase::NavigationZoomUpdate,
                                      input_binding::ActiveLayers::Only(input_binding::InputLayer::Zoom),
                                      cache.key_states, cache.plan);
  return cache.plan;
}

bool zoom_winner_present(const input_binding::DispatchPlan &plan, const input_binding::InputActionId action)
{ return plan.winner_lookup.Contains(action, input_binding::InputLayer::Zoom); }

input_binding::InputActionId first_zoom_winner(const input_binding::DispatchPlan                  &plan,
                                               const std::span<const input_binding::InputActionId> actions)
{ return plan.winner_lookup.First(actions); }
} // namespace

/** @brief Calls IL2CPP MathUtils::GetMouseWorldPos to project screen coords to world space. */
vec3 GetMouseWorldPos(void *cam, vec3 *pos)
{
  static auto class_helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils");
  static auto fn           = class_helper.GetMethodInfo("GetMouseWorldPos");

  void            *args[2]   = {cam, (void *)pos};
  Il2CppException *exception = NULL;
  auto             result    = il2cpp_runtime_invoke(fn, nullptr, args, &exception);
  return *(vec3 *)(il2cpp_object_unbox(result));
}

/// Flag set by depth/view hooks to trigger a zoom-to-default on the next Update.
auto do_default_zoom = false;

static float           s_expectedScale = 0.0f;
static void           *s_cachedFR      = nullptr;
static void           *s_lastScaledFR  = nullptr;
static NavigationZoom *s_navZoom       = nullptr;

inline void SetSceneCameraFarClip(NavigationZoom *zoom, float farClipPlane)
{
  if (zoom && zoom->_sceneCamera) {
    zoom->_sceneCamera->farClipPlane = farClipPlane;
  }
}

inline void SetSceneCameraPresentation(NavigationZoom *zoom)
{
  if (!zoom || !zoom->_sceneCamera) {
    return;
  }

  SetSceneCameraFarClip(zoom, Config::Get().zoom * 3.75f);
  zoom->_sceneCamera->clearFlags      = 2;
  zoom->_sceneCamera->backgroundColor = {0, 0, 0, 0};
}

void ApplySystemZoomRange(NavigationZoom *zoom, const float radius)
{
  const auto configured_maximum = Config::Get().zoom;
  if (!zoom || radius <= 0.0f || configured_maximum <= 0.0f) {
    return;
  }

  const auto ratio                 = configured_maximum / radius;
  zoom->_farRatioSystemNormal      = 0.55f * ratio;
  zoom->_farRatioSystemExtended    = ratio;
  s_navZoom                        = zoom;
}

void EnsureSystemZoomRange(NavigationZoom *zoom)
{
  if (!zoom || zoom->_depth != NodeDepth::SolarSystem || Config::Get().zoom <= 0.0f) {
    return;
  }

  ApplySystemZoomRange(zoom, zoom->_viewRadius);
  if (zoom->_maximum < Config::Get().zoom) {
    zoom->_maximum = Config::Get().zoom;
  }

  const auto range = zoom->_maximum - zoom->_minimum;
  if (range > 0.0f) {
    zoom->_zoomtotal = range;
  }
  SetSceneCameraPresentation(zoom);
}

/**
 * @brief Saves the current zoom distance as a named preset.
 * @param label  Human-readable preset name (for log output).
 * @param zoom   Reference to the config preset slot to update.
 * @param _this  NavigationZoom instance to read current distance from.
 *
 * Normalizes distance into a [0, Config::zoom] range for portability across
 * systems with different _minimum/_maximum values.
 */
inline void StoreZoom(std::string label, float &zoom, NavigationZoom *_this)
{
  auto old_zoom = zoom;
  zoom          = (_this->Distance - _this->_minimum) / (_this->_maximum - _this->_minimum) * Config::Get().zoom;
  spdlog::info("Changing {} from {} to {}", label, old_zoom, zoom);
}

static void ScaleFR(void *fr)
{
  if (!fr) {
    return;
  }

  const auto factor = Config::Get().fr_scale;
  if (factor <= 0.0f || factor == 1.0f) {
    return;
  }

  static auto comp_helper   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  static auto get_transform = comp_helper.GetProperty("transform");

  auto *t = reinterpret_cast<Transform *>(get_transform.GetRaw<Il2CppObject>(fr));
  if (!t || !t->localScale) {
    return;
  }

  const auto *scale = t->localScale;
  if (fr == s_lastScaledFR && s_expectedScale > 0.0f && std::fabs(scale->x - s_expectedScale) < 0.1f) {
    return;
  }

  Vector3 newScale{scale->x * factor, scale->y * factor, scale->z * factor};
  t->localScale   = &newScale;
  s_lastScaledFR  = fr;
  s_expectedScale = newScale.x;
}

// ─── Main Zoom Hook ─────────────────────────────────────────────────────────

/**
 * @brief Hook: NavigationZoom::Update
 *
 * Intercepts the per-frame zoom update to process keyboard-driven zoom
 * inputs (presets, smooth zoom in/out, reset, min/max).
 * Original method: updates camera zoom from touch/pinch input.
 * Our modification: plans zoom-phase dispatcher actions each frame; for
 *                   preset keys it stores or recalls a preset; for zoom keys
 *                   it computes a per-frame delta (or absolute target) and
 *                   calls ZoomCameraAtWorldPoint() to apply the zoom anchored
 *                   at the mouse position.
 */
void NavigationZoom_Update_Hook(auto original, NavigationZoom *_this)
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::NavigationZoomUpdate, ModImpactMonitorEnabled());

  static auto GetMousePosition =
      il2cpp_resolve_icall_typed<void(vec3 *)>("UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  static auto GetDeltaTime = il2cpp_resolve_icall_typed<float()>("UnityEngine.Time::get_deltaTime()");

  if (!_this) {
    impact_timer.ExcludeCall([&] { original(_this); });
    return;
  }

  EnsureSystemZoomRange(_this);

  const auto dt               = GetDeltaTime();
  auto       zoomDelta        = 0.0f;
  bool       do_absolute_zoom = false;
  bool       do_store_zoom    = false;
  auto       config           = &Config::Get();

  if (!Key::IsInputFocused()) {
    const auto dispatcher_owns_zoom =
        hotkey_dispatcher_owns_inputs(Config::Get().hotkeys_enabled, ScopelyShortcutsPolicy());
    auto zoom_in_pressed  = false;
    auto zoom_out_pressed = false;

    if (dispatcher_owns_zoom) {
      const auto &zoom_dispatch_plan = navigation_zoom_dispatch_plan();

      const auto set_zoom_action = first_zoom_winner(
          zoom_dispatch_plan,
          std::array{input_binding::InputActionId::SetZoomPreset1, input_binding::InputActionId::SetZoomPreset2,
                     input_binding::InputActionId::SetZoomPreset3, input_binding::InputActionId::SetZoomPreset4,
                     input_binding::InputActionId::SetZoomPreset5, input_binding::InputActionId::SetZoomDefault});
      if (set_zoom_action == input_binding::InputActionId::SetZoomPreset1) {
        return StoreZoom("System Preset 1", config->system_zoom_preset_1, _this);
      } else if (set_zoom_action == input_binding::InputActionId::SetZoomPreset2) {
        return StoreZoom("System Preset 2", config->system_zoom_preset_2, _this);
      } else if (set_zoom_action == input_binding::InputActionId::SetZoomPreset3) {
        return StoreZoom("System Preset 3", config->system_zoom_preset_3, _this);
      } else if (set_zoom_action == input_binding::InputActionId::SetZoomPreset4) {
        return StoreZoom("System Preset 4", config->system_zoom_preset_4, _this);
      } else if (set_zoom_action == input_binding::InputActionId::SetZoomPreset5) {
        return StoreZoom("System Preset 5", config->system_zoom_preset_5, _this);
      } else if (set_zoom_action == input_binding::InputActionId::SetZoomDefault) {
        return StoreZoom("System Default", config->default_system_zoom, _this);
      }

      do_absolute_zoom         = true;
      const auto preset_action = first_zoom_winner(
          zoom_dispatch_plan,
          std::array{input_binding::InputActionId::ZoomPreset1, input_binding::InputActionId::ZoomPreset2,
                     input_binding::InputActionId::ZoomPreset3, input_binding::InputActionId::ZoomPreset4,
                     input_binding::InputActionId::ZoomPreset5});
      if (preset_action == input_binding::InputActionId::ZoomPreset1) {
        zoomDelta     = config->system_zoom_preset_1;
        do_store_zoom = true;
      } else if (preset_action == input_binding::InputActionId::ZoomPreset2) {
        zoomDelta     = config->system_zoom_preset_2;
        do_store_zoom = true;
      } else if (preset_action == input_binding::InputActionId::ZoomPreset3) {
        zoomDelta     = config->system_zoom_preset_3;
        do_store_zoom = true;
      } else if (preset_action == input_binding::InputActionId::ZoomPreset4) {
        zoomDelta     = config->system_zoom_preset_4;
        do_store_zoom = true;
      } else if (preset_action == input_binding::InputActionId::ZoomPreset5) {
        zoomDelta     = config->system_zoom_preset_5;
        do_store_zoom = true;
      }

      if (config->hotkeys_extended) {
        if (zoom_winner_present(zoom_dispatch_plan, input_binding::InputActionId::ZoomReset)) {
          do_absolute_zoom = false;
          do_default_zoom  = true;
        } else if (zoom_winner_present(zoom_dispatch_plan, input_binding::InputActionId::ZoomMin)) {
          zoomDelta = config->zoom;
        } else if (zoom_winner_present(zoom_dispatch_plan, input_binding::InputActionId::ZoomMax)) {
          zoomDelta = 100;
        }
      }

      zoom_in_pressed  = zoom_winner_present(zoom_dispatch_plan, input_binding::InputActionId::ZoomIn);
      zoom_out_pressed = zoom_winner_present(zoom_dispatch_plan, input_binding::InputActionId::ZoomOut);
    }

    if (do_default_zoom) {
      do_absolute_zoom = true;
      zoomDelta        = config->default_system_zoom;
    }

    if (zoomDelta == 0.0f) {
      do_absolute_zoom = false;
      zoomDelta        = config->keyboard_zoom_speed * dt;
    }

    if (zoom_in_pressed || do_absolute_zoom) {
      if (!_this->_sceneCamera) {
        impact_timer.ExcludeCall([&] { original(_this); });
        do_default_zoom = false;
        return;
      }

      vec3 mousePos;
      GetMousePosition(&mousePos);
      _this->_zoomLocation = vec2{.x = mousePos.x, .y = mousePos.y};
      if (do_absolute_zoom) {
        auto zoom_distance = _this->_minimum + (_this->_maximum - _this->_minimum) * (zoomDelta / config->zoom);
        _this->Distance    = zoom_distance;
      } else {
        _this->_zoomDelta     = zoomDelta;
        _this->_lastZoomDelta = zoomDelta;
      }
      auto worldPos      = GetMouseWorldPos(_this->_sceneCamera, &mousePos);
      _this->_worldPoint = worldPos;
      _this->ZoomCameraAtWorldPoint();
    } else if (zoom_out_pressed) {
      if (!_this->_sceneCamera) {
        impact_timer.ExcludeCall([&] { original(_this); });
        do_default_zoom = false;
        return;
      }

      vec3 mousePos;
      GetMousePosition(&mousePos);
      _this->_zoomLocation  = vec2{.x = mousePos.x, .y = mousePos.y};
      _this->_zoomDelta     = -1.0f * zoomDelta;
      _this->_lastZoomDelta = -1.0f * zoomDelta;
      auto worldPos         = GetMouseWorldPos(_this->_sceneCamera, &mousePos);
      _this->_worldPoint    = worldPos;
      _this->ZoomCameraAtWorldPoint();
    }
  }

  if (zoomDelta > 0.0f && config->use_presets_as_default && do_store_zoom) {
    StoreZoom("System Preset Default from Preset", config->default_system_zoom, _this);
  }

  do_default_zoom = false;

  impact_timer.ExcludeCall([&] { original(_this); });
  EnsureSystemZoomRange(_this);
}

void PlanetViewUtils_CameraZoomedEventHandler_Hook(auto original, PlanetViewUtils *_this, float zoomDistance,
                                                   float normalizedZoom)
{
  original(_this, zoomDistance, normalizedZoom);

  // The game often reads the flat renderable field directly. Force the accessor path so our
  // detour gets a chance to rescale the backdrop after zoom/view transitions.
  if (_this) {
    _this->GetFlatRenderable();
  }

  if (!s_navZoom || !s_navZoom->_sceneCamera) {
    return;
  }

  const auto clear_flags = s_navZoom->_sceneCamera->clearFlags;
  if (clear_flags >= 0 && clear_flags <= 4 && clear_flags != 2) {
    SetSceneCameraPresentation(s_navZoom);
  }
}

// ─── View Parameter / Depth Hooks ──────────────────────────────────────────

/**
 * @brief Hook: NavigationZoom::SetViewParameters
 *
 * Intercepts system-view setup to extend the zoom range and far-clip plane
 * proportionally to Config::zoom / radius. Only modifies SolarSystem depth;
 * galaxy/sector views pass through unmodified.
 */
void NavigationZoom_SetViewParameters_Hook(auto original, NavigationZoom *_this, float radius, NodeDepth depth)
{
  if (!_this) {
    original(_this, radius, depth);
    return;
  }

  if (depth == NodeDepth::SolarSystem) {
    ApplySystemZoomRange(_this, radius);
    SetSceneCameraPresentation(_this);
    original(_this, radius, depth);
    SetSceneCameraPresentation(_this);
    do_default_zoom = true;
  } else {
    original(_this, radius, depth);
  }
}

/**
 * @brief Hook: NavigationZoom::ApplyRangeChanges
 *
 * Same far-ratio / far-clip extension as SetViewParameters, but triggered
 * when the game recalculates range after a dynamic change.
 * Currently commented out in hook installation (kept for reference).
 */
void NavigationZoom_ApplyRangeChanges_Hook(auto original, NavigationZoom *_this)
{
  if (!_this) {
    original(_this);
    return;
  }

  if (_this->_depth == NodeDepth::SolarSystem) {
    auto ratio                     = (Config::Get().zoom / _this->_viewRadius);
    _this->_farRatioSystemNormal   = 0.55f * ratio;
    _this->_farRatioSystemExtended = 1 * ratio;
    original(_this);
    SetSceneCameraFarClip(_this, Config::Get().zoom * 2.75f);
    do_default_zoom = true;
  } else {
    original(_this);
  }
}

/**
 * @brief Hook: NavigationZoom::SetDepth
 *
 * Intercepts depth transitions to adjust far-ratio and far-clip for the
 * solar-system view. Sets do_default_zoom so the next Update applies the
 * user's default zoom level upon entering a system.
 */
void NavigationZoom_SetDepth_Hook(auto original, NavigationZoom *_this, NodeDepth depth)
{
  if (!_this) {
    original(_this, depth);
    return;
  }

  if (depth == NodeDepth::SolarSystem) {
    ApplySystemZoomRange(_this, _this->_viewRadius);
    SetSceneCameraPresentation(_this);
    original(_this, depth);
    SetSceneCameraPresentation(_this);
    do_default_zoom = true;
    if (s_cachedFR) {
      ScaleFR(s_cachedFR);
    }
  } else {
    original(_this, depth);
  }
}

void *PlanetViewUtils_get_FlatRenderable_Hook(auto original, PlanetViewUtils *_this)
{
  auto *fr = original(_this);
  if (!fr) {
    return fr;
  }

  s_cachedFR = fr;
  ScaleFR(fr);
  return fr;
}

/**
 * @brief Hook: NavigationCamera::SetSystemViewSizeData
 *
 * Alternative entry point for system-view setup (accesses NavigationZoom
 * at a fixed offset from the NavigationCamera pointer).
 * Currently commented out in hook installation.
 */
void NavigationCamera_SetSystemViewSizeData_Hook(auto original, uint8_t *_this_cam, float radius, Vector3 *systemPos,
                                                 NodeDepth depth)
{
  if (depth == NodeDepth::SolarSystem) {
    if (!_this_cam) {
      original(_this_cam, radius, systemPos, depth);
      return;
    }

    auto _this = *(NavigationZoom **)(_this_cam + 0x20);
    if (!_this) {
      original(_this_cam, radius, systemPos, depth);
      return;
    }

    auto ratio                     = (Config::Get().zoom / radius);
    _this->_farRatioSystemNormal   = 0.55f * ratio;
    _this->_farRatioSystemExtended = 1 * ratio;
    original(_this_cam, radius, systemPos, depth);
    SetSceneCameraFarClip(_this, Config::Get().zoom * 2.75f);
    do_default_zoom = true;
  } else {
    original(_this_cam, radius, systemPos, depth);
  }
}

// ─── Hook Installation ──────────────────────────────────────────────────────

/** @brief Resolves NavigationZoom IL2CPP methods and installs all zoom hooks. */
void InstallZoomHooks()
{
  HookModuleHealth hooks("ZoomPlanetViewHooks");

  auto planet_view_utils_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils");
  if (!planet_view_utils_helper.isValidHelper()) {
    hooks.record_missing_helper(kPlanetViewUtilsCameraZoomedEventHandlerHook);
    hooks.record_missing_helper(kPlanetViewUtilsGetFlatRenderableHook);
    ErrorMsg::MissingHelper("Navigation", "PlanetViewUtils");
  } else {
    auto ptr_camera_zoomed = planet_view_utils_helper.GetMethod("CameraZoomedEventHandler");
    if (ptr_camera_zoomed == nullptr) {
      hooks.record_missing_method(kPlanetViewUtilsCameraZoomedEventHandlerHook);
      ErrorMsg::MissingMethod("PlanetViewUtils", "CameraZoomedEventHandler");
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kPlanetViewUtilsCameraZoomedEventHandlerHook, ptr_camera_zoomed,
                                       PlanetViewUtils_CameraZoomedEventHandler_Hook);
    }

    auto ptr_flat_renderable = planet_view_utils_helper.GetMethod("get_FlatRenderable");
    if (ptr_flat_renderable == nullptr) {
      hooks.record_missing_method(kPlanetViewUtilsGetFlatRenderableHook);
      ErrorMsg::MissingMethod("PlanetViewUtils", "get_FlatRenderable");
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kPlanetViewUtilsGetFlatRenderableHook, ptr_flat_renderable,
                                       PlanetViewUtils_get_FlatRenderable_Hook);
    }
  }

  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom");
  if (!screen_manager_helper.isValidHelper()) {
    hooks.record_missing_helper(kNavigationZoomUpdateHook);
    hooks.record_missing_helper(kNavigationZoomSetViewParametersHook);
#if _WIN32
    hooks.record_missing_helper(kNavigationZoomSetDepthHook);
#else
    hooks.record_skipped(kNavigationZoomSetDepthHook, "SetDepth detour is only required on Windows");
#endif
    ErrorMsg::MissingHelper("Navigation", "NavigationZoom");
  } else {
    auto ptr_update = screen_manager_helper.GetMethod("Update");
    if (ptr_update == nullptr) {
      hooks.record_missing_method(kNavigationZoomUpdateHook);
      ErrorMsg::MissingMethod("NavigationZoom", "Update");
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kNavigationZoomUpdateHook, ptr_update, NavigationZoom_Update_Hook);
    }

#if _WIN32
    auto ptr_set_depth = screen_manager_helper.GetMethod("SetDepth");
    if (ptr_set_depth == nullptr) {
      hooks.record_missing_method(kNavigationZoomSetDepthHook);
      ErrorMsg::MissingMethod("NavigationZoom", "SetDepth");
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kNavigationZoomSetDepthHook, ptr_set_depth,
                                       NavigationZoom_SetDepth_Hook);
    }
#else
    hooks.record_skipped(kNavigationZoomSetDepthHook, "SetDepth detour is only required on Windows");
#endif

    auto ptr_set_view_parameters = screen_manager_helper.GetMethod("SetViewParameters");
    if (ptr_set_view_parameters == nullptr) {
      hooks.record_missing_method(kNavigationZoomSetViewParametersHook);
      ErrorMsg::MissingMethod("NavigationZoom", "SetViewParameters");
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kNavigationZoomSetViewParametersHook, ptr_set_view_parameters,
                                       NavigationZoom_SetViewParameters_Hook);
    }
  }

  hooks.log_summary();
}
