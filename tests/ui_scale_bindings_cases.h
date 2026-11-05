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
float ui_scale_fixture_dpi = 1.0f;
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
  int            writes = 0;
  bool           fail_read = false;
};
struct UiScaleGameObject {
  Il2CppObject      header;
  unity::Transform* transform = nullptr;
};
struct UiScaleSelectorObject {
  Il2CppObject        header;
  unity::GameObject* object = nullptr;
};
struct UiScaleScreenObject {
  Il2CppObject         header;
  unity::CanvasScaler* scaler = nullptr;
};
struct UiScaleCanvasObject {
  Il2CppObject      header;
  Il2CppString*     name = nullptr;
  unity::Transform* transform = nullptr;
};
struct UiScaleScalerObject {
  Il2CppObject header;
  float        factor = 0.75f;
  int          writes = 0;
};
unity::GameObject* UiScaleLoadedObject(FleetMeshSelector* selector, const MethodInfo*)
{ return reinterpret_cast<UiScaleSelectorObject*>(selector)->object; }
unity::Transform* UiScaleObjectTransform(unity::GameObject* object, const MethodInfo*)
{ return reinterpret_cast<UiScaleGameObject*>(object)->transform; }
unity::Transform* UiScaleCanvasTransform(unity::Component* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->transform; }
Il2CppString* UiScaleCanvasName(CanvasController* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->name; }
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

