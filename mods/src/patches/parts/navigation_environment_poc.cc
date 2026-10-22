#include "patches/navigation_environment_poc.h"
#include "patches/navigation_environment_art.h"
#include "patches/navigation_environment_work.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>
#include <vector>

namespace navigation_environment_poc
{
namespace
{
  struct Root {
    Il2CppGCHandle handle = nullptr;
    bool           scoped = true;
    explicit Root(bool temporary = true)
        : scoped(temporary)
    {
    }
    Root(const Root &)            = delete;
    Root &operator=(const Root &) = delete;
    ~Root()
    {
      if (scoped)
        Reset();
    }
    Il2CppObject *Get() const
    { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
    void Reset(Il2CppObject *object = nullptr)
    {
      if (object && Get() == object)
        return;
      if (handle)
        il2cpp_gchandle_free(handle);
      handle = object ? il2cpp_gchandle_new(object, false) : nullptr;
    }
  };
  // Scene cleanup is explicit. Avoid calling into IL2CPP from static destructors
  // after Unity has already torn down its runtime during process exit.
  Root shell{false}, mesh{false}, material{false}, source{false}, shell_renderer{false};
  Root native_background{false};
  Root sky_texture{false}, owning_camera{false};
  Root source_loader{false};
  Root scenery_pool{false};
  Root loader_type_root{false}, filter_type_root{false}, renderer_type_root{false};
  Root discovery_loaders{false}, scan_loader{false}, scan_background{false}, scan_pool{false};
  Root candidate_flat{false}, candidate_texture{false}, candidate_renderer{false};
  Root candidate_background{false}, candidate_pool{false};
  navigation_environment_work::OrderedPool  artwork_order;
  navigation_environment_work::PixelWorker *pixel_worker     = nullptr;
  size_t                                    discovery_cursor = 0, discovery_count = 0;
  bool        scan_active = false, scan_cached = false, candidate_valid = false, candidate_is_background = false;
  size_t      candidate_index = 0, artwork_budget = 0;
  int         candidate_mask = 0, candidate_layer = -1, candidate_background_layer = -1;
  int         artwork_frame = -1, shell_layer = -1;
  std::string candidate_name, shell_name;
  std::chrono::steady_clock::time_point next_discovery{};
  struct Bounds {
    Vector3 center, extents;
  };
  Bounds            scenery_bounds{};
  Bounds            pending_bounds{};
  bool              bounds_valid    = false;
  bool              pending_bounds_valid = false;
  Il2CppObject     *scenery_owner = nullptr;
  size_t            scenery_cursor = 0, scenery_count = 0;
  int               scenery_mask = 0, pending_mask = 0, next_bounds_scan = 0, last_bounds_frame = -1;
  bool              reported_bounds = false;
  bool              attempted = false, ready = false, failed = false;
  int               next_scan = 0;
  const MethodInfo *destroy, *alive, *find_all, *active, *background, *shared_material, *main_texture;
  const MethodInfo *get_transform, *game_transform, *get_game_object, *get_layer, *set_layer, *get_position,
      *set_position, *set_scale;
  const MethodInfo *set_parent, *set_angles;
  const MethodInfo *far_clip, *near_clip, *culling_mask, *game_ctor, *add_component, *mesh_ctor, *material_ctor;
  const MethodInfo *set_vertices, *set_uv, *set_triangles, *set_mesh, *set_material, *shader_find, *set_texture;
  const MethodInfo *set_queue, *shader_supported, *renderer_visible, *get_bounds, *recalculate_bounds;
  const MethodInfo *set_shadows, *receive_shadows;
  const MethodInfo *texture_width, *texture_height, *object_name, *renderer_enabled, *active_in_hierarchy;
  const MethodInfo *texture_ctor, *set_pixels, *texture_apply, *wrap_u, *wrap_v, *set_far_clip;
  Il2CppClass      *game_class, *mesh_class, *material_class, *vector3_class, *vector2_class, *int_class;
  Il2CppClass      *loader_class, *object_class, *flat_class;
  Il2CppClass      *texture_class, *color32_class;
  void             *loader_type, *filter_type, *renderer_type;
  FieldInfo        *renderer_field, *pool_field;
  int (*frame_count)() = nullptr;

  const MethodInfo *Method(const char *type, const char *name, bool is_static, const char *result,
                           std::initializer_list<const char *> args = {})
  {
    auto cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", type);
    return method_contract::Resolve(cls.get_cls(), name, is_static, result, args);
  }

