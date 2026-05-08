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
#include "config.h"
#include "errormsg.h"

#include "patches/hotkey_router.h"

#include "patches/cargo_display.h"
#include "patches/fleet_actions.h"
#include "patches/hotkey_dispatch.h"
#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "patches/key.h"
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
#include <vector>

namespace
{
constexpr std::array kHotkeyStartupActions{
    input_binding::InputActionId::HotkeysDisable,
    input_binding::InputActionId::HotkeysEnable,
};

constexpr std::array kHotkeyQuitActions{
    input_binding::InputActionId::Quit,
};

constexpr std::array kHotkeyQueueActions{
    input_binding::InputActionId::FleetQueueToggle,
};

constexpr std::array kHotkeyShipSelectionActions{
    input_binding::InputActionId::SelectShip1, input_binding::InputActionId::SelectShip2,
    input_binding::InputActionId::SelectShip3, input_binding::InputActionId::SelectShip4,
    input_binding::InputActionId::SelectShip5, input_binding::InputActionId::SelectShip6,
    input_binding::InputActionId::SelectShip7, input_binding::InputActionId::SelectShip8,
};

constexpr std::array kHotkeySelectCurrentActions{
    input_binding::InputActionId::SelectCurrent,
};

constexpr std::array kHotkeyChatOpenActions{
    input_binding::InputActionId::ShowChat,
    input_binding::InputActionId::ShowChatSide1,
    input_binding::InputActionId::ShowChatSide2,
};

constexpr std::array kHotkeyChatChannelActions{
    input_binding::InputActionId::SelectChatGlobal,
    input_binding::InputActionId::SelectChatAlliance,
    input_binding::InputActionId::SelectChatPrivate,
};

constexpr std::array kHotkeyOfficerCanvasActions{
    input_binding::InputActionId::MoveLeft,
    input_binding::InputActionId::MoveRight,
};

constexpr std::array kHotkeySimpleFleetActions{
    input_binding::InputActionId::FleetQueueClear,
    input_binding::InputActionId::FleetViewInfo,
};

constexpr std::array kHotkeyRuntimeSpaceActions{
    input_binding::InputActionId::FleetPrimary,
    input_binding::InputActionId::FleetSecondary,
    input_binding::InputActionId::FleetService,
};

constexpr std::array kHotkeyTableDispatchActions{
    input_binding::InputActionId::ShowQTrials,
    input_binding::InputActionId::ShowBookmarks,
    input_binding::InputActionId::ShowLookup,
    input_binding::InputActionId::ShowRefinery,
    input_binding::InputActionId::ShowFactions,
    input_binding::InputActionId::ShowStationExterior,
    input_binding::InputActionId::ShowGalaxy,
    input_binding::InputActionId::ShowStationInterior,
    input_binding::InputActionId::ShowSystem,
    input_binding::InputActionId::ShowArtifacts,
    input_binding::InputActionId::ShowInventory,
    input_binding::InputActionId::ShowMissions,
    input_binding::InputActionId::ShowResearch,
    input_binding::InputActionId::ShowScrapYard,
    input_binding::InputActionId::ShowOfficers,
    input_binding::InputActionId::ShowCommander,
    input_binding::InputActionId::ShowAwayTeam,
    input_binding::InputActionId::ShowEvents,
    input_binding::InputActionId::ShowExoComp,
    input_binding::InputActionId::ShowDaily,
    input_binding::InputActionId::ShowGifts,
    input_binding::InputActionId::ShowAlliance,
    input_binding::InputActionId::ShowAllianceHelp,
    input_binding::InputActionId::ShowAllianceArmada,
    input_binding::InputActionId::ShowSettings,
    input_binding::InputActionId::UiScaleUp,
    input_binding::InputActionId::UiScaleDown,
    input_binding::InputActionId::UiViewerScaleUp,
    input_binding::InputActionId::UiViewerScaleDown,
    input_binding::InputActionId::TogglePreviewLocate,
    input_binding::InputActionId::TogglePreviewRecall,
    input_binding::InputActionId::ToggleCargoDefault,
    input_binding::InputActionId::ToggleCargoPlayer,
    input_binding::InputActionId::ToggleCargoStation,
    input_binding::InputActionId::ToggleCargoHostile,
    input_binding::InputActionId::ToggleCargoArmada,
    input_binding::InputActionId::LogOff,
    input_binding::InputActionId::LogError,
    input_binding::InputActionId::LogWarn,
    input_binding::InputActionId::LogInfo,
    input_binding::InputActionId::LogDebug,
    input_binding::InputActionId::LogTrace,
    input_binding::InputActionId::ShowShips,
};

constexpr auto kHotkeyFrameActions = [] {
  std::array<input_binding::InputActionId, kHotkeyStartupActions.size() + kHotkeyQuitActions.size()
                                               + kHotkeyQueueActions.size() + kHotkeyShipSelectionActions.size()
                                               + kHotkeySelectCurrentActions.size() + kHotkeySimpleFleetActions.size()
                                               + kHotkeyRuntimeSpaceActions.size() + kHotkeyChatOpenActions.size()
                                               + kHotkeyChatChannelActions.size() + kHotkeyOfficerCanvasActions.size()
                                               + kHotkeyTableDispatchActions.size()>
       actions{};
  auto output = actions.begin();

  for (const auto action : kHotkeyStartupActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyQuitActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyQueueActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyShipSelectionActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeySelectCurrentActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeySimpleFleetActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyRuntimeSpaceActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyChatOpenActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyChatChannelActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyOfficerCanvasActions) {
    *output++ = action;
  }
  for (const auto action : kHotkeyTableDispatchActions) {
    *output++ = action;
  }

  return actions;
}();

input_binding::ModifierMask held_modifier_mask()
{
  input_binding::ModifierMask modifiers;
  for (const auto modifier_key : {KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                                  KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::LeftWindows, KeyCode::RightWindows,
                                  KeyCode::LeftCommand, KeyCode::RightCommand, KeyCode::AltGr}) {
    if (Key::Pressed(modifier_key)) {
      modifiers.Merge(input_binding::ModifierMask::FromPressedKey(modifier_key));
    }
  }

  return modifiers;
}

std::vector<input_binding::DispatchKeyState> build_dispatch_key_snapshot(std::span<const KeyCode> watched_keys)
{
  auto key_states = std::vector<input_binding::DispatchKeyState>{};
  key_states.reserve(watched_keys.size());

  const auto modifiers = held_modifier_mask();
  for (const auto key : watched_keys) {
    key_states.push_back({key, modifiers, Key::Down(key), Key::Pressed(key)});
  }

  return key_states;
}

input_binding::DispatchPlan frame_runtime_dispatch_plan()
{
  const auto& runtime_bindings = input_binding::RuntimeBindingModel();
  const auto  watched_keys =
      input_binding::WatchedKeysForActions(runtime_bindings, input_binding::InputPhase::Frame, kHotkeyFrameActions);

  const auto key_states = build_dispatch_key_snapshot(watched_keys);
  return input_binding::PlanDispatchSnapshot(runtime_bindings, input_binding::InputPhase::Frame,
                                             input_binding::ActiveLayers::All(), key_states);
}

bool runtime_binding_winner_present(const input_binding::DispatchPlan& plan, const input_binding::InputActionId action,
                                    const input_binding::InputLayer layer)
{
  return std::ranges::any_of(
      plan.winners, [action, layer](const auto& winner) { return winner.action == action && winner.layer == layer; });
}

input_binding::InputActionId first_runtime_binding_winner(const input_binding::DispatchPlan&                  plan,
                                                          const std::span<const input_binding::InputActionId> actions)
{
  const auto found = std::ranges::find_if(plan.winners, [&actions](const auto& winner) {
    return std::ranges::find(actions, winner.action) != actions.end();
  });
  return found == plan.winners.end() ? input_binding::InputActionId::Max : found->action;
}

int ship_select_request_from_runtime_bindings(const input_binding::DispatchPlan& plan)
{
  return hotkey_router_ship_select_request(std::array<bool, 8>{
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip1, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip2, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip3, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip4, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip5, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip6, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip7, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip8, input_binding::InputLayer::Fleet),
  });
}

