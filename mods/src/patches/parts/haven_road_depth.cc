#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <patches/haven_visuals_cache.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
struct RoadDepthApi {
  const MethodInfo *has = nullptr, *get = nullptr, *set = nullptr;

  RoadDepthApi()
  {
    auto cls = [](const char *n) {
      return il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", n).get_cls();
    };
    auto *material = cls("Material");
    has            = method_contract::Resolve(material, "HasProperty", false, "System.Boolean", {"System.String"});
    get            = method_contract::Resolve(material, "GetFloat", false, "System.Single", {"System.String"});
    set = method_contract::Resolve(material, "SetFloat", false, "System.Void", {"System.String", "System.Single"});
  }

  bool Valid() const
  { return HavenVisuals::Api().Valid() && has && get && set; }
};

Il2CppObject *Invoke(const MethodInfo *method, Il2CppObject *object, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  return Il2CppRuntime::TryInvoke(method, object, args, &result) ? result : nullptr;
}

bool ReadDepth(RoadDepthApi &api, Il2CppObject *material, Il2CppString *property, float &value)
{
  void *args[]  = {property};
  bool  present = false;
  if (!Il2CppRuntime::TryBoolean(Invoke(api.has, material, args), present) || !present)
    return false;
  auto *boxed = Invoke(api.get, material, args);
  if (!boxed || !method_contract::Type(il2cpp_class_get_type(boxed->klass), "System.Single"))
    return false;
  value = *static_cast<float *>(il2cpp_object_unbox(boxed));
  return std::isfinite(value);
}
} // namespace

// Called only by the qualified Haven camera path. The small bias stays on at
// every zoom level while expansion is enabled, and is restored when disabled.
// Discovery is shared with water visibility. Only the small set of relevant
// materials is read on normal passes; weak references never retain scene assets.
void ApplyHavenRoadDepthBias(bool enabled)
{
  struct Original {
    HavenVisuals::WeakObject material;
    std::array<float, 2>     depth;
  };
  // Do not run IL2CPP handle destructors after the native runtime shuts down.
  static auto &originals = *new std::unordered_map<int, Original>;
  if (!enabled && originals.empty())
    return;
  using Clock      = std::chrono::steady_clock;
  static auto next = Clock::time_point{};
  if (Clock::now() < next)
    return;
  next = Clock::now() + std::chrono::seconds(1);
  static RoadDepthApi api;
  if (!api.Valid()) {
    static bool warned = false;
    if (!warned) {
      if (!HavenVisuals::Api().Valid())
        spdlog::warn("[HavenZoom] road depth unavailable: incompatible Haven discovery API");
      for (const auto &[method, signature] : std::array<std::pair<const MethodInfo *, const char *>, 3>{
               {{api.has, "Material.HasProperty(String) -> Boolean"},
                {api.get, "Material.GetFloat(String) -> Single"},
                {api.set, "Material.SetFloat(String, Single) -> Void"}}}) {
        if (!method)
          spdlog::warn("[HavenZoom] keeping native road depth: missing or incompatible UnityEngine.{}", signature);
      }
      warned = true;
    }
    return;
  }
  auto &cache = HavenVisuals::Cache();
  if (enabled)
    cache.Refresh(Clock::now());
  const std::array<HavenVisuals::StrongObject, 2> property_roots{
      HavenVisuals::StrongObject(reinterpret_cast<Il2CppObject *>(il2cpp_string_new("_OffsetFactor"))),
      HavenVisuals::StrongObject(reinterpret_cast<Il2CppObject *>(il2cpp_string_new("_OffsetUnits")))};
  if (!property_roots[0] || !property_roots[1])
    return;
  const std::array<Il2CppString *, 2> properties{reinterpret_cast<Il2CppString *>(property_roots[0].Get()),
                                                 reinterpret_cast<Il2CppString *>(property_roots[1].Get())};
  // Keep originals with their actual object, not just an instance ID. A streamed
  // material can disappear from discovery while its shared asset remains alive.
  std::unordered_map<int, Il2CppObject *> active;
  std::vector<HavenVisuals::StrongObject> active_roots;
  if (enabled) {
    for (const auto &weak : cache.Roads()) {
      HavenVisuals::StrongObject root(weak.Get());
      auto                      *material = root.Get();
      int                        id       = 0;
      if (material && HavenVisuals::InstanceId(material, id)) {
        active.emplace(id, material);
        active_roots.push_back(std::move(root));
      }
    }
  }
  for (auto it = originals.begin(); it != originals.end();) {
    HavenVisuals::StrongObject root(it->second.material.Get());
    auto                      *material = root.Get();
    const auto                 found    = active.find(it->first);
    if (!material || (found != active.end() && material != found->second)) {
      it = originals.erase(it);
      continue;
    }
    if (found != active.end()) {
      ++it;
      continue;
    }
    bool restored = true;
    for (unsigned k = 0; k < 2; ++k) {
      float before = 0.0f;
      if (!ReadDepth(api, material, properties[k], before)) {
        restored = false;
        continue;
      }
      // Do not overwrite another owner's edit after our last applied bias.
      if (before != std::min(it->second.depth[k], -1.0f))
        continue;
      float target = it->second.depth[k];
      if (target == before)
        continue;
      void *args[]{properties[k], &target};
      float after = 0.0f;
      restored    = Il2CppRuntime::TryInvoke(api.set, material, args) && ReadDepth(api, material, properties[k], after)
                    && after == target && restored;
    }
    if (restored)
      it = originals.erase(it);
    else
      ++it;
  }
  for (const auto &[id, material] : active) {
    std::array<float, 2> before{};
    if (!ReadDepth(api, material, properties[0], before[0]) || !ReadDepth(api, material, properties[1], before[1]))
      continue;
    if ((before[0] > -1.0f || before[1] > -1.0f) && !originals.contains(id)) {
      if (originals.size() >= 256)
        continue;
      HavenVisuals::WeakObject weak(material);
      if (!weak)
        continue;
      originals.emplace(id, Original{std::move(weak), before});
    }
    bool changed = false, verified = true;
    for (unsigned k = 0; k < 2; ++k) {
      // Preserve an already stronger native bias.
      float target = std::min(before[k], -1.0f);
      if (target == before[k])
        continue;
      void *args[] = {properties[k], &target};
      if (!Il2CppRuntime::TryInvoke(api.set, material, args)) {
        verified = false;
        continue;
      }
      changed     = true;
      float after = 0.0f;
      verified    = ReadDepth(api, material, properties[k], after) && after == target && verified;
    }
    if (changed) {
      spdlog::debug("[HavenZoom] road depth bias material {}: verified={}", id, verified);
      static bool reported = false;
      if (enabled && verified && !reported) {
        spdlog::info("[HavenZoom] corrected road surface depth for expanded Haven zoom");
        reported = true;
      }
    }
  }
}
