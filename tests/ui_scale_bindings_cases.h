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
  Il2CppObject         header;
  unity::Transform*    transform = nullptr;
  il2cpp::ArrayObject* scrollers = nullptr;
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
  Il2CppObject       header;
  Il2CppString*      name              = nullptr;
  unity::Transform*  transform         = nullptr;
  bool               visible           = true;
  unity::GameObject* object            = nullptr;
  bool               in_transition     = false;
  VisibilityState    state             = VisibilityState::Visible;
  bool               requested_visible = true;
};
struct UiScaleRectObject {
  UiScaleTransformObject transform;
  unity::Transform*      parent = nullptr;
  unity::Vector2         delta{}, anchor_min{}, anchor_max{1, 1};
  unity::Rect            bounds{0, 0, 1600, 900};
  int                    delta_writes   = 0;
  bool                   dynamic_rect   = false;
  float*                 applied_factor = nullptr;
  unity::Vector2         position{};
};
struct UiScaleScalerObject {
  Il2CppObject header;
  float        factor           = 0.75f;
  float        applied_factor   = 0.75f;
  int          writes           = 0;
  int          immediate_writes = 0;
};
struct UiScaleScrollerObject {
  Il2CppObject          header;
  bool                  initialized = true, ready = true, regenerating = false;
  Il2CppObject *        spawning = nullptr, *initializing = nullptr;
  unity::RectTransform *viewport = nullptr, *content = nullptr;
  float                 screen_width = 1425, screen_height = 1109;
  int                   notifications = 0, current_index = 0, first_row = 0;
  CanvasController*     canvas = nullptr;
};
struct UiScaleScrollerArray {
  Il2CppObject        header;
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 1;
  Il2CppObject*       values[1]{};
};
struct UiScaleVector2Box {
  Il2CppObject   header;
  unity::Vector2 value{};
} ui_scale_position_box;
const MethodInfo *ui_scale_normalized_method = nullptr, *ui_scale_notify_method = nullptr;
int               ui_scale_scroller_scans = 0, ui_scale_position_reads = 0;

