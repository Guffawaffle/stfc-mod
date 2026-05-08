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
#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
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
#include <vector>

namespace {
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
  std::array<input_binding::InputActionId,
             kHotkeyStartupActions.size() + kHotkeyQuitActions.size() + kHotkeyQueueActions.size()
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
  for (const auto action : kHotkeyTableDispatchActions) {
    *output++ = action;
  }

  return actions;
}();

input_binding::ModifierMask held_modifier_mask()
{
  input_binding::ModifierMask modifiers;
  for (const auto modifier_key : {KeyCode::LeftShift,
                                  KeyCode::RightShift,
                                  KeyCode::LeftControl,
                                  KeyCode::RightControl,
                                  KeyCode::LeftAlt,
                                  KeyCode::RightAlt,
                                  KeyCode::LeftWindows,
                                  KeyCode::RightWindows,
                                  KeyCode::LeftCommand,
                                  KeyCode::RightCommand,
                                  KeyCode::AltGr}) {
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
  const auto watched_keys =
      input_binding::WatchedKeysForActions(runtime_bindings, input_binding::InputPhase::Frame, kHotkeyFrameActions);

  const auto key_states = build_dispatch_key_snapshot(watched_keys);
  return input_binding::PlanDispatchSnapshot(runtime_bindings,
                                             input_binding::InputPhase::Frame,
                                             input_binding::ActiveLayers::All(),
                                             key_states);
}

bool runtime_binding_winner_present(const input_binding::DispatchPlan& plan,
                                    const input_binding::InputActionId action,
                                    const input_binding::InputLayer layer)
{
  return std::ranges::any_of(plan.winners, [action, layer](const auto& winner) {
    return winner.action == action && winner.layer == layer;
  });
}

input_binding::InputActionId first_runtime_binding_winner(const input_binding::DispatchPlan& plan,
                                                          const std::span<const input_binding::InputActionId> actions)
{
  const auto found = std::ranges::find_if(plan.winners, [&actions](const auto& winner) {
    return std::ranges::find(actions, winner.action) != actions.end();
  });
  return found == plan.winners.end() ? input_binding::InputActionId::Max : found->action;
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

bool is_dispatcher_owned_game_function(const GameFunction game_function)
{
  switch (game_function) {
    case GameFunction::ShowQTrials:
    case GameFunction::ShowBookmarks:
    case GameFunction::ShowLookup:
    case GameFunction::ShowRefinery:
    case GameFunction::ShowFactions:
    case GameFunction::ShoWStationExterior:
    case GameFunction::ShowGalaxy:
    case GameFunction::ShowStationInterior:
    case GameFunction::ShowSystem:
    case GameFunction::ShowArtifacts:
    case GameFunction::ShowInventory:
    case GameFunction::ShowMissions:
    case GameFunction::ShowResearch:
    case GameFunction::ShowScrapYard:
    case GameFunction::ShowOfficers:
    case GameFunction::ShowCommander:
    case GameFunction::ShowAwayTeam:
    case GameFunction::ShowEvents:
    case GameFunction::ShowExoComp:
    case GameFunction::ShowDaily:
    case GameFunction::ShowGifts:
    case GameFunction::ShowAlliance:
    case GameFunction::ShowAllianceHelp:
    case GameFunction::ShowAllianceArmada:
    case GameFunction::ShowSettings:
    case GameFunction::UiScaleUp:
    case GameFunction::UiScaleDown:
    case GameFunction::UiViewerScaleUp:
    case GameFunction::UiViewerScaleDown:
    case GameFunction::TogglePreviewLocate:
    case GameFunction::TogglePreviewRecall:
    case GameFunction::ToggleCargoDefault:
    case GameFunction::ToggleCargoPlayer:
    case GameFunction::ToggleCargoStation:
    case GameFunction::ToggleCargoHostile:
    case GameFunction::ToggleCargoArmada:
    case GameFunction::LogLevelOff:
    case GameFunction::LogLevelError:
    case GameFunction::LogLevelWarn:
    case GameFunction::LogLevelInfo:
    case GameFunction::LogLevelDebug:
    case GameFunction::LogLevelTrace:
    case GameFunction::ShowShips:
      return true;
    default:
      return false;
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
    return hotkey_router_dispatch_action(true,
                                         decision == DispatchDecision::HandledStop,
                                         decision == DispatchDecision::HandledAllowOriginal);
  }

  return HotkeyRouterDispatchAction::Continue;
}

HotkeyRouterStartupAction startup_action_from_runtime_bindings(const input_binding::DispatchPlan& plan,
                                                               bool use_scopely_hotkeys,
                                                               bool hotkeys_enabled)
{
  return hotkey_router_startup_action(runtime_binding_winner_present(plan,
                                                                     input_binding::InputActionId::HotkeysDisable,
                                                                     input_binding::InputLayer::Global),
                                      runtime_binding_winner_present(plan,
                                                                     input_binding::InputActionId::HotkeysEnable,
                                                                     input_binding::InputLayer::Global),
                                      use_scopely_hotkeys,
                                      hotkeys_enabled);
}
}

// ─── Main Per-Frame Hotkey Router ─────────────────────────────────────────────────────

// Returns true when the original ScreenManager::Update should be called.
bool hotkey_router_screen_update(ScreenManager* _this)
{
  Key::ResetCache();

  const auto runtime_dispatch_plan = frame_runtime_dispatch_plan();

  switch (startup_action_from_runtime_bindings(runtime_dispatch_plan,
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
  if (first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQuitActions)
      == input_binding::InputActionId::Quit) {
    TerminateProcess(GetCurrentProcess(), 1);
    return false;
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
      const auto queue_toggle_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQueueActions);
      if (hotkey_router_should_toggle_queue(is_in_chat, Key::IsInputFocused(),
                                            queue_toggle_action == input_binding::InputActionId::FleetQueueToggle)) {
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

      if (const auto action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyTableDispatchActions);
          action != input_binding::InputActionId::Max) {
        if (dispatch_runtime_bound_table_action(action) == HotkeyRouterDispatchAction::SuppressOriginal) {
          return false;
        }
      }

      // Table-driven hotkey dispatch
      for (const auto& entry : GetHotkeyDispatchTable()) {
        if (is_dispatcher_owned_game_function(entry.game_function)) {
          continue;
        }

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
          auto deferred_generation = DeferredSpaceActionGeneration();
          ExecuteSpaceAction(fleet_bar);
          if (was_forced && DeferredSpaceActionGeneration() == deferred_generation) {
            ClearDeferredSpaceAction();
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
