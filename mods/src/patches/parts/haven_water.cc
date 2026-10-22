#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <patches/haven_visuals_cache.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <unordered_map>

namespace
{
struct WaterApi {
  const MethodInfo *get_enabled = nullptr, *set_enabled = nullptr;
  WaterApi()
  {
    auto *renderer = HavenVisuals::Api().renderer;
    get_enabled    = method_contract::Resolve(renderer, "get_enabled", false, "System.Boolean", {});
    set_enabled    = method_contract::Resolve(renderer, "set_enabled", false, "System.Void", {"System.Boolean"});
  }
  bool Valid() const
  { return HavenVisuals::Api().Valid() && get_enabled && set_enabled; }
};
WaterApi &Api()
{
  static WaterApi api;
  return api;
}
} // namespace

bool HavenWaterVisibilityAvailable()
{ return Api().Valid(); }

// Shares owner discovery and material classification with road depth. Weak
// references retain neither scene renderers nor their original shared assets.
void ApplyHavenWaterVisibility(bool hidden)
{
  struct Original {
    HavenVisuals::WeakObject renderer;
    bool                     enabled;
  };
  // Do not run IL2CPP handle destructors after the native runtime shuts down.
  static auto &originals = *new std::unordered_map<int, Original>;
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

  auto                                   &cache = HavenVisuals::Cache();
  std::unordered_map<int, Il2CppObject *> active;
  if (hidden) {
    cache.Refresh(Clock::now());
    for (const auto &weak : cache.Water()) {
      auto *renderer = weak.Get();
      int   id       = 0;
      if (renderer && HavenVisuals::InstanceId(renderer, id))
        active.emplace(id, renderer);
    }
  }
  for (auto it = originals.begin(); it != originals.end();) {
    auto      *renderer = it->second.renderer.Get();
    const auto found    = active.find(it->first);
    if (!renderer || (found != active.end() && renderer != found->second)) {
      it = originals.erase(it);
      continue;
    }
    if (found != active.end()) {
      ++it;
      continue;
    }
    bool before   = false;
    bool restored = Il2CppRuntime::TryBoolean(HavenVisuals::Invoke(api.get_enabled, renderer), before);
    // Restore our visibility change without disabling a renderer enabled by
    // another owner. Also restore live assets dropped by a streaming refresh.
    bool target = before || it->second.enabled;
    if (restored && target != before) {
      void *args[]{&target};
      bool  after = false;
      restored    = Il2CppRuntime::TryInvoke(api.set_enabled, renderer, args)
                    && Il2CppRuntime::TryBoolean(HavenVisuals::Invoke(api.get_enabled, renderer), after)
                    && after == target;
      spdlog::info("[HavenWater] hidden=false verified={}", restored);
    }
    if (restored)
      it = originals.erase(it);
    else
      ++it;
  }
  for (const auto &[id, renderer] : active) {
    bool before = false;
    if (!Il2CppRuntime::TryBoolean(HavenVisuals::Invoke(api.get_enabled, renderer), before))
      continue;
    if (!originals.contains(id)) {
      if (originals.size() >= 8)
        continue;
      HavenVisuals::WeakObject weak(renderer);
      if (!weak)
        continue;
      originals.emplace(id, Original{std::move(weak), before});
    }
    if (!before)
      continue;
    bool       target = false;
    void      *args[]{&target};
    bool       after    = true;
    const bool verified = Il2CppRuntime::TryInvoke(api.set_enabled, renderer, args)
                          && Il2CppRuntime::TryBoolean(HavenVisuals::Invoke(api.get_enabled, renderer), after)
                          && after == target;
    spdlog::info("[HavenWater] hidden=true verified={}", verified);
  }
}