Il2CppObject* UiScaleScrollerInvoke(const MethodInfo* method, void* target, void** args, Il2CppException**,
                                    bool& handled)
{
  handled = method == ui_scale_normalized_method || method == ui_scale_notify_method;
  if (!handled)
    return nullptr;
  auto* smart   = static_cast<UiScaleScrollerObject*>(target);
  auto* content = reinterpret_cast<UiScaleRectObject*>(smart->content);
  if (method == ui_scale_normalized_method) {
    ++ui_scale_position_reads;
    ui_scale_position_box.value = {1, 1 - content->position.y / 3435.58545f};
    return &ui_scale_position_box.header;
  }
  const auto position = *static_cast<unity::Vector2*>(args[0]);
  Require(UiScaleNear(position, {1, 1 - content->position.y / 3435.58545f}),
          "recycling notification uses the actual current normalized position");
  ++smart->notifications;
  // Controlled native recycler: notifying a bottom viewport replaces the stale top pool rows.
  smart->current_index = std::min(7, std::max(0, int(content->position.y / 345)));
  smart->first_row     = std::max(0, smart->current_index - 2);
  return nullptr;
}
il2cpp::ArrayObject* UiScaleScrollerChildren(unity::GameObject* object, Il2CppReflectionType* type, bool inactive,
                                             const MethodInfo*)
{
  ++ui_scale_scroller_scans;
  Require(type && !inactive, "settled recycling discovery observes the active typed descendants");
  return reinterpret_cast<UiScaleGameObject*>(object)->scrollers;
}
unity::GameObject* UiScaleCanvasGameObject(unity::Component* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->object; }
bool UiScaleCanvasInTransition(CanvasController* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->in_transition; }
unity::Vector2 UiScaleAnchoredPosition(unity::RectTransform* rect, const MethodInfo*)
{ return reinterpret_cast<UiScaleRectObject*>(rect)->position; }
CanvasController* UiScaleScrollerCanvas(SmartScrollerBase* smart, Il2CppReflectionType*, const MethodInfo*)
{ return reinterpret_cast<UiScaleScrollerObject*>(smart)->canvas; }
unity::GameObject* UiScaleLoadedObject(FleetMeshSelector* selector, const MethodInfo*)
{ return reinterpret_cast<UiScaleSelectorObject*>(selector)->object; }
unity::Transform* UiScaleObjectTransform(unity::GameObject* object, const MethodInfo*)
{ return reinterpret_cast<UiScaleGameObject*>(object)->transform; }
unity::Transform* UiScaleCanvasTransform(unity::Component* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->transform; }
Il2CppString* UiScaleCanvasName(CanvasController* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->name; }
bool UiScaleCanvasVisible(CanvasController* canvas, const MethodInfo*)
{ return reinterpret_cast<UiScaleCanvasObject*>(canvas)->visible; }
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
{
  auto* value  = reinterpret_cast<UiScaleRectObject*>(transform);
  auto  bounds = value->bounds;
  if (value->applied_factor) {
    bounds.width /= *value->applied_factor;
    bounds.height /= *value->applied_factor;
  } else if (value->dynamic_rect && value->parent) {
    const auto parent = UiScaleBounds(reinterpret_cast<unity::RectTransform*>(value->parent), nullptr);
    bounds.width      = parent.width * (value->anchor_max.x - value->anchor_min.x) + value->delta.x;
    bounds.height     = parent.height * (value->anchor_max.y - value->anchor_min.y) + value->delta.y;
  }
  return bounds;
}
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
  value->factor         = factor;
  value->applied_factor = factor;
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
  auto& canvas_object = AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&UiScaleCanvasGameObject),
                                  T(game_object_class), {});
  AddProperty(component_class, "gameObject", &canvas_object);
  AddMethod(canvas_class, "get_InTransition", reinterpret_cast<void*>(&UiScaleCanvasInTransition), T(bool_class), {});
  auto& visibility_class   = Define("Digit.Client.UI", "VisibilityState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  visibility_class.is_enum = true;
  AddField(canvas_class, "_state", visibility_class, offsetof(UiScaleCanvasObject, state));
  AddField(canvas_class, "m_visible", bool_class, offsetof(UiScaleCanvasObject, requested_visible));
  auto& anchored_get = AddMethod(rect_transform_class, "get_anchoredPosition",
                                 reinterpret_cast<void*>(&UiScaleAnchoredPosition), T(vector2_class), {});
  AddProperty(rect_transform_class, "anchoredPosition", &anchored_get);
  auto& smart_class = Define("Digit.Client.UI", "SmartScrollerBase", IL2CPP_TYPE_CLASS, &component_class);
  auto& grid_class  = Define("Digit.Client.UI", "SmartScrollerGrid", IL2CPP_TYPE_CLASS, &smart_class);
  AddField(smart_class, "_isInitialized", bool_class, offsetof(UiScaleScrollerObject, initialized));
  AddField(smart_class, "_isReadyForInitialization", bool_class, offsetof(UiScaleScrollerObject, ready));
  AddField(smart_class, "_regeneratingContent", bool_class, offsetof(UiScaleScrollerObject, regenerating));
  AddField(smart_class, "_spawnerCoroutine", object_class, offsetof(UiScaleScrollerObject, spawning));
  AddField(smart_class, "_dataHeldWhilstInitializing", object_class, offsetof(UiScaleScrollerObject, initializing));
  AddField(smart_class, "_viewport", rect_transform_class, offsetof(UiScaleScrollerObject, viewport));
  AddField(smart_class, "_content", rect_transform_class, offsetof(UiScaleScrollerObject, content));
  AddField(smart_class, "_screenWidth", float_class, offsetof(UiScaleScrollerObject, screen_width));
  AddField(smart_class, "_screenHeight", float_class, offsetof(UiScaleScrollerObject, screen_height));
  AddMethod(component_class, "GetComponentInParent", reinterpret_cast<void*>(&UiScaleScrollerCanvas), T(canvas_class),
            {T(type_class)});
  auto& normalized =
      AddMethod(smart_class, "get_NormalizedPosition", reinterpret_cast<void*>(&NativeTarget), T(vector2_class), {});
  auto& notify = AddMethod(smart_class, "OnValueChangedCallback", reinterpret_cast<void*>(&NativeTarget), T(void_class),
                           {T(vector2_class)});
  normalized.invoker_method = notify.invoker_method = &WarpInvokerSentinel;
  ui_scale_normalized_method                        = &normalized;
  ui_scale_notify_method                            = &notify;
  ui_scale_position_box.header.klass                = &vector2_class.klass;
  for (auto* method : game_object_class.methods)
    if (std::string_view(method->name) == "GetComponentsInChildren") {
      auto* mutable_method                 = const_cast<MethodInfo*>(method);
      mutable_method->methodPointer        = reinterpret_cast<Il2CppMethodPointer>(&UiScaleScrollerChildren);
      mutable_method->virtualMethodPointer = mutable_method->methodPointer;
    }

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

  const auto opening_graphics = Config::Get().graphics;
  const auto opening_dpi      = ui_scale_fixture_dpi;
  ui_scale_fixture_dpi        = 1.0f;
  for (const bool instant : {false, true}) {
    UiScaleRectObject opening_frame{{{&rect_transform_class.klass}}}, opening_rect{{{&rect_transform_class.klass}}};
    opening_frame.applied_factor = &scaler.applied_factor;
    opening_rect.dynamic_rect    = true;
    opening_rect.transform.scale = {1, 1, 1};
    opening_rect.parent          = reinterpret_cast<unity::Transform*>(&opening_frame);
    UiScaleCanvasObject opening{{&canvas_class.klass},
                                InstantWarpString("OfficerAssignment_Canvas"),
                                reinterpret_cast<unity::Transform*>(&opening_rect),
                                false};
    auto*               canvas           = reinterpret_cast<CanvasController*>(&opening);
    Config::Get().graphics.ui_scale      = 1.0f;
    Config::Get().graphics.ui_scale_menu = 1.08f;
    ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
    int        opening_calls = 0, measured_columns = 0;
    float      measured_width   = 0;
    const auto opening_original = [&](auto* receiver, int32_t entry, bool requested_instant) {
      ++opening_calls;
      Require(receiver == canvas && entry == 9 && requested_instant == instant,
              "first-open and reopen preserve native Show arguments");
      Require(!opening.visible, "native Show can return before the visibility transition is evaluated");
      // AboutToShow can generate the native grid from its local viewport before Show returns.
      measured_width   = UiScaleBounds(reinterpret_cast<unity::RectTransform*>(&opening_rect), nullptr).width;
      measured_columns = std::max(1, int(std::floor(measured_width / 160.0f)));
    };
    const auto check_displayed_grid = [&] {
      const auto rect            = UiScaleBounds(reinterpret_cast<unity::RectTransform*>(&opening_rect), nullptr);
      const auto pixels_per_unit = opening_rect.transform.scale.x * scaler.applied_factor;
      Require(std::abs(rect.width * pixels_per_unit - 1600.0f) < .001f,
              "the independently sized viewport still reaches both physical screen edges");
      const auto offset = (measured_width - measured_columns * 160.0f) / 2;
      Require((offset + measured_columns * 160.0f) * pixels_per_unit <= 1600.001f,
              "the row measured during Show fits its rightmost whole card inside the displayed viewport");
      const auto displayed_center = (offset + (measured_columns - .5f) * 160.0f) * pixels_per_unit;
      const auto selected_column  = int(std::floor((displayed_center / pixels_per_unit - offset) / 160.0f));
      Require(selected_column == measured_columns - 1, "displayed card centers retain their logical hit coordinates");
    };
    CanvasController_Show_Hook(opening_original, canvas, 9, instant);
    Require(!opening.visible && measured_columns == 9,
            "first opening measures the independent menu viewport before native late visibility");
    check_displayed_grid();
    const auto first_frame_writes = opening_rect.transform.writes + opening_rect.delta_writes;
    ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
    opening.visible = true;
    check_displayed_grid();
    ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
    Require(opening_rect.transform.writes + opening_rect.delta_writes == first_frame_writes,
            "the next visible refresh does not produce a delayed resize");

    opening.visible                 = false;
    Config::Get().graphics.ui_scale = .5f;
    ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
    Require(opening_rect.transform.writes + opening_rect.delta_writes == first_frame_writes,
            "ordinary root refresh still leaves hidden canvases alone");
    // ScreenManager resets the root and safe area before calling CanvasController.Show on reopen.
    opening_rect.transform.scale = {1, 1, 1};
    opening_rect.delta           = {};
    CanvasController_Show_Hook(opening_original, canvas, 9, instant);
    Require(opening_calls == 2 && measured_columns == 9,
            "reopening keeps the same menu columns after HUD size changes the parent's logical viewport");
    check_displayed_grid();
    const auto native_layout_original = [&](auto* receiver, int32_t entry, bool requested_instant) {
      opening_original(receiver, entry, requested_instant);
      opening_rect.transform.scale = {1, 1, 1};
      opening_rect.delta           = {};
    };
    CanvasController_Show_Hook(native_layout_original, canvas, 9, instant);
    check_displayed_grid();
    ResetUiScaleState();
    Require(warp_roots.size() == roots, "first-open and reopen observation releases its weak handles on reset");
  }

  // Reproduces the observed purchase return: native setup rebuilds pooled cards at row zero, then restores the
  // existing bottom scroll position without emitting a change event. Exercise the real feature callbacks.
  {
    const auto invoke_before    = tow_runtime_invoke_override;
    const auto box_before       = std::exchange(extra_value_box, &ui_scale_position_box.header);
    const auto payload_before   = std::exchange(extra_value_payload, &ui_scale_position_box.value);
    tow_runtime_invoke_override = &UiScaleScrollerInvoke;
    UiScaleRectObject list_frame{{{&rect_transform_class.klass}}}, list_rect{{{&rect_transform_class.klass}}},
        viewport{{{&rect_transform_class.klass}}}, content{{{&rect_transform_class.klass}}};
    list_frame.applied_factor = &scaler.applied_factor;
    list_rect.dynamic_rect    = true;
    list_rect.transform.scale = {1, 1, 1};
    list_rect.parent          = reinterpret_cast<unity::Transform*>(&list_frame);
    viewport.bounds           = {0, 0, 1425, 1109};
    content.bounds            = {0, -4545, 1425, 4545};
    UiScaleScrollerObject smart{{&grid_class.klass}};
    smart.viewport = reinterpret_cast<unity::RectTransform*>(&viewport);
    smart.content  = reinterpret_cast<unity::RectTransform*>(&content);
    UiScaleScrollerArray children{{&component_array_class.klass}, nullptr, 1, {&smart.header}};
    UiScaleGameObject    list_object{{&game_object_class.klass},
                                     reinterpret_cast<unity::Transform*>(&list_rect),
                                     reinterpret_cast<il2cpp::ArrayObject*>(&children)};
    UiScaleCanvasObject  list_canvas{{&canvas_class.klass},
                                     InstantWarpString("ReusableList_Canvas"),
                                     reinterpret_cast<unity::Transform*>(&list_rect),
                                     false,
                                     reinterpret_cast<unity::GameObject*>(&list_object),
                                     true};
    list_canvas.state           = VisibilityState::Hidden;
    auto* list_owner            = reinterpret_cast<CanvasController*>(&list_canvas);
    smart.canvas                = list_owner;
    const auto native_list_show = [&](auto*, int32_t, bool) {
      list_canvas.requested_visible = true;
      smart.current_index = smart.first_row = 0;
      content.position.y                    = 0;
      content.position.y                    = 3435.58545f;
    };
    const auto root_pass = [&] {
      ScreenManager_UpdateCanvasRootScaleFactor_Hook(canvas_original, reinterpret_cast<ScreenManager*>(&screen));
    };
    Config::Get().graphics.ui_scale      = 1.2f;
    Config::Get().graphics.ui_scale_menu = 1.08f;
    root_pass();
    ui_scale_scroller_scans = ui_scale_position_reads = 0;
    CanvasController_Show_Hook(native_list_show, list_owner, 0, false);
    const auto shown_scale  = list_rect.transform.scale;
    const auto shown_delta  = list_rect.delta;
    const auto shown_writes = list_rect.transform.writes + list_rect.delta_writes;
    Require(smart.current_index == 0 && content.position.y > 3000 && smart.notifications == 0,
            "native preserved-position return can leave the bottom viewport with the top pooled rows");
    root_pass();
    root_pass();
    Require(ui_scale_scroller_scans == 0 && smart.notifications == 0,
            "pooled hidden state retains a requested show while recycling waits for appearance");
    list_canvas.visible = true;
    list_canvas.state   = VisibilityState::Show;
    root_pass();
    Require(ui_scale_scroller_scans == 0 && smart.notifications == 0,
            "requested show does not discover recycled rows during the native appearance transition");
    list_canvas.state         = VisibilityState::Visible;
    list_canvas.in_transition = false;
    root_pass();
    Require(smart.notifications == 0, "recycling waits for a second settled root observation");
    root_pass();
    Require(smart.notifications == 1 && smart.current_index == 7 && smart.first_row == 5
                && UiScaleNear(content.position, {0, 3435.58545f}),
            "settled native position notification recycles bottom rows without moving the player's viewport");
    Require(UiScaleNear(list_rect.transform.scale, shown_scale) && UiScaleNear(list_rect.delta, shown_delta)
                && list_rect.transform.writes + list_rect.delta_writes == shown_writes,
            "purchase recycling correction preserves already-correct independent menu geometry");
    const auto scans_done = ui_scale_scroller_scans, reads_done = ui_scale_position_reads;
    for (int pass = 0; pass < 8; ++pass)
      root_pass();
    Require(ui_scale_scroller_scans == scans_done && ui_scale_position_reads == reads_done && smart.notifications == 1,
            "completed recycling correction stops descendant scans, readiness polling and repeated notifications");

    Il2CppObject spawning{&object_class.klass};
    smart.spawning      = &spawning;
    smart.current_index = smart.first_row = 0;
    QueueCanvasUiScaleScrollerRefresh(list_owner);
    QueueCanvasUiScaleScrollerRefresh(list_owner);
    for (int pass = 0; pass < 3; ++pass)
      root_pass();
    Require(smart.notifications == 1, "duplicate refresh requests cannot notify during native lazy spawning");
    smart.spawning        = nullptr;
    viewport.bounds.width = 1415;
    root_pass();
    root_pass();
    Require(smart.notifications == 1, "stale native dimensions defer recycling until the native rebuild settles");
    smart.screen_width = 1415;
    root_pass();
    root_pass();
    Require(smart.notifications == 2 && smart.first_row == 5,
            "coalesced refreshes notify once after spawning and viewport metrics settle");

    QueueCanvasUiScaleScrollerRefresh(list_owner);
    list_canvas.requested_visible = false;
    list_canvas.visible           = false;
    list_canvas.state             = VisibilityState::Hidden;
    root_pass();
    list_canvas.requested_visible = true;
    list_canvas.visible           = true;
    list_canvas.state             = VisibilityState::Visible;
    root_pass();
    root_pass();
    Require(smart.notifications == 2, "hiding a canvas cancels its pending correction instead of reviving it later");
    QueueCanvasUiScaleScrollerRefresh(list_owner);
    visual_dead_object = &list_canvas.header;
    root_pass();
    visual_dead_object = nullptr;
    root_pass();
    root_pass();
    Require(smart.notifications == 2, "destroyed canvas ownership cancels queued recycling");
    ResetUiScaleState();
    tow_runtime_invoke_override = invoke_before;
    extra_value_box             = box_before;
    extra_value_payload         = payload_before;
    Require(warp_roots.size() == roots, "completed and cancelled recycling work releases its weak owners");
  }
  Config::Get().graphics = opening_graphics;
  ui_scale_fixture_dpi   = opening_dpi;

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
