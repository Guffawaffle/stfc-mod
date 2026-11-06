// Actual production environment maintenance against controlled metadata; no game or graphics device.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#endif
#define main ExistingBindingsMain
#include "il2cpp_bindings.cc"
#undef main
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include "features/navigation_environment.h"
#include "game/navigation_environment.h"
#include <future>
#undef near
#undef far

namespace env_fixture
{
constexpr size_t kPool = 257;
template <class T, size_t N> struct Array {
  Il2CppObject        header{};
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = N;
  std::array<T, N>    items{};
};
Il2CppObject loader{}, flat{}, renderer{}, material{}, texture{}, game_object{}, transform{}, camera{}, shell{},
    shell_transform{}, replacement_camera{}, config_object{}, dead_flat{}, bad_loader{}, bad_flat{}, bad_renderer{},
    bad_game_object{}, replacement_texture{}, far_flat{}, far_renderer{}, unknown_flat{}, near_renderer{},
    secondary_loader{}, secondary_flat{}, secondary_renderer{};
Array<Il2CppObject*, 2>                             loaders;
Array<navigation_environment_binding::Flat*, kPool> pool, replacement_pool;
Array<navigation_environment_binding::Flat*, 256>   cohort_pool;
float                                               far_member_x     = 10000;
uint64_t                                            far_bounds_reads = 0;
bool pool_known = true, configuration_known = true, background_known = true;
bool renderer_known = true, renderer_null = false;
bool material_known = true, texture_known = true, material_null = false, texture_null = false, background_null = false,
     bad_background = false, bad_layer = false;
Il2CppObject* texture_value = &texture;
struct NameStorage {
  Il2CppObject header{};
  int32_t      length    = 13;
  char16_t     chars[14] = u"StableArtwork";
} name_storage;
Il2CppString* artwork_name           = nullptr;
Il2CppObject* env_alive_throw_object = nullptr;
uint64_t      alive_delegate_calls = 0, unknown_alive_throws = 0, name_reads = 0;
bool          Alive(Il2CppObject* object, const MethodInfo* method)
{
  ++alive_delegate_calls;
  if (object == env_alive_throw_object) {
    ++unknown_alive_throws;
    throw Il2CppExceptionWrapper(&thrown);
  }
  return VisualAlive(object, method);
}
il2cpp::Array<navigation_environment_binding::Flat*>* pool_value =
    reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&pool);
navigation_environment_binding::Config* configuration_value = nullptr;
struct BoundsBox {
  Il2CppObject  header;
  unity::Bounds value;
} bounds_box{};
const MethodInfo* bounds_method    = nullptr;
const MethodInfo* transform_method = nullptr;
const MethodInfo* position_method  = nullptr;
const MethodInfo* euler_set        = nullptr;
const MethodInfo* position_set     = nullptr;
const MethodInfo* scale_set        = nullptr;
uint64_t          discovery_calls = 0, pool_reads = 0, renderer_reads = 0, bounds_reads = 0;
uint64_t          transform_invokes = 0, vector_invokes = 0, vector_set_invokes = 0, parent_sets = 0, clip_sets = 0,
                  scale_sets = 0, local_position_sets = 0, destroys = 0;
float             far_clip = 100, near_clip = 1;
void (*background_callback)() = nullptr;
void (*bounds_callback)()     = nullptr;
void (*parent_callback)()     = nullptr;
void (*destroy_callback)()    = nullptr;

