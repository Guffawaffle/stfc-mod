#include "patches/background_layer_science.h"
#include "config.h"
#include "errormsg.h"
#include "patches/key.h"
#include "patches/screen_update_hook.h"
#include <algorithm>
#include <cstring>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Color.h>
#include <prime/Hub.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>
#include <vector>

// Temporary science: compare native scenery renderers from the game baseline.
// Only renderer visibility changes; no scene transforms or native objects are destroyed.
namespace background_layer_science
{
bool              ready = false, prior_enabled = false;
Il2CppGCHandle    hidden        = nullptr;
Il2CppClass      *flat_class    = nullptr;
FieldInfo        *mesh_renderer = nullptr;
const MethodInfo *get_enabled = nullptr, *set_enabled = nullptr, *find_views = nullptr;
const MethodInfo *get_flat = nullptr, *active = nullptr, *object_name = nullptr;
const MethodInfo *get_material = nullptr, *get_texture = nullptr;
void             *view_type = nullptr;
int (*frame_count)()        = nullptr;
int next_scan               = 0;
// -1 shows all native scenery; >=0 hides one pool renderer.
int selection = -1;
struct Layer {
  Il2CppGCHandle renderer;
  std::string    label;
};
std::vector<Layer> layers;
Il2CppGCHandle     native_fr = nullptr;
std::string        native_identity;
FieldInfo         *pool_field      = nullptr;
void              *loader_type     = nullptr;
const MethodInfo  *get_game_object = nullptr, *active_object = nullptr, *alive = nullptr;

Il2CppGCHandle    observed_camera = nullptr;
int               clear_mode = 0, native_flags = 0;
color             native_color{};
float             native_far_clip = 0;
const MethodInfo *camera_flags = nullptr, *camera_color = nullptr, *camera_far = nullptr;
const MethodInfo *set_camera_flags = nullptr, *set_camera_color = nullptr;
void              Report();
bool              InSystem();

bool Live(Il2CppObject *object)
{
  Il2CppObject *boxed = nullptr;
  bool          value = false;
  void         *args[]{object};
  return object && Il2CppRuntime::TryInvoke(alive, nullptr, args, &boxed) && Il2CppRuntime::TryBoolean(boxed, value)
         && value;
}

void RestoreCamera()
{
  auto *camera = observed_camera ? il2cpp_gchandle_get_target(observed_camera) : nullptr;
  if (clear_mode && Live(camera)) {
    void *flags[]{&native_flags}, *background[]{&native_color};
    Il2CppRuntime::TryInvoke(set_camera_flags, camera, flags);
    Il2CppRuntime::TryInvoke(set_camera_color, camera, background);
  }
  if (observed_camera)
    il2cpp_gchandle_free(observed_camera);
  observed_camera = nullptr;
  clear_mode      = 0;
}

void ObserveCamera(Il2CppObject *camera)
{
  if (!ready || !InSystem() || !Live(camera) || !camera_flags || !camera_color || !camera_far || !set_camera_flags
      || !set_camera_color)
    return;
  if (!observed_camera || il2cpp_gchandle_get_target(observed_camera) != camera) {
    RestoreCamera();
    Il2CppObject *flags = nullptr, *background = nullptr, *far = nullptr;
    if (!Il2CppRuntime::TryInvoke(camera_flags, camera, nullptr, &flags) || !flags
        || !method_contract::Type(il2cpp_class_get_type(flags->klass), "UnityEngine.CameraClearFlags")
        || !Il2CppRuntime::TryInvoke(camera_color, camera, nullptr, &background) || !background
        || !method_contract::Type(il2cpp_class_get_type(background->klass), "UnityEngine.Color")
        || !Il2CppRuntime::TryInvoke(camera_far, camera, nullptr, &far) || !far
        || !method_contract::Type(il2cpp_class_get_type(far->klass), "System.Single"))
      return;
    auto *flag_value  = il2cpp_object_unbox(flags);
    auto *color_value = il2cpp_object_unbox(background);
    auto *far_value   = il2cpp_object_unbox(far);
    if (!flag_value || !color_value || !far_value)
      return;
    std::memcpy(&native_flags, flag_value, sizeof(native_flags));
    std::memcpy(&native_color, color_value, sizeof(native_color));
    std::memcpy(&native_far_clip, far_value, sizeof(native_far_clip));
    observed_camera = il2cpp_gchandle_new(camera, false);
    spdlog::info("[BackgroundLayers] native-camera clearFlags={} background=({},{},{},{}) farClip={}", native_flags,
                 native_color.r, native_color.g, native_color.b, native_color.a, native_far_clip);
  }
  if (clear_mode) {
    int   flags      = 2; // Unity CameraClearFlags.SolidColor.
    color background = clear_mode == 1 ? native_color : color{0, 0, 0, 0};
    void *flag_args[]{&flags}, *color_args[]{&background};
    Il2CppRuntime::TryInvoke(set_camera_flags, camera, flag_args);
    Il2CppRuntime::TryInvoke(set_camera_color, camera, color_args);
  }
}

void NextClearMode()
{
  if (!ready || !InSystem() || !observed_camera)
    return;
  clear_mode   = (clear_mode + 1) % 3;
  auto *camera = il2cpp_gchandle_get_target(observed_camera);
  if (!clear_mode && Live(camera)) {
    void *flags[]{&native_flags}, *background[]{&native_color};
    Il2CppRuntime::TryInvoke(set_camera_flags, camera, flags);
    Il2CppRuntime::TryInvoke(set_camera_color, camera, background);
  } else {
    ObserveCamera(camera);
  }
  spdlog::info("[BackgroundLayers] camera-clear comparison={} nativeFlags={} farClip-unchanged={}", clear_mode,
               native_flags, native_far_clip);
  Report();
}

void ReleaseLayers()
{
  for (auto &layer : layers)
    il2cpp_gchandle_free(layer.renderer);
  layers.clear();
  if (native_fr)
    il2cpp_gchandle_free(native_fr);
  native_fr = nullptr;
  native_identity.clear();
}

bool IsHiddenRenderer(Il2CppObject *renderer)
{ return renderer && hidden && il2cpp_gchandle_get_target(hidden) == renderer; }

bool InSystem()
{
  auto *sections = Hub::get_SectionManager();
  return sections && sections->CurrentSection == SectionID::Navigation_System;
}

std::string Name(Il2CppObject *object)
{
  Il2CppObject *result = nullptr;
  if (!object || !Il2CppRuntime::TryInvoke(object_name, object, nullptr, &result) || !result
      || !method_contract::Type(il2cpp_class_get_type(result->klass), "System.String"))
    return "unknown";
  auto *name = reinterpret_cast<Il2CppString *>(result);
  return name->length >= 0 && name->length <= 256 ? to_string(name) : "unknown";
}

void Restore()
{
  if (!hidden)
    return;
  auto *renderer = il2cpp_gchandle_get_target(hidden);
  void *args[]{&prior_enabled};
  if (Live(renderer))
    Il2CppRuntime::TryInvoke(set_enabled, renderer, args);
  il2cpp_gchandle_free(hidden);
  hidden = nullptr;
}

std::string Describe(Il2CppObject *renderer)
{
  Il2CppObject *material = nullptr, *texture = nullptr;
  Il2CppRuntime::TryInvoke(get_material, renderer, nullptr, &material);
  if (material)
    Il2CppRuntime::TryInvoke(get_texture, material, nullptr, &texture);
  return "object=" + Name(renderer) + "\nmaterial=" + Name(material) + "\ntexture=" + Name(texture);
}

void Apply()
{
  Il2CppObject *target = nullptr;
  if (selection >= 0 && size_t(selection) < layers.size())
    target = il2cpp_gchandle_get_target(layers[selection].renderer);
  if (!Live(target)) {
    Restore();
    return;
  }
  if (!IsHiddenRenderer(target)) {
    Restore();
    Il2CppObject *boxed = nullptr;
    if (!Il2CppRuntime::TryInvoke(get_enabled, target, nullptr, &boxed)
        || !Il2CppRuntime::TryBoolean(boxed, prior_enabled))
      return;
    hidden = il2cpp_gchandle_new(target, false);
    if (!hidden)
      return;
    spdlog::info("[BackgroundLayers] hide selection={} {}", selection, Describe(target));
  }
  bool  off = false;
  void *args[]{&off};
  Il2CppRuntime::TryInvoke(set_enabled, target, args);
}

void Report()
{
  std::string status = selection == -1
                           ? "Native scenery: all layers visible"
                           : "Hide layer " + std::to_string(selection + 1) + "/" + std::to_string(layers.size());
  if (selection >= 0 && size_t(selection) < layers.size())
    status += "\n" + layers[selection].label;
  status += "\nClear: "
            + std::string(clear_mode == 0   ? "Native"
                          : clear_mode == 1 ? "Solid native colour"
                                            : "Solid black");
  if (observed_camera)
    status += " (native flag " + std::to_string(native_flags) + ")";
  UpdatePanel(status, true);
}

void Next()
{
  if (!ready || !InSystem())
    return;
  Restore();
  ++selection;
  if (selection >= int(layers.size()))
    selection = -1;
  Apply();
  Report();
  spdlog::info("[BackgroundLayers] cycle selection={} count={}", selection, layers.size());
}

void Reset()
{
  if (!ready || !InSystem())
    return;
  Restore();
  selection = -1;
  Report();
  spdlog::info("[BackgroundLayers] restore-all=true");
}

bool Hide(Il2CppObject *flat)
{
  if (!ready || !InSystem() || !Live(flat) || flat->klass != flat_class)
    return false;
  Il2CppObject *renderer = nullptr;
  il2cpp_field_get_value(flat, mesh_renderer, &renderer);
  if (!Live(renderer))
    return false;
  const auto identity = Describe(renderer);
  if (!native_fr || il2cpp_gchandle_get_target(native_fr) != renderer || identity != native_identity) {
    Restore();
    ReleaseLayers();
    RestoreCamera();
    selection       = -1;
    native_fr       = il2cpp_gchandle_new(renderer, false);
    native_identity = identity;
    next_scan       = 0;
    spdlog::info("[BackgroundLayers] fr_scale target {} scale={} bypassed", Describe(renderer), Config::Get().fr_scale);
  }
  Apply();
  return native_fr != nullptr;
}

void ScanLayers()
{
  bool          include_inactive = false;
  void         *args[]{loader_type, &include_inactive};
  Il2CppObject *result = nullptr;
  if (!Il2CppRuntime::TryInvoke(find_views, nullptr, args, &result) || !result || !result->klass
      || result->klass->rank != 1 || !result->klass->element_class)
    return;
  auto *array = reinterpret_cast<Il2CppArray *>(result);
  if (array->max_length > 64)
    return;
  auto root = il2cpp_gchandle_new(result, false);
  if (!root)
    return;
  std::vector<Layer> found;
  for (size_t i = 0; i < array->max_length; ++i) {
    auto         *loader  = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, i, sizeof(void *)));
    Il2CppObject *boxed   = nullptr;
    bool          enabled = false;
    if (!Live(loader) || !Il2CppRuntime::TryInvoke(active, loader, nullptr, &boxed)
        || !Il2CppRuntime::TryBoolean(boxed, enabled) || !enabled)
      continue;
    Il2CppArray *pool = nullptr;
    il2cpp_field_get_value(loader, pool_field, &pool);
    if (!pool || !pool->klass || pool->klass->rank != 1 || !pool->klass->element_class
        || !il2cpp_class_is_assignable_from(flat_class, pool->klass->element_class) || pool->max_length > 128)
      continue;
    auto pool_root = il2cpp_gchandle_new(reinterpret_cast<Il2CppObject *>(pool), false);
    if (!pool_root)
      continue;
    for (size_t j = 0; j < pool->max_length; ++j) {
      auto         *flat     = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(pool, j, sizeof(void *)));
      Il2CppObject *renderer = nullptr, *object = nullptr;
      if (!Live(flat) || !il2cpp_class_is_assignable_from(flat_class, flat->klass))
        continue;
      il2cpp_field_get_value(flat, mesh_renderer, &renderer);
      if (!Live(renderer) || !Il2CppRuntime::TryInvoke(get_game_object, renderer, nullptr, &object) || !Live(object)
          || !Il2CppRuntime::TryInvoke(active_object, object, nullptr, &boxed)
          || !Il2CppRuntime::TryBoolean(boxed, enabled) || !enabled
          || !Il2CppRuntime::TryInvoke(get_enabled, renderer, nullptr, &boxed)
          || !Il2CppRuntime::TryBoolean(boxed, enabled) || (!enabled && !IsHiddenRenderer(renderer)))
        continue;
      if (std::any_of(found.begin(), found.end(),
                      [renderer](const auto &layer) { return il2cpp_gchandle_get_target(layer.renderer) == renderer; }))
        continue;
      auto handle = il2cpp_gchandle_new(renderer, false);
      if (handle)
        found.push_back({handle, "pool=" + std::to_string(j) + " " + Describe(renderer)});
    }
    il2cpp_gchandle_free(pool_root);
  }
  il2cpp_gchandle_free(root);
  auto *fr = native_fr ? il2cpp_gchandle_get_target(native_fr) : nullptr;
  if (Live(fr) && std::none_of(found.begin(), found.end(), [fr](const auto &layer) {
        return il2cpp_gchandle_get_target(layer.renderer) == fr;
      })) {
    auto handle = il2cpp_gchandle_new(fr, false);
    if (handle)
      found.push_back({handle, "fr_scale target " + Describe(fr)});
  }
  // Retain selection by exact renderer identity while pools refresh or reorder.
  auto *selected = selection >= 0 && size_t(selection) < layers.size()
                       ? il2cpp_gchandle_get_target(layers[selection].renderer)
                       : nullptr;
  bool  changed  = found.size() != layers.size();
  for (size_t i = 0; !changed && i < found.size(); ++i)
    changed = found[i].label != layers[i].label
              || il2cpp_gchandle_get_target(found[i].renderer) != il2cpp_gchandle_get_target(layers[i].renderer);
  for (auto &layer : layers)
    il2cpp_gchandle_free(layer.renderer);
  layers = std::move(found);
  if (selection >= 0) {
    selection = -1;
    for (size_t i = 0; i < layers.size(); ++i)
      if (il2cpp_gchandle_get_target(layers[i].renderer) == selected)
        selection = int(i);
  }
  if (changed) {
    spdlog::info("[BackgroundLayers] inventory count={}", layers.size());
    for (size_t i = 0; i < layers.size(); ++i)
      spdlog::info("[BackgroundLayers] layer={} {}", i + 1, layers[i].label);
  }
  Apply();
  Report();
}

