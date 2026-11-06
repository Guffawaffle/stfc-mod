// Ambient enclosure for the system free-orbit camera. Native backdrop artwork,
// visibility, transforms and materials remain exclusively game-owned.
#include "features/navigation_environment.h"
#include "features/navigation_environment_art.h"
#include "features/navigation_environment_pixels.h"

#include "game/navigation_environment.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
namespace system_environment = navigation_environment_binding;
namespace environment_orbit  = navigation_orbit_binding;
using EnvironmentVector3     = unity::Vector3;
using EnvironmentVector2     = unity::Vector2;

struct NavigationEnvironmentState {
  il2cpp::GcHandle shell, mesh, material, source, renderer, native_background, texture, camera;
  il2cpp::GcHandle loader, pool, parent, shell_transform, source_flat, background;
  il2cpp::GcHandle configuration, pending_source, pending_background, pending_flat;
  std::unique_ptr<navigation_environment_art::PixelGenerator>           pixels;
  std::shared_ptr<const std::vector<navigation_environment_art::Pixel>> upload_pixels;
  bool                                                                  uploading = false, clearing = false;
  uint64_t                                                              pixel_ticket  = 0;
  uint64_t                                                              epoch         = 0;
  int32_t                                                               pending_layer = 0;
  std::string                                                           pending_key;
  unity::Bounds                                                         bounds{};
  unity::Bounds                                                         sweep_bounds{};
  bool                                  sweep_started = false, sweep_valid = false, sweep_known = true;
  std::string                           artwork_key;
  size_t                                cursor          = 0;
  size_t                                discovery_start = 0;
  int32_t                               frame = -1, next_probe = 0, next_bounds = 0, mask = 0;
  int32_t                               search_layer  = -1;
  float                                 applied_scale = 0;
  std::chrono::steady_clock::time_point next_discovery{};
  bool                                  scene_known = false;
  bool                                  attempted = false, ready = false, failed = false, bounds_valid = false;
};
NavigationEnvironmentState& SystemEnvironment()
{
  // Explicit scene cleanup, never IL2CPP calls during process-static teardown.
  static auto* state = new NavigationEnvironmentState;
  return *state;
}
bool EnvironmentLive(Il2CppObject* object)
{
  const auto observed = unity::TryAlive(object);
  return observed && observed.value;
}
void EnvironmentDestroy(il2cpp::GcHandle& object)
{
  if (EnvironmentLive(object.Get()))
    unity::Destroy(object.Get());
  object = {};
}
void EnvironmentFail(const char* stage)
{
  navigation_environment::Clear();
  SystemEnvironment().failed = true;
  spdlog::warn("[SystemEnvironment] {} unavailable; native scenery and free camera retained", stage);
}
std::string EnvironmentName(il2cpp::Object* object)
{
  const auto observed = unity::TryName(object);
  return observed && observed.value ? il2cpp::ToUtf8(observed.value) : "SystemAmbient";
}
template <il2cpp::Name N, class B, class T, class Element>
bool EnvironmentArray(B* object, const std::vector<T>& values)
{
  const auto observed = unity::CopyValueArray<T, Element>(std::span<const T>(values));
  auto       root     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  return root && object->template TryCall<N>(root.template Get<il2cpp::Array<T>>()).succeeded;
}
struct EnvironmentResources {
  il2cpp::GcHandle shell, mesh, material, texture, renderer;
  ~EnvironmentResources()
  {
    EnvironmentDestroy(shell);
    EnvironmentDestroy(mesh);
    EnvironmentDestroy(material);
    EnvironmentDestroy(texture);
  }
};
bool EnvironmentTexture(EnvironmentResources& state, std::string_view name,
                        const std::vector<navigation_environment_art::Pixel>& pixels)
{
  constexpr int32_t width = 1024, height = 512;
  const auto        started = std::chrono::steady_clock::now();
  const auto        theme   = navigation_environment_art::SelectTheme(name);
  using Color               = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Color32">;
  const auto observed       = unity::CopyValueArray<navigation_environment_art::Pixel, Color>(pixels);
  auto       pixel_root     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  state.texture             = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::Texture2D>());
  auto* texture             = state.texture.Get<unity::Texture2D>();
  // Exact PLAY choices: RGBA32=4, Repeat=0, Clamp=1; no mipmaps or CPU copy.
  if (!texture || !pixel_root || pixels.empty() || !texture->TryCall<".ctor">(width, height, int32_t{4}, false)
      || !texture->TryCall<"SetPixels32">(pixel_root.Get<il2cpp::Array<navigation_environment_art::Pixel>>())
      || !texture->TryCall<"set_wrapModeU">(int32_t{0}) || !texture->TryCall<"set_wrapModeV">(int32_t{1})
      || !texture->TryCall<"Apply">(false, true))
    return false;
  const auto actual_width  = texture->TryCall<"get_width", int32_t>();
  const auto actual_height = texture->TryCall<"get_height", int32_t>();
  if (!actual_width || !actual_height || actual_width.value != width || actual_height.value != height)
    return false;
  spdlog::debug(
      "[SystemEnvironment] directional sky {}x{} palette={} uploaded={}ms; native artwork retained", width, height,
      static_cast<int>(theme),
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
  return true;
}
bool EnvironmentCreate(EnvironmentResources& state, std::string_view name, int32_t layer,
                       const std::vector<navigation_environment_art::Pixel>& pixels)
{
  constexpr int32_t               longitude = 64, latitude = 32;
  constexpr double                pi = 3.14159265358979323846;
  std::vector<EnvironmentVector3> vertices;
  std::vector<EnvironmentVector2> uv;
  std::vector<int32_t>            triangles;
  vertices.reserve((latitude + 1) * (longitude + 1));
  uv.reserve(vertices.capacity());
  triangles.reserve(longitude * (latitude - 1) * 6);
  for (int32_t y = 0; y <= latitude; ++y) {
    const double theta = pi * y / latitude;
    for (int32_t x = 0; x <= longitude; ++x) {
      const double phi = 2 * pi * x / longitude;
      vertices.push_back(
          {float(std::sin(theta) * std::cos(phi)), float(std::cos(theta)), float(std::sin(theta) * std::sin(phi))});
      uv.push_back({float(x) / longitude, float(y) / latitude});
    }
  }
  for (int32_t y = 0; y < latitude; ++y)
    for (int32_t x = 0; x < longitude; ++x) {
      const int32_t a = y * (longitude + 1) + x, b = a + longitude + 1;
      if (y != 0)
        triangles.insert(triangles.end(), {a, b, a + 1});
      if (y != latitude - 1)
        triangles.insert(triangles.end(), {a + 1, b, b + 1});
    }
  state.shell    = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::GameObject>());
  state.mesh     = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::Mesh>());
  state.material = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::Material>());
  auto* shell    = state.shell.Get<unity::GameObject>();
  auto* mesh     = state.mesh.Get<unity::Mesh>();
  auto* material = state.material.Get<unity::Material>();
  if (!shell || !mesh || !material || !shell->TryCall<".ctor">(il2cpp::NewString("CommunitySystemEnvironment"))
      || !mesh->TryCall<".ctor">())
    return false;
  const auto found       = unity::Shader::Class::TryCall<"Find", unity::Shader*>(il2cpp::NewString("Unlit/Texture"));
  auto       shader_root = il2cpp::GcHandle::Strong(found ? found.value : nullptr);
  const auto supported =
      shader_root ? shader_root.Get<unity::Shader>()->TryCall<"get_isSupported", bool>() : il2cpp::CallResult<bool>{};
  if (!supported || !supported.value || !EnvironmentLive(shader_root.Get()) || !EnvironmentTexture(state, name, pixels))
    return false;
  if (!material->TryCall<".ctor">(shader_root.Get<unity::Shader>())
      || !material->TryCall<"set_mainTexture">(state.texture.Get<unity::Texture>())
      || !shell->TryCall<"set_layer">(layer))
    return false;
  using V3 = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Vector3">;
  using V2 = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Vector2">;
  using I4 = il2cpp::Class<"mscorlib", "System", "Int32">;
  if (!EnvironmentArray<"set_vertices", unity::Mesh, EnvironmentVector3, V3>(mesh, vertices)
      || !EnvironmentArray<"set_uv", unity::Mesh, EnvironmentVector2, V2>(mesh, uv)
      || !EnvironmentArray<"set_triangles", unity::Mesh, int32_t, I4>(mesh, triangles))
    return false;
  const auto filter_observed =
      shell->TryCall<"AddComponent", unity::Component*>(unity::MeshFilter::Class::TypeObject());
  auto       filter_root = il2cpp::GcHandle::Strong(filter_observed ? filter_observed.value : nullptr);
  const auto renderer_observed =
      shell->TryCall<"AddComponent", unity::Component*>(unity::MeshRenderer::Class::TypeObject());
  auto  renderer_root = il2cpp::GcHandle::Strong(renderer_observed ? renderer_observed.value : nullptr);
  auto* filter        = filter_root ? filter_root.Get<il2cpp::Object>()->As<unity::MeshFilter>() : nullptr;
  auto* renderer      = renderer_root ? renderer_root.Get<il2cpp::Object>()->As<unity::MeshRenderer>() : nullptr;
  if (!filter || !renderer || !EnvironmentLive(renderer) || !EnvironmentLive(filter)
      || !material->TryCall<"set_renderQueue">(int32_t{1000}) || !filter->TryCall<"set_sharedMesh">(mesh)
      || !renderer->TryCall<"set_sharedMaterial">(material) || !mesh->TryCall<"RecalculateBounds">()
      || !renderer->TryCall<"set_shadowCastingMode">(int32_t{0}) || !renderer->TryCall<"set_receiveShadows">(false))
    return false;
  state.renderer = std::move(renderer_root);
  return static_cast<bool>(state.renderer);
}
struct EnvironmentArtwork {
  unity::Texture*      texture  = nullptr;
  unity::MeshRenderer* renderer = nullptr;
  int32_t              layer    = -1;
};
il2cpp::CallResult<bool> EnvironmentArtworkFor(system_environment::Flat* flat, int32_t mask, int32_t required_layer,
                                               EnvironmentArtwork& art)
{
  const auto alive = unity::TryAlive(flat);
  if (!alive)
    return {};
  if (!alive.value)
    return {true, false};
  const auto observed = system_environment::Renderer(flat);
  art.renderer        = observed ? observed.value : nullptr;
  if (!observed)
    return {};
  const auto renderer_alive = unity::TryAlive(art.renderer);
  if (!renderer_alive)
    return {};
  if (!renderer_alive.value)
    return {true, false};
  const auto enabled     = art.renderer->TryCall<"get_enabled", bool>();
  const auto object      = art.renderer->TryCall<"get_gameObject", unity::GameObject*>();
  auto       object_root = il2cpp::GcHandle::Strong(object ? object.value : nullptr);
  const auto layer       = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>()
                                       : il2cpp::CallResult<int32_t>{};
  art.layer              = layer ? layer.value : -1;
  const auto active      = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_activeInHierarchy", bool>()
                                       : il2cpp::CallResult<bool>{};
  if (!enabled || !object || !layer || !active)
    return {};
  if (!enabled.value || !active.value || art.layer < 0 || art.layer > 31
      || !(static_cast<uint32_t>(mask) & (1u << art.layer)) || (required_layer >= 0 && art.layer != required_layer))
    return {true, false};
  const auto material      = art.renderer->TryCall<"get_sharedMaterial", unity::Material*>();
  auto       material_root = il2cpp::GcHandle::Strong(material ? material.value : nullptr);
  if (!material)
    return {};
  if (!material.value)
    return {true, false};
  if (!material_root)
    return {};
  const auto texture = material_root
                           ? material_root.Get<unity::Material>()->TryCall<"get_mainTexture", unity::Texture*>()
                           : il2cpp::CallResult<unity::Texture*>{};
  art.texture        = texture ? texture.value : nullptr;
  auto texture_root  = il2cpp::GcHandle::Strong(art.texture);
  if (!texture)
    return {};
  if (!texture.value)
    return {true, false};
  const auto texture_alive = unity::TryAlive(art.texture);
  if (!texture_alive)
    return {};
  if (!texture_alive.value)
    return {true, false};
  const auto width  = art.texture->TryCall<"get_width", int32_t>();
  const auto height = art.texture->TryCall<"get_height", int32_t>();
  return width && height ? il2cpp::CallResult<bool>{true, width.value >= 64 && height.value >= 64}
                         : il2cpp::CallResult<bool>{};
}
il2cpp::CallResult<std::optional<unity::Bounds>> EnvironmentObserveScenery(unity::MeshRenderer* renderer, int32_t mask)
{
  if (!renderer)
    return {true, {}};
  auto root = il2cpp::GcHandle::Strong(renderer);
  if (!root)
    return {};
  const auto alive = unity::TryAlive(renderer);
  if (!alive)
    return {};
  if (!alive.value)
    return {true, {}};
  const auto enabled = renderer->TryCall<"get_enabled", bool>();
  if (!enabled)
    return {};
  if (!enabled.value)
    return {true, {}};
  const auto object = renderer->TryCall<"get_gameObject", unity::GameObject*>();
  if (!object)
    return {};
  if (!object.value)
    return {true, {}};
  auto       object_root = il2cpp::GcHandle::Strong(object ? object.value : nullptr);
  const auto active      = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_activeInHierarchy", bool>()
                                       : il2cpp::CallResult<bool>{};
  const auto layer       = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>()
                                       : il2cpp::CallResult<int32_t>{};
  if (!active || !layer)
    return {};
  if (!active.value || layer.value < 0 || layer.value > 31 || !(static_cast<uint32_t>(mask) & (1u << layer.value)))
    return {true, {}};
  const auto bounds = unity::TryBounds(renderer);
  if (!bounds || !environment_orbit::Finite(bounds.value.center) || !environment_orbit::Finite(bounds.value.extents)
      || bounds.value.extents.x < 0 || bounds.value.extents.y < 0 || bounds.value.extents.z < 0)
    return {};
  return {true, bounds.value};
}
void EnvironmentMerge(unity::Bounds& bounds, bool& valid, const unity::Bounds& next)
{
  if (!valid) {
    bounds = next;
    valid  = true;
    return;
  }
  const auto merge = [](float& center, float& extent, float next_center, float next_extent) {
    const double low  = std::min(double(center) - extent, double(next_center) - next_extent);
    const double high = std::max(double(center) + extent, double(next_center) + next_extent);
    center            = float((low + high) * 0.5);
    extent            = float((high - low) * 0.5);
  };
  merge(bounds.center.x, bounds.extents.x, next.center.x, next.extents.x);
  merge(bounds.center.y, bounds.extents.y, next.center.y, next.extents.y);
  merge(bounds.center.z, bounds.extents.z, next.center.z, next.extents.z);
}
void EnvironmentIncludeScenery(unity::MeshRenderer* renderer, int32_t mask)
{
  auto&      state    = SystemEnvironment();
  const auto epoch    = state.epoch;
  const auto observed = EnvironmentObserveScenery(renderer, mask);
  if (state.epoch == epoch && observed && observed.value)
    EnvironmentMerge(state.bounds, state.bounds_valid, *observed.value);
}
void EnvironmentAdopt(const EnvironmentArtwork& art, system_environment::Flat* flat);
void EnvironmentBoundsSlice(system_environment::Loader* loader, int32_t mask)
{
  auto&      state         = SystemEnvironment();
  const auto initial_epoch = state.epoch;
  const auto observed      = system_environment::Pool(loader);
  if (!observed || state.epoch != initial_epoch)
    return;
  if (observed.value != state.pool.Get()) {
    auto root = il2cpp::GcHandle::Strong(observed.value);
    if (observed.value && !root)
      return;
    state.pool          = std::move(root);
    state.cursor        = 0;
    state.bounds        = {};
    state.bounds_valid  = false;
    state.sweep_started = false;
  }
  auto  pool_root = il2cpp::GcHandle::Strong(state.pool.Get());
  auto* pool      = pool_root.Get<il2cpp::Array<system_environment::Flat*>>();
  // Count examined slots, including null/dead/unavailable entries. Keep rolling:
  // pooled arrays can replace or activate scenery without changing identity or length.
  constexpr size_t budget = 4;
  const auto       length = pool ? pool->size() : 0;
  const auto       epoch  = state.epoch;
  const auto       record = [&](const auto& observed) {
    if (!observed)
      state.sweep_known = false;
    else if (observed.value) {
      EnvironmentMerge(state.sweep_bounds, state.sweep_valid, *observed.value);
      EnvironmentMerge(state.bounds, state.bounds_valid, *observed.value);
    }
  };
  const auto begin = [&] {
    state.sweep_bounds    = {};
    state.sweep_valid     = false;
    state.sweep_known     = true;
    state.sweep_started   = true;
    const auto background = EnvironmentObserveScenery(state.native_background.Get<unity::MeshRenderer>(), mask);
    if (state.epoch == epoch)
      record(background);
  };
  const auto finish = [&] {
    if (state.sweep_known) {
      state.bounds       = state.sweep_bounds;
      state.bounds_valid = state.sweep_valid;
    }
    state.sweep_started = false;
    if (state.sweep_known && state.search_layer >= 0 && !state.source && !state.pixel_ticket) {
      // A completely observed unusable loader must not hide another active loader.
      state.loader        = {};
      state.pool          = {};
      state.configuration = {};
      state.background    = {};
      state.search_layer  = -1;
      state.next_probe    = 0;
    }
  };
  if (!state.sweep_started)
    begin();
  if (state.epoch != epoch)
    return;
  if (!length) {
    finish();
    return;
  }
  for (size_t visited = 0; visited < std::min(budget, length); ++visited) {
    if (!state.sweep_started)
      begin();
    if (state.epoch != epoch)
      return;
    state.cursor %= length;
    auto       flat_root = il2cpp::GcHandle::Strong((*pool)[state.cursor++]);
    auto*      flat      = flat_root.Get<system_environment::Flat>();
    const auto alive     = unity::TryAlive(flat);
    if (!alive || ((*pool)[(state.cursor - 1) % length] && !flat_root))
      state.sweep_known = false;
    if (alive && alive.value && state.search_layer >= 0) {
      EnvironmentArtwork art;
      const auto         candidate = EnvironmentArtworkFor(flat, mask, state.search_layer, art);
      if (!candidate)
        state.sweep_known = false;
      if (candidate && candidate.value)
        EnvironmentAdopt(art, flat);
    }
    if (state.epoch != epoch)
      return;
    if (alive && alive.value) {
      const auto renderer        = system_environment::Renderer(flat);
      const auto observed_bounds = renderer ? EnvironmentObserveScenery(renderer.value, mask)
                                            : il2cpp::CallResult<std::optional<unity::Bounds>>{};
      if (state.epoch != epoch)
        return;
      record(observed_bounds);
    }
    if (state.cursor == length) {
      finish();
      // End this cohort even when the last slice used less than the budget.
      // The caller paces the next complete refresh instead of oversampling small pools.
      return;
    }
  }
}
void EnvironmentDropArtwork()
{
  auto& state = SystemEnvironment();
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
  EnvironmentResources retired;
  ++state.epoch;
  state.native_background  = {};
  retired.shell            = std::move(state.shell);
  retired.mesh             = std::move(state.mesh);
  retired.material         = std::move(state.material);
  retired.texture          = std::move(state.texture);
  retired.renderer         = std::move(state.renderer);
  state.source             = {};
  state.renderer           = {};
  state.parent             = {};
  state.shell_transform    = {};
  state.source_flat        = {};
  state.pending_source     = {};
  state.pending_background = {};
  state.pending_flat       = {};
  state.pixel_ticket       = 0;
  state.upload_pixels.reset();
  state.pending_key.clear();
  if (state.pixels)
    state.pixels->Cancel();
  state.bounds        = {};
  state.bounds_valid  = false;
  state.sweep_started = false;
  state.cursor        = 0;
  state.search_layer  = -1;
  state.next_bounds   = 0;
  state.applied_scale = 0;
  state.artwork_key.clear();
}
void EnvironmentAdopt(const EnvironmentArtwork& art, system_environment::Flat* flat)
{
  const auto epoch     = SystemEnvironment().epoch;
  auto&      state     = SystemEnvironment();
  auto       texture   = il2cpp::GcHandle::Strong(art.texture);
  auto       renderer  = il2cpp::GcHandle::Strong(art.renderer);
  auto       flat_root = il2cpp::GcHandle::Strong(flat);
  if (!texture || !renderer || !flat_root)
    return;
  const auto key         = EnvironmentName(texture.Get<il2cpp::Object>());
  const auto shell_alive = unity::TryAlive(state.shell.Get());
  if (!shell_alive || state.epoch != epoch)
    return;
  state.search_layer = -1;
  if (shell_alive.value && key == state.artwork_key) {
    if (!state.shell.Get<unity::GameObject>()->TryCall<"set_layer">(art.layer) || state.epoch != epoch)
      return;
    ++state.epoch;
    state.source            = std::move(texture);
    state.native_background = std::move(renderer);
    state.source_flat       = std::move(flat_root);
    state.bounds            = {};
    state.bounds_valid      = false;
    state.sweep_started     = false;
    state.cursor            = 0;
    EnvironmentIncludeScenery(state.native_background.Get<unity::MeshRenderer>(), state.mask);
    return;
  }
  if (state.pixel_ticket && key == state.pending_key) {
    ++state.epoch;
    state.bounds             = {};
    state.bounds_valid       = false;
    state.sweep_started      = false;
    state.cursor             = 0;
    state.pending_source     = std::move(texture);
    state.pending_background = std::move(renderer);
    state.pending_flat       = std::move(flat_root);
    state.pending_layer      = art.layer;
    return;
  }
  EnvironmentDropArtwork();
  state.pending_source     = std::move(texture);
  state.pending_background = std::move(renderer);
  state.pending_flat       = std::move(flat_root);
  state.pending_layer      = art.layer;
  state.pending_key        = key;
  if (!state.pixels)
    state.pixels = std::make_unique<navigation_environment_art::PixelGenerator>();
  state.pixel_ticket =
      state.pixels->Request({navigation_environment_art::SelectTheme(key), navigation_environment_art::Seed(key)});
}
void EnvironmentFinishArtwork()
{
  auto& state = SystemEnvironment();
  if (!state.pixel_ticket || state.uploading)
    return;
  const auto texture_alive  = unity::TryAlive(state.pending_source.Get());
  const auto renderer_alive = unity::TryAlive(state.pending_background.Get());
  if (!texture_alive || !renderer_alive)
    return;
  if (!texture_alive.value || !renderer_alive.value) {
    EnvironmentDropArtwork();
    state.next_probe = 0;
    return;
  }
  if (!state.upload_pixels) {
    auto pixels = state.pixels->Take(state.pixel_ticket);
    if (!pixels)
      return;
    state.upload_pixels = std::make_shared<const std::vector<navigation_environment_art::Pixel>>(std::move(*pixels));
  }
  const auto pixels = state.upload_pixels;
  const auto ticket = state.pixel_ticket;
  const auto key    = state.pending_key;
  const auto layer  = state.pending_layer;
  struct UploadScope {
    bool& active;
    explicit UploadScope(bool& flag)
        : active(flag)
    { active = true; }
    ~UploadScope()
    { active = false; }
  } uploading(state.uploading);
  EnvironmentResources resources;
  if (pixels->size() != size_t(1024 * 512) || !EnvironmentCreate(resources, key, layer, *pixels)) {
    if (ticket == state.pixel_ticket)
      EnvironmentFail("ambient enclosure");
    return;
  }
  if (ticket != state.pixel_ticket || layer != state.pending_layer)
    return;
  // Resource constructors/upload can call managed code. Commit only to the still-current
  // request, after reobserving its latest native owners; partial resources remain local.
  const auto current_texture  = unity::TryAlive(state.pending_source.Get());
  const auto current_renderer = unity::TryAlive(state.pending_background.Get());
  if (ticket != state.pixel_ticket || !current_texture || !current_renderer || !current_texture.value
      || !current_renderer.value)
    return;
  state.shell             = std::move(resources.shell);
  state.mesh              = std::move(resources.mesh);
  state.material          = std::move(resources.material);
  state.texture           = std::move(resources.texture);
  state.renderer          = std::move(resources.renderer);
  state.source            = std::move(state.pending_source);
  state.native_background = std::move(state.pending_background);
  state.pixel_ticket      = 0;
  state.upload_pixels.reset();
  state.source_flat = std::move(state.pending_flat);
  state.artwork_key = key;
  state.pending_key.clear();
  EnvironmentIncludeScenery(state.native_background.Get<unity::MeshRenderer>(), state.mask);
}
void EnvironmentScan(unity::Camera* camera)
{
  SystemEnvironment().scene_known = false;
  auto&      state                = SystemEnvironment();
  auto       epoch                = state.epoch;
  const auto mask                 = camera->TryCall<"get_cullingMask", int32_t>();
  if (!mask || state.epoch != epoch)
    return;
  if (state.mask != mask.value) {
    state.mask          = mask.value;
    state.bounds        = {};
    state.bounds_valid  = false;
    state.sweep_started = false;
    state.cursor        = 0;
    state.search_layer  = -1;
    state.next_bounds   = 0;
  }
  auto* loader      = state.loader.Get<system_environment::Loader>();
  auto  loader_root = il2cpp::GcHandle::Strong(loader);
  if (loader) {
    const auto alive = unity::TryAlive(loader);
    const auto active =
        alive && alive.value ? loader->TryCall<"get_isActiveAndEnabled", bool>() : il2cpp::CallResult<bool>{};
    if (state.epoch != epoch)
      return;
    if (!alive || (alive.value && !active))
      return;
    if (!alive.value || !active.value) {
      state.loader        = {};
      state.pool          = {};
      state.configuration = {};
      state.background    = {};
      EnvironmentDropArtwork();
      epoch  = state.epoch;
      loader = nullptr;
    }
  }
  if (!loader) {
    const auto now = std::chrono::steady_clock::now();
    if (now < state.next_discovery)
      return;
    state.next_discovery = now + std::chrono::milliseconds(500);
    const auto observed  = system_environment::Loaders();
    if (state.epoch != epoch)
      return;
    auto  root  = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
    auto* array = root.Get<il2cpp::Array<il2cpp::Object*>>();
    if (!observed || !array)
      return;
    for (size_t visited = 0; visited < array->size(); ++visited) {
      const auto index     = (state.discovery_start + visited) % array->size();
      auto*      object    = (*array)[index];
      auto*      candidate = object ? object->As<system_environment::Loader>() : nullptr;
      if (!EnvironmentLive(candidate))
        continue;
      const auto active = candidate->TryCall<"get_isActiveAndEnabled", bool>();
      if (active && active.value) {
        const auto background = candidate->TryCall<"get_Background", system_environment::Flat*>();
        if (state.epoch != epoch)
          return;
        const auto alive = background ? unity::TryAlive(background.value) : il2cpp::CallResult<bool>{};
        if (!alive || !alive.value)
          continue;
        EnvironmentArtwork art;
        const auto         artwork = EnvironmentArtworkFor(background.value, mask.value, -1, art);
        if (state.epoch != epoch)
          return;
        if (!artwork)
          continue;
        if (!artwork.value) {
          if (art.layer < 0 || art.layer > 31 || !(static_cast<uint32_t>(mask.value) & (1u << art.layer)))
            continue;
          const auto pool = system_environment::Pool(candidate);
          if (!pool || !pool.value || !pool.value->size())
            continue;
        }
        state.loader          = il2cpp::GcHandle::Strong(candidate);
        loader                = state.loader.Get<system_environment::Loader>();
        state.discovery_start = (index + 1) % array->size();
        break;
      }
    }
    if (!loader)
      return;
  }
  const auto configuration = system_environment::Configuration(loader);
  if (!configuration)
    return;
  if (configuration.value != state.configuration.Get()) {
    auto root = il2cpp::GcHandle::Strong(configuration.value);
    if (configuration.value && !root)
      return;
    EnvironmentDropArtwork();
    epoch               = state.epoch;
    state.pool          = {};
    state.configuration = std::move(root);
    state.cursor        = 0;
    state.background    = {};
  }
  const auto flat = loader->TryCall<"get_Background", system_environment::Flat*>();
  if (!flat || state.epoch != epoch)
    return;
  auto       flat_root  = il2cpp::GcHandle::Strong(flat.value);
  const auto flat_alive = unity::TryAlive(flat.value);
  if (!flat_alive)
    return;
  if (!flat_alive.value) {
    EnvironmentDropArtwork();
    state.search_layer  = -1;
    state.loader        = {};
    state.pool          = {};
    state.configuration = {};
    state.background    = {};
    return;
  }
  const bool background_changed = state.background.Get() != flat.value;
  if (background_changed) {
    if (!flat_root)
      return;
    state.background   = std::move(flat_root);
    state.search_layer = -1;
  }
  EnvironmentArtwork art;
  const auto         artwork = EnvironmentArtworkFor(flat.value, mask.value, -1, art);
  if (!artwork || state.epoch != epoch)
    return;
  state.scene_known = true;
  if (!artwork.value) {
    const int32_t required_layer = art.layer;
    if (required_layer < 0) {
      EnvironmentDropArtwork();
      state.loader        = {};
      state.pool          = {};
      state.configuration = {};
      state.background    = {};
      state.next_probe    = 0;
      return;
    }
    auto* cached = state.source_flat.Get<system_environment::Flat>();
    if (!cached && state.pixel_ticket)
      cached = state.pending_flat.Get<system_environment::Flat>();
    if (cached) {
      const auto fallback = EnvironmentArtworkFor(cached, mask.value, required_layer, art);
      if (!fallback || state.epoch != epoch)
        return;
      if (fallback.value) {
        const bool owned   = art.texture == state.source.Get() && art.renderer == state.native_background.Get();
        const bool pending = state.pixel_ticket && art.texture == state.pending_source.Get()
                             && art.renderer == state.pending_background.Get() && art.layer == state.pending_layer;
        if (!owned && !pending)
          EnvironmentAdopt(art, cached);
        return;
      }
    }
    if (state.search_layer == required_layer)
      return;
    EnvironmentDropArtwork();
    state.search_layer = required_layer;
    state.cursor       = 0;
    return;
  }
  if (art.texture == state.source.Get() && art.renderer == state.native_background.Get() && mask.value == state.mask
      && EnvironmentLive(state.shell.Get())) {
    return;
  }
  EnvironmentAdopt(art, flat.value);
}
} // namespace

