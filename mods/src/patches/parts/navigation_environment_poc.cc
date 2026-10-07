#include "patches/navigation_environment_poc.h"
#include "patches/navigation_environment_art.h"
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
  struct Bounds {
    Vector3 center, extents;
  };
  Bounds            scenery_bounds{};
  bool              bounds_valid    = false;
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
    loader_class   = loader.get_cls();
    object_class   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object").get_cls();
    auto filter    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshFilter");
    auto renderer  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshRenderer");
    filter_type    = filter.isValidHelper() ? filter.GetType() : nullptr;
    renderer_type  = renderer.isValidHelper() ? renderer.GetType() : nullptr;
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
    void         *args[]{object};
    Il2CppObject *boxed = nullptr;
    bool          value = false;
    return Il2CppRuntime::TryInvoke(alive, nullptr, args, &boxed) && Il2CppRuntime::TryBoolean(boxed, value) && value;
  }

  template <typename T> bool Value(const MethodInfo *method, Il2CppObject *object, const char *type, T &value)
  {
    Il2CppObject *boxed = nullptr;
    if (!Il2CppRuntime::TryInvoke(method, object, nullptr, &boxed) || !boxed
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

  bool CreateSkyTexture(const std::string &name, int width = 1024, int height = 512)
  {
    const auto started = std::chrono::steady_clock::now();
    const auto theme   = navigation_environment_art::SelectTheme(name);
    const auto pixels =
        navigation_environment_art::Generate(width, height, theme, navigation_environment_art::Seed(name));
    uint32_t alignment = 0;
    if (pixels.empty()
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
        "[SystemEnvironmentPoc] custom directional sky={}x{} palette={} generated/uploaded={}ms; native artwork "
        "preserved",
        width, height, static_cast<int>(theme),
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return true;
  }

  bool Create(Il2CppObject *texture, int layer, Il2CppObject *native_renderer)
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
    void         *name_args[]{il2cpp_string_new("CommunitySystemEnvironmentPoc")};
    void         *shader_args[]{il2cpp_string_new("Unlit/Texture")};
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
    Il2CppObject *named = nullptr;
    std::string   name  = "SystemAmbient";
    if (object_name && Il2CppRuntime::TryInvoke(object_name, texture, nullptr, &named) && named
        && method_contract::Type(il2cpp_class_get_type(named->klass), "System.String")) {
      auto *text = reinterpret_cast<Il2CppString *>(named);
      if (text->length >= 0 && text->length <= 256)
        name = to_string(text);
    }
    if (!CreateSkyTexture(name))
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
    Il2CppObject *result = nullptr;
    if (!object_name || !Il2CppRuntime::TryInvoke(object_name, object, nullptr, &result) || !result
        || !method_contract::Type(il2cpp_class_get_type(result->klass), "System.String"))
      return "unknown";
    auto *name = reinterpret_cast<Il2CppString *>(result);
    return name->length >= 0 && name->length <= 256 ? to_string(name) : "unknown";
  }

  bool Artwork(Il2CppObject *flat, int mask, int required_layer, Il2CppObject *&texture, int &layer,
               Il2CppObject *&renderer)
  {
    if (!Live(flat) || !il2cpp_class_is_assignable_from(flat_class, flat->klass))
      return false;
    Il2CppObject *native_material = nullptr, *object = nullptr;
    il2cpp_field_get_value(flat, renderer_field, &renderer);
    bool enabled = false;
    int  width = 0, height = 0;
    if (!Live(renderer) || !Value(renderer_enabled, renderer, "System.Boolean", enabled) || !enabled
        || !Il2CppRuntime::TryInvoke(get_game_object, renderer, nullptr, &object) || !Live(object)
        || !Value(get_layer, object, "System.Int32", layer) || layer < 0 || layer > 31
        || !(static_cast<unsigned>(mask) & (1u << layer)) || (required_layer >= 0 && layer != required_layer)
        || !Il2CppRuntime::TryInvoke(shared_material, renderer, nullptr, &native_material) || !Live(native_material)
        || !Il2CppRuntime::TryInvoke(main_texture, native_material, nullptr, &texture) || !Live(texture)
        || !Value(texture_width, texture, "System.Int32", width)
        || !Value(texture_height, texture, "System.Int32", height) || width < 64 || height < 64)
      return false;
    // Reject Unity's tiny white/black placeholder textures and lookup strips.
    if (texture != source.Get())
      spdlog::info("[SystemEnvironmentPoc] artwork material={} texture={} dimensions={}x{} layer={}",
                   Name(native_material), Name(texture), width, height, layer);
    return true;
  }

  void IncludeScenery(Il2CppObject *renderer, int mask)
  {
    Bounds        bounds{};
    Il2CppObject *object  = nullptr;
    bool          enabled = false, visible_object = false;
    int           layer = -1;
    if (!Live(renderer) || !Value(renderer_enabled, renderer, "System.Boolean", enabled) || !enabled
        || !Il2CppRuntime::TryInvoke(get_game_object, renderer, nullptr, &object) || !Live(object)
        || !Value(active_in_hierarchy, object, "System.Boolean", visible_object) || !visible_object
        || !Value(get_layer, object, "System.Int32", layer) || layer < 0 || layer > 31
        || !(static_cast<unsigned>(mask) & (1u << layer)) || !Value(get_bounds, renderer, "UnityEngine.Bounds", bounds))
      return;
    const auto &c = bounds.center;
    const auto &e = bounds.extents;
    if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || !std::isfinite(e.x) || !std::isfinite(e.y)
        || !std::isfinite(e.z) || e.x < 0 || e.y < 0 || e.z < 0)
      return;
    if (!bounds_valid) {
      scenery_bounds = bounds;
      bounds_valid   = true;
      return;
    }
    const auto merge = [](float &center, float &extent, float next_center, float next_extent) {
      const double low  = std::min(double(center) - extent, double(next_center) - next_extent);
      const double high = std::max(double(center) + extent, double(next_center) + next_extent);
      center            = float((low + high) * 0.5);
      extent            = float((high - low) * 0.5);
    };
    merge(scenery_bounds.center.x, scenery_bounds.extents.x, c.x, e.x);
    merge(scenery_bounds.center.y, scenery_bounds.extents.y, c.y, e.y);
    merge(scenery_bounds.center.z, scenery_bounds.extents.z, c.z, e.z);
  }

  void CaptureSceneryBounds(Il2CppObject *loader, int mask)
  {
    bounds_valid = false;
    IncludeScenery(native_background.Get(), mask);
    Il2CppArray *pool = nullptr;
    if (pool_field)
      il2cpp_field_get_value(loader, pool_field, &pool);
    if (!pool || !pool->klass || pool->klass->rank != 1 || !pool->klass->element_class
        || !il2cpp_class_is_assignable_from(flat_class, pool->klass->element_class) || pool->max_length > 128)
      return;
    Root pool_root;
    pool_root.Reset(reinterpret_cast<Il2CppObject *>(pool));
    for (size_t i = 0; pool_root.Get() && i < pool->max_length; ++i) {
      auto         *flat     = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(pool, i, sizeof(void *)));
      Il2CppObject *renderer = nullptr;
      if (Live(flat) && il2cpp_class_is_assignable_from(flat_class, flat->klass)) {
        il2cpp_field_get_value(flat, renderer_field, &renderer);
        IncludeScenery(renderer, mask);
      }
    }
  }

  void Scan(Il2CppObject *camera)
  {
    void         *args[]{loader_type};
    Il2CppObject *result = nullptr;
    if (!Il2CppRuntime::TryInvoke(find_all, nullptr, args, &result) || !result || !result->klass
        || result->klass->rank != 1 || !result->klass->element_class || !object_class
        || !il2cpp_class_is_assignable_from(object_class, result->klass->element_class)) {
      Fail("loader-discovery");
      return;
    }
    Root array_root;
    array_root.Reset(result);
    auto *array = reinterpret_cast<Il2CppArray *>(result);
    if (!array_root.Get() || array->max_length > 64) {
      Fail("loader-count");
      return;
    }
    int mask = 0;
    if (!Value(culling_mask, camera, "System.Int32", mask)) {
      Fail("camera-mask");
      return;
    }
    unsigned active_loaders = 0, textured_backgrounds = 0;
    for (size_t i = 0; i < array->max_length; ++i) {
      auto         *loader = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, i, sizeof(void *)));
      Il2CppObject *boxed = nullptr, *flat = nullptr, *texture = nullptr, *native_renderer = nullptr;
      bool          enabled = false;
      if (!Live(loader) || !loader_class || !il2cpp_class_is_assignable_from(loader_class, loader->klass)
          || !Il2CppRuntime::TryInvoke(active, loader, nullptr, &boxed) || !Il2CppRuntime::TryBoolean(boxed, enabled)
          || !enabled || !Il2CppRuntime::TryInvoke(background, loader, nullptr, &flat) || !Live(flat))
        continue;
      ++active_loaders;
      int layer = -1;
      if (!Artwork(flat, mask, -1, texture, layer, native_renderer)) {
        // Some loaders expose procedural stars as Background; their actual
        // nebula image is another active flat renderer on the same backdrop layer.
        int          background_layer = layer;
        Il2CppArray *pool             = nullptr;
        if (pool_field)
          il2cpp_field_get_value(loader, pool_field, &pool);
        if (background_layer < 0 || !pool || !pool->klass || pool->klass->rank != 1 || !pool->klass->element_class
            || !il2cpp_class_is_assignable_from(flat_class, pool->klass->element_class) || pool->max_length > 128)
          continue;
        Root pool_root;
        pool_root.Reset(reinterpret_cast<Il2CppObject *>(pool));
        bool found = false;
        for (size_t j = 0; pool_root.Get() && j < pool->max_length; ++j) {
          auto *candidate = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(pool, j, sizeof(void *)));
          if (Artwork(candidate, mask, background_layer, texture, layer, native_renderer)) {
            found = true;
            break;
          }
        }
        if (!found)
          continue;
      }
      ++textured_backgrounds;
      if (texture == source.Get() && native_renderer == native_background.Get() && Live(shell.Get())) {
        CaptureSceneryBounds(loader, mask);
        return;
      }
      Root texture_root;
      texture_root.Reset(texture);
      Root renderer_root;
      renderer_root.Reset(native_renderer);
      Clear();
      if (!texture_root.Get() || !renderer_root.Get() || !Create(texture_root.Get(), layer, renderer_root.Get()))
        Fail("shell-create");
      else
        CaptureSceneryBounds(loader, mask);
      return;
    }
    // Loading transitions must not retain another system's artwork.
    static unsigned reports = 0;
    if (reports++ < 5)
      spdlog::debug(
          "[SystemEnvironmentPoc] awaiting compatible artwork loaders={} active={} textured={} camera-mask={}",
          array->max_length, active_loaders, textured_backgrounds, mask);
    Clear();
  }
} // namespace

