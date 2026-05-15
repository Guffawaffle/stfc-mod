/**
 * @file hotkey_router_native_fleet_guard.cc
 * @brief Singleton state + public hook delegates for the native fleet-selection guard.
 *
 * The pure decision helpers live in `hotkey_router_native_fleet_guard.h`; this
 * translation unit owns the per-process guard state and the bypass-depth
 * counter, and provides the public hook entry points declared in
 * `hotkey_router.h`.
 */
#include "patches/hotkey_router_native_fleet_guard.h"

#include "config.h"

#include "patches/hotkey_router.h"
#include "patches/hotkey_router_dispatch_cache.h"
#include "patches/hotkey_router_modifier_query.h"
#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "testable_functions.h"

namespace
{
struct NativeFleetSelectionGuardState {
  std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};
  bool                                                     suppress_native_shortcuts = false;
};

NativeFleetSelectionGuardState& native_fleet_selection_guard()
{
  static auto state = NativeFleetSelectionGuardState{};
  return state;
}

int& native_fleet_selection_bypass_depth()
{
  static int depth = 0;
  return depth;
}

bool native_fleet_selection_guard_bypassed()
{ return native_fleet_selection_bypass_depth() > 0; }
}  // namespace

bool hotkey_router_native_fleet_guard_slot(const std::size_t index)
{
  const auto& slots = native_fleet_selection_guard().slots;
  return index < slots.size() && slots[index];
}

bool hotkey_router_native_fleet_guard_suppress_native_shortcuts()
{ return native_fleet_selection_guard().suppress_native_shortcuts; }

void hotkey_router_update_native_fleet_selection_guard(
    const input_binding::DispatchPlan& plan, const std::span<const input_binding::DispatchKeyState> key_states,
    const bool dispatcher_owns_inputs)
{
  auto& guard = native_fleet_selection_guard();
  guard.slots = hotkey_router_update_native_fleet_selection_guard_slots(guard.slots, plan.winners, key_states,
                                                                        dispatcher_owns_inputs);
  guard.suppress_native_shortcuts = hotkey_router_native_shortcuts_suppressed(
      guard.suppress_native_shortcuts, plan.winners, key_states, dispatcher_owns_inputs);
}

void hotkey_router_update_native_shortcut_guard_from_physical_keys()
{
  auto&       cache            = hotkey_router_dispatch_cache::frame_runtime_dispatch_cache();
  const auto& runtime_bindings = input_binding::RuntimeBindingModel();
  hotkey_router_dispatch_cache::rebuild_frame_runtime_watched_keys(cache, runtime_bindings);

  cache.key_states.clear();
  cache.key_states.reserve(cache.watched_keys.size());

  const auto modifiers = hotkey_router_modifier_query::physical_held_modifier_mask();
  for (const auto key : cache.watched_keys) {
    const auto virtual_key = hotkey_router_modifier_query::win32_virtual_key_for_key_code(key);
    const auto pressed     = virtual_key != 0 && hotkey_router_modifier_query::win32_key_pressed(virtual_key);
    cache.key_states.push_back({key, modifiers, pressed, pressed});
  }

  input_binding::PlanDispatchSnapshot(runtime_bindings, input_binding::InputPhase::Frame,
                                      input_binding::ActiveLayers::All(), cache.key_states, cache.plan);
  const auto dispatcher_owns_inputs =
      Config::Get().hotkeys_enabled && ScopelyShortcutsPolicy() != ScopelyShortcutPolicy::Native;
  hotkey_router_update_native_fleet_selection_guard(cache.plan, cache.key_states, dispatcher_owns_inputs);
}

// ─── Public hook delegates declared in hotkey_router.h ────────────────────────

bool hotkey_router_should_suppress_native_fleet_selection(const int32_t index)
{
  const auto& slots = native_fleet_selection_guard().slots;
  return hotkey_router_native_fleet::should_suppress(index, slots, native_fleet_selection_guard_bypassed());
}

bool hotkey_router_should_suppress_any_native_fleet_selection()
{
  const auto& slots = native_fleet_selection_guard().slots;
  return hotkey_router_native_fleet::should_suppress_any(slots, native_fleet_selection_guard_bypassed());
}

HotkeyRouterNativeFleetSelectionBypass::HotkeyRouterNativeFleetSelectionBypass()
{ ++native_fleet_selection_bypass_depth(); }

HotkeyRouterNativeFleetSelectionBypass::~HotkeyRouterNativeFleetSelectionBypass()
{
  auto& depth = native_fleet_selection_bypass_depth();
  depth       = hotkey_router_native_fleet::bypass_decrement(depth);
}

bool hotkey_router_should_suppress_native_shortcuts()
{ return native_fleet_selection_guard().suppress_native_shortcuts; }

void hotkey_router_refresh_native_shortcut_suppression()
{ hotkey_router_update_native_shortcut_guard_from_physical_keys(); }
