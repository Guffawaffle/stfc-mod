// Ambient enclosure for the system free-orbit camera. Native backdrop artwork,
// visibility, transforms and materials remain exclusively game-owned.
#include "features/navigation_environment.h"
#include "features/navigation_environment_art.h"

#include "game/navigation_environment.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
  unity::Bounds    bounds{};
  int32_t          next_scan = 0;
  bool             attempted = false, ready = false, failed = false, bounds_valid = false;
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
bool EnvironmentTexture(std::string_view name)
{
  auto&             state = SystemEnvironment();
  constexpr int32_t width = 1024, height = 512;
  const auto        started = std::chrono::steady_clock::now();
  const auto        theme   = navigation_environment_art::SelectTheme(name);
  const auto        pixels =
      navigation_environment_art::Generate(width, height, theme, navigation_environment_art::Seed(name));
  using Color           = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Color32">;
  const auto observed   = unity::CopyValueArray<navigation_environment_art::Pixel, Color>(pixels);
  auto       pixel_root = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  state.texture         = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::Texture2D>());
  auto* texture         = state.texture.Get<unity::Texture2D>();
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
      "[SystemEnvironment] directional sky {}x{} palette={} generated/uploaded={}ms; native artwork retained", width,
      height, static_cast<int>(theme),
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
  return true;
}
bool EnvironmentCreate(unity::Texture* native_texture, int32_t layer, unity::MeshRenderer* native_renderer)
{
  auto&                           state     = SystemEnvironment();
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
  if (!supported || !supported.value || !EnvironmentLive(shader_root.Get())
      || !EnvironmentTexture(EnvironmentName(native_texture)))
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
  state.source            = il2cpp::GcHandle::Strong(native_texture);
  state.native_background = il2cpp::GcHandle::Strong(native_renderer);
  state.renderer          = std::move(renderer_root);
  return state.source && state.native_background && state.renderer;
}
struct EnvironmentArtwork {
  unity::Texture*      texture  = nullptr;
  unity::MeshRenderer* renderer = nullptr;
  int32_t              layer    = -1;
};
bool EnvironmentArtworkFor(system_environment::Flat* flat, int32_t mask, int32_t required_layer,
                           EnvironmentArtwork& art)
{
  if (!EnvironmentLive(flat))
    return false;
  const auto observed = system_environment::Renderer(flat);
  art.renderer        = observed ? observed.value : nullptr;
  if (!EnvironmentLive(art.renderer))
    return false;
  const auto enabled     = art.renderer->TryCall<"get_enabled", bool>();
  const auto object      = art.renderer->TryCall<"get_gameObject", unity::GameObject*>();
  auto       object_root = il2cpp::GcHandle::Strong(object ? object.value : nullptr);
  const auto layer       = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>()
                                       : il2cpp::CallResult<int32_t>{};
  art.layer              = layer ? layer.value : -1;
  if (!enabled || !enabled.value || !layer || art.layer < 0 || art.layer > 31
      || !(static_cast<uint32_t>(mask) & (1u << art.layer)) || (required_layer >= 0 && art.layer != required_layer))
    return false;
  const auto material      = art.renderer->TryCall<"get_sharedMaterial", unity::Material*>();
  auto       material_root = il2cpp::GcHandle::Strong(material ? material.value : nullptr);
  const auto texture       = material_root
                                 ? material_root.Get<unity::Material>()->TryCall<"get_mainTexture", unity::Texture*>()
                                 : il2cpp::CallResult<unity::Texture*>{};
  art.texture              = texture ? texture.value : nullptr;
  auto texture_root        = il2cpp::GcHandle::Strong(art.texture);
  if (!EnvironmentLive(art.texture))
    return false;
  const auto width  = art.texture->TryCall<"get_width", int32_t>();
  const auto height = art.texture->TryCall<"get_height", int32_t>();
  return width && height && width.value >= 64 && height.value >= 64;
}
void EnvironmentIncludeScenery(unity::MeshRenderer* renderer, int32_t mask)
{
  if (!EnvironmentLive(renderer))
    return;
  const auto enabled     = renderer->TryCall<"get_enabled", bool>();
  const auto object      = renderer->TryCall<"get_gameObject", unity::GameObject*>();
  auto       object_root = il2cpp::GcHandle::Strong(object ? object.value : nullptr);
  const auto active      = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_activeInHierarchy", bool>()
                                       : il2cpp::CallResult<bool>{};
  const auto layer       = object_root ? object_root.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>()
                                       : il2cpp::CallResult<int32_t>{};
  const auto bounds      = unity::TryBounds(renderer);
  if (!enabled || !enabled.value || !active || !active.value || !layer || layer.value < 0 || layer.value > 31
      || !(static_cast<uint32_t>(mask) & (1u << layer.value)) || !bounds
      || !environment_orbit::Finite(bounds.value.center) || !environment_orbit::Finite(bounds.value.extents)
      || bounds.value.extents.x < 0 || bounds.value.extents.y < 0 || bounds.value.extents.z < 0)
    return;
  auto& state = SystemEnvironment();
  if (!state.bounds_valid) {
    state.bounds       = bounds.value;
    state.bounds_valid = true;
    return;
  }
  const auto merge = [](float& center, float& extent, float next_center, float next_extent) {
    const double low  = std::min(double(center) - extent, double(next_center) - next_extent);
    const double high = std::max(double(center) + extent, double(next_center) + next_extent);
    center            = float((low + high) * 0.5);
    extent            = float((high - low) * 0.5);
  };
  merge(state.bounds.center.x, state.bounds.extents.x, bounds.value.center.x, bounds.value.extents.x);
  merge(state.bounds.center.y, state.bounds.extents.y, bounds.value.center.y, bounds.value.extents.y);
  merge(state.bounds.center.z, state.bounds.extents.z, bounds.value.center.z, bounds.value.extents.z);
}
void EnvironmentCaptureBounds(system_environment::Loader* loader, int32_t mask)
{
  auto& state        = SystemEnvironment();
  state.bounds_valid = false;
  EnvironmentIncludeScenery(state.native_background.Get<unity::MeshRenderer>(), mask);
  const auto observed  = system_environment::Pool(loader);
  auto       pool_root = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  auto*      pool      = pool_root.Get<il2cpp::Array<system_environment::Flat*>>();
  if (!pool)
    return;
  for (auto* flat : *pool) {
    if (!EnvironmentLive(flat))
      continue;
    const auto renderer = system_environment::Renderer(flat);
    if (renderer)
      EnvironmentIncludeScenery(renderer.value, mask);
  }
}
void EnvironmentScan(unity::Camera* camera)
{
  const auto observed   = system_environment::Loaders();
  auto       array_root = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  auto*      array      = array_root.Get<il2cpp::Array<il2cpp::Object*>>();
  const auto mask       = camera->TryCall<"get_cullingMask", int32_t>();
  if (!observed || !array || !mask) {
    EnvironmentFail("loader discovery / camera mask");
    return;
  }
  for (auto* object : *array) {
    auto* loader = object ? object->As<system_environment::Loader>() : nullptr;
    if (!EnvironmentLive(loader))
      continue;
    const auto active = loader->TryCall<"get_isActiveAndEnabled", bool>();
    const auto flat   = loader->TryCall<"get_Background", system_environment::Flat*>();
    if (!active || !active.value || !flat || !EnvironmentLive(flat.value))
      continue;
    EnvironmentArtwork art;
    if (!EnvironmentArtworkFor(flat.value, mask.value, -1, art)) {
      const int32_t required_layer = art.layer;
      const auto    observed_pool  = system_environment::Pool(loader);
      auto          pool_root      = il2cpp::GcHandle::Strong(observed_pool ? observed_pool.value : nullptr);
      auto*         pool           = pool_root.Get<il2cpp::Array<system_environment::Flat*>>();
      if (required_layer < 0 || !pool)
        continue;
      bool found = false;
      for (auto* candidate : *pool)
        if (EnvironmentArtworkFor(candidate, mask.value, required_layer, art)) {
          found = true;
          break;
        }
      if (!found)
        continue;
    }
    auto& state = SystemEnvironment();
    if (art.texture == state.source.Get() && art.renderer == state.native_background.Get()
        && EnvironmentLive(state.shell.Get())) {
      EnvironmentCaptureBounds(loader, mask.value);
      return;
    }
    auto texture_root  = il2cpp::GcHandle::Strong(art.texture);
    auto renderer_root = il2cpp::GcHandle::Strong(art.renderer);
    navigation_environment::Clear();
    if (!texture_root || !renderer_root
        || !EnvironmentCreate(texture_root.Get<unity::Texture>(), art.layer, renderer_root.Get<unity::MeshRenderer>()))
      EnvironmentFail("ambient enclosure");
    else
      EnvironmentCaptureBounds(loader, mask.value);
    return;
  }
  // An in-flight system transition must not keep the previous source's artwork.
  navigation_environment::Clear();
}
} // namespace

