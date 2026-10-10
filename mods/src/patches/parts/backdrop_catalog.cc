#include "patches/backdrop_catalog.h"
#include "config.h"
#include "file.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <nlohmann/json.hpp>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>
#include <unordered_set>

// Science inventory: observe only the getter's exact result, never infer a backdrop from its name.
namespace backdrop_catalog
{
namespace
{
  using Json = nlohmann::json;
  struct Root {
    Il2CppGCHandle handle;
    explicit Root(Il2CppObject *object)
        : handle(object ? il2cpp_gchandle_new(object, false) : nullptr)
    {
    }
    ~Root()
    {
      if (handle)
        il2cpp_gchandle_free(handle);
    }
    Il2CppObject *Get() const
    { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
  };
  struct Bounds {
    Vector3 center, extents;
  };
  FieldInfo        *pop_data = nullptr, *renderer_field = nullptr, *filter_field = nullptr;
  Il2CppClass      *view_class = nullptr, *flat_class = nullptr;
  const MethodInfo *system_id, *variation, *name, *shared_material, *shader, *main_texture, *width, *height;
  const MethodInfo *shared_mesh, *vertex_count, *mesh_bounds, *renderer_bounds, *transform;
  const MethodInfo *position, *angles, *scale;
  bool              attempted = false, ready = false, warned = false;
  int64_t           current_system = 0;
  uint64_t          visit          = 0;
  std::unordered_set<std::string> recorded;

  FieldInfo *Field(IL2CppClassHelper &cls, const char *field_name, const char *type)
  {
    auto *field = cls.get_cls() ? cls.GetField(field_name).get_info() : nullptr;
    return field && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
                   && method_contract::Type(field->type, type)
               ? field
               : nullptr;
  }