  bool Init()
  {
    if (attempted)
      return ready;
    attempted           = true;
    destroy             = Method("Object", "Destroy", true, "System.Void", {"UnityEngine.Object"});
    alive               = Method("Object", "op_Implicit", true, "System.Boolean", {"UnityEngine.Object"});
    find_all            = Method("Object", "FindObjectsOfType", true, "UnityEngine.Object[]", {"System.Type"});
    active              = Method("Behaviour", "get_isActiveAndEnabled", false, "System.Boolean");
    shared_material     = Method("Renderer", "get_sharedMaterial", false, "UnityEngine.Material");
    texture_width       = Method("Texture", "get_width", false, "System.Int32");
    texture_height      = Method("Texture", "get_height", false, "System.Int32");
    object_name         = Method("Object", "get_name", false, "System.String");
    renderer_enabled    = Method("Renderer", "get_enabled", false, "System.Boolean");
    active_in_hierarchy = Method("GameObject", "get_activeInHierarchy", false, "System.Boolean");
    main_texture        = Method("Material", "get_mainTexture", false, "UnityEngine.Texture");
    get_transform       = Method("Component", "get_transform", false, "UnityEngine.Transform");
    game_transform      = Method("GameObject", "get_transform", false, "UnityEngine.Transform");
    get_game_object     = Method("Component", "get_gameObject", false, "UnityEngine.GameObject");
    get_layer           = Method("GameObject", "get_layer", false, "System.Int32");
    set_layer           = Method("GameObject", "set_layer", false, "System.Void", {"System.Int32"});
    get_position        = Method("Transform", "get_position", false, "UnityEngine.Vector3");
    set_position        = Method("Transform", "set_position", false, "System.Void", {"UnityEngine.Vector3"});
    set_scale           = Method("Transform", "set_localScale", false, "System.Void", {"UnityEngine.Vector3"});
    set_parent    = Method("Transform", "SetParent", false, "System.Void", {"UnityEngine.Transform", "System.Boolean"});
    set_angles    = Method("Transform", "set_eulerAngles", false, "System.Void", {"UnityEngine.Vector3"});
    far_clip      = Method("Camera", "get_farClipPlane", false, "System.Single");
    set_far_clip  = Method("Camera", "set_farClipPlane", false, "System.Void", {"System.Single"});
    near_clip     = Method("Camera", "get_nearClipPlane", false, "System.Single");
    culling_mask  = Method("Camera", "get_cullingMask", false, "System.Int32");
    game_ctor     = Method("GameObject", ".ctor", false, "System.Void", {"System.String"});
    add_component = Method("GameObject", "AddComponent", false, "UnityEngine.Component", {"System.Type"});
    mesh_ctor     = Method("Mesh", ".ctor", false, "System.Void");
    material_ctor = Method("Material", ".ctor", false, "System.Void", {"UnityEngine.Shader"});
    set_vertices  = Method("Mesh", "set_vertices", false, "System.Void", {"UnityEngine.Vector3[]"});
    set_uv        = Method("Mesh", "set_uv", false, "System.Void", {"UnityEngine.Vector2[]"});
    set_triangles = Method("Mesh", "set_triangles", false, "System.Void", {"System.Int32[]"});
    set_mesh      = Method("MeshFilter", "set_sharedMesh", false, "System.Void", {"UnityEngine.Mesh"});
    set_material  = Method("Renderer", "set_sharedMaterial", false, "System.Void", {"UnityEngine.Material"});
    shader_find   = Method("Shader", "Find", true, "UnityEngine.Shader", {"System.String"});
    set_texture   = Method("Material", "set_mainTexture", false, "System.Void", {"UnityEngine.Texture"});
    set_queue     = Method("Material", "set_renderQueue", false, "System.Void", {"System.Int32"});
    shader_supported   = Method("Shader", "get_isSupported", false, "System.Boolean");
    renderer_visible   = Method("Renderer", "get_isVisible", false, "System.Boolean");
    get_bounds         = Method("Renderer", "get_bounds", false, "UnityEngine.Bounds");
    recalculate_bounds = Method("Mesh", "RecalculateBounds", false, "System.Void");
    set_shadows =
        Method("Renderer", "set_shadowCastingMode", false, "System.Void", {"UnityEngine.Rendering.ShadowCastingMode"});
    receive_shadows = Method("Renderer", "set_receiveShadows", false, "System.Void", {"System.Boolean"});
    auto loader     = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderableLoader");
    auto flat       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable");
    flat_class      = flat.get_cls();
    pool_field      = loader.get_cls() ? il2cpp_class_get_field_from_name(loader.get_cls(), "_pool") : nullptr;
    if (pool_field
        && ((il2cpp_field_get_flags(pool_field) & FIELD_ATTRIBUTE_STATIC)
            || !method_contract::Type(pool_field->type, "Digit.Client.Rendering.FlatRenderable[]")))
      pool_field = nullptr;
    background     = method_contract::Resolve(loader.get_cls(), "get_Background", false,
                                              "Digit.Client.Rendering.FlatRenderable", {});
    renderer_field = flat.get_cls() ? il2cpp_class_get_field_from_name(flat.get_cls(), "MeshRenderer") : nullptr;
    if (renderer_field
        && ((il2cpp_field_get_flags(renderer_field) & FIELD_ATTRIBUTE_STATIC)
            || !method_contract::Type(renderer_field->type, "UnityEngine.MeshRenderer")))
      renderer_field = nullptr;
    loader_type    = loader.isValidHelper() ? loader.GetType() : nullptr;
    loader_type_root.Reset(static_cast<Il2CppObject *>(loader_type));
    loader_type    = loader_type_root.Get();
    loader_class   = loader.get_cls();
    object_class   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object").get_cls();
    auto filter    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshFilter");
    auto renderer  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshRenderer");
    filter_type    = filter.isValidHelper() ? filter.GetType() : nullptr;
    filter_type_root.Reset(static_cast<Il2CppObject *>(filter_type));
    filter_type    = filter_type_root.Get();
    renderer_type  = renderer.isValidHelper() ? renderer.GetType() : nullptr;
    renderer_type_root.Reset(static_cast<Il2CppObject *>(renderer_type));
    renderer_type  = renderer_type_root.Get();
    game_class     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject").get_cls();
    mesh_class     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Mesh").get_cls();
    material_class = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Material").get_cls();
    vector3_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Vector3").get_cls();
    vector2_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Vector2").get_cls();
    int_class      = il2cpp_get_class_helper("mscorlib", "System", "Int32").get_cls();
    texture_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D").get_cls();
    color32_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Color32").get_cls();
    texture_ctor   = Method("Texture2D", ".ctor", false, "System.Void",
                            {"System.Int32", "System.Int32", "UnityEngine.TextureFormat", "System.Boolean"});
    set_pixels     = Method("Texture2D", "SetPixels32", false, "System.Void", {"UnityEngine.Color32[]"});
    texture_apply  = Method("Texture2D", "Apply", false, "System.Void", {"System.Boolean", "System.Boolean"});
    wrap_u         = Method("Texture", "set_wrapModeU", false, "System.Void", {"UnityEngine.TextureWrapMode"});
    wrap_v         = Method("Texture", "set_wrapModeV", false, "System.Void", {"UnityEngine.TextureWrapMode"});
    frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    ready = set_parent && set_angles && destroy && alive && find_all && active && background && shared_material
            && main_texture && get_transform && game_transform && get_game_object && get_layer && set_layer
            && get_position && set_position && set_scale && far_clip && near_clip && culling_mask && game_ctor
            && add_component && mesh_ctor && material_ctor && set_vertices && set_uv && set_triangles && set_mesh
            && set_material && shader_find && set_texture && loader_type && filter_type && renderer_type
            && renderer_field && game_class && mesh_class && material_class && vector3_class && vector2_class
            && int_class && frame_count && texture_width && texture_height && renderer_enabled && set_queue
            && shader_supported && flat_class && recalculate_bounds && set_shadows && receive_shadows && texture_class
            && color32_class && texture_ctor && set_pixels && texture_apply && wrap_u && wrap_v && active_in_hierarchy
            && set_far_clip && get_bounds;
    spdlog::info(
        "[SystemEnvironmentPoc] runtime API ready={} recalc={} shader-support={} cast-shadows={} receive-shadows={}",
        ready, recalculate_bounds != nullptr, shader_supported != nullptr, set_shadows != nullptr,
        receive_shadows != nullptr);
    return ready;
  }

