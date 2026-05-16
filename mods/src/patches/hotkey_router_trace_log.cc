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

#include <array>
#include <cstddef>

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
  if (key >= KeyCode::Alpha0 && key <= KeyCode::Alpha9) {
    static constexpr std::array<std::string_view, 10> kNames = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    return kNames[static_cast<size_t>(static_cast<int>(key) - static_cast<int>(KeyCode::Alpha0))];
  }

  if (key >= KeyCode::A && key <= KeyCode::Z) {
    static constexpr std::array<std::string_view, 26> kNames = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                                                "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                                                "S", "T", "U", "V", "W", "X", "Y", "Z"};
    return kNames[static_cast<size_t>(static_cast<int>(key) - static_cast<int>(KeyCode::A))];
  }

  if (key >= KeyCode::F1 && key <= KeyCode::F15) {
    static constexpr std::array<std::string_view, 15> kNames = {"F1", "F2",  "F3",  "F4",  "F5",  "F6",  "F7", "F8",
                                                                "F9", "F10", "F11", "F12", "F13", "F14", "F15"};
    return kNames[static_cast<size_t>(static_cast<int>(key) - static_cast<int>(KeyCode::F1))];
  }

  if (key >= KeyCode::Keypad0 && key <= KeyCode::Keypad9) {
    static constexpr std::array<std::string_view, 10> kNames = {"KEY0", "KEY1", "KEY2", "KEY3", "KEY4",
                                                                "KEY5", "KEY6", "KEY7", "KEY8", "KEY9"};
    return kNames[static_cast<size_t>(static_cast<int>(key) - static_cast<int>(KeyCode::Keypad0))];
  }

  switch (key) {
    case KeyCode::None:
      return "NONE";
    case KeyCode::Backspace:
      return "BACKSPACE";
    case KeyCode::Tab:
      return "TAB";
    case KeyCode::Return:
      return "RETURN";
    case KeyCode::Pause:
      return "PAUSE";
    case KeyCode::Escape:
      return "ESCAPE";
    case KeyCode::Space:
      return "SPACE";
    case KeyCode::Exclaim:
      return "!";
    case KeyCode::DoubleQuote:
      return "\"";
    case KeyCode::Hash:
      return "#";
    case KeyCode::Dollar:
      return "$";
    case KeyCode::Percent:
      return "%";
    case KeyCode::Ampersand:
      return "&";
    case KeyCode::Quote:
      return "'";
    case KeyCode::LeftParen:
      return "(";
    case KeyCode::RightParen:
      return ")";
    case KeyCode::Asterisk:
      return "*";
    case KeyCode::Plus:
      return "+";
    case KeyCode::Comma:
      return ",";
    case KeyCode::Minus:
      return "-";
    case KeyCode::Period:
      return ".";
    case KeyCode::Slash:
      return "/";
    case KeyCode::Colon:
      return ":";
    case KeyCode::Semicolon:
      return ";";
    case KeyCode::Less:
      return "<";
    case KeyCode::Equals:
      return "=";
    case KeyCode::Greater:
      return ">";
    case KeyCode::Question:
      return "?";
    case KeyCode::At:
      return "@";
    case KeyCode::LeftBracket:
      return "[";
    case KeyCode::Backslash:
      return "\\";
    case KeyCode::RightBracket:
      return "]";
    case KeyCode::Caret:
      return "^";
    case KeyCode::Underscore:
      return "_";
    case KeyCode::BackQuote:
      return "`";
    case KeyCode::LeftCurlyBracket:
      return "{";
    case KeyCode::Pipe:
      return "|";
    case KeyCode::RightCurlyBracket:
      return "}";
    case KeyCode::Tilde:
      return "~";
    case KeyCode::Delete:
      return "DELETE";
    case KeyCode::KeypadPeriod:
      return "KEYPERIOD";
    case KeyCode::KeypadDivide:
      return "KEYDIVIDE";
    case KeyCode::KeypadMultiply:
      return "KEYMULTI";
    case KeyCode::KeypadMinus:
      return "KEYMINUS";
    case KeyCode::KeypadPlus:
      return "KEYPLUS";
    case KeyCode::KeypadEnter:
      return "KEYENTER";
    case KeyCode::KeypadEquals:
      return "KEYEQUAL";
    case KeyCode::UpArrow:
      return "UP";
    case KeyCode::DownArrow:
      return "DOWN";
    case KeyCode::RightArrow:
      return "RIGHT";
    case KeyCode::LeftArrow:
      return "LEFT";
    case KeyCode::Insert:
      return "INSERT";
    case KeyCode::Home:
      return "HOME";
    case KeyCode::End:
      return "END";
    case KeyCode::PageUp:
      return "PGUP";
    case KeyCode::PageDown:
      return "PGDOWN";
    case KeyCode::Numlock:
      return "NUMLOCK";
    case KeyCode::CapsLock:
      return "CAPS";
    case KeyCode::ScrollLock:
      return "SCROLL";
    case KeyCode::RightShift:
      return "RSHIFT";
    case KeyCode::LeftShift:
      return "LSHIFT";
    case KeyCode::RightControl:
      return "RCTRL";
    case KeyCode::LeftControl:
      return "LCTRL";
    case KeyCode::RightAlt:
      return "RALT";
    case KeyCode::LeftAlt:
      return "LALT";
    case KeyCode::RightCommand:
      return "RCOM";
    case KeyCode::LeftCommand:
      return "LCOM";
    case KeyCode::LeftWindows:
      return "LWIN";
    case KeyCode::RightWindows:
      return "RWIN";
    case KeyCode::AltGr:
      return "ALTGR";
    case KeyCode::Help:
      return "HELP";
    case KeyCode::Print:
      return "PRINT";
    case KeyCode::SysReq:
      return "SYSREQ";
    case KeyCode::Break:
      return "BREAK";
    case KeyCode::Menu:
      return "MENU";
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
    case KeyCode::Mouse5:
      return "MOUSE5";
    case KeyCode::Mouse6:
      return "MOUSE6";
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

bool hotkey_trace_space_action_probe_enabled()
{
  const auto level = RuntimeTraceLevelSetting();
  return level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
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
} // namespace

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
  spdlog::trace(
      "[HotkeyNativeGuard] frame hotkeys_enabled={} scopely_shortcuts={} original_frame_policy={} "
      "runtime_generation={} watched_keys={} unity_1_down={} unity_1_pressed={} unity_lalt={} unity_ralt={} "
      "win_1={} win_lalt={} win_ralt={} win_alt={} alt1_winner={} ship1_winner={} guard_slot0={} "
      "suppress_native_shortcuts={} show_cargo_default={}",
      Config::Get().hotkeys_enabled, scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
      original_frame_policy_name(OriginalFramePolicySetting()), input_binding::RuntimeBindingGeneration(),
      hotkey_router_dispatch_cache::frame_runtime_dispatch_cache().watched_keys.size(), Key::Down(KeyCode::Alpha1),
      Key::Pressed(KeyCode::Alpha1), Key::Pressed(KeyCode::LeftAlt), Key::Pressed(KeyCode::RightAlt),
      hotkey_router_modifier_query::win32_digit1_pressed(), hotkey_router_modifier_query::win32_left_alt_pressed(),
      hotkey_router_modifier_query::win32_right_alt_pressed(), hotkey_router_modifier_query::win32_alt_pressed(),
      alt1_winner, ship1_winner, hotkey_router_native_fleet_guard_slot(0),
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

void log_raw_input_probe()
{
#ifndef _MODDBG
  return;
#else
  if (RuntimeTraceLevelSetting() != RuntimeTraceLevel::Verbose) {
    return;
  }

  if (!hotkey_router_modifier_query::process_window_has_foreground()) {
    return;
  }

  static std::array<bool, static_cast<size_t>(KeyCode::Max)> last_unity_pressed{};
  static std::array<bool, static_cast<size_t>(KeyCode::Max)> last_win32_pressed{};

  const auto logical_modifiers  = hotkey_router_modifier_query::held_modifier_mask();
  const auto physical_modifiers = hotkey_router_modifier_query::physical_held_modifier_mask();

  const auto probe_key = [&](const KeyCode key) {
    const auto key_index = static_cast<size_t>(key);
    if (key == KeyCode::None || key_index >= static_cast<size_t>(KeyCode::Max)) {
      return;
    }

    const auto unity_down        = Key::Down(key);
    const auto unity_pressed     = Key::Pressed(key);
    const auto unity_edge        = unity_down || (unity_pressed && !last_unity_pressed[key_index]);
    const auto win32_virtual_key = hotkey_router_modifier_query::win32_virtual_key_for_key_code(key);
    const auto win32_pressed =
        win32_virtual_key != 0 && hotkey_router_modifier_query::win32_key_pressed(win32_virtual_key);
    const auto win32_edge = win32_pressed && !last_win32_pressed[key_index];

    if (unity_edge || win32_edge) {
      spdlog::info("[RawInputProbe] key={} code={} unity_down={} unity_pressed={} win32_vk={} win32_pressed={} "
                   "modifiers_logical={} modifiers_physical={}",
                   key_code_name(key), static_cast<int>(key), unity_down, unity_pressed, win32_virtual_key,
                   win32_pressed, logical_modifiers.logical_bits(), physical_modifiers.physical_bits());
    }

    last_unity_pressed[key_index] = unity_pressed;
    last_win32_pressed[key_index] = win32_pressed;
  };

  const auto probe_range = [&](const KeyCode first, const KeyCode last) {
    for (auto key_value = static_cast<int>(first); key_value <= static_cast<int>(last); ++key_value) {
      probe_key(static_cast<KeyCode>(key_value));
    }
  };

  probe_key(KeyCode::Backspace);
  probe_key(KeyCode::Tab);
  probe_key(KeyCode::Return);
  probe_key(KeyCode::Pause);
  probe_key(KeyCode::Escape);
  probe_key(KeyCode::Space);
  probe_range(KeyCode::Exclaim, KeyCode::Slash);
  probe_range(KeyCode::Alpha0, KeyCode::Alpha9);
  probe_range(KeyCode::Colon, KeyCode::At);
  probe_range(KeyCode::A, KeyCode::Z);
  probe_range(KeyCode::LeftBracket, KeyCode::BackQuote);
  probe_range(KeyCode::LeftCurlyBracket, KeyCode::Delete);
  probe_range(KeyCode::Keypad0, KeyCode::KeypadEquals);
  probe_range(KeyCode::UpArrow, KeyCode::PageDown);
  probe_range(KeyCode::F1, KeyCode::F15);

  for (const auto key :
       {KeyCode::Numlock, KeyCode::CapsLock, KeyCode::ScrollLock, KeyCode::RightShift, KeyCode::LeftShift,
        KeyCode::RightControl, KeyCode::LeftControl, KeyCode::RightAlt, KeyCode::LeftAlt, KeyCode::RightCommand,
        KeyCode::LeftCommand, KeyCode::LeftWindows, KeyCode::RightWindows, KeyCode::AltGr, KeyCode::Help,
        KeyCode::Print, KeyCode::SysReq, KeyCode::Break, KeyCode::Menu}) {
    probe_key(key);
  }

  probe_range(KeyCode::Mouse0, KeyCode::Mouse6);
#endif
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
} // namespace hotkey_router_trace_log
