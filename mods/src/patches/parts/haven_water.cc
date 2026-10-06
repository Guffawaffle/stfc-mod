#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
struct WaterApi {
  Il2CppClass      *renderer = nullptr;
  const MethodInfo *find = nullptr, *children = nullptr, *materials = nullptr;
  const MethodInfo *name = nullptr, *shader = nullptr, *instance_id = nullptr;
  const MethodInfo *get_enabled = nullptr, *set_enabled = nullptr;
  WaterApi()
  {
    auto cls = [](const char *name) {
      return il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", name).get_cls();
    };
    renderer  = cls("Renderer");
    find      = method_contract::Resolve(cls("GameObject"), "Find", true, "UnityEngine.GameObject", {"System.String"});
    children  = method_contract::Resolve(cls("GameObject"), "GetComponentsInChildren", false, "UnityEngine.Component[]",
                                         {"System.Type", "System.Boolean"});
    materials = method_contract::Resolve(renderer, "get_sharedMaterials", false, "UnityEngine.Material[]", {});
    name      = method_contract::Resolve(cls("Object"), "get_name", false, "System.String", {});
    shader    = method_contract::Resolve(cls("Material"), "get_shader", false, "UnityEngine.Shader", {});
    instance_id = method_contract::Resolve(cls("Object"), "GetInstanceID", false, "System.Int32", {});
    get_enabled = method_contract::Resolve(renderer, "get_enabled", false, "System.Boolean", {});
    set_enabled = method_contract::Resolve(renderer, "set_enabled", false, "System.Void", {"System.Boolean"});
  }
  bool Valid() const
  { return renderer && find && children && materials && name && shader && instance_id && get_enabled && set_enabled; }
};
WaterApi &Api()
{
  static WaterApi api;
  return api;
}
Il2CppObject *Invoke(const MethodInfo *method, Il2CppObject *object, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  return Il2CppRuntime::TryInvoke(method, object, args, &result) ? result : nullptr;
}
std::string Name(WaterApi &api, Il2CppObject *object)
{
  auto *value = object ? reinterpret_cast<Il2CppString *>(Invoke(api.name, object)) : nullptr;
  return value && value->length >= 0 && value->length <= 256 ? to_string(value) : std::string{};
}
} // namespace

bool HavenWaterVisibilityAvailable()
{ return Api().Valid(); }

// Shares the qualified Haven camera hook, including when Haven zoom is native.
// Retain scalar identities/original values only; rediscover Unity objects on each pass.
void ApplyHavenWaterVisibility(bool hidden)
{
  static std::unordered_map<int, bool> originals;
  if (!hidden && originals.empty())
    return;
  using Clock      = std::chrono::steady_clock;
  static auto next = Clock::time_point{};
  if (Clock::now() < next)
    return;
  next      = Clock::now() + std::chrono::seconds(1);
  auto &api = Api();
  if (!api.Valid())
    return;
  void *root_args[]  = {il2cpp_string_new("StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder")};
  auto *root         = Invoke(api.find, nullptr, root_args);
  bool  inactive     = true;
  void *child_args[] = {il2cpp_type_get_object(il2cpp_class_get_type(api.renderer)), &inactive};
  auto *renderers    = root ? reinterpret_cast<Il2CppArraySize *>(Invoke(api.children, root, child_args)) : nullptr;
  if (!renderers || renderers->max_length > 4096)
    return;
  std::unordered_set<int> seen;
  for (uintptr_t i = 0; i < renderers->max_length; ++i) {
    auto *renderer  = reinterpret_cast<Il2CppObject *>(renderers->vector[i]);
    auto *materials = renderer ? reinterpret_cast<Il2CppArraySize *>(Invoke(api.materials, renderer)) : nullptr;
    if (!materials || materials->max_length > 16)
      continue;
    bool water = false;
    for (uintptr_t j = 0; j < materials->max_length; ++j) {
      auto *material = reinterpret_cast<Il2CppObject *>(materials->vector[j]);
      if (Name(api, material) == "mat_PB_water" && Name(api, Invoke(api.shader, material)) == "Custom/WaterLite") {
        water = true;
        break;
      }
    }
    if (!water)
      continue;
    auto *boxed_id = Invoke(api.instance_id, renderer);
    bool  before   = false;
    if (!boxed_id || !method_contract::Type(il2cpp_class_get_type(boxed_id->klass), "System.Int32")
        || !Il2CppRuntime::TryBoolean(Invoke(api.get_enabled, renderer), before))
      continue;
    const auto id = *static_cast<int *>(il2cpp_object_unbox(boxed_id));
    seen.insert(id);
    if (hidden && !originals.contains(id)) {
      if (originals.size() >= 8)
        continue;
      originals.emplace(id, before);
    }
    const auto native = originals.find(id);
    if (native == originals.end())
      continue;
    // Restore our visibility change without disabling a renderer enabled by another owner.
    bool target   = hidden ? false : before || native->second;
    bool verified = true;
    if (before != target) {
      void *args[] = {&target};
      verified     = Il2CppRuntime::TryInvoke(api.set_enabled, renderer, args);
      bool after   = false;
      verified     = Il2CppRuntime::TryBoolean(Invoke(api.get_enabled, renderer), after) && after == target && verified;
      spdlog::info("[HavenWater] hidden={} verified={}", hidden, verified);
    }
    if (!hidden && verified)
      originals.erase(id);
  }
  std::erase_if(originals, [&seen](const auto &entry) { return !seen.contains(entry.first); });
}
