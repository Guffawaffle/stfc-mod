#pragma once

#include <chrono>
#include <utility>
#include <vector>

namespace HavenVisuals
{
// Both Haven features share one owner and classification pass. Periodic scoped
// refresh is deliberate: streamed descendants and shared-material replacements
// do not necessarily destroy the owner or change its direct child count.
template <typename Backend> class SnapshotCache
{
public:
  using Clock = std::chrono::steady_clock;
  using Weak  = typename Backend::Weak;

  void Refresh(Clock::time_point now)
  {
    auto owner = Backend::GetOwner(owner_);
    if (!owner) {
      owner_ = {};
      roads_.clear();
      water_.clear();
      if (now < next_lookup_)
        return;
      next_lookup_ = now + std::chrono::seconds(5);
      owner        = Backend::FindOwner();
      if (!owner)
        return;
      owner_ = Backend::Retain(owner);
      // Never publish a cohort without a retained owner. Allocation failures
      // follow the same lookup backoff rather than retrying every camera call.
      if (!owner_)
        return;
      next_refresh_ = {};
    }
    if (now < next_refresh_)
      return;
    next_refresh_ = now + std::chrono::seconds(5);
    std::vector<typename Backend::Object> roads, water;
    if (!Backend::ScanOwner(owner, roads, water)) {
      // A failed/oversized read cannot establish that the previous cohort is
      // still owned here. Let callers restore it and retry after the backoff.
      roads_.clear();
      water_.clear();
      return;
    }
    std::vector<Weak> next_roads, next_water;
    for (auto object : roads) {
      if (next_roads.size() == 256)
        break;
      if (auto weak = Backend::Retain(object); weak)
        next_roads.push_back(std::move(weak));
    }
    for (auto object : water) {
      if (next_water.size() == 8)
        break;
      if (auto weak = Backend::Retain(object); weak)
        next_water.push_back(std::move(weak));
    }
    roads_ = std::move(next_roads);
    water_ = std::move(next_water);
  }

  const std::vector<Weak> &Roads() const
  { return roads_; }
  const std::vector<Weak> &Water() const
  { return water_; }

private:
  Weak              owner_;
  std::vector<Weak> roads_, water_;
  Clock::time_point next_lookup_{}, next_refresh_{};
};
} // namespace HavenVisuals

#ifndef STFC_HAVEN_VISUALS_PORTABLE_TEST
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <str_utils.h>

#include <string>
#include <unordered_set>

namespace HavenVisuals
{
inline Il2CppObject *Invoke(const MethodInfo *method, Il2CppObject *object, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  return Il2CppRuntime::TryInvoke(method, object, args, &result) ? result : nullptr;
}

struct DiscoveryApi {
  Il2CppClass      *renderer = nullptr;
  const MethodInfo *alive = nullptr, *active = nullptr, *find = nullptr, *children = nullptr;
  const MethodInfo *materials = nullptr, *name = nullptr, *shader = nullptr, *instance_id = nullptr;

  DiscoveryApi()
  {
    auto cls = [](const char *name) {
      return il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", name).get_cls();
    };
    renderer  = cls("Renderer");
    alive     = method_contract::Resolve(cls("Object"), "op_Implicit", true, "System.Boolean", {"UnityEngine.Object"});
    active    = method_contract::Resolve(cls("GameObject"), "get_activeInHierarchy", false, "System.Boolean", {});
    find      = method_contract::Resolve(cls("GameObject"), "Find", true, "UnityEngine.GameObject", {"System.String"});
    children  = method_contract::Resolve(cls("GameObject"), "GetComponentsInChildren", false, "UnityEngine.Component[]",
                                         {"System.Type", "System.Boolean"});
    materials = method_contract::Resolve(renderer, "get_sharedMaterials", false, "UnityEngine.Material[]", {});
    name      = method_contract::Resolve(cls("Object"), "get_name", false, "System.String", {});
    shader    = method_contract::Resolve(cls("Material"), "get_shader", false, "UnityEngine.Shader", {});
    instance_id = method_contract::Resolve(cls("Object"), "GetInstanceID", false, "System.Int32", {});
  }

