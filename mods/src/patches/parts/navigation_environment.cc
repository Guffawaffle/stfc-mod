#include "patches/navigation_environment.h"
#include "patches/background_layer_science.h"
#include "patches/navigation_environment_art.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Vector2.h>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>
#include <type_traits>
#include <utility>
#include <vector>

namespace navigation_environment
{
namespace
{
  // Scene roots support explicit reset and survive until Clear; settings roots are scope-only.
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

  bool Init()
  {
    if (attempted)
      return ready;
    attempted              = true;
    auto unity_object      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
    auto unity_behaviour   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Behaviour");
    auto unity_renderer    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Renderer");
    auto unity_texture     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture");
    auto unity_game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    auto unity_material    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Material");
    auto unity_component   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    auto unity_transform   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    auto unity_camera      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Camera");
    auto unity_mesh        = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Mesh");
    auto unity_mesh_filter = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshFilter");
    auto unity_shader      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Shader");
    auto unity_texture_2d  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
    destroy = method_contract::Resolve(unity_object.get_cls(), "Destroy", true, "System.Void", {"UnityEngine.Object"});
    alive =
        method_contract::Resolve(unity_object.get_cls(), "op_Implicit", true, "System.Boolean", {"UnityEngine.Object"});
    find_all = method_contract::Resolve(unity_object.get_cls(), "FindObjectsOfType", true, "UnityEngine.Object[]",
                                        {"System.Type"});
    active = method_contract::Resolve(unity_behaviour.get_cls(), "get_isActiveAndEnabled", false, "System.Boolean", {});
    shared_material =
        method_contract::Resolve(unity_renderer.get_cls(), "get_sharedMaterial", false, "UnityEngine.Material", {});
    texture_width    = method_contract::Resolve(unity_texture.get_cls(), "get_width", false, "System.Int32", {});
    texture_height   = method_contract::Resolve(unity_texture.get_cls(), "get_height", false, "System.Int32", {});
    object_name      = method_contract::Resolve(unity_object.get_cls(), "get_name", false, "System.String", {});
    renderer_enabled = method_contract::Resolve(unity_renderer.get_cls(), "get_enabled", false, "System.Boolean", {});
    active_in_hierarchy =
        method_contract::Resolve(unity_game_object.get_cls(), "get_activeInHierarchy", false, "System.Boolean", {});
    main_texture =
        method_contract::Resolve(unity_material.get_cls(), "get_mainTexture", false, "UnityEngine.Texture", {});
    get_transform =
        method_contract::Resolve(unity_component.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
    game_transform =
        method_contract::Resolve(unity_game_object.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
    get_game_object =
        method_contract::Resolve(unity_component.get_cls(), "get_gameObject", false, "UnityEngine.GameObject", {});
    get_layer = method_contract::Resolve(unity_game_object.get_cls(), "get_layer", false, "System.Int32", {});
    set_layer =
        method_contract::Resolve(unity_game_object.get_cls(), "set_layer", false, "System.Void", {"System.Int32"});
    get_position =
        method_contract::Resolve(unity_transform.get_cls(), "get_position", false, "UnityEngine.Vector3", {});
    set_position = method_contract::Resolve(unity_transform.get_cls(), "set_position", false, "System.Void",
                                            {"UnityEngine.Vector3"});
    set_scale    = method_contract::Resolve(unity_transform.get_cls(), "set_localScale", false, "System.Void",
                                            {"UnityEngine.Vector3"});
    set_parent   = method_contract::Resolve(unity_transform.get_cls(), "SetParent", false, "System.Void",
                                            {"UnityEngine.Transform", "System.Boolean"});
    set_angles   = method_contract::Resolve(unity_transform.get_cls(), "set_eulerAngles", false, "System.Void",
                                            {"UnityEngine.Vector3"});
    far_clip     = method_contract::Resolve(unity_camera.get_cls(), "get_farClipPlane", false, "System.Single", {});
    set_far_clip =
        method_contract::Resolve(unity_camera.get_cls(), "set_farClipPlane", false, "System.Void", {"System.Single"});
    near_clip    = method_contract::Resolve(unity_camera.get_cls(), "get_nearClipPlane", false, "System.Single", {});
    culling_mask = method_contract::Resolve(unity_camera.get_cls(), "get_cullingMask", false, "System.Int32", {});
    game_ctor = method_contract::Resolve(unity_game_object.get_cls(), ".ctor", false, "System.Void", {"System.String"});
    add_component = method_contract::Resolve(unity_game_object.get_cls(), "AddComponent", false,
                                             "UnityEngine.Component", {"System.Type"});
    mesh_ctor     = method_contract::Resolve(unity_mesh.get_cls(), ".ctor", false, "System.Void", {});
    material_ctor =
        method_contract::Resolve(unity_material.get_cls(), ".ctor", false, "System.Void", {"UnityEngine.Shader"});
    set_vertices =
        method_contract::Resolve(unity_mesh.get_cls(), "set_vertices", false, "System.Void", {"UnityEngine.Vector3[]"});
    set_uv = method_contract::Resolve(unity_mesh.get_cls(), "set_uv", false, "System.Void", {"UnityEngine.Vector2[]"});
    set_triangles =
        method_contract::Resolve(unity_mesh.get_cls(), "set_triangles", false, "System.Void", {"System.Int32[]"});
    set_mesh     = method_contract::Resolve(unity_mesh_filter.get_cls(), "set_sharedMesh", false, "System.Void",
                                            {"UnityEngine.Mesh"});
    set_material = method_contract::Resolve(unity_renderer.get_cls(), "set_sharedMaterial", false, "System.Void",
                                            {"UnityEngine.Material"});
    shader_find =
        method_contract::Resolve(unity_shader.get_cls(), "Find", true, "UnityEngine.Shader", {"System.String"});
    set_texture = method_contract::Resolve(unity_material.get_cls(), "set_mainTexture", false, "System.Void",
                                           {"UnityEngine.Texture"});
    set_queue =
        method_contract::Resolve(unity_material.get_cls(), "set_renderQueue", false, "System.Void", {"System.Int32"});
    shader_supported = method_contract::Resolve(unity_shader.get_cls(), "get_isSupported", false, "System.Boolean", {});
    renderer_visible = method_contract::Resolve(unity_renderer.get_cls(), "get_isVisible", false, "System.Boolean", {});
    get_bounds = method_contract::Resolve(unity_renderer.get_cls(), "get_bounds", false, "UnityEngine.Bounds", {});
    recalculate_bounds = method_contract::Resolve(unity_mesh.get_cls(), "RecalculateBounds", false, "System.Void", {});
    set_shadows     = method_contract::Resolve(unity_renderer.get_cls(), "set_shadowCastingMode", false, "System.Void",
                                               {"UnityEngine.Rendering.ShadowCastingMode"});
    receive_shadows = method_contract::Resolve(unity_renderer.get_cls(), "set_receiveShadows", false, "System.Void",
                                               {"System.Boolean"});
    auto loader     = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderableLoader");
    auto flat       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable");
    flat_class      = flat.get_cls();
    pool_field      = loader.get_cls() ? loader.GetField("_pool").get_info() : nullptr;
    if (pool_field
        && ((il2cpp_field_get_flags(pool_field) & FIELD_ATTRIBUTE_STATIC)
            || !method_contract::Type(pool_field->type, "Digit.Client.Rendering.FlatRenderable[]")))
      pool_field = nullptr;
    background     = method_contract::Resolve(loader.get_cls(), "get_Background", false,
                                              "Digit.Client.Rendering.FlatRenderable", {});
    renderer_field = flat.get_cls() ? flat.GetField("MeshRenderer").get_info() : nullptr;
    if (renderer_field
        && ((il2cpp_field_get_flags(renderer_field) & FIELD_ATTRIBUTE_STATIC)
            || !method_contract::Type(renderer_field->type, "UnityEngine.MeshRenderer")))
      renderer_field = nullptr;
    loader_type    = loader.get_cls() ? loader.GetType() : nullptr;
    loader_class   = loader.get_cls();
    object_class   = unity_object.get_cls();
    auto renderer  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshRenderer");
    filter_type    = unity_mesh_filter.get_cls() ? unity_mesh_filter.GetType() : nullptr;
    renderer_type  = renderer.get_cls() ? renderer.GetType() : nullptr;
    game_class     = unity_game_object.get_cls();
    mesh_class     = unity_mesh.get_cls();
    material_class = unity_material.get_cls();
    vector3_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Vector3").get_cls();
    vector2_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Vector2").get_cls();
    int_class      = il2cpp_get_class_helper("mscorlib", "System", "Int32").get_cls();
    texture_class  = unity_texture_2d.get_cls();
    color32_class  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Color32").get_cls();
    texture_ctor =
        method_contract::Resolve(unity_texture_2d.get_cls(), ".ctor", false, "System.Void",
                                 {"System.Int32", "System.Int32", "UnityEngine.TextureFormat", "System.Boolean"});
    set_pixels    = method_contract::Resolve(unity_texture_2d.get_cls(), "SetPixels32", false, "System.Void",
                                             {"UnityEngine.Color32[]"});
    texture_apply = method_contract::Resolve(unity_texture_2d.get_cls(), "Apply", false, "System.Void",
                                             {"System.Boolean", "System.Boolean"});
    wrap_u        = method_contract::Resolve(unity_texture.get_cls(), "set_wrapModeU", false, "System.Void",
                                             {"UnityEngine.TextureWrapMode"});
    wrap_v        = method_contract::Resolve(unity_texture.get_cls(), "set_wrapModeV", false, "System.Void",
                                             {"UnityEngine.TextureWrapMode"});
    frame_count   = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    const std::pair<const MethodInfo *, const char *> methods[] = {
        {destroy, "object.Destroy"},
        {alive, "object.op_Implicit"},
        {find_all, "object.FindObjectsOfType"},
        {active, "behaviour.get_isActiveAndEnabled"},
        {shared_material, "renderer.get_sharedMaterial"},
        {texture_width, "texture.get_width"},
        {texture_height, "texture.get_height"},
        {renderer_enabled, "renderer.get_enabled"},
        {active_in_hierarchy, "game_object.get_activeInHierarchy"},
        {main_texture, "material.get_mainTexture"},
        {get_transform, "component.get_transform"},
        {game_transform, "game_object.get_transform"},
        {get_game_object, "component.get_gameObject"},
        {get_layer, "game_object.get_layer"},
        {set_layer, "game_object.set_layer"},
        {get_position, "transform.get_position"},
        {set_position, "transform.set_position"},
        {set_scale, "transform.set_localScale"},
        {set_parent, "transform.SetParent"},
        {set_angles, "transform.set_eulerAngles"},
        {far_clip, "camera.get_farClipPlane"},
        {set_far_clip, "camera.set_farClipPlane"},
        {near_clip, "camera.get_nearClipPlane"},
        {culling_mask, "camera.get_cullingMask"},
        {game_ctor, "game_object..ctor"},
        {add_component, "game_object.AddComponent"},
        {mesh_ctor, "mesh..ctor"},
        {material_ctor, "material..ctor"},
        {set_vertices, "mesh.set_vertices"},
        {set_uv, "mesh.set_uv"},
        {set_triangles, "mesh.set_triangles"},
        {set_mesh, "mesh_filter.set_sharedMesh"},
        {set_material, "renderer.set_sharedMaterial"},
        {shader_find, "shader.Find"},
        {set_texture, "material.set_mainTexture"},
        {set_queue, "material.set_renderQueue"},
        {shader_supported, "shader.get_isSupported"},
        {get_bounds, "renderer.get_bounds"},
        {recalculate_bounds, "mesh.RecalculateBounds"},
        {set_shadows, "renderer.set_shadowCastingMode"},
        {receive_shadows, "renderer.set_receiveShadows"},
        {background, "loader.get_Background"},
        {texture_ctor, "texture_2d..ctor"},
        {set_pixels, "texture_2d.SetPixels32"},
        {texture_apply, "texture_2d.Apply"},
        {wrap_u, "texture.set_wrapModeU"},
        {wrap_v, "texture.set_wrapModeV"},
    };
    for (const auto &[method, name] : methods) {
      if (!method) {
        spdlog::warn("[SystemEnvironment] {} signature unavailable; ambient sky skipped; native scenery retained",
                     name);
        return false;
      }
    }
    if (!loader_type || !filter_type || !renderer_type || !renderer_field || !vector3_class || !vector2_class
        || !int_class || !color32_class || !frame_count) {
      spdlog::warn("[SystemEnvironment] metadata unavailable: FlatRenderableLoader={} MeshFilter={} MeshRenderer={} "
                   "FlatRenderable.MeshRenderer={} Vector3={} Vector2={} Int32={} Color32={} Time.get_frameCount={}; "
                   "ambient sky skipped; native scenery retained",
                   loader_type != nullptr, filter_type != nullptr, renderer_type != nullptr, renderer_field != nullptr,
                   vector3_class != nullptr, vector2_class != nullptr, int_class != nullptr, color32_class != nullptr,
                   frame_count != nullptr);
      return false;
    }
    ready = true;
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

  // Copy boxed values immediately, before another managed call can collect the temporary.
  // Existing checked helpers accept reference arguments; these calls also return structs.
  template <typename T> bool Value(const MethodInfo *method, Il2CppObject *object, const char *type, T &value)
  {
    Il2CppObject *boxed = nullptr;
    if (!Il2CppRuntime::TryInvoke(method, object, nullptr, &boxed))
      return false;
    if constexpr (std::is_same_v<T, bool>)
      return Il2CppRuntime::TryBoolean(boxed, value);
    if (!boxed || !method_contract::Type(il2cpp_class_get_type(boxed->klass), type))
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
    spdlog::warn("[SystemEnvironment] unavailable at {}; leaving native scenery and free camera intact", stage);
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
    spdlog::debug(
        "[SystemEnvironment] custom directional sky={}x{} palette={} generated/uploaded={}ms; native artwork "
        "preserved",
        width, height, static_cast<int>(theme),
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return true;
  }

  bool Create(Il2CppObject *texture, int layer, Il2CppObject *native_renderer)
  {
    // A full panorama is authored by direction, not stitched from a finite
    // system painting. Keep the original ship/portal/nebula meshes visible.
    constexpr int        longitude = 64, latitude = 32;
    constexpr double     pi = 3.14159265358979323846;
    std::vector<Vector3> vertices;
    std::vector<vec2>    uv;
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
    void         *name_args[]{il2cpp_string_new("CommunitySystemEnvironment")};
    void         *shader_args[]{il2cpp_string_new("Unlit/Texture")};
    Il2CppObject *shader = nullptr, *filter = nullptr, *renderer = nullptr;
    if (!Il2CppRuntime::TryInvoke(game_ctor, shell.Get(), name_args)
        || !Il2CppRuntime::TryInvoke(mesh_ctor, mesh.Get(), nullptr)) {
      spdlog::warn("[SystemEnvironment] GameObject/Mesh construction failed");
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
      spdlog::warn("[SystemEnvironment] object/mesh/shader setup failed (shader-present={})", Live(shader));
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
    spdlog::debug("[SystemEnvironment] ambient enclosure layer={} vertices={} triangles={} "
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
    if (!Live(renderer) || !Value(renderer_enabled, renderer, "System.Boolean", enabled)
        || (!enabled && !background_layer_science::IsHiddenRenderer(renderer))
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
      spdlog::debug("[SystemEnvironment] artwork material={} texture={} dimensions={}x{} layer={}",
                    Name(native_material), Name(texture), width, height, layer);
    return true;
  }

  void IncludeScenery(Il2CppObject *renderer, int mask)
  {
    Bounds        bounds{};
    Il2CppObject *object  = nullptr;
    bool          enabled = false, visible_object = false;
    int           layer = -1;
    if (!Live(renderer) || !Value(renderer_enabled, renderer, "System.Boolean", enabled)
        || (!enabled && !background_layer_science::IsHiddenRenderer(renderer))
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
      spdlog::debug("[SystemEnvironment] awaiting compatible artwork loaders={} active={} textured={} camera-mask={}",
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
      spdlog::debug("[SystemEnvironment] enclosure world center=({},{},{}) extents=({},{},{}) camera=({},{},{}) "
                    "near={} far={} visible={}",
                    bounds.center.x, bounds.center.y, bounds.center.z, bounds.extents.x, bounds.extents.y,
                    bounds.extents.z, position.x, position.y, position.z, near_plane, far_plane, visible);
      spdlog::debug("[SystemEnvironment] native scenery retained bounds-valid={} center=({},{},{}) extents=({},{},{})",
                    bounds_valid, scenery_bounds.center.x, scenery_bounds.center.y, scenery_bounds.center.z,
                    scenery_bounds.extents.x, scenery_bounds.extents.y, scenery_bounds.extents.z);
      reported_bounds = true;
    }
  }
}
} // namespace navigation_environment
