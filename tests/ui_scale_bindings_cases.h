#pragma once

// Production scaling callbacks and setting mutations. The existing controlled
// runtime supplies metadata, tracking snapshots and the setting-writer sink.
// DPI/cursor calls are replaced here; this does not qualify a real display.
#include <unity/tracker.h>
#if _WIN32
#include <Windows.h>
#endif

namespace
{
float ui_scale_fixture_dpi           = 1.0f;
int   ui_scale_fixture_dpi_refreshes = 0, ui_scale_fixture_cursor_sets = 0;
float UiScaleFixtureDpi(bool refresh = false)
{
  ui_scale_fixture_dpi_refreshes += refresh;
  return ui_scale_fixture_dpi;
}
#if _WIN32
HCURSOR UiScaleFixtureLoadCursor(HINSTANCE, LPCTSTR)
{ return reinterpret_cast<HCURSOR>(uintptr_t{1}); }
HCURSOR UiScaleFixtureSetCursor(HCURSOR)
{
  ++ui_scale_fixture_cursor_sets;
  return nullptr;
}
#endif
} // namespace

// The top-level shortcut fixture already defines these three exports as sinks.
// Compile this feature's actual definitions under private fixture names.
#define AdjustUiScale UiScaleConsumerAdjust
#define AdjustUiViewerScale UiScaleConsumerAdjustViewer
#define AdjustUiShipScale UiScaleConsumerAdjustShip
#define SetUiShipScale UiScaleConsumerSetShip
#define WindowDpiScale UiScaleFixtureDpi
#if _WIN32
#pragma push_macro("LoadCursor")
#pragma push_macro("SetCursor")
#undef LoadCursor
#define LoadCursor UiScaleFixtureLoadCursor
#define SetCursor UiScaleFixtureSetCursor
#endif
#include "../src/mod/features/ui_scale.cc"
#if _WIN32
#pragma pop_macro("SetCursor")
#pragma pop_macro("LoadCursor")
#endif
#undef WindowDpiScale
#undef SetUiShipScale
#undef AdjustUiShipScale
#undef AdjustUiViewerScale
#undef AdjustUiScale