GameFunction dispatcher_owned_game_function(const input_binding::InputActionId action)
{
  switch (action) {
    case input_binding::InputActionId::ShowQTrials:
      return GameFunction::ShowQTrials;
    case input_binding::InputActionId::ShowBookmarks:
      return GameFunction::ShowBookmarks;
    case input_binding::InputActionId::ShowLookup:
      return GameFunction::ShowLookup;
    case input_binding::InputActionId::ShowRefinery:
      return GameFunction::ShowRefinery;
    case input_binding::InputActionId::ShowFactions:
      return GameFunction::ShowFactions;
    case input_binding::InputActionId::ShowStationExterior:
      return GameFunction::ShoWStationExterior;
    case input_binding::InputActionId::ShowGalaxy:
      return GameFunction::ShowGalaxy;
    case input_binding::InputActionId::ShowStationInterior:
      return GameFunction::ShowStationInterior;
    case input_binding::InputActionId::ShowSystem:
      return GameFunction::ShowSystem;
    case input_binding::InputActionId::ShowArtifacts:
      return GameFunction::ShowArtifacts;
    case input_binding::InputActionId::ShowInventory:
      return GameFunction::ShowInventory;
    case input_binding::InputActionId::ShowMissions:
      return GameFunction::ShowMissions;
    case input_binding::InputActionId::ShowResearch:
      return GameFunction::ShowResearch;
    case input_binding::InputActionId::ShowScrapYard:
      return GameFunction::ShowScrapYard;
    case input_binding::InputActionId::ShowOfficers:
      return GameFunction::ShowOfficers;
    case input_binding::InputActionId::ShowCommander:
      return GameFunction::ShowCommander;
    case input_binding::InputActionId::ShowAwayTeam:
      return GameFunction::ShowAwayTeam;
    case input_binding::InputActionId::ShowEvents:
      return GameFunction::ShowEvents;
    case input_binding::InputActionId::ShowExoComp:
      return GameFunction::ShowExoComp;
    case input_binding::InputActionId::ShowDaily:
      return GameFunction::ShowDaily;
    case input_binding::InputActionId::ShowGifts:
      return GameFunction::ShowGifts;
    case input_binding::InputActionId::ShowAlliance:
      return GameFunction::ShowAlliance;
    case input_binding::InputActionId::ShowAllianceHelp:
      return GameFunction::ShowAllianceHelp;
    case input_binding::InputActionId::ShowAllianceArmada:
      return GameFunction::ShowAllianceArmada;
    case input_binding::InputActionId::ShowSettings:
      return GameFunction::ShowSettings;
    case input_binding::InputActionId::UiScaleUp:
      return GameFunction::UiScaleUp;
    case input_binding::InputActionId::UiScaleDown:
      return GameFunction::UiScaleDown;
    case input_binding::InputActionId::UiViewerScaleUp:
      return GameFunction::UiViewerScaleUp;
    case input_binding::InputActionId::UiViewerScaleDown:
      return GameFunction::UiViewerScaleDown;
    case input_binding::InputActionId::TogglePreviewLocate:
      return GameFunction::TogglePreviewLocate;
    case input_binding::InputActionId::TogglePreviewRecall:
      return GameFunction::TogglePreviewRecall;
    case input_binding::InputActionId::ToggleCargoDefault:
      return GameFunction::ToggleCargoDefault;
    case input_binding::InputActionId::ToggleCargoPlayer:
      return GameFunction::ToggleCargoPlayer;
    case input_binding::InputActionId::ToggleCargoStation:
      return GameFunction::ToggleCargoStation;
    case input_binding::InputActionId::ToggleCargoHostile:
      return GameFunction::ToggleCargoHostile;
    case input_binding::InputActionId::ToggleCargoArmada:
      return GameFunction::ToggleCargoArmada;
    case input_binding::InputActionId::LogOff:
      return GameFunction::LogLevelOff;
    case input_binding::InputActionId::LogError:
      return GameFunction::LogLevelError;
    case input_binding::InputActionId::LogWarn:
      return GameFunction::LogLevelWarn;
    case input_binding::InputActionId::LogInfo:
      return GameFunction::LogLevelInfo;
    case input_binding::InputActionId::LogDebug:
      return GameFunction::LogLevelDebug;
    case input_binding::InputActionId::LogTrace:
      return GameFunction::LogLevelTrace;
    case input_binding::InputActionId::ShowShips:
      return GameFunction::ShowShips;
    default:
      return GameFunction::Max;
  }
}

