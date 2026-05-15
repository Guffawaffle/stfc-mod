/**
 * @file hotkey_router_trace_log.cc
 * @brief Implementation of the trace-log formatters and name lookups.
 */
#include "patches/hotkey_router_trace_log.h"

#include "config.h"

#include "patches/hotkey_router_action_table.h"
#include "patches/hotkey_router_dispatch_cache.h"
#include "patches/hotkey_router_modifier_query.h"
#include "patches/hotkey_router_native_fleet_guard.h"
#include "patches/hotkey_router_runtime_query.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "patches/key.h"
#include "testable_functions.h"

#include <spdlog/spdlog.h>

namespace hotkey_router_trace_log
{
std::string_view input_action_name(const input_binding::InputActionId action)
{
  if (const auto* spec = input_binding::FindActionSpec(action); spec) {
    return spec->canonical_key;
  }
  return "unknown";
}

std::string_view trigger_mode_name(const input_binding::TriggerMode trigger_mode)
{
  switch (trigger_mode) {
    case input_binding::TriggerMode::Down:
      return "down";
    case input_binding::TriggerMode::Pressed:
      return "pressed";
    default:
      return "unknown";
  }
}

std::string_view input_layer_name(const input_binding::InputLayer layer)
{
  switch (layer) {
    case input_binding::InputLayer::Global:
      return "global";
    case input_binding::InputLayer::Fleet:
      return "fleet";
    case input_binding::InputLayer::Diagnostics:
      return "diagnostics";
    case input_binding::InputLayer::Zoom:
      return "zoom";
    default:
      return "unknown";
  }
}

std::string_view key_code_name(const KeyCode key)
{
  switch (key) {
    case KeyCode::Mouse0:
      return "MOUSE0";
    case KeyCode::Mouse1:
      return "MOUSE1";
    case KeyCode::Mouse2:
      return "MOUSE2";
    case KeyCode::Mouse3:
      return "MOUSE3";
    case KeyCode::Mouse4:
      return "MOUSE4";
    case KeyCode::Space:
      return "SPACE";
    case KeyCode::BackQuote:
      return "`";
    case KeyCode::Alpha1:
      return "1";
    case KeyCode::Alpha2:
      return "2";
    case KeyCode::Alpha3:
      return "3";
    case KeyCode::Alpha4:
      return "4";
    case KeyCode::Alpha5:
      return "5";
    case KeyCode::Alpha6:
      return "6";
    case KeyCode::Alpha7:
      return "7";
    case KeyCode::Alpha8:
      return "8";
    case KeyCode::A:
      return "A";
    case KeyCode::C:
      return "C";
    case KeyCode::D:
      return "D";
    case KeyCode::F:
      return "F";
    case KeyCode::G:
      return "G";
    case KeyCode::H:
      return "H";
    case KeyCode::I:
      return "I";
    case KeyCode::L:
      return "L";
    case KeyCode::M:
      return "M";
    case KeyCode::O:
      return "O";
    case KeyCode::E:
      return "E";
    case KeyCode::Q:
      return "Q";
    case KeyCode::T:
      return "T";
    case KeyCode::U:
      return "U";
    case KeyCode::V:
      return "V";
    case KeyCode::Y:
      return "Y";
    default:
      return "other";
  }
}

std::string_view game_function_name(const GameFunction game_function)
{
  switch (game_function) {
    case GameFunction::ToggleCargoDefault:
      return "ToggleCargoDefault";
    case GameFunction::ToggleCargoPlayer:
      return "ToggleCargoPlayer";
    case GameFunction::ToggleCargoStation:
      return "ToggleCargoStation";
    case GameFunction::ToggleCargoHostile:
      return "ToggleCargoHostile";
    case GameFunction::ToggleCargoArmada:
      return "ToggleCargoArmada";
    case GameFunction::SelectShip1:
      return "SelectShip1";
    case GameFunction::SelectShip2:
      return "SelectShip2";
    case GameFunction::SelectShip3:
      return "SelectShip3";
    case GameFunction::SelectShip4:
      return "SelectShip4";
    case GameFunction::SelectShip5:
      return "SelectShip5";
    case GameFunction::SelectShip6:
      return "SelectShip6";
    case GameFunction::SelectShip7:
      return "SelectShip7";
    case GameFunction::SelectShip8:
      return "SelectShip8";
    case GameFunction::Max:
      return "Max";
    default:
      return "other";
  }
}

std::string_view dispatch_decision_name(const DispatchDecision decision)
{
  switch (decision) {
    case DispatchDecision::NoMatch:
      return "no-match";
    case DispatchDecision::HandledStop:
      return "handled-stop";
    case DispatchDecision::HandledAllowOriginal:
      return "handled-allow-original";
    default:
      return "unknown";
  }
}

std::string_view router_dispatch_action_name(const HotkeyRouterDispatchAction action)
{
  switch (action) {
    case HotkeyRouterDispatchAction::Continue:
      return "continue";
    case HotkeyRouterDispatchAction::SuppressOriginal:
      return "suppress-original";
    case HotkeyRouterDispatchAction::AllowOriginal:
      return "allow-original";
    default:
      return "unknown";
  }
}

std::string_view startup_action_name(const HotkeyRouterStartupAction action)
{
  switch (action) {
    case HotkeyRouterStartupAction::Continue:
      return "continue";
    case HotkeyRouterStartupAction::DisableHotkeys:
      return "disable-hotkeys";
    case HotkeyRouterStartupAction::EnableHotkeys:
      return "enable-hotkeys";
    case HotkeyRouterStartupAction::AllowOriginal:
      return "allow-original";
    case HotkeyRouterStartupAction::SuppressOriginal:
      return "suppress-original";
    default:
      return "unknown";
  }
}

bool hotkey_trace_key(const KeyCode key)
{
  switch (key) {
    case KeyCode::Alpha1:
    case KeyCode::Alpha2:
    case KeyCode::Alpha3:
    case KeyCode::Alpha4:
    case KeyCode::Alpha5:
    case KeyCode::Alpha6:
    case KeyCode::Alpha7:
    case KeyCode::Alpha8:
    case KeyCode::E:
    case KeyCode::Q:
      return true;
    default:
      return false;
  }
}

bool hotkey_trace_action(const input_binding::InputActionId action)
{
  switch (action) {
    case input_binding::InputActionId::SelectShip1:
    case input_binding::InputActionId::SelectShip2:
    case input_binding::InputActionId::SelectShip3:
    case input_binding::InputActionId::SelectShip4:
    case input_binding::InputActionId::SelectShip5:
    case input_binding::InputActionId::SelectShip6:
    case input_binding::InputActionId::SelectShip7:
    case input_binding::InputActionId::SelectShip8:
    case input_binding::InputActionId::ToggleCargoDefault:
    case input_binding::InputActionId::ToggleCargoPlayer:
    case input_binding::InputActionId::ToggleCargoStation:
    case input_binding::InputActionId::ToggleCargoHostile:
    case input_binding::InputActionId::ToggleCargoArmada:
    case input_binding::InputActionId::ShowEvents:
    case input_binding::InputActionId::ShowQTrials:
    case input_binding::InputActionId::ZoomIn:
    case input_binding::InputActionId::ZoomOut:
      return true;
    default:
      return false;
  }
}

namespace
{
bool hotkey_trace_key_signal(std::span<const input_binding::DispatchKeyState> key_states)
{
  for (const auto& state : key_states) {
    if (hotkey_trace_key(state.key) && (state.down || state.pressed)) {
      return true;
    }
  }
  return false;
}

bool hotkey_trace_plan_signal(const input_binding::DispatchPlan& plan)
{
  for (const auto& candidate : plan.candidates) {
    if (hotkey_trace_action(candidate.action)) {
      return true;
    }
  }
  for (const auto& winner : plan.winners) {
    if (hotkey_trace_action(winner.action)) {
      return true;
    }
  }
  return false;
}

void log_hotkey_trace_candidate(const char* phase, const input_binding::DispatchCandidate& candidate)
{
  spdlog::trace("[HotkeyTrace] {} action={} layer={} key={} trigger={} modifiers_logical={} modifiers_physical={} "
                "priority={} consumes_original={}",
                phase, input_action_name(candidate.action), input_layer_name(candidate.layer),
                key_code_name(candidate.key), trigger_mode_name(candidate.trigger_mode),
                candidate.held_modifiers.logical_bits(), candidate.held_modifiers.physical_bits(), candidate.priority,
                input_binding::ConsumesOriginalKeyEvent(candidate));
}
}  // namespace

void log_hotkey_trace_frame(std::span<const input_binding::DispatchKeyState> key_states,
                            const input_binding::DispatchPlan&               plan)
{
  if (!hotkey_trace_key_signal(key_states) && !hotkey_trace_plan_signal(plan)) {
    return;
  }

  spdlog::trace("[HotkeyTrace] frame candidates={} winners={}", plan.candidates.size(), plan.winners.size());

  for (const auto& state : key_states) {
    if (!hotkey_trace_key(state.key) || (!state.down && !state.pressed)) {
      continue;
    }
    spdlog::trace("[HotkeyTrace] key key={} down={} pressed={} modifiers_logical={} modifiers_physical={}",
                  key_code_name(state.key), state.down, state.pressed, state.held_modifiers.logical_bits(),
                  state.held_modifiers.physical_bits());
  }

  for (const auto& candidate : plan.candidates) {
    if (hotkey_trace_action(candidate.action)) {
      log_hotkey_trace_candidate("candidate", candidate);
    }
  }

  for (const auto& winner : plan.winners) {
    if (hotkey_trace_action(winner.action)) {
      log_hotkey_trace_candidate("winner", winner);
    }
  }

  const auto alt1_winner =
      plan.winner_lookup.Contains(input_binding::InputActionId::ToggleCargoDefault, input_binding::InputLayer::Global);
  const auto ship1_winner =
      plan.winner_lookup.Contains(input_binding::InputActionId::SelectShip1, input_binding::InputLayer::Fleet);
  spdlog::trace("[HotkeyNativeGuard] frame hotkeys_enabled={} scopely_shortcuts={} original_frame_policy={} "
                "runtime_generation={} watched_keys={} unity_1_down={} unity_1_pressed={} unity_lalt={} unity_ralt={} "
                "win_1={} win_lalt={} win_ralt={} win_alt={} alt1_winner={} ship1_winner={} guard_slot0={} "
                "suppress_native_shortcuts={} show_cargo_default={}",
                Config::Get().hotkeys_enabled, scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
                original_frame_policy_name(OriginalFramePolicySetting()), input_binding::RuntimeBindingGeneration(),
                hotkey_router_dispatch_cache::frame_runtime_dispatch_cache().watched_keys.size(),
                Key::Down(KeyCode::Alpha1), Key::Pressed(KeyCode::Alpha1), Key::Pressed(KeyCode::LeftAlt),
                Key::Pressed(KeyCode::RightAlt), hotkey_router_modifier_query::win32_digit1_pressed(),
                hotkey_router_modifier_query::win32_left_alt_pressed(),
                hotkey_router_modifier_query::win32_right_alt_pressed(),
                hotkey_router_modifier_query::win32_alt_pressed(), alt1_winner, ship1_winner,
                hotkey_router_native_fleet_guard_slot(0),
                hotkey_router_native_fleet_guard_suppress_native_shortcuts(), Config::Get().show_cargo_default);
}

void log_hotkey_trace_startup_gate(std::span<const input_binding::DispatchKeyState> key_states,
                                   const input_binding::DispatchPlan& plan, const HotkeyRouterStartupAction action)
{
  if (!hotkey_trace_key_signal(key_states) && !hotkey_trace_plan_signal(plan)) {
    return;
  }

  spdlog::trace("[HotkeyTrace] startup-gate action={} hotkeys_enabled={} scopely_shortcuts={}",
                startup_action_name(action), Config::Get().hotkeys_enabled,
                scopely_shortcut_policy_name(ScopelyShortcutsPolicy()));
}

void log_hotkey_trace_context_gate(const input_binding::DispatchPlan& plan, const bool is_in_chat,
                                   const bool input_focused)
{
  const auto action =
      hotkey_router_runtime_query::first_runtime_binding_winner(plan, hotkey_router_actions::kTableDispatch);
  if (!hotkey_trace_action(action)) {
    return;
  }

  spdlog::trace("[HotkeyTrace] context-gate action={} is_in_chat={} input_focused={} table_dispatch_enabled={}",
                input_action_name(action), is_in_chat, input_focused, !is_in_chat && !input_focused);
}

void log_runtime_winner(const char* route, const input_binding::DispatchPlan& plan,
                        const input_binding::InputActionId action, const input_binding::InputLayer layer)
{
  if (!ModImpactMonitorEnabled()) {
    return;
  }

  const auto* winner = hotkey_router_runtime_query::runtime_binding_winner(plan, action, layer);
  if (!winner) {
    return;
  }

  spdlog::info("[HotkeyDiag] route={} action={} key={} trigger={} modifiers_logical={} modifiers_physical={}", route,
               input_action_name(action), key_code_name(winner->key), trigger_mode_name(winner->trigger_mode),
               winner->held_modifiers.logical_bits(), winner->held_modifiers.physical_bits());
}
}  // namespace hotkey_router_trace_log