namespace
{
struct UiScaleTransformObject {
  Il2CppObject   header;
  unity::Vector3 scale{2, 3, 4};
  int            writes    = 0;
  bool           fail_read = false;
};
struct UiScaleGameObject {
  Il2CppObject      header;
  unity::Transform* transform = nullptr;
};
struct UiScaleSelectorObject {
  Il2CppObject       header;
  unity::GameObject* object = nullptr;
};
struct UiScaleScreenObject {
  Il2CppObject         header;
  unity::CanvasScaler* scaler = nullptr;
};
struct UiScaleCanvasObject {
  Il2CppObject      header;
  Il2CppString*     name      = nullptr;
  unity::Transform* transform = nullptr;
};
struct UiScaleRectObject {
  UiScaleTransformObject transform;
  unity::Transform*      parent = nullptr;
  unity::Vector2         delta{}, anchor_min{}, anchor_max{1, 1};
  unity::Rect            bounds{0, 0, 1600, 900};
  int                    delta_writes = 0;
};
struct UiScaleScalerObject {
  Il2CppObject header;
  float        factor           = 0.75f;
  int          writes           = 0;
  int          immediate_writes = 0;
};
unity::GameObject* UiScaleLoadedObject(FleetMeshSelector* selector, const MethodInfo*)
{ return reinterpret_cast<UiScaleSelectorObject*>(selector)->object; }
unity::Transform* UiScaleObjectTransform(unity::GameObject* object, const MethodInfo*)
{ return reinterpret_cast<UiScaleGameObject*>(object)->transform; }
unity::Transform* UiScaleCanvasTransform(unity::Component* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->transform; }
Il2CppString* UiScaleCanvasName(CanvasController* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->name; }
bool UiScaleCanvasVisible(CanvasController*, const MethodInfo*)
{ return true; }
il2cpp::Object* UiScaleCanvasContext(CanvasController*, const MethodInfo*)
{ return nullptr; }
unity::Transform* UiScaleParent(unity::Transform* transform, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(transform)->parent; }
unity::Vector2 UiScaleDelta(unity::RectTransform* transform, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(transform)->delta; }
void UiScaleSetDelta(unity::RectTransform* transform, unity::Vector2 delta, const MethodInfo*)
{
  auto* value  = reinterpret_cast<UiScaleRectObject*>(transform);
  value->delta = delta;
  ++value->delta_writes;
}
unity::Rect UiScaleBounds(unity::RectTransform* transform, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(transform)->bounds; }
unity::Vector2 UiScaleAnchorMin(unity::RectTransform* transform, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(transform)->anchor_min; }
unity::Vector2 UiScaleAnchorMax(unity::RectTransform* transform, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(transform)->anchor_max; }
unity::Vector3 UiScaleLocalScale(unity::Transform* transform, const MethodInfo*)
{
  auto* value = reinterpret_cast<UiScaleTransformObject*>(transform);
  if (value->fail_read)
    throw Il2CppExceptionWrapper(&thrown);
  return value->scale;
}
void UiScaleSetLocalScale(unity::Transform* transform, unity::Vector3 scale, const MethodInfo*)
{
  auto* value = reinterpret_cast<UiScaleTransformObject*>(transform);
  ++value->writes;
  value->scale = scale;
}
void UiScaleSetFactor(unity::CanvasScaler* scaler, float factor, const MethodInfo*)
{
  auto* value = reinterpret_cast<UiScaleScalerObject*>(scaler);
  ++value->writes;
  value->factor = factor;
}
float UiScaleFactor(unity::CanvasScaler* scaler, const MethodInfo*)
{ return reinterpret_cast<UiScaleScalerObject*>(scaler)->factor; }
void UiScaleApplyFactor(unity::CanvasScaler* scaler, float factor, const MethodInfo*)
{
  auto* value = reinterpret_cast<UiScaleScalerObject*>(scaler);
  ++value->immediate_writes;
  value->factor = factor;
}

void UiScaleNativeCases(std::string_view mode)
{
  ResetUiScaleState();
  const bool missing_local = mode == "scale-missing-local";
  Require(missing_local || mode == "scale-native", "unknown scale fixture mode");
  auto& transform_class      = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& rect_transform_class = Define("UnityEngine", "RectTransform", IL2CPP_TYPE_CLASS, &transform_class);
  auto& vector2_class        = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& rect_class           = Define("UnityEngine", "Rect", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Rect));
  auto& selector_class       = Define("Digit.Prime.Navigation", "FleetMeshSelector", IL2CPP_TYPE_CLASS, &object_class);
  auto& screen_class         = Define("Digit.Client.UI", "ScreenManager", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& canvas_class         = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &component_class);
  auto& scaler_class         = Define("UnityEngine.UI", "CanvasScaler", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& loaded = AddMethod(selector_class, "get_LoadedObject", reinterpret_cast<void*>(&UiScaleLoadedObject),
                           T(game_object_class), {});
  AddProperty(selector_class, "LoadedObject", &loaded);
  auto& object_transform = AddMethod(game_object_class, "get_transform",
                                     reinterpret_cast<void*>(&UiScaleObjectTransform), T(transform_class), {});
  AddProperty(game_object_class, "transform", &object_transform);
  const MethodInfo* get_scale = nullptr;
  if (!missing_local)
    get_scale =
        &AddMethod(transform_class, "get_localScale", reinterpret_cast<void*>(&UiScaleLocalScale), T(vec3_class), {});
  auto& set_scale = AddMethod(transform_class, "set_localScale", reinterpret_cast<void*>(&UiScaleSetLocalScale),
                              T(void_class), {T(vec3_class)});
  AddProperty(transform_class, "localScale", get_scale, &set_scale);
  auto& set_factor = AddMethod(scaler_class, "set_scaleFactor", reinterpret_cast<void*>(&UiScaleSetFactor),
                               T(void_class), {T(float_class)});
  auto& get_factor =
      AddMethod(scaler_class, "get_scaleFactor", reinterpret_cast<void*>(&UiScaleFactor), T(float_class), {});
  AddProperty(scaler_class, "scaleFactor", &get_factor, &set_factor);
  AddMethod(scaler_class, "SetScaleFactor", reinterpret_cast<void*>(&UiScaleApplyFactor), T(void_class),
            {T(float_class)});
  AddField(screen_class, "m_canvasRootScaler", scaler_class, offsetof(UiScaleScreenObject, scaler));
  auto& canvas_name =
      AddMethod(canvas_class, "get_name", reinterpret_cast<void*>(&UiScaleCanvasName), T(string_class), {});
  AddProperty(canvas_class, "name", &canvas_name);
  auto& canvas_visible =
      AddMethod(canvas_class, "get_Visible", reinterpret_cast<void*>(&UiScaleCanvasVisible), T(bool_class), {});
  AddProperty(canvas_class, "Visible", &canvas_visible);
  auto& canvas_context =
      AddMethod(canvas_class, "get_Context", reinterpret_cast<void*>(&UiScaleCanvasContext), T(object_class), {});
  AddProperty(canvas_class, "Context", &canvas_context);
  auto& parent_get =
      AddMethod(transform_class, "get_parent", reinterpret_cast<void*>(&UiScaleParent), T(transform_class), {});
  AddProperty(transform_class, "parent", &parent_get);
  auto& delta_get =
      AddMethod(rect_transform_class, "get_sizeDelta", reinterpret_cast<void*>(&UiScaleDelta), T(vector2_class), {});
  auto& delta_set = AddMethod(rect_transform_class, "set_sizeDelta", reinterpret_cast<void*>(&UiScaleSetDelta),
                              T(void_class), {T(vector2_class)});
  AddProperty(rect_transform_class, "sizeDelta", &delta_get, &delta_set);
  auto& rect_get =
      AddMethod(rect_transform_class, "get_rect", reinterpret_cast<void*>(&UiScaleBounds), T(rect_class), {});
  AddProperty(rect_transform_class, "rect", &rect_get);
  auto& min_get = AddMethod(rect_transform_class, "get_anchorMin", reinterpret_cast<void*>(&UiScaleAnchorMin),
                            T(vector2_class), {});
  auto& max_get = AddMethod(rect_transform_class, "get_anchorMax", reinterpret_cast<void*>(&UiScaleAnchorMax),
                            T(vector2_class), {});
  AddProperty(rect_transform_class, "anchorMin", &min_get);
  AddProperty(rect_transform_class, "anchorMax", &max_get);
  // CanvasController inherits Component::transform(); its bound accessor resolves
  // on Component, rather than a property redeclared only on the derived canvas.
  auto& canvas_transform = AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&UiScaleCanvasTransform),
                                     T(transform_class), {});
  AddProperty(component_class, "transform", &canvas_transform);

  UiScaleTransformObject model{{&transform_class.klass}}, empty{{&transform_class.klass}},
      failed{{&transform_class.klass}};
  empty.scale      = {};
  failed.fail_read = true;
  UiScaleGameObject     object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&model)};
  UiScaleGameObject     empty_object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&empty)};
  UiScaleGameObject     failed_object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&failed)};
  UiScaleGameObject     no_transform{{&game_object_class.klass}};
  UiScaleSelectorObject selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&object)};
  UiScaleSelectorObject empty_selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&empty_object)};
  UiScaleSelectorObject failed_selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&failed_object)};
  UiScaleSelectorObject no_transform_selector{{&selector_class.klass},
                                              reinterpret_cast<unity::GameObject*>(&no_transform)};
  UiScaleSelectorObject no_object_selector{{&selector_class.klass}};
  fleet_native_widgets[&selector_class.klass] = {&selector.header, &empty_selector.header, &failed_selector.header,
                                                 &no_transform_selector.header, &no_object_selector.header};
  const auto roots                            = warp_roots.size();
  Config::Get().graphics.ui_scale_ship        = 1.0f;
  Require(UiScaleConsumerSetShip(2.0f), "valid ship setting is accepted");
  if (missing_local) {
    Require(model.writes == 0 && empty.writes == 0 && failed.writes == 0 && model.scale.x == 2,
            "unavailable local-scale observation cannot collapse loaded models to default zero");
    fleet_native_widgets.clear();
    ResetUiScaleState();
    return;
  }
  Require(model.writes == 1 && model.scale.x == 4 && model.scale.y == 6 && model.scale.z == 8,
          "actual loaded-model consumer applies new/old ratio to every axis");
  Require(empty.writes == 1 && empty.scale.x == 0 && failed.writes == 0 && failed.scale.x == 2,
          "known zero scale remains valid while failed getter is skipped");
  Require(UiScaleConsumerSetShip(3.0f) && model.scale.x == 6 && model.scale.y == 9 && model.scale.z == 12,
          "repeated setting changes preserve original model proportions");
  Require(UiScaleConsumerSetShip(1.0f) && model.scale.x == 2 && model.scale.y == 3 && model.scale.z == 4,
          "resetting the ship multiplier restores loaded model proportions");
  const auto writes = model.writes;
  for (const auto invalid_previous : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()}) {
    Config::Get().graphics.ui_scale_ship = invalid_previous;
    Require(UiScaleConsumerSetShip(1.0f) && Config::Get().graphics.ui_scale_ship == 1.0f && model.writes == writes,
            "valid positive setting recovers an unusable previous value without guessing a loaded-model ratio");
  }
  Require(!UiScaleConsumerSetShip(0.0f) && !UiScaleConsumerSetShip(-1.0f)
              && !UiScaleConsumerSetShip(std::numeric_limits<float>::infinity())
              && Config::Get().graphics.ui_scale_ship == 1.0f && model.writes == writes,
          "invalid new ship settings are refused");
  RescaleLoadedShips(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
  Require(model.writes == writes, "overflowed multiplier ratio cannot write infinite model transforms");

  Il2CppObject ship{}, resource{};
  void*        returned_resource = nullptr;
  int          asset_calls       = 0;
  const auto   asset_original    = [&](auto* source, float* scale, void** asset) {
    ++asset_calls;
    Require(source == reinterpret_cast<il2cpp::Object*>(&ship) && asset == &returned_resource,
            "asset hook preserves its native receiver and resource output");
    if (scale)
      *scale = 4.0f;
    *asset = &resource;
    return true;
  };
  Config::Get().graphics.ui_scale_ship = 3.0f;
  float native_scale                   = 0;
  Require(SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship),
                                                            &native_scale, &returned_resource)
              && native_scale == 12 && asset_calls == 1 && returned_resource == &resource,
          "newly loaded model multiplies the original native asset scale exactly once");
  Config::Get().graphics.ui_scale_ship = 0;
  SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship),
                                                    &native_scale, &returned_resource);
  Require(native_scale == 4, "nonpositive ship multiplier leaves new assets native");
  native_scale             = 7;
  const auto asset_failure = [&](auto*, auto*, auto*) {
    ++asset_calls;
    return false;
  };
  Require(!SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_failure, reinterpret_cast<il2cpp::Object*>(&ship),
                                                             &native_scale, &returned_resource)
              && native_scale == 7,
          "failed asset load does not modify its scale output");
  SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship), nullptr,
                                                    &returned_resource);

  UiScaleScalerObject scaler{{&scaler_class.klass}};
  UiScaleScreenObject screen{{&screen_class.klass}, reinterpret_cast<unity::CanvasScaler*>(&scaler)};
  int                 canvas_updates  = 0;
  const auto          canvas_original = [&](auto*) {
    ++canvas_updates;
    scaler.factor = 0.75f;
  };
  Config::Get().graphics.allow_cursor  = true;
  Config::Get().graphics.ui_scale      = 0;
  Config::Get().graphics.ui_scale_menu = 0;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(canvas_updates == 1 && scaler.writes == 1 && scaler.immediate_writes == 1 && scaler.factor == 0.75f,
          "native HUD scale is observed and applied before dependent canvas layouts");
  ui_scale_fixture_dpi            = 1.5f;
  Config::Get().graphics.ui_scale = 0.6f;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(std::abs(scaler.factor - 0.9f) < 0.00001f && canvas_updates == 2,
          "shared HUD frame retains configured scale times monitor DPI after original");
  Config::Get().graphics.ui_scale_menu = 0.8f;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(std::abs(scaler.factor - 0.9f) < 0.00001f && UiScaleNear(ui_scale_menu_factor, 1.2f),
          "independent menu preference does not resize the shared HUD frame");
  Config::Get().graphics.ui_scale = 10;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(scaler.factor == 5, "render factor upper clamp remains five");
  Config::Get().graphics.ui_scale = -1;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(scaler.factor == 0.1f, "render factor lower clamp remains one tenth");
  Config::Get().graphics.ui_scale = std::numeric_limits<float>::quiet_NaN();
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(scaler.factor == 1, "NaN render factor keeps frozen fallback to one");
#if _WIN32
  Config::Get().graphics.allow_cursor = false;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(ui_scale_fixture_cursor_sets == 1, "cursor restoration remains scoped to disabled native cursor changes");
