// [graphics] ui_scale and ui_scale_menu independently size the HUD and menus. Zero uses native scaling.
// Ship models and object viewers retain their separate overrides. HUD shortcuts start at the observed native size.

#include "config.h"
#include "features/features.h"
#include "runtime_config.h"
#include "settings/basic_gameplay_settings.h"

#include "game/fleet.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/tracker.h>
#include <unity/ui.h>

#include <algorithm>
#include <cmath>
#include <vector>

#if _WIN32
#include <Windows.h>
#endif

namespace
{
struct CanvasUiScaleState {
  il2cpp::GcHandle owner, rect;
  unity::Vector3   native_scale{}, applied_scale{};
  unity::Vector2   native_delta{}, applied_delta{};
  float            ratio       = 1.0f;
  bool             initialized = false;
  bool             hud = false, side_chat = false;
  int32_t          depth = 0;
};
std::vector<CanvasUiScaleState> ui_scale_canvases;
float                           ui_scale_native_factor = 0.0f, ui_scale_menu_factor = 0.0f, ui_scale_hud_factor = 0.0f;

bool UiScaleNear(float a, float b)
{ return std::abs(a - b) < 0.0001f; }
bool UiScaleNear(unity::Vector2 a, unity::Vector2 b)
{ return UiScaleNear(a.x, b.x) && UiScaleNear(a.y, b.y); }
bool UiScaleNear(unity::Vector3 a, unity::Vector3 b)
{ return UiScaleNear(a.x, b.x) && UiScaleNear(a.y, b.y) && UiScaleNear(a.z, b.z); }

float UiScaleRenderFactor(float setting, float native, float dpi)
{
  if (setting == 0.0f)
    return native;
  const auto factor = setting * dpi;
  return std::isnan(factor) ? 1.0f : std::clamp(factor, 0.1f, 5.0f);
}

// Increasing the logical viewport as its content shrinks preserves the native edge anchors and click geometry.
unity::Vector2 UiScaleViewportDelta(unity::Vector2 native_delta, unity::Vector2 parent_size, unity::Vector2 anchor_min,
                                    unity::Vector2 anchor_max, float ratio)
{
  const auto inverse = 1.0f / ratio - 1.0f;
  return {native_delta.x + ((anchor_max.x - anchor_min.x) * parent_size.x + native_delta.x) * inverse,
          native_delta.y + ((anchor_max.y - anchor_min.y) * parent_size.y + native_delta.y) * inverse};
}

bool IsHudScaleCanvas(CanvasController* canvas, unity::RectTransform* rect)
{
  const auto name = il2cpp::ToUtf8(canvas->name());
  if (name.starts_with("HUD") || name.starts_with("Hud"))
    return true;
  auto* context = canvas->Context();
  if (context && il2cpp::ClassName(context->klass).starts_with("Digit.Prime.HUD."))
    return true;
  // The same chat screen is used fullscreen and docked. Only its native side-frame placement is part of the HUD.
  if (name == "MainChatScreen_Canvas") {
    auto* parent = rect->parent();
    auto* object = parent ? parent->gameObject() : nullptr;
    return object && object->NameIs("SideFrame");
  }
  return false;
}

void ApplyCanvasUiScale(CanvasUiScaleState& state)
{
  auto* canvas = state.owner.Get<CanvasController>();
  auto* rect   = state.rect.Get<unity::RectTransform>();
  if (!unity::Alive(canvas) || !unity::Alive(rect) || !canvas->Visible() || ui_scale_menu_factor <= 0.0f
      || ui_scale_hud_factor <= 0.0f)
    return;
  auto* parent      = rect->parent();
  auto* parent_rect = parent ? parent->As<unity::RectTransform>() : nullptr;
  if (!parent_rect)
    return;
  float inherited = 1.0f;
  for (auto* ancestor = parent; ancestor; ancestor = ancestor->parent())
    for (const auto& other : ui_scale_canvases)
      if (&other != &state && other.rect.Get() == ancestor)
        inherited *= other.ratio;
  auto*      parent_object = state.side_chat ? parent->gameObject() : nullptr;
  const auto hud           = state.side_chat ? parent_object && parent_object->NameIs("SideFrame") : state.hud;
  const auto desired       = hud ? ui_scale_hud_factor : ui_scale_menu_factor;
  const auto ratio         = desired / ui_scale_hud_factor / inherited;
  if (!std::isfinite(ratio) || ratio <= 0.0f || (ratio == 1.0f && state.ratio == 1.0f))
    return;
  const auto scale  = rect->TryCall<"get_localScale", unity::Vector3>();
  const auto delta  = rect->TryCall<"get_sizeDelta", unity::Vector2>();
  const auto bounds = parent_rect->TryCall<"get_rect", unity::Rect>();
  const auto min    = rect->TryCall<"get_anchorMin", unity::Vector2>();
  const auto max    = rect->TryCall<"get_anchorMax", unity::Vector2>();
  if (!scale || !delta || !bounds || !min || !max || bounds.value.width <= 0 || bounds.value.height <= 0)
    return;
  const auto first = !state.initialized;
  // Native layout/animation may update only one axis. Keep untouched axes on their unmodified baseline.
  const auto baseline = [first](float current, float applied, float& native) {
    if (first || !UiScaleNear(current, applied))
      native = current;
  };
  baseline(scale.value.x, state.applied_scale.x, state.native_scale.x);
  baseline(scale.value.y, state.applied_scale.y, state.native_scale.y);
  baseline(scale.value.z, state.applied_scale.z, state.native_scale.z);
  baseline(delta.value.x, state.applied_delta.x, state.native_delta.x);
  baseline(delta.value.y, state.applied_delta.y, state.native_delta.y);
  state.initialized = true;

  const auto next_scale = state.native_scale * ratio;
  const auto next_delta =
      UiScaleViewportDelta(state.native_delta, {bounds.value.width, bounds.value.height}, min.value, max.value, ratio);
  if (!std::isfinite(next_delta.x) || !std::isfinite(next_delta.y))
    return;
  if (!UiScaleNear(scale.value, next_scale))
    rect->SetLocalScale(next_scale);
  if (!UiScaleNear(delta.value, next_delta))
    rect->Set<"sizeDelta">(next_delta);
  state.applied_scale = next_scale;
  state.applied_delta = next_delta;
  state.ratio         = ratio;
}

void RefreshCanvasUiScales()
{
  if (ui_scale_hud_factor <= 0.0f || ui_scale_menu_factor <= 0.0f)
    return;
  if (UiScaleNear(ui_scale_hud_factor, ui_scale_menu_factor)
      && std::ranges::all_of(ui_scale_canvases, [](const CanvasUiScaleState& state) { return state.ratio == 1.0f; }))
    return;
  std::erase_if(ui_scale_canvases, [](const CanvasUiScaleState& state) {
    return !unity::Alive(state.owner.Get<CanvasController>()) || !unity::Alive(state.rect.Get<unity::RectTransform>());
  });
  for (auto& state : ui_scale_canvases)
    ApplyCanvasUiScale(state);
}

void ObserveCanvasUiScale(CanvasController* canvas)
{
  // Also retire closed screens when both scales are native and the regular refresh is a no-op.
  std::erase_if(ui_scale_canvases, [](const CanvasUiScaleState& state) {
    return !unity::Alive(state.owner.Get<CanvasController>()) || !unity::Alive(state.rect.Get<unity::RectTransform>());
  });
  auto* transform = canvas->transform();
  auto* rect      = transform ? transform->As<unity::RectTransform>() : nullptr;
  if (!rect)
    return;
  const auto found = std::ranges::find_if(
      ui_scale_canvases, [canvas](const CanvasUiScaleState& state) { return state.owner.Get() == canvas; });
  int32_t depth = 0;
  for (auto* parent = rect->parent(); parent; parent = parent->parent())
    ++depth;
  if (found == ui_scale_canvases.end()) {
    CanvasUiScaleState state;
    state.owner     = il2cpp::GcHandle::Weak(canvas);
    state.rect      = il2cpp::GcHandle::Weak(rect);
    state.hud       = IsHudScaleCanvas(canvas, rect);
    state.side_chat = il2cpp::Equals(canvas->name(), "MainChatScreen_Canvas");
    state.depth     = depth;
    if (state.owner && state.rect)
      ui_scale_canvases.push_back(std::move(state));
  } else {
    found->hud   = IsHudScaleCanvas(canvas, rect);
    found->depth = depth;
  }
  std::ranges::stable_sort(ui_scale_canvases, {}, &CanvasUiScaleState::depth);
  RefreshCanvasUiScales();
}

void ResetUiScaleState()
{
  ui_scale_canvases.clear();
  ui_scale_native_factor = ui_scale_menu_factor = ui_scale_hud_factor = 0.0f;
}

// Ships already in the system keep the scale they were loaded with, so rescale them by the change.
void RescaleLoadedShips(float old_multiplier, float new_multiplier)
{
  if (!std::isfinite(old_multiplier) || !std::isfinite(new_multiplier) || old_multiplier <= 0.0f
      || new_multiplier <= 0.0f)
    return;

  const auto ratio = new_multiplier / old_multiplier;
  if (!std::isfinite(ratio) || ratio <= 0.0f)
    return;
  for (auto* selector : unity::Instances<FleetMeshSelector>()) {
    auto* object    = selector->LoadedObject();
    auto* transform = object ? object->transform() : nullptr;
    if (!transform)
      continue;
    const auto local_scale = transform->TryCall<"get_localScale", unity::Vector3>();
    if (local_scale)
      transform->SetLocalScale(local_scale.value * ratio);
  }
}

// The scale a ship model is loaded with; ships loaded earlier are handled by RescaleLoadedShips.
bool SystemViewShip_TryGetShipAssetBundleResource_Hook(auto original, il2cpp::Object* _this, float* scale,
                                                       void** asset_bundle_resource)
{
  const auto result     = original(_this, scale, asset_bundle_resource);
  const auto multiplier = Config::Get().graphics.ui_scale_ship;
  if (result && scale && multiplier > 0.0f)
    *scale *= multiplier;
  return result;
}

void ScreenManager_UpdateCanvasRootScaleFactor_Hook(auto original, ScreenManager* _this)
{
  original(_this);

  // allow_cursor = false, as in window.cc: rescaling canvases makes the game set its own cursor again.
#if _WIN32
  static auto cursor = LoadCursor(nullptr, IDC_ARROW);
  if (!Config::Get().graphics.allow_cursor)
    SetCursor(cursor);
#endif

  auto* scaler = _this->canvasRootScaler();
  if (!scaler)
    return;
  const auto native = scaler->scaleFactor();
  if (!std::isfinite(native) || native <= 0.0f)
    return;
  const auto& graphics   = Config::Get().graphics;
  const auto  dpi        = WindowDpiScale();
  ui_scale_native_factor = native;
  ui_scale_menu_factor   = UiScaleRenderFactor(graphics.ui_scale_menu, native, dpi);
  ui_scale_hud_factor    = UiScaleRenderFactor(graphics.ui_scale, native, dpi);
  // Retain the native frame widths and HUD coordinate space. Menus compensate inside their own canvases.
  scaler->SetScaleFactor(ui_scale_hud_factor);
  scaler->ApplyScaleFactor(ui_scale_hud_factor);
  RefreshCanvasUiScales();
}

void CanvasController_Show_Hook(auto original, CanvasController* _this, int32_t entry_point, bool instant)
{
  const auto viewer_scale = Config::Get().graphics.ui_scale_viewer;
  if (viewer_scale != 0.0f && il2cpp::Equals(_this->name(), "ObjectViewerTemplate_Canvas"))
    if (auto* transform = _this->transform())
      transform->SetLocalScale({viewer_scale, viewer_scale, viewer_scale});
  original(_this, entry_point, instant);
  ObserveCanvasUiScale(_this);
}

bool InstallUiScale()
{
  il2cpp::HookSet hooks("UiScale");
  IL2CPP_STAGE(hooks, SystemViewShip::Method("TryGetShipAssetBundleResource", 2),
               SystemViewShip_TryGetShipAssetBundleResource_Hook);
  IL2CPP_STAGE(hooks, ScreenManager::Class::Method("UpdateCanvasRootScaleFactor"),
               ScreenManager_UpdateCanvasRootScaleFactor_Hook);
  IL2CPP_STAGE(hooks, CanvasController::Class::Method<void(int32_t, bool)>("Show"), CanvasController_Show_Hook);
  return hooks.Commit();
}
} // namespace