  bool Live(Il2CppObject *object)
  {
    if (!object)
      return false;
    Root retained;
    retained.Reset(object);
    if (!retained.Get())
      return false;
    void         *args[]{retained.Get()};
    Il2CppObject *boxed = nullptr;
    bool          value = false;
    return Il2CppRuntime::TryInvoke(alive, nullptr, args, &boxed) && Il2CppRuntime::TryBoolean(boxed, value) && value;
  }

  template <typename T> bool Value(const MethodInfo *method, Il2CppObject *object, const char *type, T &value)
  {
    Root retained;
    retained.Reset(object);
    Il2CppObject *boxed = nullptr;
    if (!retained.Get() || !Il2CppRuntime::TryInvoke(method, retained.Get(), nullptr, &boxed) || !boxed
        || !method_contract::Type(il2cpp_class_get_type(boxed->klass), type))
      return false;
    auto *data = static_cast<T *>(il2cpp_object_unbox(boxed));
    if (!data)
      return false;
    value = *data;
    return true;
  }

  void Destroy(Root &object)
  {
    auto *target = object.Get();
    if (Live(target)) {
      void *args[]{target};
      Il2CppRuntime::TryInvoke(destroy, nullptr, args);
    }
    object.Reset();
  }

  void Fail(const char *stage)
  {
    Clear();
    failed = true;
    spdlog::warn("[SystemEnvironmentPoc] unavailable at {}; leaving native scenery and free camera intact", stage);
  }

  template <typename T> bool Array(const std::vector<T> &values, Il2CppClass *cls, const MethodInfo *setter)
  {
    uint32_t alignment = 0;
    if (!cls || il2cpp_class_value_size(cls, &alignment) != sizeof(T))
      return false;
    auto *array = il2cpp_array_new(cls, values.size());
    if (!array)
      return false;
    Root root;
    root.Reset(reinterpret_cast<Il2CppObject *>(array));
    if (!root.Get())
      return false;
    std::memcpy(il2cpp_array_addr_with_size(array, 0, sizeof(T)), values.data(), values.size() * sizeof(T));
    void *args[]{array};
    return Il2CppRuntime::TryInvoke(setter, mesh.Get(), args);
  }

  navigation_environment_work::PixelWorker::Result PreparePurePixels(const std::string &name)
  {
    try {
      if (!pixel_worker)
        pixel_worker = new navigation_environment_work::PixelWorker;
      return pixel_worker->Prepare(name);
    } catch (...) {
      return {{}, true};
    }
  }