  bool Init()
  {
    if (attempted)
      return ready;
    attempted      = true;
    auto view      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils");
    auto data      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PopulatedSystemData");
    auto flat      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable");
    auto object    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
    auto renderer  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Renderer");
    auto material  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Material");
    auto texture   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture");
    auto mesh      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Mesh");
    auto filter    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MeshFilter");
    auto component = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    auto tr        = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    view_class     = view.get_cls();
    flat_class     = flat.get_cls();
    pop_data       = Field(view, "_popData", "Digit.Prime.Navigation.PopulatedSystemData");
    renderer_field = Field(flat, "MeshRenderer", "UnityEngine.MeshRenderer");
    filter_field   = Field(flat, "MeshFilter", "UnityEngine.MeshFilter");
    system_id      = method_contract::Resolve(data.get_cls(), "get_SystemId", false, "System.Int64", {});
    variation      = method_contract::Resolve(data.get_cls(), "get_VariationCollection", false,
                                              "SystemVariationConfigCollection", {});
    name           = method_contract::Resolve(object.get_cls(), "get_name", false, "System.String", {});
    shared_material =
        method_contract::Resolve(renderer.get_cls(), "get_sharedMaterial", false, "UnityEngine.Material", {});
    shader          = method_contract::Resolve(material.get_cls(), "get_shader", false, "UnityEngine.Shader", {});
    main_texture    = method_contract::Resolve(material.get_cls(), "get_mainTexture", false, "UnityEngine.Texture", {});
    width           = method_contract::Resolve(texture.get_cls(), "get_width", false, "System.Int32", {});
    height          = method_contract::Resolve(texture.get_cls(), "get_height", false, "System.Int32", {});
    shared_mesh     = method_contract::Resolve(filter.get_cls(), "get_sharedMesh", false, "UnityEngine.Mesh", {});
    vertex_count    = method_contract::Resolve(mesh.get_cls(), "get_vertexCount", false, "System.Int32", {});
    mesh_bounds     = method_contract::Resolve(mesh.get_cls(), "get_bounds", false, "UnityEngine.Bounds", {});
    renderer_bounds = method_contract::Resolve(renderer.get_cls(), "get_bounds", false, "UnityEngine.Bounds", {});
    transform = method_contract::Resolve(component.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
    position  = method_contract::Resolve(tr.get_cls(), "get_position", false, "UnityEngine.Vector3", {});
    angles    = method_contract::Resolve(tr.get_cls(), "get_eulerAngles", false, "UnityEngine.Vector3", {});
    scale     = method_contract::Resolve(tr.get_cls(), "get_localScale", false, "UnityEngine.Vector3", {});
    ready     = view_class && flat_class && pop_data && renderer_field && system_id && name;
    spdlog::info("[BackgroundCatalog] ready={} exact-selector=PlanetViewUtils.get_FlatRenderable", ready);
    return ready;
  }

  Il2CppObject *Ref(const MethodInfo *method, Il2CppObject *object)
  {
    Il2CppObject *result = nullptr;
    if (method && object)
      Il2CppRuntime::TryInvoke(method, object, nullptr, &result);
    return result;
  }
  template <typename T> bool Value(const MethodInfo *method, Il2CppObject *object, const char *type, T &value)
  {
    auto *boxed = Ref(method, object);
    if (!boxed || !method_contract::Type(il2cpp_class_get_type(boxed->klass), type))
      return false;
    uint32_t alignment = 0;
    auto    *data      = il2cpp_object_unbox(boxed);
    if (!data || il2cpp_class_value_size(boxed->klass, &alignment) != sizeof(T))
      return false;
    std::memcpy(&value, data, sizeof(T));
    return true;
  }
  Json Name(Il2CppObject *object)
  {
    auto *value = Ref(name, object);
    if (!value || !method_contract::Type(il2cpp_class_get_type(value->klass), "System.String"))
      return nullptr;
    auto *text = reinterpret_cast<Il2CppString *>(value);
    return text->length >= 0 && text->length <= 512 ? Json(to_string(text)) : Json(nullptr);
  }
  Json Vector(const Vector3 &value)
  { return Json::array({value.x, value.y, value.z}); }
  Json Vector(const MethodInfo *method, Il2CppObject *object)
  {
    Vector3 value{};
    return Value(method, object, "UnityEngine.Vector3", value) ? Vector(value) : Json(nullptr);
  }
  Json BoundsOf(const MethodInfo *method, Il2CppObject *object)
  {
    Bounds value{};
    return Value(method, object, "UnityEngine.Bounds", value)
               ? Json{{"center", Vector(value.center)}, {"extents", Vector(value.extents)}}
               : Json(nullptr);
  }
  Json Int(const MethodInfo *method, Il2CppObject *object)
  {
    int value = 0;
    return Value(method, object, "System.Int32", value) ? Json(value) : Json(nullptr);
  }
} // namespace

void Leave()
{
  current_system = 0;
  recorded.clear();
}

void Observe(Il2CppObject *view, Il2CppObject *flat)
{
  if (!view || !flat || !Init() || !il2cpp_class_is_assignable_from(view_class, view->klass)
      || !il2cpp_class_is_assignable_from(flat_class, flat->klass))
    return;
  Root          view_root(view), flat_root(flat);
  Il2CppObject *data = nullptr, *renderer = nullptr, *filter = nullptr;
  il2cpp_field_get_value(view, pop_data, &data);
  Root    data_root(data);
  int64_t id = 0;
  if (!data_root.Get() || !Value(system_id, data, "System.Int64", id) || id <= 0)
    return;
  if (id != current_system) {
    current_system = id;
    recorded.clear();
    ++visit;
  }
  il2cpp_field_get_value(flat, renderer_field, &renderer);
  if (filter_field)
    il2cpp_field_get_value(flat, filter_field, &filter);
  Root renderer_root(renderer), filter_root(filter);
  Root mat(Ref(shared_material, renderer)), tex(Ref(main_texture, mat.Get()));
  Root native_mesh(Ref(shared_mesh, filter)), tr(Ref(transform, renderer));
  Root native_shader(Ref(shader, mat.Get())), collection(Ref(variation, data));
  if (!renderer_root.Get() || !mat.Get())
    return; // Retry while scenery is loading.
  const Json identity{{"renderer", Name(renderer)},
                      {"material", Name(mat.Get())},
                      {"texture", Name(tex.Get())},
                      {"mesh", Name(native_mesh.Get())}};
  const auto key = Json{{"identity", identity},
                        {"shader", Name(native_shader.Get())},
                        {"width", Int(width, tex.Get())},
                        {"height", Int(height, tex.Get())}}
                       .dump();
  if (recorded.contains(key))
    return;
  Json entry{{"schema", 1},
             {"timeUnixMs",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count()},
             {"systemId", id},
             {"visit", visit},
             {"selectedBy", "PlanetViewUtils.get_FlatRenderable"},
             {"identity", identity},
             {"variationCollection", Name(collection.Get())},
             {"shader", Name(native_shader.Get())},
             {"textureWidth", Int(width, tex.Get())},
             {"textureHeight", Int(height, tex.Get())},
             {"vertexCount", Int(vertex_count, native_mesh.Get())},
             {"meshLocalBounds", BoundsOf(mesh_bounds, native_mesh.Get())},
             {"rendererWorldBounds", BoundsOf(renderer_bounds, renderer)},
             {"position", Vector(position, tr.Get())},
             {"eulerAngles", Vector(angles, tr.Get())},
             {"localScale", Vector(scale, tr.Get())},
             {"configuredMaxZoom", Config::Get().zoom},
             {"configuredFrScale", Config::Get().fr_scale},
             {"frScaleApplied", false}};
  try {
    auto path = std::filesystem::u8path(File::Log());
#if __APPLE__
    if (!File::hasCustomNames())
      path = std::filesystem::path(File::MakePath(File::Log(), true));
#endif
    path = path.parent_path() / "system_backdrop_catalog.jsonl";
    std::ofstream file(path, std::ios::app | std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << entry.dump() << '\n';
    file.flush();
    recorded.insert(key);
    spdlog::info("[BackgroundCatalog] system={} visit={} identity={} path={}", id, visit, key, path.string());
  } catch (const std::exception &error) {
    if (!warned)
      spdlog::warn("[BackgroundCatalog] append failed: {}", error.what());
    warned = true;
  }
}
} // namespace backdrop_catalog
