/**
 * @file hotkey_router.cc
 * @brief Central hotkey routing logic — called every frame from ScreenManager::Update hook.
 *
 * This is the main keyboard input processing loop for the community patch.
 * It handles enable/disable toggling, ship selection, escape behavior, chat
 * channel shortcuts, space actions (engage/scan/mine/recall/repair), and the
 * table-driven dispatch system. Escape-driven exit suppression is enforced at
 * SectionManager::BackButtonPressed rather than this frame router.
 */
#include "errormsg.h"
#include "config.h"

#include "patches/hotkey_router.h"

#include "patches/cargo_display.h"
#include "patches/fleet_actions.h"
#include "patches/hotkey_dispatch.h"
#include "patches/key.h"
#include "patches/mapkey.h"
#include "patches/navigation.h"
#include "patches/viewer_mgmt.h"
#include "testable_functions.h"

#include "prime/ChatManager.h"
#include "prime/ChatMessageListLocalViewController.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/KeyCode.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ScreenManager.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <spdlog/spdlog.h>

#include <array>

// ─── Main Per-Frame Hotkey Router ─────────────────────────────────────────────────────

// Returns true when the original ScreenManager::Update should be called.
bool hotkey_router_screen_update(ScreenManager* _this)
{
  Key::ResetCache();

  switch (hotkey_router_startup_action(MapKey::IsDown(GameFunction::DisableHotKeys),
                                       MapKey::IsDown(GameFunction::EnableHotKeys),
                                       Config::Get().use_scopely_hotkeys,
                                       Config::Get().hotkeys_enabled)) {
    case HotkeyRouterStartupAction::DisableHotkeys:
      Config::Get().hotkeys_enabled = false;
      spdlog::warn("Setting hotkeys to DISABLED");
      return false;
    case HotkeyRouterStartupAction::EnableHotkeys:
      Config::Get().hotkeys_enabled = true;
      spdlog::warn("Setting hotkeys to ENABLED");
      return false;
    case HotkeyRouterStartupAction::AllowOriginal:
      return true;
    case HotkeyRouterStartupAction::SuppressOriginal:
      return false;
    default:
      break;
  }

  const auto is_in_chat = Hub::IsInChat();
  const auto config     = &Config::Get();

#ifdef _WIN32
  if (MapKey::IsDown(GameFunction::Quit)) {
    TerminateProcess(GetCurrentProcess(), 1);
  }
#endif

  // ─── Ship selection (1-8 keys) ───────────────────────────────────────────────────────
  const auto ship_select_request = hotkey_router_ship_select_request(std::array<bool, 8>{
      MapKey::IsDown(GameFunction::SelectShip1),
      MapKey::IsDown(GameFunction::SelectShip2),
      MapKey::IsDown(GameFunction::SelectShip3),
      MapKey::IsDown(GameFunction::SelectShip4),
      MapKey::IsDown(GameFunction::SelectShip5),
      MapKey::IsDown(GameFunction::SelectShip6),
      MapKey::IsDown(GameFunction::SelectShip7),
      MapKey::IsDown(GameFunction::SelectShip8),
  });

  if (HandleShipSelection(ship_select_request)) {
    return true;
  }

  // ─── Escape in chat / input focus ───────────────────────────────────────────────────
  if (hotkey_router_should_clear_input_focus(Key::Pressed(KeyCode::Escape), Key::IsInputFocused(), Hub::IsInChat())) {
    Key::ClearInputFocus();
    return false;
  }

  if (!is_in_chat) {
    if (!Key::IsInputFocused()) {
      // SelectCurrent — locate active fleet
      if (MapKey::IsDown(GameFunction::SelectCurrent)) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          auto fleet = fleet_bar->_fleetPanelController->fleet;
          if (fleet) {
            if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
              NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
            }
            FleetsManager::Instance()->RequestViewFleet(fleet, true);
            return false;
          }
        }
      }

      // ToggleQueue
      if (hotkey_router_should_toggle_queue(is_in_chat, Key::IsInputFocused(), MapKey::IsDown(GameFunction::ToggleQueue))) {
        config->queue_enabled = !config->queue_enabled;
        return false;
      }

      // ShowChat
      if ((MapKey::IsDown(GameFunction::ShowChat) || MapKey::IsDown(GameFunction::ShowChatSide1)
           || MapKey::IsDown(GameFunction::ShowChatSide2))) {
        if (auto chat_manager = ChatManager::Instance(); chat_manager) {
          if (chat_manager->IsSideChatOpen) {
            if (auto view_controller = ObjectFinder<FullScreenChatViewController>::Get(); view_controller) {
              if (auto message_list = view_controller->_messageList; message_list) {
                if (auto message_field = message_list->_inputField; message_field) {
                  message_field->ActivateInputField();
                }
              }
            }
          } else if (MapKey::IsDown(GameFunction::ShowChatSide1) || MapKey::IsDown(GameFunction::ShowChatSide2)) {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Side);
          } else {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Fullscreen);
          }
        }
      }

      // MoveLeft / MoveRight (officer canvas)
      if (MapKey::IsDown(GameFunction::MoveLeft)) {
        if (MoveOfficerCanvas(true)) {
          return false;
        }
      }

      if (MapKey::IsDown(GameFunction::MoveRight)) {
        if (MoveOfficerCanvas(false)) {
          return false;
        }
      }

      // Table-driven hotkey dispatch
      for (const auto& entry : GetHotkeyDispatchTable()) {
        bool active = (entry.input_mode == InputMode::Pressed) ? MapKey::IsPressed(entry.game_function)
                                                               : MapKey::IsDown(entry.game_function);
        if (active) {
          auto decision = entry.handler();
          auto action = hotkey_router_dispatch_action(true,
                                                      decision == DispatchDecision::HandledStop,
                                                      decision == DispatchDecision::HandledAllowOriginal);
          if (action == HotkeyRouterDispatchAction::SuppressOriginal) {
            return false;
          }
          // HandledAllowOriginal falls through to original() at end
          break;
        }
      }
    }
  } else {
    // ─── In-chat channel selection ─────────────────────────────────────────────────────
    if (auto chat_manager = ChatManager::Instance(); chat_manager) {
      if (MapKey::IsDown(GameFunction::SelectChatGlobal)) {
        chat_manager->OpenChannel(ChatChannelCategory::Global);
        return false;
      } else if (MapKey::IsDown(GameFunction::SelectChatAlliance)) {
        chat_manager->OpenChannel(ChatChannelCategory::Alliance);
        return false;
      } else if (MapKey::IsDown(GameFunction::SelectChatPrivate)) {
        chat_manager->OpenChannel(ChatChannelCategory::Private);
        return false;
      }
    }
  }

  if (!Key::IsInputFocused()) {
    // Escape to hide object viewers
    if (Key::Pressed(KeyCode::Escape) && DidHideViewers()) {
      return false;
    }

    // Dismiss golden rewards screen
    if (MapKey::IsDown(GameFunction::ActionPrimary) || Key::Pressed(KeyCode::Escape)) {
      if (TryDismissRewardsScreen()) {
        return false;
      }
    }

    // Space actions (engage, scan, recall, repair, queue, etc.)
    if (MapKey::IsDown(GameFunction::ActionPrimary) || MapKey::IsDown(GameFunction::ActionSecondary)
        || MapKey::IsDown(GameFunction::ActionRecall) || MapKey::IsDown(GameFunction::ActionRepair)
        || MapKey::IsDown(GameFunction::ActionQueue) || MapKey::IsDown(GameFunction::ActionQueueClear)
        || force_space_action_next_frame) {
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat() && !Key::IsInputFocused()) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          bool was_forced = force_space_action_next_frame;
          ExecuteSpaceAction(fleet_bar);
          if (was_forced) {
            force_space_action_next_frame = false;
          }
        }
      }
    }

    // ActionView — toggle cargo/rewards info panel
    if (MapKey::IsDown(GameFunction::ActionView)) {
      HandleActionView();
    }

    // Tick the info pending counter (multi-frame show)
    TickInfoPending();
  }

  return true;
}

// ─── Hook Delegate Functions ─────────────────────────────────────────────────────────

bool hotkey_router_should_call_original_initialize_actions()
{
  return should_call_original_initialize_actions(Config::Get().use_scopely_hotkeys, AllowKeyFallthrough());
}

bool hotkey_router_should_call_original_screen_update(bool routerAllowsOriginal)
{
  return should_call_original_screen_update(routerAllowsOriginal, AllowKeyFallthrough());
}

void hotkey_router_bind_context(RewardsButtonWidget* _this)
{
  HandleCargoBindContext(_this);
}

void hotkey_router_show_fleet(PreScanTargetWidget* _this)
{
  HandleCargoShowFleet(_this);
}
