#pragma once

// Exercise the real top-level shortcut ordering with the screen fixture's live
// native owners. Scale effects and unrelated badge/chat/quit services are sinks.
#include "../src/mod/features/hotkeys.cc"

namespace
{
int  hotkey_order_scale = 0;
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
