#pragma once

// Exercise the real top-level shortcut ordering with the screen fixture's live
// native owners. Scale effects and unrelated badge/chat/quit services are sinks.
#include "../src/mod/features/hotkeys.cc"

namespace
{
int  hotkey_order_scale = 0;
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