  bool CreateSkyTexture(const navigation_environment_work::Pixels &pixels, int64_t generated_ms)
  {
    const auto started = std::chrono::steady_clock::now();
    int        width = navigation_environment_work::kWidth, height = navigation_environment_work::kHeight;
    uint32_t alignment = 0;
    if (pixels.size() != navigation_environment_work::kPixels
        || il2cpp_class_value_size(color32_class, &alignment) != sizeof(navigation_environment_art::Pixel))
      return false;
    sky_texture.Reset(il2cpp_object_new(texture_class));
    int   format  = 4; // Unity TextureFormat.RGBA32, verified against the client dump.
    bool  mipmaps = false;
    void *ctor_args[]{&width, &height, &format, &mipmaps};
    if (!sky_texture.Get() || !Il2CppRuntime::TryInvoke(texture_ctor, sky_texture.Get(), ctor_args))
      return false;
    auto *array = il2cpp_array_new(color32_class, pixels.size());
    if (!array)
      return false;
    Root pixel_root;
    pixel_root.Reset(reinterpret_cast<Il2CppObject *>(array));
    if (!pixel_root.Get())
      return false;
    std::memcpy(il2cpp_array_addr_with_size(array, 0, sizeof(navigation_environment_art::Pixel)), pixels.data(),
                pixels.size() * sizeof(navigation_environment_art::Pixel));
    bool  unreadable = true;
    int   repeat = 0, clamp = 1;
    void *pixel_args[]{array}, *apply_args[]{&mipmaps, &unreadable}, *u_args[]{&repeat}, *v_args[]{&clamp};
    if (!Il2CppRuntime::TryInvoke(set_pixels, sky_texture.Get(), pixel_args)
        || !Il2CppRuntime::TryInvoke(wrap_u, sky_texture.Get(), u_args)
        || !Il2CppRuntime::TryInvoke(wrap_v, sky_texture.Get(), v_args)
        || !Il2CppRuntime::TryInvoke(texture_apply, sky_texture.Get(), apply_args))
      return false;
    int actual_width = 0, actual_height = 0;
    if (!Value(texture_width, sky_texture.Get(), "System.Int32", actual_width)
        || !Value(texture_height, sky_texture.Get(), "System.Int32", actual_height) || actual_width != width
        || actual_height != height)
      return false;
    spdlog::info(
        "[SystemEnvironmentPoc] custom directional sky={}x{} generated-worker={}ms uploaded-owner={}ms; native artwork "
        "preserved",
        width, height, generated_ms,
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return true;
  }

  bool Create(Il2CppObject *texture, int layer, Il2CppObject *native_renderer,
              const navigation_environment_work::Pixels &pixels, int64_t generated_ms)
  {
    // A full panorama is authored by direction, not stitched from a finite
    // system painting. Keep the original ship/portal/nebula meshes visible.
    constexpr int    longitude = 64, latitude = 32;
    constexpr double pi = 3.14159265358979323846;
    struct UV {
      float x, y;
    };
    std::vector<Vector3> vertices;
    std::vector<UV>      uv;
    std::vector<int>     indices;
    for (int y = 0; y <= latitude; ++y) {
      const double theta = pi * y / latitude;
      for (int x = 0; x <= longitude; ++x) {
        const double phi = 2.0 * pi * x / longitude;
        vertices.push_back(
            {float(std::sin(theta) * std::cos(phi)), float(std::cos(theta)), float(std::sin(theta) * std::sin(phi))});
        uv.push_back({float(x) / longitude, float(y) / latitude});
      }
    }
    for (int y = 0; y < latitude; ++y) {
      for (int x = 0; x < longitude; ++x) {
        const int a = y * (longitude + 1) + x, b = a + longitude + 1;
        if (y != 0)
          indices.insert(indices.end(), {a, b, a + 1});
        if (y != latitude - 1)
          indices.insert(indices.end(), {a + 1, b, b + 1});
      }
    }
    shell.Reset(il2cpp_object_new(game_class));
    mesh.Reset(il2cpp_object_new(mesh_class));
    material.Reset(il2cpp_object_new(material_class));
    if (!shell.Get() || !mesh.Get() || !material.Get())
      return false;
    Root name_root, shader_name_root;
    name_root.Reset(reinterpret_cast<Il2CppObject *>(il2cpp_string_new("CommunitySystemEnvironmentPoc")));
    shader_name_root.Reset(reinterpret_cast<Il2CppObject *>(il2cpp_string_new("Unlit/Texture")));
    if (!name_root.Get() || !shader_name_root.Get())
      return false;
    void         *name_args[]{name_root.Get()};
    void         *shader_args[]{shader_name_root.Get()};
    Il2CppObject *shader = nullptr, *filter = nullptr, *renderer = nullptr;
    if (!Il2CppRuntime::TryInvoke(game_ctor, shell.Get(), name_args)
        || !Il2CppRuntime::TryInvoke(mesh_ctor, mesh.Get(), nullptr)) {
      spdlog::warn("[SystemEnvironmentPoc] GameObject/Mesh construction failed");
      return false;
    }
    Il2CppRuntime::TryInvoke(shader_find, nullptr, shader_args, &shader);
    // Texture upload allocates a sizeable managed pixel array. Keep the shader
    // wrapper rooted across that allocation and the following component calls.
    Root shader_root;
    shader_root.Reset(shader);
    bool supported = false;
    if (!shader_root.Get() || !Live(shader_root.Get())
        || !Value(shader_supported, shader_root.Get(), "System.Boolean", supported) || !supported) {
      spdlog::warn("[SystemEnvironmentPoc] object/mesh/shader setup failed (shader-present={})", Live(shader));
      return false;
    }
    if (!CreateSkyTexture(pixels, generated_ms))
      return false;
    void *material_args[]{shader_root.Get()}, *texture_args[]{sky_texture.Get()}, *layer_args[]{&layer};
    void *filter_args[]{filter_type}, *renderer_args[]{renderer_type};
    if (!Il2CppRuntime::TryInvoke(material_ctor, material.Get(), material_args)
        || !Il2CppRuntime::TryInvoke(set_texture, material.Get(), texture_args)
        || !Il2CppRuntime::TryInvoke(set_layer, shell.Get(), layer_args)
        || !Array(vertices, vector3_class, set_vertices) || !Array(uv, vector2_class, set_uv)
        || !Array(indices, int_class, set_triangles)
        || !Il2CppRuntime::TryInvoke(add_component, shell.Get(), filter_args, &filter))
      return false;
    Root filter_root;
    filter_root.Reset(filter);
    if (!filter_root.Get() || !Live(filter_root.Get())
        || !Il2CppRuntime::TryInvoke(add_component, shell.Get(), renderer_args, &renderer))
      return false;
    Root renderer_root;
    renderer_root.Reset(renderer);
    if (!renderer_root.Get() || !Live(renderer_root.Get()))
      return false;
    // This shader ignores source alpha and has no UI clipping/stencil state.
    // It writes depth: draw the enclosure first, near the far plane, so closer
    // native scenery and gameplay geometry still pass their normal depth tests.
    int   queue = 1000;
    void *queue_args[]{&queue};
    if (!Il2CppRuntime::TryInvoke(set_queue, material.Get(), queue_args))
      return false;
    void *mesh_args[]{mesh.Get()}, *render_args[]{material.Get()};
    if (!Il2CppRuntime::TryInvoke(set_mesh, filter, mesh_args)
        || !Il2CppRuntime::TryInvoke(set_material, renderer, render_args))
      return false;
    int   shadows = 0;
    bool  off     = false;
    void *shadows_args[]{&shadows}, *off_args[]{&off};
    if (!Il2CppRuntime::TryInvoke(recalculate_bounds, mesh.Get(), nullptr)
        || !Il2CppRuntime::TryInvoke(set_shadows, renderer, shadows_args)
        || !Il2CppRuntime::TryInvoke(receive_shadows, renderer, off_args))
      return false;
    native_background.Reset(native_renderer);
    shell_renderer.Reset(renderer);
    source.Reset(texture);
    shell_layer  = layer;
    bool visible = false, enabled = false;
    Value(renderer_enabled, renderer, "System.Boolean", enabled);
    Value(renderer_visible, renderer, "System.Boolean", visible);
    spdlog::info("[SystemEnvironmentPoc] ambient enclosure layer={} vertices={} triangles={} "
                 "shader=Unlit/Texture supported={} renderer-enabled={} initial-visible={}",
                 layer, vertices.size(), indices.size() / 3, supported, enabled, visible);
    return source.Get() != nullptr && native_background.Get() != nullptr;
  }

  std::string Name(Il2CppObject *object)
  {
    Root object_root;
    object_root.Reset(object);
    Il2CppObject *result = nullptr;
    if (!object_root.Get() || !object_name
        || !Il2CppRuntime::TryInvoke(object_name, object_root.Get(), nullptr, &result) || !result
        || !method_contract::Type(il2cpp_class_get_type(result->klass), "System.String"))
      return "SystemAmbient";
    Root name_root;
    name_root.Reset(result);
    auto *name = reinterpret_cast<Il2CppString *>(name_root.Get());
    if (!name || name->length < 0 || name->length > 256)
      return "SystemAmbient";
    auto text = to_string(name);
    return text.size() <= 256 ? text : "SystemAmbient";
  }

  bool LayerOfFlat(Il2CppObject *flat, int mask, int &layer, Root &renderer)
  {
    Root flat_root;
    flat_root.Reset(flat);
    if (!flat_root.Get() || !Live(flat_root.Get()) || !il2cpp_class_is_assignable_from(flat_class, flat->klass))
      return false;
    Il2CppObject *native_renderer = nullptr, *object = nullptr;
    il2cpp_field_get_value(flat_root.Get(), renderer_field, &native_renderer);
    renderer.Reset(native_renderer);
    Root object_root;
    bool enabled = false;
    if (!renderer.Get() || !Live(renderer.Get()) || !Value(renderer_enabled, renderer.Get(), "System.Boolean", enabled)
        || !enabled || !Il2CppRuntime::TryInvoke(get_game_object, renderer.Get(), nullptr, &object))
      return false;
    object_root.Reset(object);
    return object_root.Get() && Live(object_root.Get()) && Value(get_layer, object_root.Get(), "System.Int32", layer)
           && layer >= 0 && layer <= 31 && (static_cast<unsigned>(mask) & (1u << layer));
  }

  bool Artwork(Il2CppObject *flat, int mask, int required_layer, Root &texture, int &layer, Root &renderer)
  {
    texture.Reset();
    renderer.Reset();
    if (!LayerOfFlat(flat, mask, layer, renderer) || (required_layer >= 0 && layer != required_layer))
      return false;
    Il2CppObject *native_material = nullptr, *native_texture = nullptr;
    if (!Il2CppRuntime::TryInvoke(shared_material, renderer.Get(), nullptr, &native_material))
      return false;
    Root material_root;
    material_root.Reset(native_material);
    if (!material_root.Get() || !Live(material_root.Get())
        || !Il2CppRuntime::TryInvoke(main_texture, material_root.Get(), nullptr, &native_texture))
      return false;
    texture.Reset(native_texture);
    int  width = 0, height = 0;
    if (!texture.Get() || !Live(texture.Get()) || !Value(texture_width, texture.Get(), "System.Int32", width)
        || !Value(texture_height, texture.Get(), "System.Int32", height) || width < 64 || height < 64)
      return false;
    // Reject Unity's tiny white/black placeholder textures and lookup strips.
    if (texture.Get() != source.Get())
      spdlog::info("[SystemEnvironmentPoc] artwork material={} texture={} dimensions={}x{} layer={}",
                   Name(material_root.Get()), Name(texture.Get()), width, height, layer);
    return true;
  }

  void IncludeScenery(Il2CppObject *renderer, int mask, Bounds &combined, bool &valid)
  {
    Root renderer_root;
    renderer_root.Reset(renderer);
    if (!renderer_root.Get())
      return;
    Bounds        bounds{};
    Il2CppObject *object  = nullptr;
    bool          enabled = false, visible_object = false;
    int           layer = -1;
    if (!Live(renderer_root.Get()) || !Value(renderer_enabled, renderer_root.Get(), "System.Boolean", enabled)
        || !enabled || !Il2CppRuntime::TryInvoke(get_game_object, renderer_root.Get(), nullptr, &object))
      return;
    Root object_root;
    object_root.Reset(object);
    if (!object_root.Get() || !Live(object_root.Get())
        || !Value(active_in_hierarchy, object_root.Get(), "System.Boolean", visible_object) || !visible_object
        || !Value(get_layer, object_root.Get(), "System.Int32", layer) || layer < 0 || layer > 31
        || !(static_cast<unsigned>(mask) & (1u << layer)) || !Value(get_bounds, renderer, "UnityEngine.Bounds", bounds))
      return;
    const auto &c = bounds.center;
    const auto &e = bounds.extents;
    if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || !std::isfinite(e.x) || !std::isfinite(e.y)
        || !std::isfinite(e.z) || e.x < 0 || e.y < 0 || e.z < 0)
      return;
    if (!valid) {
      combined = bounds;
      valid    = true;
      return;
    }
    const auto merge = [](float &center, float &extent, float next_center, float next_extent) {
      const double low  = std::min(double(center) - extent, double(next_center) - next_extent);
      const double high = std::max(double(center) + extent, double(next_center) + next_extent);
      center            = float((low + high) * 0.5);
      extent            = float((high - low) * 0.5);
    };
    merge(combined.center.x, combined.extents.x, c.x, e.x);
    merge(combined.center.y, combined.extents.y, c.y, e.y);
    merge(combined.center.z, combined.extents.z, c.z, e.z);
  }