void navigation_environment::Clear()
{
  auto& state             = SystemEnvironment();
  state.native_background = {};
  EnvironmentDestroy(state.shell);
  EnvironmentDestroy(state.mesh);
  EnvironmentDestroy(state.material);
  EnvironmentDestroy(state.texture);
  state.source       = {};
  state.renderer     = {};
  state.camera       = {};
  state.bounds       = {};
  state.bounds_valid = false;
}
void navigation_environment::ApplyDrawDistance(unity::Camera* camera)
{
  auto& state = SystemEnvironment();
  if (!state.ready || state.failed || !state.bounds_valid || state.camera.Get() != camera || !EnvironmentLive(camera)
      || !EnvironmentLive(state.shell.Get()))
    return;
  const auto transform      = environment_orbit::Transform(camera);
  auto       transform_root = il2cpp::GcHandle::Strong(transform ? transform.value : nullptr);
  const auto position =
      environment_orbit::Vector(environment_orbit::VectorGetter<"get_position">(), transform_root.Get());
  const auto baseline = camera->TryCall<"get_farClipPlane", float>();
  if (!position || !baseline)
    return;
  const auto& center  = state.bounds.center;
  const auto& extents = state.bounds.extents;
  const float expanded =
      navigation_environment_art::SceneryFarClip(baseline.value, {position.value.x, position.value.y, position.value.z},
                                                 {center.x, center.y, center.z}, {extents.x, extents.y, extents.z});
  if (expanded > baseline.value && !camera->TryCall<"set_farClipPlane">(expanded))
    EnvironmentFail("scenery draw distance");
}
void navigation_environment::Update(unity::Camera* camera)
{
  auto& state = SystemEnvironment();
  if (!state.attempted) {
    state.attempted = true;
    state.ready     = system_environment::Available();
    if (!state.ready)
      spdlog::warn("[SystemEnvironment] scene metadata unavailable; native scenery retained");
  }
  if (!state.ready || state.failed || !EnvironmentLive(camera))
    return;
  const auto frame = unity::Time::frameCount();
  if (frame >= state.next_scan) {
    state.next_scan = frame + 30;
    EnvironmentScan(camera);
  }
  if (!EnvironmentLive(state.shell.Get()))
    return;
  if (state.camera.Get() != camera)
    state.camera = il2cpp::GcHandle::Strong(camera);
  if (!state.camera) {
    EnvironmentFail("camera root");
    return;
  }
  ApplyDrawDistance(camera);
  if (state.failed)
    return;
  const auto far                   = camera->TryCall<"get_farClipPlane", float>();
  const auto near                  = camera->TryCall<"get_nearClipPlane", float>();
  const auto camera_transform      = environment_orbit::Transform(camera);
  auto       camera_transform_root = il2cpp::GcHandle::Strong(camera_transform ? camera_transform.value : nullptr);
  const auto shell_transform      = state.shell.Get<unity::GameObject>()->TryCall<"get_transform", unity::Transform*>();
  auto       shell_transform_root = il2cpp::GcHandle::Strong(shell_transform ? shell_transform.value : nullptr);
  const auto position =
      environment_orbit::Vector(environment_orbit::VectorGetter<"get_position">(), camera_transform_root.Get());
  if (!far || !near || !std::isfinite(far.value) || !std::isfinite(near.value) || near.value <= 0
      || far.value <= 4 * near.value || !position || !shell_transform_root) {
    EnvironmentFail("camera-follow query");
    return;
  }
  auto*      shell = shell_transform_root.Get<unity::Transform>();
  const auto scale = EnvironmentVector3{far.value * 0.95f, far.value * 0.95f, far.value * 0.95f};
  if (!shell->TryCall<"SetParent">(camera_transform_root.Get<unity::Transform>(), true)
      || !environment_orbit::SetVector<"set_eulerAngles">(shell, {})
      || !environment_orbit::SetVector<"set_position">(shell, position.value)
      || !environment_orbit::SetVector<"set_localScale">(shell, scale))
    EnvironmentFail("camera-follow update");
}