#endif

  UiScaleCanvasObject viewer{{&canvas_class.klass},
                             InstantWarpString("ObjectViewerTemplate_Canvas"),
                             reinterpret_cast<unity::Transform*>(&model)};
  int                 shows         = 0;
  const auto          show_original = [&](auto*, int32_t entry, bool instant) {
    ++shows;
    Require(entry == 7 && instant, "viewer forwards native Show arguments");
    Require(model.scale.x == 1.25f && model.scale.y == 1.25f && model.scale.z == 1.25f,
            "native Show observes the viewer scale already applied");
  };
  Config::Get().graphics.ui_scale_viewer = 1.25f;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Require(shows == 1 && model.scale.x == 1.25f && model.scale.y == 1.25f && model.scale.z == 1.25f,
          "matching viewer canvas scales every axis before native Show");
  const auto viewer_writes               = model.writes;
  Config::Get().graphics.ui_scale_viewer = 0;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Config::Get().graphics.ui_scale_viewer = 2;
  viewer.name                            = InstantWarpString("Other_Canvas");
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  viewer.name      = InstantWarpString("ObjectViewerTemplate_Canvas");
  viewer.transform = nullptr;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Require(shows == 4 && model.writes == viewer_writes,
          "zero, unrelated canvas and missing transform always keep native Show without an override");

  Require(UiScaleRenderFactor(0, .75f, 1.5f) == .75f && UiScaleRenderFactor(10, .75f, 1.5f) == 5
              && UiScaleRenderFactor(-1, .75f, 1.5f) == .1f
              && UiScaleRenderFactor(std::numeric_limits<float>::quiet_NaN(), .75f, 1.5f) == 1,
          "native scaling, render limits and invalid-number fallback remain shared by both preferences");
  const auto stretched = UiScaleViewportDelta({0, 0}, {1600, 900}, {0, 0}, {1, 1}, .5f);
  Require(UiScaleNear(stretched, {1600, 900}),
          "shrinking a stretched root expands its logical viewport instead of moving edge-anchored controls inward");
  const auto partial = UiScaleViewportDelta({-20, -40}, {1600, 900}, {.25f, .25f}, {.75f, .75f}, 2.0f);
  Require(UiScaleNear(partial, {-410, -245}), "partial anchors retain their physical viewport at larger content scale");
  for (const float pivot : {0.0f, .2f, 1.0f}) {
    const auto width = (1600 + stretched.x) * .5f;
    Require(UiScaleNear(-pivot * width, -pivot * 1600) && UiScaleNear((1 - pivot) * width, (1 - pivot) * 1600),
            "viewport compensation preserves both edges for centered and off-center pivots");
  }

  UiScaleRectObject frame{{{&rect_transform_class.klass}}}, menu_rect{{{&rect_transform_class.klass}}};
  menu_rect.transform.scale = {1, 1, 1};
  menu_rect.parent          = reinterpret_cast<unity::Transform*>(&frame);
  menu_rect.delta           = {-20, -40};
  UiScaleCanvasObject menu{
      {&canvas_class.klass}, InstantWarpString("Store_Canvas"), reinterpret_cast<unity::Transform*>(&menu_rect)};
  CanvasUiScaleState state;
  state.owner = il2cpp::GcHandle::Strong(&menu.header);
  state.rect  = il2cpp::GcHandle::Strong(&menu_rect.transform.header);
  ui_scale_canvases.push_back(std::move(state));
  ui_scale_hud_factor  = .5f;
  ui_scale_menu_factor = 1.0f;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  Require(UiScaleNear(menu_rect.transform.scale, {2, 2, 2}) && UiScaleNear(menu_rect.delta, {-810, -470}),
          "a menu restores its independent content size while preserving the native viewport extent");
  const auto layout_writes = menu_rect.transform.writes + menu_rect.delta_writes;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  Require(menu_rect.transform.writes + menu_rect.delta_writes == layout_writes,
          "repeated layout refreshes neither accumulate scaling nor rewrite unchanged geometry");
  menu_rect.delta.x = -10;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  Require(UiScaleNear(ui_scale_canvases.back().native_delta, {-10, -40}) && UiScaleNear(menu_rect.delta, {-805, -470}),
          "a native horizontal layout change does not rebase the untouched vertical axis to its compensated value");
  menu_rect.delta.x = -20;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  ui_scale_menu_factor = .75f;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  Require(UiScaleNear(menu_rect.transform.scale.x, 1.5f)
              && UiScaleNear((1600 + menu_rect.delta.x) * menu_rect.transform.scale.x, 1580),
          "live menu changes derive from the native baseline and retain screen-edge geometry");
  ui_scale_menu_factor = ui_scale_hud_factor;
  ApplyCanvasUiScale(ui_scale_canvases.back());
  Require(UiScaleNear(menu_rect.transform.scale, {1, 1, 1}) && UiScaleNear(menu_rect.delta, {-20, -40}),
          "matching menu and HUD scales restores the native transform and offsets");
  ResetUiScaleState();
  Require(warp_roots.size() == roots, "layout reset releases both observed canvas handles");

  const auto saved                       = fleet_native_save_calls;
  ui_scale_native_factor                 = 0;
  Config::Get().graphics.ui_scale        = 0;
  Config::Get().graphics.ui_scale_viewer = 0;
  Config::Get().graphics.ui_scale_step   = 0.2f;
  UiScaleConsumerAdjust(true);
  UiScaleConsumerAdjustViewer(true);
  Require(fleet_native_save_calls == saved && Config::Get().graphics.ui_scale == 0,
          "HUD shortcut without native-size evidence and zero viewer shortcut do not persist guesses");
  ui_scale_native_factor = .75f;
  UiScaleConsumerAdjust(true);
  Require(UiScaleNear(Config::Get().graphics.ui_scale, .7f) && fleet_native_save_calls == saved + 1
              && fleet_native_saved_key == "ui_scale" && Config::Get().graphics.ui_scale_menu == .8f,
          "HUD shortcut starts from native pixels divided by DPI and persists without changing menus");
  Config::Get().graphics.ui_scale = 1.95f;
  UiScaleConsumerAdjust(true);
  Require(Config::Get().graphics.ui_scale == 2 && fleet_native_saved_key == "ui_scale"
              && std::get<double>(fleet_native_saved_any) == double(2.0f) && ui_scale_fixture_dpi_refreshes == 2,
          "HUD shortcut clamps at two and submits the canonical setting with a DPI refresh");
  Config::Get().graphics.ui_scale_viewer = 1.0f;
  UiScaleConsumerAdjustViewer(true);
  Require(Config::Get().graphics.ui_scale_viewer == 1.05f && fleet_native_saved_key == "ui_scale_viewer",
          "viewer shortcut retains quarter-step arithmetic and persistence identity");
  Config::Get().graphics.ui_scale_ship = 19.95f;
  UiScaleConsumerAdjustShip(true);
  Require(Config::Get().graphics.ui_scale_ship == 20 && fleet_native_saved_key == "ui_scale_ship"
              && std::get<double>(fleet_native_saved_any) == double(20.0f),
          "ship shortcut retains its independent upper limit and persisted float");
  ResetUiScaleState();
  Require(warp_roots.size() == roots, "scale reset releases any managed custody retained during layout observation");
  fleet_native_widgets.clear();
}
} // namespace