  void CaptureSceneryBounds(int frame)
  {
    auto *loader = source_loader.Get();
    if (!loader || frame == last_bounds_frame)
      return;
    last_bounds_frame = frame;
    Il2CppArray *pool = nullptr;
    if (pool_field)
      il2cpp_field_get_value(loader, pool_field, &pool);
    if (!pool || !pool->klass || pool->klass->rank != 1 || !pool->klass->element_class
        || !il2cpp_class_is_assignable_from(flat_class, pool->klass->element_class) || pool->max_length > 128)
      pool = nullptr;
    const size_t count = pool ? pool->max_length : 0;
    // Pool replacement and camera-layer changes start a fresh pass immediately.
    // Re-read each slot during the pass: streaming can mutate a pool in place.
    if (scenery_owner != loader || scenery_pool.Get() != reinterpret_cast<Il2CppObject *>(pool)
        || scenery_count != count || pending_mask != scenery_mask) {
      scenery_pool.Reset(reinterpret_cast<Il2CppObject *>(pool));
      scenery_owner     = loader;
      scenery_count     = count;
      scenery_cursor    = 0;
      pending_mask      = scenery_mask;
      next_bounds_scan  = frame;
    }
    if (pool && !scenery_pool.Get())
      return;
    Root pool_root;
    pool_root.Reset(reinterpret_cast<Il2CppObject *>(pool));
    if (pool && !pool_root.Get())
      return;
    if (!scenery_cursor) {
      if (frame < next_bounds_scan)
        return;
      next_bounds_scan     = frame + 30;
      pending_bounds_valid = false;
      IncludeScenery(native_background.Get(), pending_mask, pending_bounds, pending_bounds_valid);
    }
    // A large backdrop previously queried up to 128 renderers on one frame.
    // Five entries per frame finish the same maximum pool within 26 frames,
    // while still observing moving bounds, visibility and streamed renderers.
    constexpr size_t budget = 5;
    const size_t end = std::min(scenery_count, scenery_cursor + budget);
    for (; scenery_cursor < end; ++scenery_cursor) {
      auto         *flat = *reinterpret_cast<Il2CppObject **>(
          il2cpp_array_addr_with_size(pool, scenery_cursor, sizeof(void *)));
      Il2CppObject *renderer = nullptr;
      Root          flat_root;
      flat_root.Reset(flat);
      if (flat_root.Get() && Live(flat_root.Get())
          && il2cpp_class_is_assignable_from(flat_class, flat_root.Get()->klass)) {
        il2cpp_field_get_value(flat_root.Get(), renderer_field, &renderer);
        IncludeScenery(renderer, pending_mask, pending_bounds, pending_bounds_valid);
      }
    }
    if (scenery_cursor == scenery_count) {
      // Keep the last complete bounds throughout a refresh. A partial union
      // would temporarily shrink draw distance and clip unsampled scenery.
      scenery_bounds = pending_bounds;
      bounds_valid   = pending_bounds_valid;
      scenery_cursor = 0;
    }
  }

