/**
 * @file hotkey_router_trace_log.h
 * @brief Spdlog trace formatters + name lookups for the hotkey router.
 *
 * Trace logging is the noisiest concern in the router and the most volatile
 * (new actions, new probes, new diagnostics). Keeping it in its own module
 * means iterating on diagnostics never touches the routing logic itself.
 */
#pragma once

#include "patches/hotkey_dispatch.h"
#include "patches/hotkey_router.h"
#include "patches/input_binding/action_registry.h"
#include "patches/input_binding/input_dispatcher.h"
#include "testable_functions.h"

#include "prime/KeyCode.h"

#include <span>
#include <string_view>

namespace hotkey_router_trace_log
{
// ─── Name lookups ─────────────────────────────────────────────────────────────
std::string_view input_action_name(input_binding::InputActionId action);
std::string_view trigger_mode_name(input_binding::TriggerMode trigger_mode);
std::string_view input_layer_name(input_binding::InputLayer layer);
std::string_view key_code_name(KeyCode key);
std::string_view game_function_name(GameFunction game_function);
std::string_view dispatch_decision_name(DispatchDecision decision);
std::string_view router_dispatch_action_name(HotkeyRouterDispatchAction action);
std::string_view startup_action_name(HotkeyRouterStartupAction action);

// ─── Trace gates ──────────────────────────────────────────────────────────────
bool hotkey_trace_key(KeyCode key);
bool hotkey_trace_action(input_binding::InputActionId action);

// ─── Frame / startup / context formatters ─────────────────────────────────────
void log_hotkey_trace_frame(std::span<const input_binding::DispatchKeyState> key_states,
                            const input_binding::DispatchPlan&               plan);

void log_hotkey_trace_startup_gate(std::span<const input_binding::DispatchKeyState> key_states,
                                   const input_binding::DispatchPlan& plan, HotkeyRouterStartupAction action);

void log_hotkey_trace_context_gate(const input_binding::DispatchPlan& plan, bool is_in_chat, bool input_focused);

void log_runtime_winner(const char* route, const input_binding::DispatchPlan& plan,
                        input_binding::InputActionId action, input_binding::InputLayer layer);
}  // namespace hotkey_router_trace_log
