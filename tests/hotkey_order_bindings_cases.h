#pragma once

// Exercise the real top-level shortcut ordering with the screen fixture's live
// native owners. Scale effects and unrelated badge/chat/quit services are sinks.
#ifndef _MODDBG
#define HOTKEY_FIXTURE_DEFINED_MODDBG
#define _MODDBG 1
#endif
#include "../src/mod/features/features.h"
namespace
{
FeatureInstallation hotkey_tutorial_installation{"SettingsTutorialInput", InstallState::kInstalled, true, true};
std::span<const FeatureInstallation> HotkeyFixtureInstallations()
{ return {&hotkey_tutorial_installation, 1}; }
} // namespace
#define FeatureInstallations HotkeyFixtureInstallations
#include "../src/mod/features/hotkeys.cc"
#undef FeatureInstallations
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
void EscapeTimingNativeCases()
{
  using Clock   = std::chrono::steady_clock;
  const auto at = Clock::time_point{} + std::chrono::seconds(100);
  for (const auto gap : {std::chrono::milliseconds(499), std::chrono::milliseconds(500)}) {
    ResetHotkeys();
    Require(!EscapeDoubleTapAt(at, 500) && EscapeDoubleTapAt(at + gap, 500) && !escape_first_press,
            "Escape accepts an inside or exact-endpoint pair and consumes its first press");
  }
  ResetHotkeys();
  Require(!EscapeDoubleTapAt(at, 500) && !EscapeDoubleTapAt(at + std::chrono::milliseconds(501), 500)
              && EscapeDoubleTapAt(at + std::chrono::milliseconds(1001), 500),
          "an expired second press starts a fresh inclusive Escape window");
  Require(!EscapeDoubleTapAt(at + std::chrono::milliseconds(1002), 500)
              && EscapeDoubleTapAt(at + std::chrono::milliseconds(1003), 500),
          "the third press starts a new pair rather than reusing the already consumed first press");
  ResetHotkeys();
  Require(!EscapeDoubleTapAt(at, 500) && !EscapeDoubleTapAt(at - std::chrono::milliseconds(1), 500)
              && EscapeDoubleTapAt(at, 500),
          "a backwards injected clock begins a new Escape pair instead of completing the old pair");
  Require(!EscapeDoubleTapAt(at, 500) && !EscapeDoubleTapAt(at + std::chrono::milliseconds(1), 1500)
              && EscapeDoubleTapAt(at + std::chrono::milliseconds(1501), 1500),
          "changing the Escape window cannot complete a pair armed under the previous setting");
  Require(!EscapeDoubleTapAt(at, std::numeric_limits<int>::max())
              && EscapeDoubleTapAt(at + std::chrono::hours(1), std::numeric_limits<int>::max()),
          "large nonnegative accessibility windows are preserved without a one-second cap");

  const auto sample = [](bool held, bool down = false) {
    held_keys.fill(false);
    down_keys.fill(false);
    held_keys[std::to_underlying(unity::KeyCode::Escape)] = held;
    down_keys[std::to_underlying(unity::KeyCode::Escape)] = down;
    any_held                                              = held;
    any_down                                              = down;
    ++input_frame;
    unity::Key::Poll();
  };
  Config::Get().ui.disable_escape_exit = true;
  for (const int window : {0, -1}) {
    Require(!EscapeDoubleTapAt(at, 500) && escape_first_press.has_value(), "arm a pair before disabling its window");
    Config::Get().ui.disable_escape_exit_timer = window;
    sample(true, true);
    Require(SwallowEscape() && !escape_first_press, "a nonpositive window swallows Escape and retires old pair state");
  }
  Config::Get().ui.disable_escape_exit_timer = 500000;
  sample(true, true);
  Require(SwallowEscape() && escape_first_press.has_value(), "the first real fresh Escape press is swallowed");
  Require(SwallowEscape() && escape_first_press.has_value(),
          "repeated dispatch in one input frame cannot count one physical press twice");
  sample(true);
  Require(SwallowEscape() && escape_first_press.has_value(), "held Escape is not a second press");
  sample(false);
  Require(!SwallowEscape() && escape_first_press.has_value(), "normal key release preserves the pending pair");
  sample(true, true);
  Require(!SwallowEscape() && !escape_first_press, "a second distinct fresh press reaches native exit handling");
  Require(SwallowEscape() && !escape_first_press, "replaying an accepted frame cannot start a third-press pair");
  ResetHotkeys();
  Require(SwallowEscape() && !escape_first_press, "resetting pair state cannot replay the consumed down frame");

  Require(!EscapeDoubleTapAt(at, 500), "arm before changing the block master");
  Config::Get().ui.disable_escape_exit = false;
  sample(false);
  Require(!SwallowEscape() && !escape_first_press, "turning the block off retires a pending pair on an idle frame");
  Config::Get().ui.disable_escape_exit = true;
  Require(!EscapeDoubleTapAt(at, 500), "arm before changing the positive window");
  Config::Get().ui.disable_escape_exit_timer = 1500;
  Require(!SwallowEscape() && !escape_first_press,
          "changing the positive window retires its previous pair without a key");

  for (int barrier = 0; barrier != 6; ++barrier) {
    ResetHotkeys();
    Require(!EscapeDoubleTapAt(at, 1500), "arm a pair before an input ownership barrier");
    fleet_native_keys.clear();
    Config::Get().control.hotkeys_enabled     = barrier != 1;
    Config::Get().control.use_scopely_hotkeys = barrier == 2;
    fleet_native_capture                      = barrier == 0;
    has_event_system = has_input_field = true;
    selected                           = &selected_object;
    input_field_focused                = barrier == 3;
    event_system_lookup_throws         = barrier == 4;
    selected_lookup_throws = component_lookup_throws = input_focus_throws = false;
    input_focus_throw_at                                                  = 0;
    screen_actions_section = barrier == 5 ? SectionID::Chat_Main : SectionID::Navigation_System;
    sample(false);
    Require(!HandleShortcuts() && !escape_first_press,
            "capture, disabled/native hotkeys, typing, unknown focus and full chat retire an old Escape pair");
  }
  fleet_native_capture                       = false;
  Config::Get().control.hotkeys_enabled      = true;
  Config::Get().control.use_scopely_hotkeys  = false;
  input_field_focused                        = true;
  event_system_lookup_throws                 = false;
  selected                                  = &selected_object;
  screen_actions_section                    = SectionID::Navigation_System;
  sample(true, true);
  Require(HandleShortcuts() && selected == nullptr && !escape_first_press,
          "Escape clears a focused field without beginning an exit pair");
  Require(HandleShortcuts() && !escape_first_press,
          "redispatch after same-frame focus recovery cannot reuse its Escape as an exit press");
  sample(false);
  Require(!HandleShortcuts() && !escape_first_press, "release after focus recovery leaves no pending exit pair");
  sample(true, true);
  Require(HandleShortcuts() && escape_first_press.has_value(),
          "the next physical Escape begins a fresh pair rather than completing focus recovery's press");
  ResetHotkeys();
  fleet_native_capture = true;
  sample(true, true);
  Require(!HandleShortcuts() && !escape_first_press, "shortcut capture owns the current Escape down frame");
  fleet_native_capture = false;
  Require(HandleShortcuts() && !escape_first_press,
          "releasing capture in the same frame cannot replay its Escape as an exit press");
  Config::Get().ui.disable_escape_exit_timer = 0;
  input_field_focused = event_system_lookup_throws = false;
  screen_actions_section                           = SectionID::Navigation_System;
  sample(false);
  ResetHotkeys();
}