  void ResetScan()
  {
    discovery_loaders.Reset();
    scan_loader.Reset();
    scan_background.Reset();
    scan_pool.Reset();
    discovery_cursor = discovery_count = 0;
    scan_active = scan_cached = false;
    artwork_order.Reset();
  }

  void ResetCandidate()
  {
    candidate_flat.Reset();
    candidate_texture.Reset();
    candidate_renderer.Reset();
    candidate_background.Reset();
    candidate_pool.Reset();
    candidate_valid = false;
    candidate_name.clear();
    if (pixel_worker)
      pixel_worker->Cancel();
  }

  bool ReadPool(Il2CppObject *loader, Root &retained, size_t &count)
  {
    Il2CppArray *pool = nullptr;
    if (pool_field)
      il2cpp_field_get_value(loader, pool_field, &pool);
    retained.Reset(reinterpret_cast<Il2CppObject *>(pool));
    if (!retained.Get() || !pool->klass || pool->klass->rank != 1 || !pool->klass->element_class
        || !il2cpp_class_is_assignable_from(flat_class, pool->klass->element_class) || pool->max_length > 128) {
      retained.Reset();
      count = 0;
      return false;
    }
    count = pool->max_length;
    return true;
  }

  bool ReadBackground(Il2CppObject *loader, Root &retained)
  {
    Il2CppObject *flat    = nullptr;
    const bool    invoked = Il2CppRuntime::TryInvoke(background, loader, nullptr, &flat);
    retained.Reset(flat);
    return invoked && retained.Get() && Live(retained.Get());
  }

  void NextOwner()
  {
    ++discovery_cursor;
    scan_loader.Reset();
    scan_background.Reset();
    scan_pool.Reset();
    artwork_order.Reset();
  }

  bool BeginScan(Il2CppObject *camera)
  {
    scan_cached = source_loader.Get() != nullptr;
    if (scan_cached) {
      discovery_count = 1;
    } else {
      const auto now = std::chrono::steady_clock::now();
      if (now < next_discovery)
        return false;
      next_discovery = now + std::chrono::seconds(5);
      void         *args[]{loader_type};
      Il2CppObject *result = nullptr;
      if (!Il2CppRuntime::TryInvoke(find_all, nullptr, args, &result)) {
        Fail("loader-discovery");
        return false;
      }
      discovery_loaders.Reset(result);
      if (!discovery_loaders.Get() || !result->klass || result->klass->rank != 1 || !result->klass->element_class
          || !object_class || !il2cpp_class_is_assignable_from(object_class, result->klass->element_class)) {
        Fail("loader-discovery");
        return false;
      }
      const auto count = reinterpret_cast<Il2CppArray *>(result)->max_length;
      if (count > 64) {
        Fail("loader-count");
        return false;
      }
      discovery_count = count;
    }
    discovery_cursor = 0;
    scan_active      = true;
    owning_camera.Reset(camera);
    if (!owning_camera.Get()) {
      Fail("camera-root");
      return false;
    }
    return true;
  }

  void SelectCandidate(Il2CppObject *camera, Root &loader, Root &flat, Root &texture, Root &renderer, Root &backdrop,
                       Root &pool, int mask, int layer, int backdrop_layer,
                       navigation_environment_work::PoolChoice choice, int frame)
  {
    // Locals retain every discovery object across Clear and any allocating name
    // query. Cold discovery can now release its array once a winner is qualified.
    const auto name       = Name(texture.Get());
    const bool same_owner = source_loader.Get() == loader.Get();
    if (source_loader.Get() && !same_owner)
      Clear();
    const bool unchanged = same_owner && texture.Get() == source.Get() && renderer.Get() == native_background.Get()
                           && shell_layer == layer && shell_name == name && Live(shell.Get());
    ResetScan();
    next_scan = frame + 30;
    if (unchanged) {
      ResetCandidate();
      return;
    }
    source_loader.Reset(loader.Get());
    candidate_flat.Reset(flat.Get());
    candidate_texture.Reset(texture.Get());
    candidate_renderer.Reset(renderer.Get());
    candidate_background.Reset(backdrop.Get());
    candidate_pool.Reset(pool.Get());
    owning_camera.Reset(camera);
    if (!source_loader.Get() || !candidate_flat.Get() || !candidate_texture.Get() || !candidate_renderer.Get()
        || !candidate_background.Get() || !owning_camera.Get() || (pool.Get() && !candidate_pool.Get())) {
      Fail("candidate-root");
      return;
    }
    candidate_mask             = mask;
    candidate_layer            = layer;
    candidate_background_layer = backdrop_layer;
    candidate_is_background    = choice.background;
    candidate_index            = choice.index;
    candidate_name             = name;
    candidate_valid            = true;
  }