void AdjustUiScale(bool up)
{
  auto& graphics = Config::Get().graphics;
  if (graphics.ui_scale == 0.0f) {
    const auto dpi = WindowDpiScale();
    if (ui_scale_native_factor <= 0.0f || !std::isfinite(dpi) || dpi <= 0.0f)
      return;
    graphics.ui_scale = ui_scale_native_factor / dpi;
  }
  const auto change =
      gameplay_settings::AdjustScale(graphics.ui_scale, graphics.ui_scale_step, up, gameplay_settings::Scale::Interface,
                                     [](const gameplay_settings::Key& key, double value) {
                                       runtime_config::SaveSetting(key.section, key.name, value, key.delay);
                                     });
  if (!change.applied)
    return;
  spdlog::info("[UiScale] HUD scale {} -> {} (asked {}), DPI scale {}", change.previous, change.value, change.requested,
               WindowDpiScale(true));
}

void AdjustUiViewerScale(bool up)
{
  auto&      graphics = Config::Get().graphics;
  const auto change   = gameplay_settings::AdjustScale(
      graphics.ui_scale_viewer, graphics.ui_scale_step, up, gameplay_settings::Scale::Viewer,
      [](const gameplay_settings::Key& key, double value) {
        runtime_config::SaveSetting(key.section, key.name, value, key.delay);
      });
  if (!change.applied)
    return;
  spdlog::info("[UiScale] viewer scale {} -> {} (asked {})", change.previous, change.value, change.requested);
}

void AdjustUiShipScale(bool up)
{
  auto&      graphics = Config::Get().graphics;
  const auto change =
      gameplay_settings::AdjustScale(graphics.ui_scale_ship, graphics.ui_scale_step, up, gameplay_settings::Scale::Ship,
                                     [](const gameplay_settings::Key& key, double value) {
                                       runtime_config::SaveSetting(key.section, key.name, value, key.delay);
                                     });
  spdlog::info("[UiScale] ship scale {} -> {} (asked {})", change.previous, change.value, change.requested);
  RescaleLoadedShips(change.previous, change.value);
}

bool SetUiShipScale(float value)
{
  auto& current = Config::Get().graphics.ui_scale_ship;
  if (!std::isfinite(value) || value <= 0.0f)
    return false;
  const auto previous = current;
  current             = value;
  RescaleLoadedShips(previous, value);
  return true;
}

const Feature kUiScale{"UiScale", &Config::Patches::uiscalehooks, InstallUiScale,
                       nullptr,   InstallRequirement::kOptional,  ResetUiScaleState};
