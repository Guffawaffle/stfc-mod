/**
 * @file hotkey_router_runtime_query.h
 * @brief Helpers for inspecting / acting on runtime-binding dispatch winners.
 *
 * The router itself only cares about *which* action won and what it should do
 * with that information; it does not need to know about candidate-list
 * traversal, ship-select bitmask construction, or table-dispatch handler
 * lookup. Those mechanics live here.
 */
#pragma once

#include "patches/hotkey_dispatch.h"
#include "patches/hotkey_router.h"
#include "patches/input_binding/action_registry.h"
#include "patches/input_binding/input_dispatcher.h"
#include "testable_functions.h"

#include <span>

namespace hotkey_router_runtime_query
{
/// True when @p plan has a winner registered for (@p action, @p layer).
bool runtime_binding_winner_present(const input_binding::DispatchPlan& plan, input_binding::InputActionId action,
                                    input_binding::InputLayer layer);

/// Pointer to the winning candidate for (@p action, @p layer), or nullptr.
const input_binding::DispatchCandidate*
runtime_binding_winner(const input_binding::DispatchPlan& plan, input_binding::InputActionId action,
                       input_binding::InputLayer layer);

/// True when the winner for (@p action, @p layer) requested original-key suppression.
bool runtime_binding_consumes_original_key_event(const input_binding::DispatchPlan& plan,
                                                 input_binding::InputActionId action, input_binding::InputLayer layer);

/// First action from @p actions that has a winner in @p plan, or `InputActionId::Max`.
input_binding::InputActionId
first_runtime_binding_winner(const input_binding::DispatchPlan&            plan,
                             std::span<const input_binding::InputActionId> actions);

/// Index of the requested ship-select slot (0-7), or -1 when no slot is requested.
int ship_select_request_from_runtime_bindings(const input_binding::DispatchPlan& plan);

/// Convenience wrapper around `input_binding::ActionGameFunction`.
GameFunction dispatcher_owned_game_function(input_binding::InputActionId action);

/// Look up a table-dispatch handler for @p candidate and run it; returns the router's verdict.
HotkeyRouterDispatchAction dispatch_runtime_bound_table_action(const input_binding::DispatchCandidate& candidate);

/// Execute the simple-fleet side effect bound to @p action (currently only `FleetQueueClear`).
void dispatch_runtime_bound_simple_fleet_action(input_binding::InputActionId action);

/// Translate the startup hotkeys-enable / hotkeys-disable winners into a router action.
HotkeyRouterStartupAction startup_action_from_runtime_bindings(const input_binding::DispatchPlan& plan,
                                                               ScopelyShortcutPolicy              scopely_shortcuts,
                                                               bool                               hotkeys_enabled);
}  // namespace hotkey_router_runtime_query