void Tick()
{
  if (!ready)
    return;
  if (!InSystem()) {
    Restore();
    ReleaseLayers();
    RestoreCamera();
    selection = -1;
    UpdatePanel("", false);
    next_scan = 0;
    return;
  }
  Apply();
  if (Key::HasAlt() && !Key::IsInputFocused()) {
    if (Key::Down(KeyCode::F8))
      Next();
    if (Key::Down(KeyCode::F9))
      Reset();
    if (Key::Down(KeyCode::F10))
      NextClearMode();
  }
  int frame = frame_count();
  if (frame < next_scan)
    return;
  next_scan = frame + 30;
  // Find the view before the first scroll. The existing getter supplies exactly
  // the same FlatRenderable as the fr_scale path, rather than guessing a layer.
  bool          include_inactive = false;
  void         *args[]{view_type, &include_inactive};
  Il2CppObject *result = nullptr;
  if (!Il2CppRuntime::TryInvoke(find_views, nullptr, args, &result) || !result || !result->klass
      || result->klass->rank != 1 || !result->klass->element_class)
    return;
  auto *array = reinterpret_cast<Il2CppArray *>(result);
  if (array->max_length > 8)
    return;
  auto root = il2cpp_gchandle_new(result, false);
  if (!root)
    return;
  for (size_t i = 0; i < array->max_length; ++i) {
    auto         *view  = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, i, sizeof(void *)));
    Il2CppObject *boxed = nullptr, *flat = nullptr;
    bool          enabled = false;
    if (view && Il2CppRuntime::TryInvoke(active, view, nullptr, &boxed) && Il2CppRuntime::TryBoolean(boxed, enabled)
        && enabled && Il2CppRuntime::TryInvoke(get_flat, view, nullptr, &flat) && Hide(flat))
      break;
  }
  il2cpp_gchandle_free(root);
  ScanLayers();
}