  void Scan(Il2CppObject *camera, int frame)
  {
    if (!scan_active && !BeginScan(camera))
      return;
    int mask = 0;
    if (!Value(culling_mask, camera, "System.Int32", mask)) {
      Fail("camera-mask");
      return;
    }
    scenery_mask = mask;
    size_t owner_steps = 0;
    while (discovery_cursor < discovery_count && artwork_budget && owner_steps++ < 5) {
      if (!scan_loader.Get()) {
        auto *array  = reinterpret_cast<Il2CppArray *>(discovery_loaders.Get());
        auto *loader = scan_cached ? source_loader.Get()
                       : array     ? *reinterpret_cast<Il2CppObject **>(
                                         il2cpp_array_addr_with_size(array, discovery_cursor, sizeof(void *)))
                                   : nullptr;
        scan_loader.Reset(loader);
      }
      Root loader, backdrop, pool, backdrop_renderer;
      loader.Reset(scan_loader.Get());
      bool enabled        = false;
      int  backdrop_layer = -1;
      if (!loader.Get() || !Live(loader.Get()) || !loader_class
          || !il2cpp_class_is_assignable_from(loader_class, loader.Get()->klass)
          || !Value(active, loader.Get(), "System.Boolean", enabled) || !enabled
          || !ReadBackground(loader.Get(), backdrop)
          || !LayerOfFlat(backdrop.Get(), mask, backdrop_layer, backdrop_renderer)) {
        NextOwner();
        continue;
      }
      size_t count = 0;
      ReadPool(loader.Get(), pool, count);
      scan_background.Reset(backdrop.Get());
      scan_pool.Reset(pool.Get());
      if (!scan_background.Get() || (pool.Get() && !scan_pool.Get())) {
        Fail("pool-root");
        return;
      }
      Root                                            selected_flat, selected_texture, selected_renderer;
      int                                             selected_layer = -1;
      const navigation_environment_work::PoolIdentity identity{loader.Get(), pool.Get(), backdrop.Get(),
                                                               count,        mask,       backdrop_layer};
      const auto choice = artwork_order.Advance(identity, artwork_budget, [&](bool is_background, size_t index) {
        auto *array = reinterpret_cast<Il2CppArray *>(pool.Get());
        auto *flat =
            is_background
                ? backdrop.Get()
                : *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, index, sizeof(void *)));
        selected_flat.Reset(flat);
        return selected_flat.Get()
               && Artwork(selected_flat.Get(), mask, is_background ? -1 : backdrop_layer, selected_texture,
                          selected_layer, selected_renderer);
      });
      if (choice.state == navigation_environment_work::PoolChoice::matched) {
        SelectCandidate(camera, loader, selected_flat, selected_texture, selected_renderer, backdrop, pool, mask,
                        selected_layer, backdrop_layer, choice, frame);
        return;
      }
      if (choice.state == navigation_environment_work::PoolChoice::pending)
        return;
      NextOwner();
    }
    if (discovery_cursor == discovery_count) {
      // A complete unsuccessful pass must not retain another system's artwork.
      Clear();
      next_scan = frame + 30;
    }
  }

  void PublishReady(Il2CppObject *camera, int frame)
  {
    if (!candidate_valid)
      return;
    const auto prepared = PreparePurePixels(candidate_name);
    if (prepared.failed) {
      Fail("sky-generation");
      return;
    }
    if (!prepared.pixels || !artwork_budget)
      return;
    Root loader, backdrop, pool, backdrop_renderer;
    loader.Reset(source_loader.Get());
    int        mask = 0, backdrop_layer = -1;
    bool       enabled = false;
    size_t     count   = 0;
    const auto stale   = [&] {
      ResetCandidate();
      next_scan = frame;
    };
    if (!loader.Get() || !Live(loader.Get()) || !Value(active, loader.Get(), "System.Boolean", enabled) || !enabled
        || !Value(culling_mask, camera, "System.Int32", mask) || mask != candidate_mask
        || !ReadBackground(loader.Get(), backdrop) || backdrop.Get() != candidate_background.Get()
        || !LayerOfFlat(backdrop.Get(), mask, backdrop_layer, backdrop_renderer)
        || backdrop_layer != candidate_background_layer) {
      stale();
      return;
    }
    ReadPool(loader.Get(), pool, count);
    if (pool.Get() != candidate_pool.Get()) {
      stale();
      return;
    }
    if (candidate_is_background) {
      if (candidate_flat.Get() != backdrop.Get()) {
        stale();
        return;
      }
    } else {
      if (candidate_index >= count) {
        stale();
        return;
      }
      auto *array = reinterpret_cast<Il2CppArray *>(pool.Get());
      auto *flat =
          *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, candidate_index, sizeof(void *)));
      if (flat != candidate_flat.Get()) {
        stale();
        return;
      }
    }
    --artwork_budget;
    Root texture, renderer;
    int  layer = -1;
    if (!Artwork(candidate_flat.Get(), mask, candidate_is_background ? -1 : backdrop_layer, texture, layer, renderer)
        || texture.Get() != candidate_texture.Get() || renderer.Get() != candidate_renderer.Get()
        || layer != candidate_layer || Name(texture.Get()) != candidate_name) {
      stale();
      return;
    }
    const auto name = candidate_name;
    Root       camera_root;
    camera_root.Reset(camera);
    // No shell/mesh/texture allocation occurs before pixels are ready. These
    // local shared pixels and native roots deliberately survive cancellation.
    Clear();
    if (!camera_root.Get() || !Create(texture.Get(), layer, renderer.Get(), *prepared.pixels, prepared.generatedMs)) {
      Fail("shell-create");
      return;
    }
    source_loader.Reset(loader.Get());
    owning_camera.Reset(camera_root.Get());
    shell_name = name;
    next_scan  = frame + 30;
    if (!source_loader.Get() || !owning_camera.Get())
      Fail("published-owner-root");
  }
} // namespace

void Clear()
{
  ResetScan();
  ResetCandidate(); // Cancels pending/results; an active pure job cannot publish.
  shell_name.clear();
  shell_layer = -1;
  // Only mod-created objects are destroyed. Native artwork, visibility,
  // transforms and materials stay under the game's ownership throughout.
  native_background.Reset();
  Destroy(shell);
  Destroy(mesh);
  Destroy(material);
  Destroy(sky_texture);
  source.Reset();
  shell_renderer.Reset();
  owning_camera.Reset();
  source_loader.Reset();
  scenery_pool.Reset();
  scenery_owner        = nullptr;
  scenery_cursor       = 0;
  scenery_count        = 0;
  pending_bounds_valid = false;
  next_bounds_scan     = 0;
  last_bounds_frame    = -1;
  bounds_valid    = false;
  reported_bounds = false;
}

void ApplyDrawDistance(Il2CppObject *camera)
{
  if (!ready || failed || !bounds_valid || camera != owning_camera.Get() || !Live(camera) || !Live(shell.Get()))
    return;
  Il2CppObject *transform = nullptr;
  Vector3       position{};
  float         baseline = 0.0f;
  if (!Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &transform) || !Live(transform)
      || !Value(get_position, transform, "UnityEngine.Vector3", position)
      || !Value(far_clip, camera, "System.Single", baseline))
    return;
  const auto &center  = scenery_bounds.center;
  const auto &extents = scenery_bounds.extents;
  float       expanded =
      navigation_environment_art::SceneryFarClip(baseline, {position.x, position.y, position.z},
                                                 {center.x, center.y, center.z}, {extents.x, extents.y, extents.z});
  if (expanded > baseline) {
    void *args[]{&expanded};
    if (!Il2CppRuntime::TryInvoke(set_far_clip, camera, args))
      Fail("scenery-draw-distance");
  }
}