void Clear()
{
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
  // Session-only upload smoke, run on the first main-thread screen update.
  // It does not show an enclosure, touch native renderers or activate orbit.
  static bool done = false;
  if (done)
    return;
  done               = true;
  const char *opt_in = std::getenv("STFC_MOD_SYSTEM_SKY_SMOKE");
  if (!opt_in || std::strcmp(opt_in, "1") != 0 || Live(shell.Get()))
    return;
  if (!Init()) {
    spdlog::warn("[SystemEnvironmentPoc] upload smoke unavailable: runtime API");
    return;
  }
  const bool uploaded = CreateSkyTexture("SystemAmbientSmoke");
  Destroy(sky_texture);
  spdlog::info("[SystemEnvironmentPoc] upload smoke passed={}; native scene unchanged", uploaded);
#endif
}

void Update(Il2CppObject *camera)
{
  if (failed || !Init() || !Live(camera))
    return;
  const int frame = frame_count();
  if (frame >= next_scan) {
    next_scan = frame + 30;
    Scan(camera);
  }
  if (!Live(shell.Get()))
    return;
  if (camera != owning_camera.Get())
    owning_camera.Reset(camera);
  if (!owning_camera.Get()) {
    Fail("camera-root");
    return;
  }
  ApplyDrawDistance(camera);
  if (failed)
    return;
  float         far_plane = 0.0f, near_plane = 0.0f;
  Vector3       position{};
  Il2CppObject *camera_transform = nullptr, *shell_transform = nullptr;
  if (!Value(far_clip, camera, "System.Single", far_plane) || !Value(near_clip, camera, "System.Single", near_plane)
      || !std::isfinite(far_plane) || !std::isfinite(near_plane) || near_plane <= 0 || far_plane <= 4.0f * near_plane
      || !Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &camera_transform) || !Live(camera_transform)
      || !Il2CppRuntime::TryInvoke(game_transform, shell.Get(), nullptr, &shell_transform) || !Live(shell_transform)
      || !Value(get_position, camera_transform, "UnityEngine.Vector3", position) || !std::isfinite(position.x)
      || !std::isfinite(position.y) || !std::isfinite(position.z)) {
    Fail("camera-follow-query");
    return;
  }
  // Follow translation only: keep the sky's orientation independent of camera rotation.
  Vector3 scale{far_plane * 0.95f, far_plane * 0.95f, far_plane * 0.95f};
  Vector3 angles{};
  bool    world_stays = true;
  void   *parent_args[]{camera_transform, &world_stays}, *angle_args[]{&angles};
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
