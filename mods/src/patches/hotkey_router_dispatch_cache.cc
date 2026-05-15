/**
 * @file hotkey_router_dispatch_cache.cc
 * @brief Implementation of the per-frame dispatch cache.
 */
#include "patches/hotkey_router_dispatch_cache.h"

#include "config.h"

#include "patches/hotkey_router_action_table.h"
#include "patches/hotkey_router_modifier_query.h"
#include "patches/key.h"
#include "patches/mod_impact_monitor.h"

namespace hotkey_router_dispatch_cache
{
FrameRuntimeDispatchCache& frame_runtime_dispatch_cache()
{
  static auto cache = FrameRuntimeDispatchCache{};
  return cache;
}

void rebuild_frame_runtime_watched_keys(FrameRuntimeDispatchCache&          cache,
                                        const input_binding::CompileResult& runtime_bindings)
{
  const auto generation = input_binding::RuntimeBindingGeneration();
  if (cache.generation == generation) {
    return;
  }

  cache.watched_keys = input_binding::WatchedKeysForActions(runtime_bindings, input_binding::InputPhase::Frame,
                                                            hotkey_router_actions::kFrameActions);
  cache.generation   = generation;
}

void build_dispatch_key_snapshot(std::span<const KeyCode>                      watched_keys,
                                 std::vector<input_binding::DispatchKeyState>& key_states)
{
  key_states.clear();
  key_states.reserve(watched_keys.size());

  const auto modifiers = hotkey_router_modifier_query::held_modifier_mask();
  for (const auto key : watched_keys) {
    key_states.push_back({key, modifiers, Key::Down(key), Key::Pressed(key)});
  }
}

const input_binding::DispatchPlan& frame_runtime_dispatch_plan()
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyDispatchPlan, ModImpactMonitorEnabled());

  auto&       cache            = frame_runtime_dispatch_cache();
  const auto& runtime_bindings = input_binding::RuntimeBindingModel();
  rebuild_frame_runtime_watched_keys(cache, runtime_bindings);
  build_dispatch_key_snapshot(cache.watched_keys, cache.key_states);
  input_binding::PlanDispatchSnapshot(runtime_bindings, input_binding::InputPhase::Frame,
                                      input_binding::ActiveLayers::All(), cache.key_states, cache.plan);

  return cache.plan;
}
}  // namespace hotkey_router_dispatch_cache