bool Active(Il2CppObject*, const MethodInfo*)
{ return true; }
int32_t Layer(Il2CppObject* object, const MethodInfo*)
{ return object == &bad_game_object && bad_layer ? 1 : 0; }
int32_t Mask(Il2CppObject*, const MethodInfo*)
{ return 1; }
int32_t Dimension(Il2CppObject*, const MethodInfo*)
{ return 256; }
float Far(Il2CppObject*, const MethodInfo*)
{ return far_clip; }
float Near(Il2CppObject*, const MethodInfo*)
{ return near_clip; }
Il2CppObject* Background(Il2CppObject* object, const MethodInfo*)
{
  if (object == &secondary_loader)
    return &secondary_flat;
  if (!background_known)
    throw Il2CppExceptionWrapper(&thrown);
  if (background_callback) {
    auto callback = std::exchange(background_callback, nullptr);
    callback();
  }
  return background_null ? nullptr : object == &bad_loader || bad_background ? &bad_flat : &flat;
}
Il2CppObject* GameObject(Il2CppObject* object, const MethodInfo*)
{ return object == &bad_renderer ? &bad_game_object : &game_object; }
Il2CppObject* Material(Il2CppObject* object, const MethodInfo*)
{
  if (!material_known)
    throw Il2CppExceptionWrapper(&thrown);
  return material_null || object == &bad_renderer ? nullptr : &material;
}
Il2CppObject* Texture(Il2CppObject*, const MethodInfo*)
{
  if (!texture_known)
    throw Il2CppExceptionWrapper(&thrown);
  return texture_null ? nullptr : texture_value;
}
Il2CppString* Name(Il2CppObject*, const MethodInfo*)
{
  ++name_reads;
  return artwork_name;
}
void          LayerWrite(Il2CppObject*, int32_t, const MethodInfo*) {}
Il2CppObject* Transform(Il2CppObject*, const MethodInfo*)
{ return &shell_transform; }
void Parent(Il2CppObject* self, Il2CppObject* parent, bool retain_world, const MethodInfo*)
{
  Require(self == &shell_transform && parent == &transform && !retain_world,
          "shell parents to distinct camera transform using local coordinates");
  ++parent_sets;
  if (parent_callback) {
    auto callback = std::exchange(parent_callback, nullptr);
    callback();
  }
}
void Destroy(Il2CppObject*, const MethodInfo*)
{
  ++destroys;
  if (destroy_callback) {
    auto callback = std::exchange(destroy_callback, nullptr);
    callback();
  }
}
bool WriteClip(unity::Camera*, float, float value)
{
  ++clip_sets;
  far_clip = value;
  return true;
}
void* Unbox(Il2CppObject* object)
{ return object == &bounds_box.header ? &bounds_box.value : nullptr; }
Il2CppObject* Invoke(const MethodInfo* method, void* target, void**, Il2CppException**, bool& handled)
{
  handled = true;
  if (method == bounds_method) {
    ++bounds_reads;
    if (target == &far_renderer) {
      ++far_bounds_reads;
      bounds_box.value = {{far_member_x, 0, 0}, {10, 10, 10}};
    } else if (target == &near_renderer)
      bounds_box.value = {{0, 0, 0}, {10, 10, 10}};
    else
      bounds_box.value = {{0, 0, 0}, {100, 100, 100}};
    if (bounds_callback) {
      auto callback = std::exchange(bounds_callback, nullptr);
      callback();
    }
    return &bounds_box.header;
  }
  if (method == transform_method) {
    ++transform_invokes;
    return &transform;
  }
  if (method == position_method) {
    ++vector_invokes;
    warp_vector_box.value = {0, 0, -30};
    return &warp_vector_box.header;
  }
  if (method == euler_set || method == position_set || method == scale_set) {
    ++vector_set_invokes;
    if (method == scale_set)
      ++scale_sets;
    if (method == position_set)
      ++local_position_sets;
    return nullptr;
  }
  handled = false;
  return nullptr;
}
} // namespace env_fixture

namespace fixture_environment_binding
{
using Flat   = navigation_environment_binding::Flat;
using Loader = navigation_environment_binding::Loader;
using Config = navigation_environment_binding::Config;
il2cpp::CallResult<Config*> Configuration(Loader*)
{ return {env_fixture::configuration_known, env_fixture::configuration_value}; }
bool Available()
{ return true; }
il2cpp::CallResult<il2cpp::Array<Il2CppObject*>*> Loaders()
{
  ++env_fixture::discovery_calls;
  return {true, reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(&env_fixture::loaders)};
}
il2cpp::CallResult<il2cpp::Array<Flat*>*> Pool(Loader*)
{
  ++env_fixture::pool_reads;
  return {env_fixture::pool_known, env_fixture::pool_value};
}
il2cpp::CallResult<unity::MeshRenderer*> Renderer(Flat* flat)
{
  ++env_fixture::renderer_reads;
  if (flat == reinterpret_cast<Flat*>(&env_fixture::secondary_flat))
    return {true, reinterpret_cast<unity::MeshRenderer*>(&env_fixture::secondary_renderer)};
  if (flat == reinterpret_cast<Flat*>(&env_fixture::flat)
      && (!env_fixture::renderer_known || env_fixture::renderer_null))
    return {env_fixture::renderer_known, nullptr};
  return {true, reinterpret_cast<unity::MeshRenderer*>(
                    flat == reinterpret_cast<Flat*>(&env_fixture::bad_flat)       ? &env_fixture::bad_renderer
                    : flat == reinterpret_cast<Flat*>(&env_fixture::far_flat)     ? &env_fixture::far_renderer
                    : flat == reinterpret_cast<Flat*>(&env_fixture::unknown_flat) ? &env_fixture::near_renderer
                                                                                  : &env_fixture::renderer)};
}
} // namespace fixture_environment_binding

#define navigation_environment_binding fixture_environment_binding
#include "../src/mod/features/navigation_environment.cc"
#undef navigation_environment_binding

