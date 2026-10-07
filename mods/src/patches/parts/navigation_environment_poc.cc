#include "patches/navigation_environment_poc.h"
#include <cmath>
#include <cstring>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
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
  Root              shell{false}, mesh{false}, material{false}, source{false};
  bool              attempted = false, ready = false, failed = false;
  int               next_scan = 0;
  const MethodInfo *destroy, *alive, *find_all, *active, *background, *shared_material, *main_texture;
  const MethodInfo *get_transform, *game_transform, *get_game_object, *get_layer, *set_layer, *get_position,
      *set_position, *set_scale;
  const MethodInfo *set_parent, *set_angles;
  const MethodInfo *far_clip, *near_clip, *culling_mask, *game_ctor, *add_component, *mesh_ctor, *material_ctor;
  const MethodInfo *set_vertices, *set_uv, *set_triangles, *set_mesh, *set_material, *shader_find, *set_texture;
  Il2CppClass      *game_class, *mesh_class, *material_class, *vector3_class, *vector2_class, *int_class;
  void             *loader_type, *filter_type, *renderer_type;
  FieldInfo        *renderer_field;
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
    attempted       = true;
    destroy         = Method("Object", "Destroy", true, "System.Void", {"UnityEngine.Object"});
    alive           = Method("Object", "op_Implicit", true, "System.Boolean", {"UnityEngine.Object"});
    find_all        = Method("Object", "FindObjectsOfType", true, "UnityEngine.Object[]", {"System.Type"});
    active          = Method("Behaviour", "get_isActiveAndEnabled", false, "System.Boolean");
    shared_material = Method("Renderer", "get_sharedMaterial", false, "UnityEngine.Material");
    main_texture    = Method("Material", "get_mainTexture", false, "UnityEngine.Texture");
    get_transform   = Method("Component", "get_transform", false, "UnityEngine.Transform");
    game_transform  = Method("GameObject", "get_transform", false, "UnityEngine.Transform");
    get_game_object = Method("Component", "get_gameObject", false, "UnityEngine.GameObject");
    get_layer       = Method("GameObject", "get_layer", false, "System.Int32");
    set_layer       = Method("GameObject", "set_layer", false, "System.Void", {"System.Int32"});
    get_position    = Method("Transform", "get_position", false, "UnityEngine.Vector3");
    set_position    = Method("Transform", "set_position", false, "System.Void", {"UnityEngine.Vector3"});
    set_scale       = Method("Transform", "set_localScale", false, "System.Void", {"UnityEngine.Vector3"});
    set_parent    = Method("Transform", "SetParent", false, "System.Void", {"UnityEngine.Transform", "System.Boolean"});
    set_angles    = Method("Transform", "set_eulerAngles", false, "System.Void", {"UnityEngine.Vector3"});
    far_clip      = Method("Camera", "get_farClipPlane", false, "System.Single");
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
    auto loader   = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderableLoader");
    auto flat     = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable");
    background    = method_contract::Resolve(loader.get_cls(), "get_Background", false,
                                             "Digit.Client.Rendering.FlatRenderable", {});
    renderer_field = flat.get_cls() ? il2cpp_class_get_field_from_name(flat.get_cls(), "MeshRenderer") : nullptr;
    if (renderer_field
        && ((il2cpp_field_get_flags(renderer_field) & FIELD_ATTRIBUTE_STATIC)
            || !method_contract::Type(renderer_field->type, "UnityEngine.MeshRenderer")))
      renderer_field = nullptr;
    loader_type    = loader.isValidHelper() ? loader.GetType() : nullptr;
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
    frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    ready = set_parent && set_angles && destroy && alive && find_all && active && background && shared_material
            && main_texture && get_transform && game_transform && get_game_object && get_layer && set_layer
            && get_position && set_position && set_scale && far_clip && near_clip && culling_mask && game_ctor
            && add_component && mesh_ctor && material_ctor && set_vertices && set_uv && set_triangles && set_mesh
            && set_material && shader_find && set_texture && loader_type && filter_type && renderer_type
            && renderer_field && game_class && mesh_class && material_class && vector3_class && vector2_class
            && int_class && frame_count;
    spdlog::info("[SystemEnvironmentPoc] runtime API ready={}", ready);
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

  void Fail()
  {
    Clear();
    failed = true;
    spdlog::warn("[SystemEnvironmentPoc] unavailable; leaving native scenery and free camera intact");
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

  bool Create(Il2CppObject *texture, int layer)
  {
    // The packaged shader culls front faces and does not write depth. Build an
    // outward-wound closed sphere so its interior renders behind world objects.
    // No collider, no gameplay components, no changes to native materials.
    constexpr int    longitude = 48, latitude = 24;
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
        uv.push_back({float(x) / longitude, 1.0f - float(y) / latitude});
      }
    }
    for (int y = 0; y < latitude; ++y) {
      for (int x = 0; x < longitude; ++x) {
        const int a = y * (longitude + 1) + x, b = a + longitude + 1;
        if (y != 0)
          indices.insert(indices.end(), {a, a + 1, b});
        if (y != latitude - 1)
          indices.insert(indices.end(), {a + 1, b + 1, b});
      }
    }
    shell.Reset(il2cpp_object_new(game_class));
    mesh.Reset(il2cpp_object_new(mesh_class));
    material.Reset(il2cpp_object_new(material_class));
    if (!shell.Get() || !mesh.Get() || !material.Get())
      return false;
    void         *name_args[]{il2cpp_string_new("CommunitySystemEnvironmentPoc")};
    void         *shader_args[]{il2cpp_string_new("Hidden/Universe/Backdrop")};
    Il2CppObject *shader = nullptr, *filter = nullptr, *renderer = nullptr;
    if (!Il2CppRuntime::TryInvoke(game_ctor, shell.Get(), name_args)
        || !Il2CppRuntime::TryInvoke(mesh_ctor, mesh.Get(), nullptr)
        || !Il2CppRuntime::TryInvoke(shader_find, nullptr, shader_args, &shader) || !Live(shader))
      return false;
    void *material_args[]{shader}, *texture_args[]{texture}, *layer_args[]{&layer};
    void *filter_args[]{filter_type}, *renderer_args[]{renderer_type};
    if (!Il2CppRuntime::TryInvoke(material_ctor, material.Get(), material_args)
        || !Il2CppRuntime::TryInvoke(set_texture, material.Get(), texture_args)
        || !Il2CppRuntime::TryInvoke(set_layer, shell.Get(), layer_args)
        || !Array(vertices, vector3_class, set_vertices) || !Array(uv, vector2_class, set_uv)
        || !Array(indices, int_class, set_triangles)
        || !Il2CppRuntime::TryInvoke(add_component, shell.Get(), filter_args, &filter) || !Live(filter)
        || !Il2CppRuntime::TryInvoke(add_component, shell.Get(), renderer_args, &renderer) || !Live(renderer))
      return false;
    void *mesh_args[]{mesh.Get()}, *render_args[]{material.Get()};
    if (!Il2CppRuntime::TryInvoke(set_mesh, filter, mesh_args)
        || !Il2CppRuntime::TryInvoke(set_material, renderer, render_args))
      return false;
    source.Reset(texture);
    spdlog::info("[SystemEnvironmentPoc] created closed shell layer={} vertices={} triangles={}", layer,
                 vertices.size(), indices.size() / 3);
    return source.Get() != nullptr;
  }

  void Scan(Il2CppObject *camera)
  {
    void         *args[]{loader_type};
    Il2CppObject *result = nullptr;
    if (!Il2CppRuntime::TryInvoke(find_all, nullptr, args, &result) || !result
        || !method_contract::Type(il2cpp_class_get_type(result->klass), "UnityEngine.Object[]")) {
      Fail();
      return;
    }
    Root array_root;
    array_root.Reset(result);
    auto *array = reinterpret_cast<Il2CppArray *>(result);
    if (!array_root.Get() || array->max_length > 64) {
      Fail();
      return;
    }
    int mask = 0;
    if (!Value(culling_mask, camera, "System.Int32", mask)) {
      Fail();
      return;
    }
    for (size_t i = 0; i < array->max_length; ++i) {
      auto         *loader = *reinterpret_cast<Il2CppObject **>(il2cpp_array_addr_with_size(array, i, sizeof(void *)));
      Il2CppObject *boxed = nullptr, *flat = nullptr, *renderer = nullptr, *native_material = nullptr,
                   *texture = nullptr;
      bool enabled          = false;
      if (!Live(loader) || !Il2CppRuntime::TryInvoke(active, loader, nullptr, &boxed)
          || !Il2CppRuntime::TryBoolean(boxed, enabled) || !enabled
          || !Il2CppRuntime::TryInvoke(background, loader, nullptr, &flat) || !Live(flat))
        continue;
      il2cpp_field_get_value(flat, renderer_field, &renderer);
      if (!Live(renderer) || !Il2CppRuntime::TryInvoke(shared_material, renderer, nullptr, &native_material)
          || !Live(native_material) || !Il2CppRuntime::TryInvoke(main_texture, native_material, nullptr, &texture)
          || !Live(texture))
        continue;
      if (texture == source.Get() && Live(shell.Get()))
        return;
      Il2CppObject *object = nullptr;
      int           layer  = -1;
      if (!Il2CppRuntime::TryInvoke(get_game_object, renderer, nullptr, &object) || !Live(object)
          || !Value(get_layer, object, "System.Int32", layer) || layer < 0 || layer > 31
          || !(static_cast<unsigned>(mask) & (1u << layer)))
        continue;
      Root texture_root;
      texture_root.Reset(texture);
      Clear();
      if (!texture_root.Get() || !Create(texture_root.Get(), layer))
        Fail();
      return;
    }
    // Loading transitions must not retain another system's artwork.
    Clear();
  }
} // namespace

