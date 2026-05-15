/**
 * @file hotkey_router_dispatch_cache.h
 * @brief Per-frame dispatch-plan cache shared by the router and trace log.
 *
 * The cache holds the watched-key set, the most recent key snapshot, and the
 * resolved `DispatchPlan` for the current frame so callers don't pay for
 * recomputing them when several routing stages need the same data.
 */
#pragma once

#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "prime/KeyCode.h"

#include <cstdint>
#include <span>
#include <vector>

namespace hotkey_router_dispatch_cache
{
struct FrameRuntimeDispatchCache {
  std::uint64_t                                generation = 0;
  std::vector<KeyCode>                         watched_keys;
  std::vector<input_binding::DispatchKeyState> key_states;
  input_binding::DispatchPlan                  plan;
};

/// Process-wide cache instance — one frame worth of dispatch state.
FrameRuntimeDispatchCache& frame_runtime_dispatch_cache();

/// Refresh `cache.watched_keys` when the runtime binding generation has advanced.
void rebuild_frame_runtime_watched_keys(FrameRuntimeDispatchCache&          cache,
                                        const input_binding::CompileResult& runtime_bindings);

/// Snapshot every watched key's current state into @p key_states.
void build_dispatch_key_snapshot(std::span<const KeyCode>                      watched_keys,
                                 std::vector<input_binding::DispatchKeyState>& key_states);

/**
 * @brief Resolve and return the per-frame dispatch plan.
 *
 * Refreshes the watched-key set, builds the key snapshot, and asks the
 * binding system for a `DispatchPlan`. Callers should treat the returned
 * reference as valid only until the next invocation.
 */
const input_binding::DispatchPlan& frame_runtime_dispatch_plan();
}  // namespace hotkey_router_dispatch_cache