void ValidateRuntime()
{
#if defined(_MODDBG)
  // Session-only upload smoke; pure generation uses the same asynchronous path.
  // It does not show an enclosure, touch native renderers or activate orbit.
  static bool                                  done = false, requested = false;
  static std::chrono::steady_clock::time_point deadline{};
  if (done)
    return;
  if (!requested) {
    const char *opt_in = std::getenv("STFC_MOD_SYSTEM_SKY_SMOKE");
    if (!opt_in || std::strcmp(opt_in, "1") != 0) {
      done = true;
      return;
    }
    requested = true;
    deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  }
  // Gameplay owns the one worker once orbit selection starts. Never fight its
  // latest request by resubmitting a smoke key on every screen update.
  if (source_loader.Get() || scan_active || candidate_valid || Live(shell.Get())) {
    done = true;
    return;
  }
  if (!Init()) {
    spdlog::warn("[SystemEnvironmentPoc] upload smoke unavailable: runtime API");
    done = true;
    return;
  }
  const auto prepared = PreparePurePixels("SystemAmbientSmoke");
  if (!prepared.pixels && !prepared.failed && std::chrono::steady_clock::now() < deadline)
    return;
  done                = true;
  const bool uploaded = prepared.pixels && CreateSkyTexture(*prepared.pixels, prepared.generatedMs);
  Destroy(sky_texture);
  if (pixel_worker)
    pixel_worker->Cancel();
  spdlog::info(
      "[SystemEnvironmentPoc] upload smoke passed={} generation-failed={} timed-out={}; native scene unchanged",
      uploaded, prepared.failed, !prepared.failed && !prepared.pixels);
#endif
}

void Update(Il2CppObject *camera)
{
  if (failed)
    return;
  Root camera_root;
  camera_root.Reset(camera);
  if (!camera_root.Get() || !Init() || !Live(camera_root.Get()))
    return;
  if (owning_camera.Get() && owning_camera.Get() != camera)
    Clear();
  if (source_loader.Get()) {
    bool enabled = false;
    if (!Live(source_loader.Get()) || !Value(active, source_loader.Get(), "System.Boolean", enabled) || !enabled)
      Clear();
  }
  const int frame = frame_count();
  if (artwork_frame != frame) {
    artwork_frame  = frame;
    artwork_budget = 5;
  }
  if (scan_active || frame >= next_scan)
    Scan(camera, frame);
  if (failed)
    return;
  PublishReady(camera, frame);
  if (failed || !Live(shell.Get()))
    return;
  if (camera != owning_camera.Get())
    owning_camera.Reset(camera);
  if (!owning_camera.Get()) {
    Fail("camera-root");
    return;
  }
  CaptureSceneryBounds(frame);
  ApplyDrawDistance(camera);
  if (failed)
    return;
  float         far_plane = 0.0f, near_plane = 0.0f;
  Vector3       position{};
  Il2CppObject *camera_transform = nullptr, *shell_transform = nullptr;
  if (!Value(far_clip, camera, "System.Single", far_plane) || !Value(near_clip, camera, "System.Single", near_plane)
      || !std::isfinite(far_plane) || !std::isfinite(near_plane) || near_plane <= 0 || far_plane <= 4.0f * near_plane
      || !Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &camera_transform)) {
    Fail("camera-follow-query");
    return;
  }
  Root camera_transform_root, shell_transform_root;
  camera_transform_root.Reset(camera_transform);
  if (!camera_transform_root.Get() || !Live(camera_transform_root.Get())
      || !Il2CppRuntime::TryInvoke(game_transform, shell.Get(), nullptr, &shell_transform)) {
    Fail("camera-follow-query");
    return;
  }
  shell_transform_root.Reset(shell_transform);
  if (!shell_transform_root.Get() || !Live(shell_transform_root.Get())
      || !Value(get_position, camera_transform_root.Get(), "UnityEngine.Vector3", position)
      || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
    Fail("camera-follow-query");
    return;
  }
  // Follow translation only: keep the sky's orientation independent of camera rotation.
  Vector3 scale{far_plane * 0.95f, far_plane * 0.95f, far_plane * 0.95f};
  Vector3 angles{};
  bool    world_stays = true;
  void   *parent_args[]{camera_transform_root.Get(), &world_stays}, *angle_args[]{&angles};
  void   *position_args[]{&position}, *scale_args[]{&scale};
  // Parenting only the new shell makes a later pan in this frame follow
  // immediately. Reset its world orientation after the orbit write.
  if (!Il2CppRuntime::TryInvoke(set_parent, shell_transform, parent_args)
      || !Il2CppRuntime::TryInvoke(set_angles, shell_transform, angle_args)
      || !Il2CppRuntime::TryInvoke(set_position, shell_transform, position_args)
      || !Il2CppRuntime::TryInvoke(set_scale, shell_transform, scale_args))
    Fail("camera-follow-write");
  if (!failed && !reported_bounds && Live(shell_renderer.Get())) {
    Bounds bounds{};
    bool   visible = false;
    if (Value(get_bounds, shell_renderer.Get(), "UnityEngine.Bounds", bounds)) {
      Value(renderer_visible, shell_renderer.Get(), "System.Boolean", visible);
      spdlog::info("[SystemEnvironmentPoc] enclosure world center=({},{},{}) extents=({},{},{}) camera=({},{},{}) "
                   "near={} far={} visible={}",
                   bounds.center.x, bounds.center.y, bounds.center.z, bounds.extents.x, bounds.extents.y,
                   bounds.extents.z, position.x, position.y, position.z, near_plane, far_plane, visible);
      spdlog::info(
          "[SystemEnvironmentPoc] native scenery retained bounds-valid={} center=({},{},{}) extents=({},{},{})",
          bounds_valid, scenery_bounds.center.x, scenery_bounds.center.y, scenery_bounds.center.z,
          scenery_bounds.extents.x, scenery_bounds.extents.y, scenery_bounds.extents.z);
      reported_bounds = true;
    }
  }
}
} // namespace navigation_environment_poc