void Install()
{
  auto flat        = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable");
  auto view        = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils");
  auto object      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  auto renderer    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Renderer");
  auto material    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Material");
  auto behaviour   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Behaviour");
  auto loader      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderableLoader");
  auto game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  auto component   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  pool_field       = loader.get_cls() ? loader.GetField("_pool").get_info() : nullptr;
  if (!pool_field || (il2cpp_field_get_flags(pool_field) & FIELD_ATTRIBUTE_STATIC)
      || !method_contract::Type(pool_field->type, "Digit.Client.Rendering.FlatRenderable[]"))
    return;
  loader_type = loader.get_cls() ? loader.GetType() : nullptr;
  alive = method_contract::Resolve(object.get_cls(), "op_Implicit", true, "System.Boolean", {"UnityEngine.Object"});
  get_game_object =
      method_contract::Resolve(component.get_cls(), "get_gameObject", false, "UnityEngine.GameObject", {});
  active_object = method_contract::Resolve(game_object.get_cls(), "get_activeInHierarchy", false, "System.Boolean", {});
  auto camera   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Camera");
  camera_flags =
      method_contract::Resolve(camera.get_cls(), "get_clearFlags", false, "UnityEngine.CameraClearFlags", {});
  camera_color     = method_contract::Resolve(camera.get_cls(), "get_backgroundColor", false, "UnityEngine.Color", {});
  camera_far       = method_contract::Resolve(camera.get_cls(), "get_farClipPlane", false, "System.Single", {});
  set_camera_flags = method_contract::Resolve(camera.get_cls(), "set_clearFlags", false, "System.Void",
                                              {"UnityEngine.CameraClearFlags"});
  set_camera_color =
      method_contract::Resolve(camera.get_cls(), "set_backgroundColor", false, "System.Void", {"UnityEngine.Color"});
  flat_class    = flat.get_cls();
  mesh_renderer = flat_class ? flat.GetField("MeshRenderer").get_info() : nullptr;
  if (!mesh_renderer || (il2cpp_field_get_flags(mesh_renderer) & FIELD_ATTRIBUTE_STATIC)
      || !method_contract::Type(mesh_renderer->type, "UnityEngine.MeshRenderer")) {
    spdlog::warn("[BackgroundLayers] FlatRenderable.MeshRenderer unavailable; experiment skipped");
    return;
  }
  get_enabled  = method_contract::Resolve(renderer.get_cls(), "get_enabled", false, "System.Boolean", {});
  set_enabled  = method_contract::Resolve(renderer.get_cls(), "set_enabled", false, "System.Void", {"System.Boolean"});
  get_material = method_contract::Resolve(renderer.get_cls(), "get_sharedMaterial", false, "UnityEngine.Material", {});
  get_texture  = method_contract::Resolve(material.get_cls(), "get_mainTexture", false, "UnityEngine.Texture", {});
  object_name  = method_contract::Resolve(object.get_cls(), "get_name", false, "System.String", {});
  find_views   = method_contract::Resolve(object.get_cls(), "FindObjectsOfType", true, "UnityEngine.Object[]",
                                          {"System.Type", "System.Boolean"});
  get_flat     = method_contract::Resolve(view.get_cls(), "get_FlatRenderable", false,
                                          "Digit.Client.Rendering.FlatRenderable", {});
  active       = method_contract::Resolve(behaviour.get_cls(), "get_isActiveAndEnabled", false, "System.Boolean", {});
  view_type    = view.get_cls() ? il2cpp_type_get_object(il2cpp_class_get_type(view.get_cls())) : nullptr;
  frame_count  = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
  ready        = alive && loader_type && get_game_object && active_object && object_name && get_material && get_texture
                 && get_enabled && set_enabled && find_views && get_flat && active && view_type && frame_count
                 && install_screen_manager_update_hook() && register_screen_manager_update_callback(Tick);
  spdlog::info(
      "[BackgroundLayers] step=6 layer-cycle ready={} initial-mode=native extended-zoom=true border-fix=removed "
      "orbit-sky=removed clear-comparison=available keys=ALT-F8/ALT-F9/ALT-F10",
      ready);
}
} // namespace background_layer_science