HotkeyRouterDispatchAction dispatch_runtime_bound_table_action(const input_binding::InputActionId action)
{
  const auto game_function = dispatcher_owned_game_function(action);
  if (game_function == GameFunction::Max) {
    return HotkeyRouterDispatchAction::Continue;
  }

  for (const auto& entry : GetHotkeyDispatchTable()) {
    if (entry.game_function != game_function) {
      continue;
    }

    const auto decision = entry.handler();
    return hotkey_router_dispatch_action(true, decision == DispatchDecision::HandledStop,
                                         decision == DispatchDecision::HandledAllowOriginal);
  }

  return HotkeyRouterDispatchAction::Continue;
}

void dispatch_runtime_bound_simple_fleet_action(const input_binding::InputActionId action)
{
  switch (action) {
    case input_binding::InputActionId::FleetQueueClear:
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat()) {
        if (auto fleet_bar = ObjectFinder<FleetBarViewController>::Get(); fleet_bar) {
          ClearFleetActionQueue(fleet_bar);
        }
      }
      break;
    default:
      break;
  }
}

HotkeyRouterStartupAction startup_action_from_runtime_bindings(const input_binding::DispatchPlan& plan,
                                                               ScopelyShortcutPolicy              scopely_shortcuts,
                                                               bool                               hotkeys_enabled)
{
  return hotkey_router_startup_action(runtime_binding_winner_present(plan, input_binding::InputActionId::HotkeysDisable,
                                                                     input_binding::InputLayer::Global),
                                      runtime_binding_winner_present(plan, input_binding::InputActionId::HotkeysEnable,
                                                                     input_binding::InputLayer::Global),
                                      scopely_shortcuts, hotkeys_enabled);
}
} // namespace