  bool Valid() const
  { return renderer && alive && active && find && children && materials && name && shader && instance_id; }
};

inline DiscoveryApi &Api()
{
  static DiscoveryApi api;
  return api;
}

class WeakObject
{
public:
  WeakObject() = default;
  explicit WeakObject(Il2CppObject *object)
      : handle_(il2cpp_gchandle_new_weakref(object, false))
  {
  }
  WeakObject(const WeakObject &)            = delete;
  WeakObject &operator=(const WeakObject &) = delete;
  WeakObject(WeakObject &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr))
  {
  }
  WeakObject &operator=(WeakObject &&other) noexcept
  {
    if (this != &other) {
      if (handle_)
        il2cpp_gchandle_free(handle_);
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  ~WeakObject()
  {
    if (handle_)
      il2cpp_gchandle_free(handle_);
  }
  explicit operator bool() const
  { return handle_ != nullptr; }
  Il2CppObject *Get() const
  {
    auto *object = handle_ ? il2cpp_gchandle_get_target(handle_) : nullptr;
    bool  alive  = false;
    void *args[]{object};
    return object && Il2CppRuntime::TryBoolean(Invoke(Api().alive, nullptr, args), alive) && alive ? object : nullptr;
  }

private:
  Il2CppGCHandle handle_ = nullptr;
};

inline std::string Name(Il2CppObject *object)
{
  auto *value = object ? reinterpret_cast<Il2CppString *>(Invoke(Api().name, object)) : nullptr;
  return value && value->length >= 0 && value->length <= 256 ? to_string(value) : std::string{};
}

inline bool InstanceId(Il2CppObject *object, int &id)
{
  auto *boxed = Invoke(Api().instance_id, object);
  if (!boxed || !method_contract::Type(il2cpp_class_get_type(boxed->klass), "System.Int32"))
    return false;
  id = *static_cast<int *>(il2cpp_object_unbox(boxed));
  return true;
}

inline bool IsRoad(const std::string &name)
{
  for (const char *prefix :
       {"mat_PB_3101101_asphalt_", "mat_PB_3101201_asphalt_", "mat_PB_3101301_asphalt_", "mat_PB_3101401_asphalt_",
        "mat_PB_3101501_asphalt_", "mat_PB_3101601_asphalt_", "mat_PB_3101701_asphalt_"}) {
    if (name.starts_with(prefix))
      return true;
  }
  return false;
}

struct NativeBackend {
  using Object = Il2CppObject *;
  using Weak   = WeakObject;
  static Object GetOwner(const Weak &weak)
  {
    auto *object = weak.Get();
    bool  active = false;
    return object && Il2CppRuntime::TryBoolean(Invoke(Api().active, object), active) && active ? object : nullptr;
  }
  static Weak Retain(Object object)
  { return Weak(object); }
  static Object FindOwner()
  {
    void *args[]{il2cpp_string_new("StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder")};
    return Invoke(Api().find, nullptr, args);
  }
  static bool ScanOwner(Object owner, std::vector<Object> &roads, std::vector<Object> &water)
  {
    auto &api      = Api();
    bool  inactive = true;
    void *args[]{il2cpp_type_get_object(il2cpp_class_get_type(api.renderer)), &inactive};
    auto *renderers = reinterpret_cast<Il2CppArraySize *>(Invoke(api.children, owner, args));
    if (!renderers || renderers->max_length > 4096)
      return false;
    std::unordered_set<int> road_ids, water_ids;
    for (uintptr_t i = 0; i < renderers->max_length; ++i) {
      auto *renderer  = reinterpret_cast<Object>(renderers->vector[i]);
      auto *materials = renderer ? reinterpret_cast<Il2CppArraySize *>(Invoke(api.materials, renderer)) : nullptr;
      if (!materials || materials->max_length > 16)
        continue;
      for (uintptr_t j = 0; j < materials->max_length; ++j) {
        auto      *material = reinterpret_cast<Object>(materials->vector[j]);
        const auto name     = Name(material);
        int        id       = 0;
        if (material && IsRoad(name) && roads.size() < 256 && Name(Invoke(api.shader, material)) == "Custom/LitMSAE"
            && InstanceId(material, id) && road_ids.insert(id).second)
          roads.push_back(material);
        if (material && name == "mat_PB_water" && water.size() < 8
            && Name(Invoke(api.shader, material)) == "Custom/WaterLite" && InstanceId(renderer, id)
            && water_ids.insert(id).second)
          water.push_back(renderer);
      }
    }
    return true;
  }
};

inline SnapshotCache<NativeBackend> &Cache()
{
  // Unity/IL2CPP may shut down before module static destructors. Runtime
  // replacement frees retired handles; this bounded owner lives until exit.
  static auto *cache = new SnapshotCache<NativeBackend>;
  return *cache;
}
} // namespace HavenVisuals
#endif