struct SettingsTutorialNativeManager {
  Il2CppObject          header;
  TutorialMaskUILoader* loader = nullptr;
};
struct SettingsTutorialNativeLoader {
  Il2CppObject      header;
  unity::Behaviour* controller = nullptr;
};
struct SettingsTutorialNativeController {
  Il2CppObject       header;
  unity::GameObject* mask = nullptr;
};
struct SettingsTutorialNativeMask {
  Il2CppObject header;
  bool         active = true;
};
struct SettingsTutorialNativeFlags {
  bool active = true, blocking = true;
} settings_tutorial_native_flags;
bool                           settings_tutorial_native_touch   = true;
SettingsTutorialNativeManager* settings_tutorial_native_manager = nullptr;
std::function<void()>          settings_tutorial_controller_callback, settings_tutorial_mask_callback;
int                            settings_tutorial_mask_writes = 0, settings_tutorial_controller_reads = 0;

Il2CppObject* SettingsTutorialNativeManagerGet(const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(settings_tutorial_native_manager); }
unity::Behaviour* SettingsTutorialNativeControllerGet(SettingsTutorialNativeLoader* self, const MethodInfo*)
{
  ++settings_tutorial_controller_reads;
  if (settings_tutorial_controller_callback)
    settings_tutorial_controller_callback();
  return self->controller;
}
unity::GameObject* SettingsTutorialNativeMaskGet(SettingsTutorialNativeController* self, const MethodInfo*)
{ return self->mask; }
bool SettingsTutorialNativeActive(SettingsTutorialNativeMask* self, const MethodInfo*)
{ return self->active; }
void SettingsTutorialNativeSetActive(SettingsTutorialNativeMask* self, bool active, const MethodInfo*)
{
  ++settings_tutorial_mask_writes;
  self->active = active;
  if (settings_tutorial_mask_callback)
    settings_tutorial_mask_callback();
}