void UiScaleNativeCases(std::string_view mode)
{
  const bool missing_local = mode == "scale-missing-local";
  Require(missing_local || mode == "scale-native", "unknown scale fixture mode");
  auto& transform_class = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& selector_class = Define("Digit.Prime.Navigation", "FleetMeshSelector", IL2CPP_TYPE_CLASS, &object_class);
  auto& screen_class = Define("Digit.Client.UI", "ScreenManager", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& canvas_class = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &component_class);
  auto& scaler_class = Define("UnityEngine.UI", "CanvasScaler", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& loaded = AddMethod(selector_class, "get_LoadedObject", reinterpret_cast<void*>(&UiScaleLoadedObject),
                           T(game_object_class), {});
  AddProperty(selector_class, "LoadedObject", &loaded);
  auto& object_transform = AddMethod(game_object_class, "get_transform", reinterpret_cast<void*>(&UiScaleObjectTransform),
                                     T(transform_class), {});
  AddProperty(game_object_class, "transform", &object_transform);
  const MethodInfo* get_scale = nullptr;
  if (!missing_local)
    get_scale = &AddMethod(transform_class, "get_localScale", reinterpret_cast<void*>(&UiScaleLocalScale), T(vec3_class), {});
  auto& set_scale = AddMethod(transform_class, "set_localScale", reinterpret_cast<void*>(&UiScaleSetLocalScale),
                              T(void_class), {T(vec3_class)});
  AddProperty(transform_class, "localScale", get_scale, &set_scale);
  auto& set_factor = AddMethod(scaler_class, "set_scaleFactor", reinterpret_cast<void*>(&UiScaleSetFactor), T(void_class),
                               {T(float_class)});
  AddProperty(scaler_class, "scaleFactor", nullptr, &set_factor);
  AddField(screen_class, "m_canvasRootScaler", scaler_class, offsetof(UiScaleScreenObject, scaler));
  auto& canvas_name = AddMethod(canvas_class, "get_name", reinterpret_cast<void*>(&UiScaleCanvasName), T(string_class), {});
  AddProperty(canvas_class, "name", &canvas_name);
  // CanvasController inherits Component::transform(); its bound accessor resolves
  // on Component, rather than a property redeclared only on the derived canvas.
  auto& canvas_transform = AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&UiScaleCanvasTransform),
                                     T(transform_class), {});
  AddProperty(component_class, "transform", &canvas_transform);

  UiScaleTransformObject model{{&transform_class.klass}}, empty{{&transform_class.klass}}, failed{{&transform_class.klass}};
  empty.scale = {};
  failed.fail_read = true;
  UiScaleGameObject object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&model)};
  UiScaleGameObject empty_object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&empty)};
  UiScaleGameObject failed_object{{&game_object_class.klass}, reinterpret_cast<unity::Transform*>(&failed)};
  UiScaleGameObject no_transform{{&game_object_class.klass}};
  UiScaleSelectorObject selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&object)};
  UiScaleSelectorObject empty_selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&empty_object)};
  UiScaleSelectorObject failed_selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&failed_object)};
  UiScaleSelectorObject no_transform_selector{{&selector_class.klass}, reinterpret_cast<unity::GameObject*>(&no_transform)};
  UiScaleSelectorObject no_object_selector{{&selector_class.klass}};
  fleet_native_widgets[&selector_class.klass] = {&selector.header, &empty_selector.header, &failed_selector.header,
                                                &no_transform_selector.header, &no_object_selector.header};
  const auto roots = warp_roots.size();
  Config::Get().graphics.ui_scale_ship = 1.0f;
  Require(UiScaleConsumerSetShip(2.0f), "valid ship setting is accepted");
  if (missing_local) {
    Require(model.writes == 0 && empty.writes == 0 && failed.writes == 0 && model.scale.x == 2,
            "unavailable local-scale observation cannot collapse loaded models to default zero");
    fleet_native_widgets.clear();
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
  void* returned_resource = nullptr;
  int asset_calls = 0;
  const auto asset_original = [&](auto* source, float* scale, void** asset) {
    ++asset_calls;
    Require(source == reinterpret_cast<il2cpp::Object*>(&ship) && asset == &returned_resource,
            "asset hook preserves its native receiver and resource output");
    if (scale)
      *scale = 4.0f;
    *asset = &resource;
    return true;
  };
  Config::Get().graphics.ui_scale_ship = 3.0f;
  float native_scale = 0;
  Require(SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship),
                                                          &native_scale, &returned_resource)
              && native_scale == 12 && asset_calls == 1 && returned_resource == &resource,
          "newly loaded model multiplies the original native asset scale exactly once");
  Config::Get().graphics.ui_scale_ship = 0;
  SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship),
                                                 &native_scale, &returned_resource);
  Require(native_scale == 4, "nonpositive ship multiplier leaves new assets native");
  native_scale = 7;
  const auto asset_failure = [&](auto*, auto*, auto*) { ++asset_calls; return false; };
  Require(!SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_failure, reinterpret_cast<il2cpp::Object*>(&ship),
                                                           &native_scale, &returned_resource)
              && native_scale == 7, "failed asset load does not modify its scale output");
  SystemViewShip_TryGetShipAssetBundleResource_Hook(asset_original, reinterpret_cast<il2cpp::Object*>(&ship), nullptr,
                                                 &returned_resource);

  UiScaleScalerObject scaler{{&scaler_class.klass}};
  UiScaleScreenObject screen{{&screen_class.klass}, reinterpret_cast<unity::CanvasScaler*>(&scaler)};
  int canvas_updates = 0;
  const auto canvas_original = [&](auto*) { ++canvas_updates; scaler.factor = 0.75f; };
  Config::Get().graphics.allow_cursor = true;
  Config::Get().graphics.ui_scale = 0;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(canvas_updates == 1 && scaler.writes == 0 && scaler.factor == 0.75f,
          "zero interface scale preserves original native update");
  ui_scale_fixture_dpi = 1.5f;
  Config::Get().graphics.ui_scale = 0.6f;
  ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
  Require(std::abs(scaler.factor - 0.9f) < 0.00001f && canvas_updates == 2,
          "interface uses configured scale times monitor DPI after original");
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

  UiScaleCanvasObject viewer{{&canvas_class.klass}, InstantWarpString("ObjectViewerTemplate_Canvas"),
                             reinterpret_cast<unity::Transform*>(&model)};
  int shows = 0;
  const auto show_original = [&](auto*, int32_t entry, bool instant) {
    ++shows;
    Require(entry == 7 && instant, "viewer forwards native Show arguments");
    Require(model.scale.x == 1.25f && model.scale.y == 1.25f && model.scale.z == 1.25f,
            "native Show observes the viewer scale already applied");
  };
  Config::Get().graphics.ui_scale_viewer = 1.25f;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Require(shows == 1 && model.scale.x == 1.25f && model.scale.y == 1.25f && model.scale.z == 1.25f,
          "matching viewer canvas scales every axis before native Show");
  const auto viewer_writes = model.writes;
  Config::Get().graphics.ui_scale_viewer = 0;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Config::Get().graphics.ui_scale_viewer = 2;
  viewer.name = InstantWarpString("Other_Canvas");
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  viewer.name = InstantWarpString("ObjectViewerTemplate_Canvas");
  viewer.transform = nullptr;
  CanvasController_Show_Hook(show_original, reinterpret_cast<CanvasController*>(&viewer), 7, true);
  Require(shows == 4 && model.writes == viewer_writes,
          "zero, unrelated canvas and missing transform always keep native Show without an override");

  const auto saved = fleet_native_save_calls;
  Config::Get().graphics.ui_scale = 0;
  Config::Get().graphics.ui_scale_viewer = 0;
  Config::Get().graphics.ui_scale_step = 0.2f;
  UiScaleConsumerAdjust(true);
  UiScaleConsumerAdjustViewer(true);
  Require(fleet_native_save_calls == saved, "zero interface/viewer shortcuts do not enable scaling or persist");
  Config::Get().graphics.ui_scale = 1.95f;
  UiScaleConsumerAdjust(true);
  Require(Config::Get().graphics.ui_scale == 2 && fleet_native_saved_key == "ui_scale"
              && std::get<double>(fleet_native_saved_any) == double(2.0f) && ui_scale_fixture_dpi_refreshes == 1,
          "interface shortcut clamps at two and submits the canonical setting with a DPI refresh");
  Config::Get().graphics.ui_scale_viewer = 1.0f;
  UiScaleConsumerAdjustViewer(true);
  Require(Config::Get().graphics.ui_scale_viewer == 1.05f && fleet_native_saved_key == "ui_scale_viewer",
          "viewer shortcut retains quarter-step arithmetic and persistence identity");
  Config::Get().graphics.ui_scale_ship = 19.95f;
  UiScaleConsumerAdjustShip(true);
  Require(Config::Get().graphics.ui_scale_ship == 20 && fleet_native_saved_key == "ui_scale_ship"
              && std::get<double>(fleet_native_saved_any) == double(20.0f),
          "ship shortcut retains its independent upper limit and persisted float");
  Require(warp_roots.size() == roots, "scale consumers leave no managed custody between operations");
  fleet_native_widgets.clear();
}
} // namespace