int main()
try {
  std::cout.setf(std::ios::unitbuf);
  BuildRuntime();
  il2cpp::SetErrorHandler([](std::string_view message) { errors.emplace_back(message); });
  using namespace env_fixture;
  auto& camera_class        = Define("UnityEngine", "Camera", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& transform_class     = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &component_class);
  auto& texture_class       = Define("UnityEngine", "Texture", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& mesh_renderer_class = Define("UnityEngine", "MeshRenderer", IL2CPP_TYPE_CLASS, &renderer_class);
  auto& loader_class = Define("Digit.Client.Rendering", "FlatRenderableLoader", IL2CPP_TYPE_CLASS, &component_class);
  auto& flat_class   = Define("Digit.Client.Rendering", "FlatRenderable", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& config_class = Define("Digit.Client.Rendering", "FlatRenderableConfig", IL2CPP_TYPE_CLASS, &object_class);
  auto& bounds_class = Define("UnityEngine", "Bounds", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Bounds));
  camera.klass = replacement_camera.klass = &camera_class.klass;
  transform.klass = shell_transform.klass = &transform_class.klass;
  texture.klass                           = &texture_class.klass;
  config_object.klass                     = &config_class.klass;
  dead_flat.klass                         = &flat_class.klass;
  renderer.klass = bad_renderer.klass = far_renderer.klass = near_renderer.klass = &mesh_renderer_class.klass;
  loader.klass = bad_loader.klass = &loader_class.klass;
  secondary_loader.klass          = &loader_class.klass;
  secondary_flat.klass            = &flat_class.klass;
  secondary_renderer.klass        = &mesh_renderer_class.klass;
  flat.klass = bad_flat.klass = far_flat.klass = unknown_flat.klass = &flat_class.klass;
  replacement_texture.klass                                         = &texture_class.klass;
  bad_game_object.klass                                             = &game_object_class.klass;
  name_storage.header.klass                                         = &string_class.klass;
  artwork_name                                                      = reinterpret_cast<Il2CppString*>(&name_storage);
  for (const auto* method : unity_object_class.methods)
    if (std::string_view(method->name) == "get_name")
      const_cast<MethodInfo*>(method)->methodPointer = const_cast<MethodInfo*>(method)->virtualMethodPointer =
          reinterpret_cast<Il2CppMethodPointer>(&Name);
  for (const auto* method : unity_object_class.methods)
    if (std::string_view(method->name) == "op_Implicit")
      const_cast<MethodInfo*>(method)->methodPointer = const_cast<MethodInfo*>(method)->virtualMethodPointer =
          reinterpret_cast<Il2CppMethodPointer>(&Alive);
  game_object.klass = shell.klass = &game_object_class.klass;
  material.klass                  = &material_class.klass;
  bounds_box.header.klass         = &bounds_class.klass;
  bounds_box.value                = {{0, 0, 0}, {100, 100, 100}};
  loaders.items[0]                = &loader;
  pool.items.fill(reinterpret_cast<navigation_environment_binding::Flat*>(&flat));
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&Destroy), T(void_class), {T(unity_object_class)},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(camera_class, "get_cullingMask", reinterpret_cast<void*>(&Mask), T(int_class), {});
  AddMethod(camera_class, "get_farClipPlane", reinterpret_cast<void*>(&Far), T(float_class), {});
  AddMethod(camera_class, "get_nearClipPlane", reinterpret_cast<void*>(&Near), T(float_class), {});
  AddMethod(loader_class, "get_isActiveAndEnabled", reinterpret_cast<void*>(&Active), T(bool_class), {});
  AddMethod(loader_class, "get_Background", reinterpret_cast<void*>(&Background), T(flat_class), {});
  AddMethod(renderer_class, "get_gameObject", reinterpret_cast<void*>(&GameObject), T(game_object_class), {});
  AddMethod(renderer_class, "get_sharedMaterial", reinterpret_cast<void*>(&Material), T(material_class), {});
  AddMethod(material_class, "get_mainTexture", reinterpret_cast<void*>(&Texture), T(texture_class), {});
  AddMethod(texture_class, "get_width", reinterpret_cast<void*>(&Dimension), T(int_class), {});
  AddMethod(texture_class, "get_height", reinterpret_cast<void*>(&Dimension), T(int_class), {});
  AddMethod(game_object_class, "set_layer", reinterpret_cast<void*>(&LayerWrite), T(void_class), {T(int_class)});
  AddMethod(game_object_class, "get_layer", reinterpret_cast<void*>(&Layer), T(int_class), {});
  AddMethod(game_object_class, "get_transform", reinterpret_cast<void*>(&Transform), T(transform_class), {});
  AddMethod(transform_class, "SetParent", reinterpret_cast<void*>(&Parent), T(void_class),
            {T(transform_class), T(bool_class)});
  auto& bounds_get = AddMethod(renderer_class, "get_bounds", reinterpret_cast<void*>(&Never), T(bounds_class), {});
  auto& transform_get =
      AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&Never), T(transform_class), {});
  auto& position_get = AddMethod(transform_class, "get_position", reinterpret_cast<void*>(&Never), T(vec3_class), {});
  auto& euler_write =
      AddMethod(transform_class, "set_eulerAngles", reinterpret_cast<void*>(&Never), T(void_class), {T(vec3_class)});
  auto& position_write =
      AddMethod(transform_class, "set_localPosition", reinterpret_cast<void*>(&Never), T(void_class), {T(vec3_class)});
  auto& scale_write =
      AddMethod(transform_class, "set_localScale", reinterpret_cast<void*>(&Never), T(void_class), {T(vec3_class)});
  for (auto* method : {&bounds_get, &transform_get, &position_get, &euler_write, &position_write, &scale_write})
    method->invoker_method = &WarpInvokerSentinel;
  bounds_method               = &bounds_get;
  transform_method            = &transform_get;
  position_method             = &position_get;
  euler_set                   = &euler_write;
  position_set                = &position_write;
  scale_set                   = &scale_write;
  tow_runtime_invoke_override = &Invoke;
  extra_value_box             = &bounds_box.header;
  extra_value_payload         = &bounds_box.value;
  auto& state                 = SystemEnvironment();
  state.attempted = state.ready = true;
  state.shell                   = il2cpp::GcHandle::Strong(&shell);
  state.source                  = il2cpp::GcHandle::Strong(&texture);
  state.native_background       = il2cpp::GcHandle::Strong(&renderer);
  state.camera                  = il2cpp::GcHandle::Strong(&camera);
  auto*      camera_ptr         = reinterpret_cast<unity::Camera*>(&camera);
  auto*      parent_ptr         = reinterpret_cast<unity::Transform*>(&transform);
  const auto initial_root_count = warp_roots.size(), root_counter = warp_next_root;
  unsigned   checks = 0;
  const auto check  = [&](bool pass, const char* text) {
    ++checks;
    Require(pass, text);
  };
  const auto frame = [&] {
    ++input_frame;
    navigation_environment::Update(camera_ptr, parent_ptr);
  };
  for (unsigned i = 0; i < 120; ++i) {
    const auto before = state.cursor;
    frame();
    const auto visited = (state.cursor + kPool - (before % kPool)) % kPool;
    check(visited <= 4, "each real Update examines at most four pooled slots");
    navigation_environment::ApplyDrawDistance(camera_ptr, {0, 0, -30}, 100, &WriteClip);
  }
  check(!state.failed, "production environment remains healthy");
  check(discovery_calls == 1, "healthy cached loader avoids repeated global discovery");
  check(pool_reads == 120, "bounded refresh reads current pool identity once per frame");
  check(transform_invokes == 0 && vector_invokes == 0, "supplied pose and parent eliminate reflected pose getters");
  check(parent_sets == 1 && local_position_sets == 1, "stable parent is assigned exactly once");
  check(scale_sets == 1, "unchanged clip sets shell scale once");
  std::cout << "ENV_EFFICIENCY updates=120 pool_size=" << kPool << " discovery_calls=" << discovery_calls
            << " pool_reads=" << pool_reads << " renderer_reads=" << renderer_reads
            << " boxed_bounds_reads=" << bounds_reads << " transform_invokes=" << transform_invokes
            << " boxed_vector_invokes=" << vector_invokes << " vector_set_invokes=" << vector_set_invokes
            << " parent_sets=" << parent_sets << " scale_sets=" << scale_sets
            << " gc_handle_creates=" << warp_next_root - root_counter
            << " net_roots=" << warp_roots.size() - initial_root_count << '\n';
  const auto repeats_pool = pool_reads, repeats_bounds = bounds_reads;
  for (unsigned i = 0; i < 10; ++i)
    navigation_environment::Update(camera_ptr, parent_ptr);
  check(pool_reads == repeats_pool && bounds_reads == repeats_bounds,
        "same-frame calls do not repeat bounded maintenance");
  const auto steady_scale = scale_sets;
  navigation_environment::ApplyDrawDistance(camera_ptr, {0, 0, -30}, 100, &WriteClip);
  check(scale_sets == steady_scale, "duplicate draw-distance call keeps unchanged scale");

  // Test budget accounting independently of periodic background validation.
  pool.items.fill(nullptr);
  state.pool   = {};
  state.cursor = 0;
  for (unsigned i = 0; i < 65; ++i) {
    const auto before = state.cursor;
    EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
    check((state.cursor + kPool - (before % kPool)) % kPool > 0
              && (state.cursor + kPool - (before % kPool)) % kPool <= 4,
          "null slots consume the four-slot budget and keep advancing");
  }
  check(state.cursor == kPool, "257-slot null pool completes a sweep without truncation");
  pool.items.fill(reinterpret_cast<navigation_environment_binding::Flat*>(&dead_flat));
  visual_dead_object        = &dead_flat;
  state.cursor              = 0;
  const auto dead_renderers = renderer_reads;
  for (unsigned i = 0; i < 65; ++i) {
    const auto before = state.cursor;
    EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
    check((state.cursor + kPool - (before % kPool)) % kPool > 0
              && (state.cursor + kPool - (before % kPool)) % kPool <= 4,
          "dead slots consume the four-slot budget and keep advancing");
  }
  check(renderer_reads == dead_renderers && state.cursor == kPool,
        "dead entries never read renderers or starve later slots");
  // Shrink only after a complete known sweep, preserving conservative extents meanwhile.
  state.bounds        = {{0, 0, 0}, {1000, 1000, 1000}};
  state.bounds_valid  = true;
  state.cursor        = 0;
  state.sweep_started = false;
  for (unsigned i = 0; i < 64; ++i)
    EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(state.bounds.extents.x == 1000, "partial known sweep never shrinks published bounds early");
  EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(state.bounds_valid && state.bounds.extents.x == 100,
        "complete known dead-entry sweep shrinks to current background bounds");
  visual_dead_object = nullptr;
  pool.items.fill(reinterpret_cast<navigation_environment_binding::Flat*>(&flat));
  state.bounds                   = {{0, 0, 0}, {1000, 1000, 1000}};
  state.bounds_valid             = true;
  state.cursor                   = 0;
  state.sweep_started            = false;
  env_alive_throw_object         = &flat;
  const auto flat_unknown_before = unknown_alive_throws;
  for (unsigned i = 0; i < 65; ++i)
    EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(state.bounds.extents.x == 1000, "complete sweep containing unavailable entries cannot shrink published bounds");
  check(unknown_alive_throws == flat_unknown_before + kPool,
        "every unavailable pooled slot invokes and throws from the per-object liveness delegate");
  env_alive_throw_object = nullptr;

  const auto kept_pool   = state.pool.Get();
  const auto kept_cursor = state.cursor;
  pool_known             = false;
  EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(state.pool.Get() == kept_pool && state.cursor == kept_cursor,
        "unavailable pool observation retains cache and progress");
  pool_known = true;
  replacement_pool.items.fill(reinterpret_cast<navigation_environment_binding::Flat*>(&flat));
  pool_value = reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&replacement_pool);
  EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(state.pool.Get() == pool_value && state.cursor == 4, "changed pool identity resets its bounded cursor");

  pool_value = nullptr;
  EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);
  check(!state.pool && state.cursor == 0, "known null pool clears the prior cached array and cursor");
  pool_value = reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&pool);
  EnvironmentBoundsSlice(reinterpret_cast<navigation_environment_binding::Loader*>(&loader), 1);

  // Unavailable cached-loader liveness is not confirmed death.
  env_alive_throw_object = &loader;
  state.next_probe       = 0;
  const auto held_loader = state.loader.Get(), held_shell = state.shell.Get();
  const auto loader_unknown_before = unknown_alive_throws, loader_delegate_before = alive_delegate_calls;
  frame();
  check(unknown_alive_throws > loader_unknown_before && alive_delegate_calls > loader_delegate_before,
        "unavailable cached loader actually reaches the throwing liveness delegate");
  check(state.loader.Get() == held_loader && state.shell.Get() == held_shell,
        "unknown loader liveness preserves owned state");
  env_alive_throw_object = nullptr;
  visual_dead_object     = &loader;
  state.next_probe       = 0;
  state.next_discovery   = std::chrono::steady_clock::time_point::max();
  frame();
  check(!state.loader && !state.shell && !state.pool, "confirmed dead loader releases its artwork and pool cache");
  visual_dead_object = nullptr;
  check(destroys >= 1, "dead loader cleanup destroys the owned shell");
  navigation_environment::Clear();
  check(!state.camera && !state.loader && !state.pool && !state.parent && !state.shell_transform,
        "explicit clear releases all environment owner handles");
  check(warp_roots.empty(), "clear returns controlled-runtime GC roots to baseline");
  const auto seed = [&] {
    navigation_environment::Clear();
    state.failed    = false;
    state.attempted = state.ready = true;
    state.shell                   = il2cpp::GcHandle::Strong(&shell);
    state.source                  = il2cpp::GcHandle::Strong(&texture);
    state.native_background       = il2cpp::GcHandle::Strong(&renderer);
    state.camera                  = il2cpp::GcHandle::Strong(&camera);
    state.source_flat             = il2cpp::GcHandle::Strong(&flat);
    loaders.items[0]              = &loader;
    loaders.items[1]              = nullptr;
    pool_known = configuration_known = background_known = true;
    material_known = texture_known = true;
    renderer_known                 = true;
    renderer_null                  = false;
    material_null = texture_null = background_null = bad_background = bad_layer = false;
    texture_value                                                               = &texture;
    pool_value          = reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&pool);
    configuration_value = nullptr;
    state.next_probe    = 0;
  };
  seed();
  frame();
  configuration_known     = false;
  state.next_probe        = 0;
  const auto config_shell = state.shell.Get();
  frame();
  check(state.shell.Get() == config_shell, "unknown configuration observation preserves current artwork");
  configuration_known = true;
  configuration_value = reinterpret_cast<navigation_environment_binding::Config*>(&config_object);
  background_known    = false;
  state.next_probe    = 0;
  frame();
  check(state.configuration.Get() == &config_object && !state.shell && !state.pixel_ticket,
        "confirmed configuration replacement retires old artwork before later unavailable background observation");
  background_known = true;

  seed();
  loaders.items[0] = nullptr;
  navigation_environment::Update(reinterpret_cast<unity::Camera*>(&replacement_camera), parent_ptr);
  check(state.camera.Get() == &replacement_camera && !state.shell && !state.loader && !state.pool,
        "camera replacement clears prior artwork, loader and pool before binding the new camera");
  navigation_environment::Clear();
  check(warp_roots.empty(), "replacement camera teardown releases every retained root");

  // Managed calls may synchronously tear down or replace the context they were observing.
  seed();
  frame();
  state.next_probe    = 0;
  background_callback = +[] { navigation_environment::Clear(); };
  frame();
  check(!state.camera && !state.loader && !state.shell && !state.pending_source && !state.pixel_ticket,
        "clear reentry from background getter cannot republish old artwork or start a pixel request");

  seed();
  frame();
  bounds_callback = +[] { navigation_environment::Clear(); };
  frame();
  check(!state.camera && !state.loader && !state.bounds_valid && state.cursor == 0,
        "clear reentry from Bounds observation cannot publish stale bounds or advance an old pool cursor");

  seed();
  parent_callback = +[] { navigation_environment::Clear(); };
  frame();
  check(!state.camera && !state.parent && !state.shell_transform && !state.shell,
        "clear reentry from SetParent cannot reacquire a stale parent or shell owner");
  navigation_environment::Clear();
  check(warp_roots.empty(), "all lifecycle and reentry cases release their controlled roots");
  {
    EnvironmentArtwork art;
    material_null = true;
    auto result   = EnvironmentArtworkFor(reinterpret_cast<navigation_environment_binding::Flat*>(&flat), 1, -1, art);
    check(result && !result.value, "known null material is known unusable artwork");
    material_null  = false;
    material_known = false;
    result         = EnvironmentArtworkFor(reinterpret_cast<navigation_environment_binding::Flat*>(&flat), 1, -1, art);
    check(!result, "unavailable material observation remains distinct from known null");
    material_known = true;
    texture_null   = true;
    result         = EnvironmentArtworkFor(reinterpret_cast<navigation_environment_binding::Flat*>(&flat), 1, -1, art);
    check(result && !result.value, "known null texture is known unusable artwork");
    texture_null  = false;
    texture_known = false;
    result        = EnvironmentArtworkFor(reinterpret_cast<navigation_environment_binding::Flat*>(&flat), 1, -1, art);
    check(!result, "unavailable texture observation remains distinct from known null");
    texture_known = true;
  }
  seed();
  bad_layer        = true;
  loaders.items[0] = &bad_loader;
  loaders.items[1] = &loader;
  frame();
  check(state.loader.Get() == &loader && !state.pixel_ticket,
        "discovery skips first active loader with artwork outside the camera mask");
  seed();
  frame();
  background_null  = true;
  state.next_probe = 0;
  frame();
  check(!state.loader && !state.shell && !state.pool, "known null cached background releases stale loader and artwork");
  check(EnvironmentName(reinterpret_cast<il2cpp::Object*>(&replacement_texture)) == "StableArtwork" && name_reads > 0,
        "replacement texture name executes the overridden native getter and reads valid synthetic UTF16");
  seed();
  frame();
  state.artwork_key       = "StableArtwork";
  const auto reused_shell = state.shell.Get();
  bad_background          = true;
  texture_value           = &replacement_texture;
  state.next_probe        = 0;
  frame();
  check(state.source.Get() == &replacement_texture && state.native_background.Get() == &renderer
            && state.source_flat.Get() == &flat && state.shell.Get() == reused_shell && !state.pixel_ticket,
        "cached fallback adopts changed texture ownership while reusing the same generated artwork");
  navigation_environment::Clear();
  check(warp_roots.empty(), "qualification and fallback cases release every retained root");
  seed();
  frame();
  const auto before_destroy_reentry = destroys;
  destroy_callback                  = +[] {
    navigation_environment::Clear();
    navigation_environment::Update(reinterpret_cast<unity::Camera*>(&camera),
                                   reinterpret_cast<unity::Transform*>(&transform));
  };
  navigation_environment::Clear();
  check(destroys == before_destroy_reentry + 1 && !state.camera && !state.shell && !state.parent,
        "Destroy reentry cannot double-destroy retired resources or bind a new context while clearing");
  check(warp_roots.empty(), "Destroy reentry releases all retained roots");
  // Natural cohort regression: the native background is not a pool member.
  // Only actual renderer observations publish/retire the far geometry; no cursor,
  // sweep flag or published bounds are forced by this test.
  seed();
  cohort_pool.items.fill(nullptr);
  cohort_pool.items[0]       = reinterpret_cast<navigation_environment_binding::Flat*>(&far_flat);
  cohort_pool.items[127]     = reinterpret_cast<navigation_environment_binding::Flat*>(&unknown_flat);
  const auto cohort_identity = reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&cohort_pool);
  pool_value                 = cohort_identity;
  far_member_x               = 10000;
  check(std::none_of(cohort_pool.items.begin(), cohort_pool.items.end(),
                     [](const auto* item) {
                       return item == reinterpret_cast<const navigation_environment_binding::Flat*>(&flat);
                     }),
        "native background is outside the observed pool cohort");
  const auto far_reads_before = far_bounds_reads;
  for (unsigned i = 0; i < 64; ++i)
    frame();
  const auto far_max = state.bounds.center.x + state.bounds.extents.x;
  check(state.pool.Get() == cohort_identity && state.bounds_valid && far_max == 10010
            && far_bounds_reads > far_reads_before,
        "complete actual pool observations publish the distinct far renderer extent");
  far_clip = 100;
  navigation_environment::ApplyDrawDistance(camera_ptr, {0, 0, -30}, 100, &WriteClip);
  const auto far_requested_clip = far_clip;

  // Remove the far member in the same managed array. Unknown liveness elsewhere
  // prevents this fully traversed cohort from disproving the old far extent.
  cohort_pool.items[0]             = nullptr;
  env_alive_throw_object           = &unknown_flat;
  const auto cohort_unknown_before = unknown_alive_throws;
  for (unsigned i = 0; i < 64; ++i)
    frame();
  check(state.pool.Get() == cohort_identity && unknown_alive_throws == cohort_unknown_before + 1
            && state.bounds.center.x + state.bounds.extents.x == far_max,
        "a complete unknown cohort with far member removed retains the published far extent");
  env_alive_throw_object = nullptr;
  for (unsigned i = 0; i < 63; ++i)
    frame();
  check(state.bounds.center.x + state.bounds.extents.x == far_max,
        "a partially observed known cohort still cannot retire the old far extent");
  frame();
  check(state.pool.Get() == cohort_identity && state.bounds_valid
            && state.bounds.center.x - state.bounds.extents.x == -100
            && state.bounds.center.x + state.bounds.extents.x == 100,
        "completed known cohort after removal shrinks exactly to native background outside the pool");
  far_clip = 100;
  navigation_environment::ApplyDrawDistance(camera_ptr, {0, 0, -30}, 100, &WriteClip);
  check(far_requested_clip > 9000 && far_clip > 100 && far_clip < 1000,
        "cohort retirement reduces actual scenery clip requests while preserving native background reach");

  // Moving the same renderer inside background bounds must also retire its old
  // location without replacing the Flat, renderer, pool array or its length.
  cohort_pool.items[0] = reinterpret_cast<navigation_environment_binding::Flat*>(&far_flat);
  far_member_x         = 10000;
  for (unsigned i = 0; i < 64; ++i)
    frame();
  check(state.bounds.center.x + state.bounds.extents.x == 10010,
        "restored same-object far renderer publishes its far location again");
  far_member_x = 20;
  for (unsigned i = 0; i < 63; ++i)
    frame();
  check(state.bounds.center.x + state.bounds.extents.x == 10010,
        "partial refresh after same-renderer movement retains its last published far location");
  frame();
  check(state.pool.Get() == cohort_identity && state.bounds.center.x - state.bounds.extents.x == -100
            && state.bounds.center.x + state.bounds.extents.x == 100,
        "complete known refresh retires the moved renderer's old location with all identities unchanged");
  navigation_environment::Clear();
  check(warp_roots.empty(), "natural cohort cases release all retained roots");

  // Small pools retain the old refresh cadence while their work is spread out.
  {
    Array<navigation_environment_binding::Flat*, 32> compact_pool;
    compact_pool.items.fill(reinterpret_cast<navigation_environment_binding::Flat*>(&flat));
    seed();
    pool_value               = reinterpret_cast<il2cpp::Array<navigation_environment_binding::Flat*>*>(&compact_pool);
    const auto before_bounds = bounds_reads, before_pool = pool_reads;
    for (unsigned i = 0; i < 120; ++i)
      frame();
    check(bounds_reads - before_bounds == 4 * (32 + 1) && pool_reads - before_pool == 4 * 8,
          "a 32-slot scene completes four paced sweeps instead of oversampling every frame");
    navigation_environment::Clear();
  }

  // Hold generation across multiple native probes. Finding a late fallback must
  // not depend on either a small pool or a fast CPU completion.
  struct Gate {
    std::promise<void>       entered, release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<unsigned>    calls{0};
    std::atomic<bool>        opened{false};
    void                     Open()
    {
      if (!opened.exchange(true))
        release.set_value();
    }
  };
  auto gate = std::make_shared<Gate>();
  {
    struct Release {
      std::shared_ptr<Gate> gate;
      ~Release()
      { gate->Open(); }
    } release{gate};
    state.pixels = std::make_unique<navigation_environment_art::PixelGenerator>([gate](auto) {
      if (++gate->calls == 1)
        gate->entered.set_value();
      gate->released.wait();
      return std::vector<navigation_environment_art::Pixel>(1024 * 512);
    });
    seed();
    navigation_environment::Clear();
    bad_background = true;
    pool.items.fill(nullptr);
    pool.items[200] = reinterpret_cast<navigation_environment_binding::Flat*>(&flat);
    for (unsigned i = 0; i < 100 && !state.pixel_ticket; ++i)
      frame();
    check(state.pixel_ticket != 0 && state.pending_flat.Get() == &flat,
          "natural Update reaches a fallback at slot 200 across periodic source probes");
    check(gate->entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready,
          "the fallback CPU generation is actually blocked during the following observations");
    const auto pending_ticket = state.pixel_ticket;
    for (unsigned i = 0; i < 130; ++i)
      frame();
    check(state.pixel_ticket == pending_ticket && gate->calls == 1 && state.pending_flat.Get() == &flat,
          "periodic probes preserve the same blocked fallback generation and selected owner");
    navigation_environment::Clear();

    // A fully unusable large loader must finish its bounded search and yield.
    seed();
    navigation_environment::Clear();
    bad_background   = true;
    loaders.items[1] = &secondary_loader;
    pool.items.fill(nullptr);
    for (unsigned i = 0; i < 66; ++i)
      frame();
    check(!state.loader && state.search_layer < 0,
          "a complete unusable 257-slot search releases its loader across repeated probes");
    state.next_discovery = {};
    for (unsigned i = 0; i < 30 && state.loader.Get() != &secondary_loader; ++i)
      frame();
    check(state.loader.Get() == &secondary_loader,
          "the next paced discovery considers a second active loader with usable artwork");
    navigation_environment::Clear();

    seed();
    frame();
    const auto unknown_shell = state.shell.Get();
    renderer_known           = false;
    state.next_probe         = 0;
    frame();
    check(state.shell.Get() == unknown_shell && state.loader.Get() == &loader,
          "unavailable background renderer observation retains recoverable artwork and owner");
    renderer_known   = true;
    renderer_null    = true;
    state.next_probe = 0;
    frame();
    check(!state.shell && !state.loader && !state.background,
          "confirmed null background renderer retires stale artwork and its loader");
    loaders.items[1]     = &secondary_loader;
    state.next_probe     = 0;
    state.next_discovery = {};
    frame();
    check(state.loader.Get() == &secondary_loader, "confirmed renderer loss cannot hide another usable loader");
    navigation_environment::Clear();

    seed();
    frame();
    visual_dead_object = &renderer;
    state.next_probe   = 0;
    frame();
    check(!state.shell && !state.loader, "confirmed destroyed background renderer also retires cached artwork");
    visual_dead_object = nullptr;
    navigation_environment::Clear();
  }
  state.pixels.reset(); // The test joins only after releasing its blocked work, outside a game callback.
  check(warp_roots.empty(), "paced refresh, fallback and renderer-loss cases release every retained root");
  artwork_name = nullptr;

  std::cout << "Production environment efficiency/lifecycle: " << checks << " checks passed\n";
  return 0;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