void navigation_environment::Clear()
{
  SystemEnvironment().scene_known = false;
  EnvironmentDropArtwork();
  auto& state           = SystemEnvironment();
  state.camera          = {};
  state.loader          = {};
  state.pool            = {};
  state.configuration   = {};
  state.background      = {};
  state.frame           = -1;
  state.next_probe      = 0;
  state.next_bounds     = 0;
  state.cursor          = 0;
  state.discovery_start = 0;
  state.search_layer    = -1;
  state.next_discovery  = {};
}
void navigation_environment::ApplyDrawDistance(unity::Camera* camera, unity::Vector3 position, float clip,
                                               ClipWriter write_clip)
{
  auto&      state = SystemEnvironment();
  const auto epoch = state.epoch;
  if (!state.ready || state.failed || state.camera.Get() != camera || !state.shell_transform
      || !environment_orbit::Finite(position) || !std::isfinite(clip) || clip <= 0)
    return;
  if (state.bounds_valid) {
    const auto& center   = state.bounds.center;
    const auto& extents  = state.bounds.extents;
    const float expanded = navigation_environment_art::SceneryFarClip(
        clip, {position.x, position.y, position.z}, {center.x, center.y, center.z}, {extents.x, extents.y, extents.z});
    if (expanded > clip) {
      const bool written = write_clip(camera, clip, expanded);
      if (state.epoch != epoch)
        return;
      if (!written) {
        EnvironmentFail("scenery draw distance");
        return;
      }
      clip = expanded;
    }
  }
  if (clip != state.applied_scale) {
    const auto near_clip = camera->TryCall<"get_nearClipPlane", float>();
    if (state.epoch != epoch)
      return;
    const float radius = clip * 0.95f;
    if (!near_clip || !std::isfinite(near_clip.value) || near_clip.value <= 0 || clip <= 4 * near_clip.value)
      return;
    if (environment_orbit::SetVector<"set_localScale">(state.shell_transform.Get<unity::Transform>(),
                                                       {radius, radius, radius})
        && state.epoch == epoch)
      state.applied_scale = clip;
  }
}
void navigation_environment::Update(unity::Camera* camera, unity::Transform* transform)
{
  auto& state = SystemEnvironment();
  if (state.clearing || state.uploading)
    return;
  if (!state.attempted) {
    state.attempted = true;
    state.ready     = system_environment::Available();
    if (!state.ready)
      spdlog::warn("[SystemEnvironment] scene metadata unavailable; native scenery retained");
  }
  if (!state.ready || state.failed || !EnvironmentLive(camera) || !EnvironmentLive(transform))
    return;
  if (state.camera.Get() != camera) {
    Clear();
    state.camera = il2cpp::GcHandle::Strong(camera);
  }
  if (!state.camera) {
    EnvironmentFail("camera root");
    return;
  }
  const auto frame = unity::Time::frameCount();
  if (frame < 0)
    return;
  if (state.frame != frame) {
    state.frame = frame;
    if (frame >= state.next_probe) {
      state.next_probe = frame + 30;
      EnvironmentScan(camera);
    }
    if (auto* loader = state.loader.Get<system_environment::Loader>();
        loader && state.scene_known && (state.sweep_started || frame >= state.next_bounds)) {
      if (!state.sweep_started)
        state.next_bounds = frame + 30;
      EnvironmentBoundsSlice(loader, state.mask);
    }
    if (state.scene_known)
      EnvironmentFinishArtwork();
  }
  if (!EnvironmentLive(state.shell.Get()))
    return;
  const auto epoch = state.epoch;
  if (!state.shell_transform) {
    const auto shell = state.shell.Get<unity::GameObject>()->TryCall<"get_transform", unity::Transform*>();
    if (state.epoch != epoch)
      return;
    state.shell_transform = il2cpp::GcHandle::Strong(shell ? shell.value : nullptr);
  }
  if (!state.shell_transform)
    return;
  auto* shell = state.shell_transform.Get<unity::Transform>();
  if (state.parent.Get() != transform) {
    auto parent = il2cpp::GcHandle::Strong(transform);
    if (!parent || !shell->TryCall<"SetParent">(transform, false) || state.epoch != epoch
        || !environment_orbit::SetVector<"set_localPosition">(shell, {}) || state.epoch != epoch)
      return;
    state.parent = std::move(parent);
  }
  // The parent moves with the camera; preserve the original fixed world orientation.
  environment_orbit::SetVector<"set_eulerAngles">(shell, {});
}
