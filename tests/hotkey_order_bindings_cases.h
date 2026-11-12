#pragma once

// Exercise the real top-level shortcut ordering with the screen fixture's live
// native owners. Scale effects and unrelated badge/chat/quit services are sinks.
#ifndef _MODDBG
#define HOTKEY_FIXTURE_DEFINED_MODDBG
#define _MODDBG 1
#endif
#include "../src/mod/features/hotkeys.cc"
#ifdef HOTKEY_FIXTURE_DEFINED_MODDBG
#undef _MODDBG
#undef HOTKEY_FIXTURE_DEFINED_MODDBG
#endif

namespace
{
int  hotkey_order_scale = 0;
void KeyboardConfirmFocusNativeCases()
{
  has_event_system = has_input_field = true;
  selected                           = &selected_object;
  event_system_lookup_throws = selected_lookup_throws = component_lookup_throws = input_focus_throws = false;
  fleet_native_capture                                                                               = false;
  auto*                                   receiver = reinterpret_cast<KeyboardConfirmHandler*>(&event_system);
  const unity::InputActionCallbackContext context{&selected_object, 17};
  int                                     calls = 0;
  const auto original = [&](KeyboardConfirmHandler* owner, unity::InputActionCallbackContext received) {
    Require(owner == receiver && received.state == context.state && received.action_index == context.action_index,
            "native keyboard confirmation keeps its exact receiver and callback context");
    ++calls;
  };
  const auto sample = [] {
    ++input_frame;
    unity::Key::Poll();
  };

  input_field_focused = true;
  for (const auto key : {unity::KeyCode::Space, unity::KeyCode::Return, unity::KeyCode::KeypadEnter}) {
    held_keys.fill(false);
    down_keys.fill(false);
    held_keys[std::to_underlying(key)] = down_keys[std::to_underlying(key)] = true;
    any_held = any_down = true;
    sample();
    KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
    Require(calls == 0, "typing a native confirm key must not activate a popup behind chat");
  }
  // Chat submission may blur the field before the confirm callback arrives in this same frame.
  input_field_focused = false;
  KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
  Require(calls == 0, "the Enter that submitted chat cannot also confirm after native submission clears focus");
  sample();
  KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
  Require(calls == 1, "a later unfocused frame restores exactly one native popup confirmation");

  fleet_native_capture = true;
  sample();
  KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
  Require(calls == 1, "shortcut capture also owns the native popup confirm action");
  fleet_native_capture    = false;
  component_lookup_throws = true;
  sample();
  KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
  Require(calls == 1, "unavailable text focus reserves the native confirm key");
  component_lookup_throws = false;
  selected                = nullptr;
  sample();
  KeyboardConfirmHandler_OnConfirmPerformed_Hook(original, receiver, context);
  Require(calls == 2, "ordinary native confirmation remains available with no selected text field");
  std::cout
      << "Native popup keyboard confirmation: text focus, submit ordering, capture and unavailable focus passed\n";
}

void HotkeyFocusNativeCases()
{
  ScreenActionsNativeCases("screen-actions-dispatch", [] {
    hotkeys_installed = true;
    selected          = &selected_object;
    has_event_system = has_input_field = input_field_focused = true;
    const auto navigations                                   = screen_actions_navigations;
    const auto sample                                        = [] {
      ++input_frame;
      unity::Key::Poll();
    };
    for (bool* failure :
         {&component_lookup_throws, &input_focus_throws, &selected_lookup_throws, &event_system_lookup_throws}) {
      *failure          = true;
      fleet_native_keys = {Action::kShowFactions};
      sample();
      Require(!HandleShortcuts() && screen_actions_navigations == navigations,
              "unavailable text focus must not dispatch the faction-store shortcut or consume native Update");
      unity::Key::ClearInputFocus();
      Require(selected == &selected_object, "unknown text focus must not clear native selection");
      fleet_native_keys.clear();
      Require(!HandleShortcuts(), "unavailable text focus leaves ordinary native input delivery available");
      *failure          = false;
      fleet_native_keys = {Action::kShowFactions};
      sample();
      Require(!HandleShortcuts() && screen_actions_navigations == navigations,
              "a later known focused field continues to own typed shortcut keys");
    }
    input_field_focused = false;
    sample();
    Require(HandleShortcuts() && screen_actions_navigations == navigations + 1,
            "confirmed unfocused text field restores one ordinary faction-store navigation");
    fleet_native_keys.clear();
    input_field_focused = true;
    selected            = nullptr;
    sample();
  });
  std::cout
      << "Actual shortcut dispatcher: unavailable text focus preserves native input and blocks faction navigation\n";
}
void HotkeyFocusRecoveryNativeCases(std::string_view mode)
{
  // ScreenActionsNativeCases supplies live native owners through a noncapturing callback. Only raw Escape and
  // the production focus bindings are exercised here; configured action matching/capture remain controlled sinks.
  static std::string_view recovery_mode;
  recovery_mode = mode;
  ScreenActionsNativeCases("screen-actions-dispatch", [] {
    hotkeys_installed = true;
    selected          = &selected_object;
    has_event_system = has_input_field = input_field_focused = true;
    event_system_lookup_throws = selected_lookup_throws = component_lookup_throws = input_focus_throws = false;
    selection_set_throws = selection_set_throws_after = redirect_selection = false;
    input_focus_throw_at                                                   = 0;

    Config::Get().ui.disable_escape_exit       = true;
    Config::Get().ui.disable_escape_exit_timer = 0;
    screen_actions_section                     = SectionID::Navigation_System;
    fleet_native_keys                          = {Action::kShowFactions};
    const auto navigations                     = screen_actions_navigations;
    const auto clears                          = selection_set_calls;
    const auto sample                          = [](bool escape, bool down = false) {
      held_keys.fill(false);
      down_keys.fill(false);
      held_keys[std::to_underlying(unity::KeyCode::Escape)] = escape;
      down_keys[std::to_underlying(unity::KeyCode::Escape)] = down;
      any_held                                              = escape;
      any_down                                              = down;
      ++input_frame;
      unity::Key::Poll();
    };

    bool* failure = nullptr;
    if (recovery_mode == "chat-focus-recovery-event-system")
      failure = &event_system_lookup_throws;
    else if (recovery_mode == "chat-focus-recovery-selection")
      failure = &selected_lookup_throws;
    else if (recovery_mode == "chat-focus-recovery-component")
      failure = &component_lookup_throws;
    else if (recovery_mode == "chat-focus-recovery-field")
      failure = &input_focus_throws;

    if (failure) {
      *failure = true;
      for (const auto section : {SectionID::Navigation_System, SectionID::Chat_Main}) {
        screen_actions_section = section;
        for (int frame = 0; frame != 2; ++frame) {
          sample(true, frame == 0);
          Require(!HandleShortcuts() && selected == &selected_object && selection_set_calls == clears
                      && screen_actions_navigations == navigations,
                  "unknown focus retains native Escape on every held frame without clearing or navigating");
          Require(!unity::Key::TryIsInputFocused(), "the cached diagnostic observation retains unknown focus");
        }
      }
      screen_actions_section = SectionID::Navigation_System;
      sample(false);
      Require(!HandleShortcuts() && screen_actions_navigations == navigations && selection_set_calls == clears,
              "unknown focus still reserves ordinary faction shortcut input without an Escape press");
      *failure = false;
      sample(false);
      Require(!HandleShortcuts() && screen_actions_navigations == navigations,
              "a recovered known focused field continues to own ordinary shortcut input");
    } else if (recovery_mode == "chat-focus-recovery-setter-before"
               || recovery_mode == "chat-focus-recovery-setter-after") {
      const bool after           = recovery_mode == "chat-focus-recovery-setter-after";
      selection_set_throws       = !after;
      selection_set_throws_after = after;
      int attempts               = 0;
      for (const auto section : {SectionID::Navigation_System, SectionID::Chat_Main}) {
        screen_actions_section = section;
        selected               = &selected_object;
        sample(true, true);
        ++attempts;
        Require(!HandleShortcuts() && selection_set_calls == clears + attempts
                    && selected == (after ? nullptr : &selected_object) && screen_actions_navigations == navigations,
                "failed clear invocation retains native Escape whether or not its setter already mutated selection");
      }
      selection_set_throws = selection_set_throws_after = false;
      screen_actions_section                            = SectionID::Navigation_System;
      selected                                          = &selected_object;
      sample(true, true);
      Require(HandleShortcuts() && selected == nullptr && selection_set_calls == clears + attempts + 1
                  && !unity::Key::IsInputFocused() && screen_actions_navigations == navigations,
              "a later successful clear consumes Escape and invalidates focused state within the same frame");
    } else if (recovery_mode == "chat-focus-recovery-known-focused"
               || recovery_mode == "chat-focus-recovery-redirect") {
      redirect_selection = recovery_mode == "chat-focus-recovery-redirect";
      sample(false);
      Require(!HandleShortcuts() && screen_actions_navigations == navigations,
              "a known focused field blocks ordinary configured actions before Escape");
      sample(true, true);
      Require(HandleShortcuts() && selection_set_calls == clears + 1 && screen_actions_navigations == navigations,
              "a successful confirmed-focus clear invocation consumes its Escape");
      const auto reads = event_system_reads;
      Require(unity::Key::IsInputFocused() == redirect_selection && event_system_reads == reads + 1
                  && selected == (redirect_selection ? &selected_object : nullptr),
              "clear invalidates the frame cache and observes a callback's redirected focus without claiming blur");
      redirect_selection = false;
    } else if (recovery_mode == "chat-focus-recovery-late-observation") {
      spdlog::set_level(spdlog::level::debug);
      input_field_focused    = false;
      screen_actions_section = SectionID::Chat_Main;
      input_focus_throw_at   = input_focus_reads + 2;
      sample(true, true);
      Require(!HandleShortcuts() && selected == &selected_object && selection_set_calls == clears
                  && screen_actions_navigations == navigations,
              "an initially unfocused full-chat snapshot cannot mask a later failed clear observation");
      input_focus_throw_at = 0;
    } else {
      Require(recovery_mode == "chat-focus-recovery-known-unfocused-fullchat", "unknown focus recovery fixture mode");
      input_field_focused    = false;
      screen_actions_section = SectionID::Chat_Main;
      for (int frame = 0; frame != 2; ++frame) {
        sample(true, frame == 0);
        Require(HandleShortcuts() && selected == &selected_object && selection_set_calls == clears
                    && screen_actions_navigations == navigations,
                "known unfocused full chat preserves its existing Escape ownership without a clear request");
      }
    }

    // A known unfocused observation, rather than a failed read or an application-focus toggle, releases keys.
    screen_actions_section = SectionID::Navigation_System;
    selected               = &selected_object;
    input_field_focused    = false;
    sample(false);
    Require(HandleShortcuts() && screen_actions_navigations == navigations + 1,
            "confirmed unfocused input restores exactly one ordinary faction-store navigation");
    const auto known = unity::Key::TryIsInputFocused();
    Require(known && !known.value, "a successful false focus observation stays distinct from unknown");
    fleet_native_keys.clear();
    selected = nullptr;
    sample(false);
    Config::Get().ui.disable_escape_exit = false;
  });
  std::cout << "Actual shortcut dispatcher: raw Escape focus recovery passed " << mode << '\n';
}
void HotkeyOrderNativeCases()
{
  ScreenActionsNativeCases("screen-actions-dispatch", [] {
    hotkeys_installed   = true;
    const auto requests = screen_actions_requests;
    fleet_native_keys   = {Action::kShowShips, Action::kUiScaleUp};
    Require(!HandleShortcuts() && hotkey_order_scale == 1 && screen_actions_requests == requests,
            "actual dispatcher lets settings precede Manage Ships on a shared chord and retains native Update");
    fleet_native_keys = {Action::kShowShips};
    Require(!HandleShortcuts() && screen_actions_requests == requests + 1,
            "lone late Manage Ships keeps native Update and submits the original selected-fleet action");
    fleet_native_keys    = {Action::kShowShips, Action::kUiScaleUp};
    fleet_native_capture = true;
    Require(!HandleShortcuts() && hotkey_order_scale == 1 && screen_actions_requests == requests + 1,
            "capture owns both the late screen and setting shortcuts");
    fleet_native_capture                      = false;
    Config::Get().control.use_scopely_hotkeys = true;
    Require(!HandleShortcuts() && hotkey_order_scale == 1 && screen_actions_requests == requests + 1,
            "native-hotkey mode leaves mod setting and late screen routes inactive");
    Config::Get().control.use_scopely_hotkeys = false;
    fleet_native_keys.clear();
  });
  std::cout << "Actual shortcut dispatcher: setting/Manage precedence, native Update and capture passed\n";
}
} // namespace

void AdjustUiScale(bool up)
{ hotkey_order_scale += up ? 1 : -1; }
namespace fleet_watch
{
void InstallRuntimeProbe()
{ Require(false, "the input fixture must not install a native fleet probe"); }
} // namespace fleet_watch
void AdjustUiShipScale(bool) {}
void AdjustUiViewerScale(bool) {}
void InstallKeybindBadges() {}
bool ToggleKeybindBadges()
{ return false; }
void HideKeybindBadges() {}
void OpenChatShortcut() {}
bool ChatChannelShortcuts()
{ return false; }
bool Config::Reload()
{ return false; }
namespace runtime_config
{
void ForceClose() noexcept
{ std::abort(); } // Never exercise process termination in this host.
} // namespace runtime_config
