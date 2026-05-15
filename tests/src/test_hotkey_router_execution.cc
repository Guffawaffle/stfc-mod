#include "test_pure_common.h"

#include "patches/hotkey_router_native_fleet_guard.h"

TEST_SUITE("hotkey_router_execution")
{
  TEST_CASE("startup helpers still cover hotkey disable enable and fallthrough")
  {
    CHECK(hotkey_router_startup_action(true, false, false, true) == HotkeyRouterStartupAction::DisableHotkeys);
    CHECK(hotkey_router_startup_action(false, true, false, false) == HotkeyRouterStartupAction::EnableHotkeys);
    CHECK(hotkey_router_startup_action(false, false, ScopelyShortcutPolicy::Native, true)
          == HotkeyRouterStartupAction::AllowOriginal);
    CHECK(hotkey_router_startup_action(false, false, ScopelyShortcutPolicy::Fallback, false)
          == HotkeyRouterStartupAction::SuppressOriginal);
  }

  TEST_CASE("quit action only routes when quit winner is present")
  {
    CHECK(hotkey_router_quit_action(false) == HotkeyRouterQuitAction::None);
    CHECK(hotkey_router_quit_action(true) == HotkeyRouterQuitAction::QuitProcess);
  }

  TEST_CASE("ship selection keeps the first fleet winner")
  {
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{false, false, true, false, false, false, false, false})
          == 2);
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{true, false, true, false, false, false, false, false})
          == 0);
  }

  TEST_CASE("escape focus clearing stays a pure routing decision")
  {
    CHECK(hotkey_router_should_clear_input_focus(true, true, false));
    CHECK(hotkey_router_should_clear_input_focus(true, false, true));
    CHECK_FALSE(hotkey_router_should_clear_input_focus(true, false, false));
  }

  TEST_CASE("select current only routes on the gameplay surface")
  {
    CHECK(hotkey_router_select_current_action(false, false, true) == HotkeyRouterSelectCurrentAction::ViewActiveFleet);
    CHECK(hotkey_router_select_current_action(true, false, true) == HotkeyRouterSelectCurrentAction::None);
    CHECK(hotkey_router_select_current_action(false, true, true) == HotkeyRouterSelectCurrentAction::None);
  }

  TEST_CASE("queue toggle remains blocked in chat and focused inputs")
  {
    CHECK(hotkey_router_should_toggle_queue(false, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(true, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(false, true, true));
  }

  TEST_CASE("chat open can activate the existing side-chat input field")
  {
    CHECK(hotkey_router_chat_open_action(false, false, true, input_binding::InputActionId::ShowChat)
          == HotkeyRouterChatOpenAction::ActivateExistingInput);
    CHECK(hotkey_router_chat_open_action(false, false, true, input_binding::InputActionId::ShowChatSide1)
          == HotkeyRouterChatOpenAction::ActivateExistingInput);
  }

  TEST_CASE("chat open winners map to side or fullscreen channel opens")
  {
    CHECK(hotkey_router_chat_open_action(false, false, false, input_binding::InputActionId::ShowChat)
          == HotkeyRouterChatOpenAction::OpenAllianceFullscreen);
    CHECK(hotkey_router_chat_open_action(false, false, false, input_binding::InputActionId::ShowChatSide2)
          == HotkeyRouterChatOpenAction::OpenAllianceSide);
  }

  TEST_CASE("chat channel winners only route while already in chat")
  {
    CHECK(hotkey_router_chat_channel_action(true, input_binding::InputActionId::SelectChatGlobal)
          == HotkeyRouterChatChannelAction::Global);
    CHECK(hotkey_router_chat_channel_action(true, input_binding::InputActionId::SelectChatAlliance)
          == HotkeyRouterChatChannelAction::Alliance);
    CHECK(hotkey_router_chat_channel_action(false, input_binding::InputActionId::SelectChatPrivate)
          == HotkeyRouterChatChannelAction::None);
  }

  TEST_CASE("officer canvas movement only routes on the gameplay surface")
  {
    CHECK(hotkey_router_officer_canvas_action(false, false, input_binding::InputActionId::MoveLeft)
          == HotkeyRouterOfficerCanvasAction::MoveLeft);
    CHECK(hotkey_router_officer_canvas_action(false, false, input_binding::InputActionId::MoveRight)
          == HotkeyRouterOfficerCanvasAction::MoveRight);
    CHECK(hotkey_router_officer_canvas_action(true, false, input_binding::InputActionId::MoveLeft)
          == HotkeyRouterOfficerCanvasAction::None);
  }

  TEST_CASE("table dispatch requests only surface on unfocused gameplay frames")
  {
    CHECK(hotkey_router_table_dispatch_request(false, false, input_binding::InputActionId::ShowBookmarks)
          == input_binding::InputActionId::ShowBookmarks);
    CHECK(hotkey_router_table_dispatch_request(true, false, input_binding::InputActionId::ShowBookmarks)
          == input_binding::InputActionId::Max);
    CHECK(hotkey_router_table_dispatch_request(false, true, input_binding::InputActionId::ShowBookmarks)
          == input_binding::InputActionId::Max);
  }

  TEST_CASE("simple fleet winners map to queue clear and view info only when unfocused")
  {
    CHECK(hotkey_router_simple_fleet_action(false, input_binding::InputActionId::FleetQueueClear)
          == HotkeyRouterSimpleFleetAction::QueueClear);
    CHECK(hotkey_router_simple_fleet_action(false, input_binding::InputActionId::FleetViewInfo)
          == HotkeyRouterSimpleFleetAction::ViewInfo);
    CHECK(hotkey_router_simple_fleet_action(true, input_binding::InputActionId::FleetViewInfo)
          == HotkeyRouterSimpleFleetAction::None);
  }

  TEST_CASE("original call policy still distinguishes handled suppressed and allow-original paths")
  {
    CHECK(hotkey_router_dispatch_action(true, true, false) == HotkeyRouterDispatchAction::SuppressOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, true) == HotkeyRouterDispatchAction::AllowOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, false) == HotkeyRouterDispatchAction::Continue);
    CHECK(hotkey_router_dispatch_action(true, false, true, true) == HotkeyRouterDispatchAction::SuppressOriginal);

    CHECK_FALSE(should_call_original_screen_update(false, OriginalFramePolicy::Mod));
    CHECK(should_call_original_screen_update(true, OriginalFramePolicy::Mod));
    CHECK(should_call_original_screen_update(false, OriginalFramePolicy::FallthroughAll));
  }

  // -------------------------------------------------------------------------
  // Native fleet-selection guard / RAII bypass (issues #94, #96)
  // -------------------------------------------------------------------------

  TEST_CASE("native fleet-selection guard suppresses only the armed slot")
  {
    using hotkey_router_native_fleet::should_suppress;
    std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};
    slots[3] = true;

    CHECK(should_suppress(3, slots, false));
    CHECK_FALSE(should_suppress(2, slots, false));
    CHECK_FALSE(should_suppress(4, slots, false));
  }

  TEST_CASE("native fleet-selection guard rejects out-of-range indices")
  {
    using hotkey_router_native_fleet::should_suppress;
    std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};
    slots[0] = true;
    slots[7] = true;

    CHECK_FALSE(should_suppress(-1, slots, false));
    CHECK_FALSE(should_suppress(8, slots, false));
    CHECK_FALSE(should_suppress(99, slots, false));
    CHECK(should_suppress(0, slots, false));
    CHECK(should_suppress(7, slots, false));
  }

  TEST_CASE("native fleet-selection bypass disables suppression for both per-slot and any queries")
  {
    using hotkey_router_native_fleet::should_suppress;
    using hotkey_router_native_fleet::should_suppress_any;
    std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};
    slots[2] = true;

    CHECK(should_suppress(2, slots, false));
    CHECK(should_suppress_any(slots, false));

    CHECK_FALSE(should_suppress(2, slots, true));
    CHECK_FALSE(should_suppress_any(slots, true));
  }

  TEST_CASE("any-suppression query reflects whether any slot is armed")
  {
    using hotkey_router_native_fleet::should_suppress_any;
    std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};

    CHECK_FALSE(should_suppress_any(slots, false));

    slots[5] = true;
    CHECK(should_suppress_any(slots, false));

    slots[5] = false;
    slots[0] = true;
    CHECK(should_suppress_any(slots, false));
  }

  TEST_CASE("RAII bypass counter saturates at zero so unbalanced destructors cannot underflow")
  {
    using hotkey_router_native_fleet::bypass_decrement;

    CHECK(bypass_decrement(2) == 1);
    CHECK(bypass_decrement(1) == 0);
    CHECK(bypass_decrement(0) == 0);
    CHECK(bypass_decrement(-3) == -3); // already pathological; stay put rather than make it worse
  }

  TEST_CASE("nested RAII bypass keeps suppression disabled until the outermost scope exits")
  {
    using hotkey_router_native_fleet::bypass_decrement;
    using hotkey_router_native_fleet::should_suppress_any;
    std::array<bool, hotkey_router_native_fleet::kSlotCount> slots{};
    slots[1] = true;

    int depth = 0;
    CHECK(should_suppress_any(slots, depth > 0));

    ++depth;
    CHECK_FALSE(should_suppress_any(slots, depth > 0));

    ++depth;
    CHECK_FALSE(should_suppress_any(slots, depth > 0));

    depth = bypass_decrement(depth);
    CHECK_FALSE(should_suppress_any(slots, depth > 0));

    depth = bypass_decrement(depth);
    CHECK(should_suppress_any(slots, depth > 0));
  }
}