void SettingsTutorialNativeCases()
{
  auto& behaviour  = Fake(unity::Behaviour::Class::Raw());
  auto& tutorial   = Define("Digit.Prime.Tutorial", "TutorialManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& loader     = Define("Digit.Prime.Tutorial", "TutorialMaskUILoader", IL2CPP_TYPE_CLASS, &object_class);
  auto& controller = Define("Digit.Prime.Tutorial", "TutorialMaskViewController", IL2CPP_TYPE_CLASS, &behaviour);
  auto& touch      = Define("", "TouchKit", IL2CPP_TYPE_CLASS, &object_class);
  tutorial.klass.static_fields = &settings_tutorial_native_flags;
  touch.klass.static_fields    = &settings_tutorial_native_touch;
  AddField(tutorial, "_isActive", bool_class, offsetof(SettingsTutorialNativeFlags, active), true);
  AddField(tutorial, "_isBlockingInput", bool_class, offsetof(SettingsTutorialNativeFlags, blocking), true);
  AddField(tutorial, "_loadAndShow", loader, offsetof(SettingsTutorialNativeManager, loader));
  AddField(touch, "BlockTouch", bool_class, 0, true);
  auto&       hub = Fake(Hub::Class::Raw());
  const auto& manager_get =
      AddMethod(hub, "get_TutorialManager", reinterpret_cast<void*>(&SettingsTutorialNativeManagerGet), T(tutorial), {},
                METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "TutorialManager", &manager_get);
  const auto& controller_get = AddMethod(
      loader, "get_Controller", reinterpret_cast<void*>(&SettingsTutorialNativeControllerGet), T(behaviour), {});
  AddProperty(loader, "Controller", &controller_get);
  AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&SettingsTutorialNativeMaskGet),
            T(game_object_class), {});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&SettingsTutorialNativeActive), T(bool_class),
            {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&SettingsTutorialNativeSetActive), T(void_class),
            {T(bool_class)});
  SettingsTutorialNativeMask       mask{{&game_object_class.klass}}, replacement_mask{{&game_object_class.klass}};
  SettingsTutorialNativeController view{{&controller.klass}, reinterpret_cast<unity::GameObject*>(&mask)};
  SettingsTutorialNativeController replacement_view{{&controller.klass},
                                                    reinterpret_cast<unity::GameObject*>(&replacement_mask)};
  SettingsTutorialNativeLoader     native_loader{{&loader.klass}, reinterpret_cast<unity::Behaviour*>(&view)};
  SettingsTutorialNativeLoader     replacement_loader{{&loader.klass},
                                                      reinterpret_cast<unity::Behaviour*>(&replacement_view)};
  SettingsTutorialNativeManager    manager{{&tutorial.klass}, reinterpret_cast<TutorialMaskUILoader*>(&native_loader)};
  SettingsTutorialNativeManager    replacement{{&tutorial.klass},
                                               reinterpret_cast<TutorialMaskUILoader*>(&replacement_loader)};
  settings_tutorial_native_manager = &manager;
  settings_tutorial_installed = true; // Hook callbacks are exercised directly; no fixture detours live native code.
  const auto sample           = [] {
    held_keys.fill(false);
    down_keys.fill(false);
    any_held = any_down = false;
    ++input_frame;
    unity::Key::Poll();
  };
  const auto dispatch = [&] {
    fleet_native_keys = {Action::kShowSettings};
    sample();
    const auto handled = HandleShortcuts();
    fleet_native_keys.clear();
    return handled;
  };
  const auto fresh = [&] {
    kSettingsTutorialInput.reset();
    hotkey_tutorial_installation.frame_ready = true;
    settings_tutorial_native_flags = {};
    settings_tutorial_native_touch = true;
    mask.active = replacement_mask.active = true;
    settings_tutorial_native_manager      = &manager;
    settings_tutorial_controller_callback = settings_tutorial_mask_callback = {};
    screen_actions_navigation_callback                                      = {};
    screen_actions_accepted                                                 = true;
    screen_actions_section                                                  = SectionID::Navigation_System;
    Config::Get().control.hotkeys_enabled                                   = true;
    Config::Get().control.use_scopely_hotkeys                               = false;
    fleet_native_capture                                                    = false;
    has_event_system = has_input_field = true;
    selected                           = nullptr;
    input_field_focused = event_system_lookup_throws = selected_lookup_throws = component_lookup_throws =
        input_focus_throws                                                    = false;
    sample();
  };
  fresh();
  int        original_calls = 0;
  const auto original       = [&] {
    ++original_calls;
    return true;
  };
  screen_actions_section = SectionID::GameSettings;
  kSettingsTutorialInput.frame();
  Require(TutorialManager_get_IsBlockingInput_Hook(original) && original_calls == 1
              && settings_tutorial_native_flags.active && mask.active,
          "native Settings navigation does not acquire tutorial gates or override its input getter");

  fresh();
  hotkey_tutorial_installation.frame_ready = false;
  const auto unavailable_navigations       = screen_actions_navigations;
  Require(dispatch() && screen_actions_navigations == unavailable_navigations + 1 && !settings_tutorial_request
              && settings_tutorial_native_flags.active && settings_tutorial_native_flags.blocking
              && settings_tutorial_native_touch && mask.active,
          "installed hooks without restoration delivery leave tutorial gates untouched during ordinary navigation");

  for (int gate = 0; gate != 3; ++gate) {
    fresh();
    Require(dispatch() && settings_tutorial_request, "acquire tutorial input before a native gate reassertion");
    settings_tutorial_controller_callback = [gate] {
      if (gate == 0)
        settings_tutorial_native_flags.active = true;
      else if (gate == 1)
        settings_tutorial_native_flags.blocking = true;
      else
        settings_tutorial_native_touch = true;
    };
    const auto calls_before_gate = original_calls;
    Require(TutorialManager_get_IsBlockingInput_Hook(original) && original_calls == calls_before_gate + 1
                && settings_tutorial_request && !settings_tutorial_request->released,
            "a native gate reasserted during owner observation wins before the next feature frame");
    settings_tutorial_controller_callback = {};
    kSettingsTutorialInput.frame();
    Require(!settings_tutorial_request && mask.active && settings_tutorial_native_flags.active
                && settings_tutorial_native_flags.blocking && settings_tutorial_native_touch,
            "the next frame restores remaining owned gates without clearing the native reassertion");
  }
  original_calls = 1;
  fresh();
  const auto navigations = screen_actions_navigations;
  selected               = &selected_object;
  input_field_focused    = true;
  Require(!dispatch() && screen_actions_navigations == navigations && !settings_tutorial_request,
          "the real dispatcher reserves the Settings keyboard shortcut for a focused text field");
  input_field_focused        = false;
  event_system_lookup_throws = true;
  Require(!dispatch() && screen_actions_navigations == navigations && !settings_tutorial_request,
          "unknown focus also cannot request a tutorial-input override");
  event_system_lookup_throws = false;
  selected                   = nullptr;
  fleet_native_capture       = true;
  Require(!dispatch() && screen_actions_navigations == navigations && !settings_tutorial_request,
          "shortcut capture keeps Settings and tutorial gates under its existing input ownership");
  fleet_native_capture            = false;
  settings_tutorial_mask_callback = [] { ShowSettings(); };
  Require(dispatch() && screen_actions_navigations == navigations + 1 && settings_tutorial_request
              && !settings_tutorial_native_flags.active && !settings_tutorial_native_flags.blocking
              && !settings_tutorial_native_touch && !mask.active,
          "one accepted screen shortcut releases the actual bound tutorial fields and mask despite native reentry");
  settings_tutorial_mask_callback = {};
  Require(!TutorialManager_get_IsBlockingInput_Hook(original) && original_calls == 1,
          "the owned pending Settings transition overrides its tutorial input getter");
  screen_actions_section = SectionID::GameSettings;
  kSettingsTutorialInput.frame();
  ResetHotkeys();
  RetireEscapeInput();
  fleet_native_capture = true;
  sample();
  Require(!HandleShortcuts() && settings_tutorial_request && !mask.active,
          "Escape timing reset and Settings keybinding capture do not restore tutorial input");
  fleet_native_capture                      = false;
  Config::Get().control.use_scopely_hotkeys = true;
  Require(!HandleShortcuts() && settings_tutorial_request && !mask.active,
          "changing hotkey mode does not end a still-open Settings tutorial suspension");
  Config::Get().control.use_scopely_hotkeys = false;
  screen_actions_section                    = SectionID::Navigation_System;
  kSettingsTutorialInput.frame();
  Require(!settings_tutorial_request && settings_tutorial_native_flags.active && settings_tutorial_native_flags.blocking
              && settings_tutorial_native_touch && mask.active,
          "normal Settings exit restores exactly the captured tutorial and TouchKit restrictions");

  fresh();
  mask.active                             = false;
  settings_tutorial_native_flags.blocking = false;
  const auto mask_writes                  = settings_tutorial_mask_writes;
  Require(dispatch(), "inactive original tutorial mask still permits ordinary accepted Settings navigation");
  screen_actions_section = SectionID::GameSettings;
  kSettingsTutorialInput.frame();
  screen_actions_section = SectionID::Navigation_System;
  kSettingsTutorialInput.frame();
  Require(!settings_tutorial_request && !mask.active && !settings_tutorial_native_flags.blocking
              && settings_tutorial_native_flags.active && settings_tutorial_native_touch
              && settings_tutorial_mask_writes == mask_writes,
          "restoration preserves an originally inactive mask and false blocker rather than enabling them");

  fresh();
  screen_actions_accepted = false;
  Require(dispatch() && !settings_tutorial_request && mask.active && settings_tutorial_native_flags.active,
          "rejected native Settings navigation restores the acquired gates immediately");
  fresh();
  Require(dispatch(), "arm a Settings request whose native transition never completes");
  for (int i = 0; i != 300; ++i)
    kSettingsTutorialInput.frame();
  Require(!settings_tutorial_request && mask.active && settings_tutorial_native_flags.active,
          "an uncompleted transition restores its original tutorial state after the bounded wait");

  fresh();
  settings_tutorial_mask_callback = [] { kSettingsTutorialInput.reset(); };
  const auto before_reload        = screen_actions_navigations;
  Require(dispatch() && !settings_tutorial_request && screen_actions_navigations == before_reload && !mask.active
              && !settings_tutorial_native_flags.active,
          "reload during mask deactivation cancels navigation without restoring into the teardown session");
  fresh();
  Require(dispatch(), "arm a request before checking the feature's forget-only teardown callback");
  const auto before_reset_writes = settings_tutorial_mask_writes;
  const auto before_reset_reads  = settings_tutorial_controller_reads;
  kSettingsTutorialInput.reset();
  Require(!settings_tutorial_request && settings_tutorial_mask_writes == before_reset_writes
              && settings_tutorial_controller_reads == before_reset_reads && !mask.active
              && !settings_tutorial_native_flags.active,
          "Feature::reset forgets native custody without invoking or restoring old game objects");

  fresh();
  Require(dispatch(), "arm a request before a replacement tutorial manager arrives");
  settings_tutorial_native_manager = &replacement;
  settings_tutorial_native_flags   = {false, true};
  settings_tutorial_native_touch   = true;
  const auto before_replacement    = settings_tutorial_mask_writes;
  kSettingsTutorialInput.frame();
  Require(!settings_tutorial_request && !settings_tutorial_native_flags.active
              && settings_tutorial_native_flags.blocking && settings_tutorial_native_touch && !mask.active
              && replacement_mask.active && settings_tutorial_mask_writes == before_replacement,
          "a replacement tutorial abandons old ownership without touching its flags or either mask");
  fresh();
  settings_tutorial_controller_callback = [&] { settings_tutorial_native_manager = &replacement; };
  const auto before_capture             = screen_actions_navigations;
  Require(dispatch() && !settings_tutorial_request && screen_actions_navigations == before_capture && mask.active
              && settings_tutorial_native_flags.active,
          "manager replacement during a bound controller getter cannot authorize unlock or stale navigation");
  fresh();
  Require(dispatch(), "arm a request before its loader replaces the tutorial mask controller");
  native_loader.controller     = reinterpret_cast<unity::Behaviour*>(&replacement_view);
  const auto before_controller = settings_tutorial_mask_writes;
  kSettingsTutorialInput.frame();
  Require(!settings_tutorial_request && !mask.active && replacement_mask.active
              && settings_tutorial_mask_writes == before_controller,
          "controller replacement within the same loader never enables an old or replacement mask");
  native_loader.controller = reinterpret_cast<unity::Behaviour*>(&view);
  fresh();
  screen_actions_navigation_callback = [] { kSettingsTutorialInput.reset(); };
  Require(dispatch() && !settings_tutorial_request && !mask.active && !settings_tutorial_native_flags.active,
          "reload inside native navigation cannot publish the cancelled request again");
  fresh();
  settings_tutorial_installed      = false;
  settings_tutorial_native_manager = nullptr;
  std::cout << "Settings tutorial input: actual dispatcher, binding ownership, normal restore and teardown passed\n";
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
    EscapeTimingNativeCases();
    SettingsTutorialNativeCases();
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