// ─── Main Per-Frame Hotkey Router ─────────────────────────────────────────────────────

// Returns true when the original ScreenManager::Update should be called.
bool hotkey_router_screen_update(ScreenManager* _this)
{
  Key::ResetCache();

  const auto runtime_dispatch_plan = frame_runtime_dispatch_plan();

  switch (startup_action_from_runtime_bindings(runtime_dispatch_plan, ScopelyShortcutsPolicy(),
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
  if (first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQuitActions) == input_binding::InputActionId::Quit) {
    TerminateProcess(GetCurrentProcess(), 1);
    return false;
  }
#endif

  // ─── Ship selection (1-8 keys) ───────────────────────────────────────────────────────
  const auto ship_select_request = ship_select_request_from_runtime_bindings(runtime_dispatch_plan);

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
      if (first_runtime_binding_winner(runtime_dispatch_plan, kHotkeySelectCurrentActions)
          == input_binding::InputActionId::SelectCurrent) {
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
      const auto queue_toggle_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQueueActions);
      if (hotkey_router_should_toggle_queue(is_in_chat, Key::IsInputFocused(),
                                            queue_toggle_action == input_binding::InputActionId::FleetQueueToggle)) {
        config->queue_enabled = !config->queue_enabled;
        return false;
      }

      // ShowChat
      const auto chat_open_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyChatOpenActions);
      if (chat_open_action != input_binding::InputActionId::Max) {
        if (auto chat_manager = ChatManager::Instance(); chat_manager) {
          if (chat_manager->IsSideChatOpen) {
            if (auto view_controller = ObjectFinder<FullScreenChatViewController>::Get(); view_controller) {
              if (auto message_list = view_controller->_messageList; message_list) {
                if (auto message_field = message_list->_inputField; message_field) {
                  message_field->ActivateInputField();
                }
              }
            }
          } else if (chat_open_action == input_binding::InputActionId::ShowChatSide1
                     || chat_open_action == input_binding::InputActionId::ShowChatSide2) {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Side);
          } else {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Fullscreen);
          }
        }
      }

      // MoveLeft / MoveRight (officer canvas)
      switch (first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyOfficerCanvasActions)) {
        case input_binding::InputActionId::MoveLeft:
          if (MoveOfficerCanvas(true)) {
            return false;
          }
          break;
        case input_binding::InputActionId::MoveRight:
          if (MoveOfficerCanvas(false)) {
            return false;
          }
          break;
        default:
          break;
      }

      if (const auto action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyTableDispatchActions);
          action != input_binding::InputActionId::Max) {
        if (dispatch_runtime_bound_table_action(action) == HotkeyRouterDispatchAction::SuppressOriginal) {
          return false;
        }
      }
    }
  } else {
    // ─── In-chat channel selection ─────────────────────────────────────────────────────
    if (auto chat_manager = ChatManager::Instance(); chat_manager) {
      switch (first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyChatChannelActions)) {
        case input_binding::InputActionId::SelectChatGlobal:
          chat_manager->OpenChannel(ChatChannelCategory::Global);
          return false;
        case input_binding::InputActionId::SelectChatAlliance:
          chat_manager->OpenChannel(ChatChannelCategory::Alliance);
          return false;
        case input_binding::InputActionId::SelectChatPrivate:
          chat_manager->OpenChannel(ChatChannelCategory::Private);
          return false;
        default:
          break;
      }
    }
  }

  if (!Key::IsInputFocused()) {
    const auto simple_fleet_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeySimpleFleetActions);
    const auto space_action_inputs = hotkey_router_runtime_space_action_inputs(
        runtime_binding_winner_present(runtime_dispatch_plan, input_binding::InputActionId::FleetPrimary,
                                       input_binding::InputLayer::Fleet),
        runtime_binding_winner_present(runtime_dispatch_plan, input_binding::InputActionId::FleetSecondary,
                                       input_binding::InputLayer::Fleet),
        runtime_binding_winner_present(runtime_dispatch_plan, input_binding::InputActionId::FleetService,
                                       input_binding::InputLayer::Fleet));

    // Escape to hide object viewers
    if (Key::Pressed(KeyCode::Escape) && DidHideViewers()) {
      return false;
    }

    // Dismiss golden rewards screen
    if (Key::Pressed(KeyCode::Escape) || space_action_inputs.primary) {
      if (TryDismissRewardsScreen()) {
        return false;
      }
    }

    if (simple_fleet_action == input_binding::InputActionId::FleetQueueClear) {
      dispatch_runtime_bound_simple_fleet_action(simple_fleet_action);
    }

    // Space actions (engage, scan, recall, repair, queue, etc.)
    if (hotkey_router_should_execute_space_action(space_action_inputs, force_space_action_next_frame)) {
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat() && !Key::IsInputFocused()) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          bool was_forced          = force_space_action_next_frame;
          auto deferred_generation = DeferredSpaceActionGeneration();
          ExecuteSpaceAction(fleet_bar, space_action_inputs);
          if (was_forced && DeferredSpaceActionGeneration() == deferred_generation) {
            ClearDeferredSpaceAction();
          }
        }
      }
    }

    // ActionView — toggle cargo/rewards info panel
    if (simple_fleet_action == input_binding::InputActionId::FleetViewInfo) {
      HandleActionView();
    }

    // Tick the info pending counter (multi-frame show)
    TickInfoPending();
  }

  return true;
}

// ─── Hook Delegate Functions ─────────────────────────────────────────────────────────

bool hotkey_router_should_call_original_initialize_actions()
{ return should_call_original_initialize_actions(ScopelyShortcutsPolicy()); }

bool hotkey_router_should_call_original_screen_update(bool routerAllowsOriginal)
{ return should_call_original_screen_update(routerAllowsOriginal, OriginalFramePolicySetting()); }

void hotkey_router_bind_context(RewardsButtonWidget* _this)
{ HandleCargoBindContext(_this); }

void hotkey_router_show_fleet(PreScanTargetWidget* _this)
{ HandleCargoShowFleet(_this); }