void Clear()
{
  Destroy(shell);
  Destroy(mesh);
  Destroy(material);
  source.Reset();
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
  float         far_plane = 0.0f, near_plane = 0.0f;
  Vector3       position{};
  Il2CppObject *camera_transform = nullptr, *shell_transform = nullptr;
  if (!Value(far_clip, camera, "System.Single", far_plane) || !Value(near_clip, camera, "System.Single", near_plane)
      || !std::isfinite(far_plane) || !std::isfinite(near_plane) || near_plane <= 0 || far_plane <= 4.0f * near_plane
      || !Il2CppRuntime::TryInvoke(get_transform, camera, nullptr, &camera_transform) || !Live(camera_transform)
      || !Il2CppRuntime::TryInvoke(game_transform, shell.Get(), nullptr, &shell_transform) || !Live(shell_transform)
      || !Value(get_position, camera_transform, "UnityEngine.Vector3", position) || !std::isfinite(position.x)
      || !std::isfinite(position.y) || !std::isfinite(position.z)) {
    Fail();
    return;
  }
  // Follow translation only: keep the sky's orientation independent of camera rotation.
  Vector3 scale{far_plane * 0.6f, far_plane * 0.6f, far_plane * 0.6f};
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
    Fail();
}
} // namespace navigation_environment_poc
