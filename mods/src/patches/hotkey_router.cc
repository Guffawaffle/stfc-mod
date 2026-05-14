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
#include "patches/input_binding/action_registry.h"
#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "patches/key.h"
#include "patches/mod_impact_monitor.h"
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
struct FrameRuntimeDispatchCache {
  uint64_t                                     generation = 0;
  std::vector<KeyCode>                         watched_keys;
  std::vector<input_binding::DispatchKeyState> key_states;
  input_binding::DispatchPlan                  plan;
};

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

FrameRuntimeDispatchCache& frame_runtime_dispatch_cache()
{
  static auto cache = FrameRuntimeDispatchCache{};
  return cache;
}

void rebuild_frame_runtime_watched_keys(FrameRuntimeDispatchCache&          cache,
                                        const input_binding::CompileResult& runtime_bindings)
{
  const auto generation = input_binding::RuntimeBindingGeneration();
  if (cache.generation == generation) {
    return;
  }

  cache.watched_keys =
      input_binding::WatchedKeysForActions(runtime_bindings, input_binding::InputPhase::Frame, kHotkeyFrameActions);
  cache.generation = generation;
}

void build_dispatch_key_snapshot(std::span<const KeyCode>                      watched_keys,
                                 std::vector<input_binding::DispatchKeyState>& key_states)
{
  key_states.clear();
  key_states.reserve(watched_keys.size());

  const auto modifiers = held_modifier_mask();
  for (const auto key : watched_keys) {
    key_states.push_back({key, modifiers, Key::Down(key), Key::Pressed(key)});
  }
}

const input_binding::DispatchPlan& frame_runtime_dispatch_plan()
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyDispatchPlan, ModImpactMonitorEnabled());

  auto&       cache            = frame_runtime_dispatch_cache();
  const auto& runtime_bindings = input_binding::RuntimeBindingModel();
  rebuild_frame_runtime_watched_keys(cache, runtime_bindings);
  build_dispatch_key_snapshot(cache.watched_keys, cache.key_states);
  input_binding::PlanDispatchSnapshot(runtime_bindings, input_binding::InputPhase::Frame,
                                      input_binding::ActiveLayers::All(), cache.key_states, cache.plan);

  return cache.plan;
}

bool runtime_binding_winner_present(const input_binding::DispatchPlan& plan, const input_binding::InputActionId action,
                                    const input_binding::InputLayer layer)
{ return plan.winner_lookup.Contains(action, layer); }

const input_binding::DispatchCandidate* runtime_binding_winner(const input_binding::DispatchPlan& plan,
                                                               const input_binding::InputActionId action,
                                                               const input_binding::InputLayer    layer)
{
  for (const auto& winner : plan.winners) {
    if (winner.action == action && winner.layer == layer) {
      return &winner;
    }
  }
  return nullptr;
}

bool runtime_binding_consumes_original_key_event(const input_binding::DispatchPlan& plan,
                                                 const input_binding::InputActionId action,
                                                 const input_binding::InputLayer    layer)
{
  const auto* winner = runtime_binding_winner(plan, action, layer);
  return winner && input_binding::ConsumesOriginalKeyEvent(*winner);
}

input_binding::InputActionId first_runtime_binding_winner(const input_binding::DispatchPlan&                  plan,
                                                          const std::span<const input_binding::InputActionId> actions)
{ return plan.winner_lookup.First(actions); }

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

void log_runtime_winner(const char* route, const input_binding::DispatchPlan& plan,
                        const input_binding::InputActionId action, const input_binding::InputLayer layer)
{
  if (!ModImpactMonitorEnabled()) {
    return;
  }

  const auto* winner = runtime_binding_winner(plan, action, layer);
  if (!winner) {
    return;
  }

  spdlog::info("[HotkeyDiag] route={} action={} key={} trigger={} modifiers_logical={} modifiers_physical={}", route,
               input_action_name(action), key_code_name(winner->key), trigger_mode_name(winner->trigger_mode),
               winner->held_modifiers.logical_bits(), winner->held_modifiers.physical_bits());
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
{ return input_binding::ActionGameFunction(action); }

HotkeyRouterDispatchAction dispatch_runtime_bound_table_action(const input_binding::DispatchCandidate& candidate)
{
  const auto action        = candidate.action;
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
                                         decision == DispatchDecision::HandledAllowOriginal,
                                         input_binding::ConsumesOriginalKeyEvent(candidate));
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
  {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyResetCache, ModImpactMonitorEnabled());
    Key::ResetCache();
  }

  const auto& runtime_dispatch_plan = frame_runtime_dispatch_plan();

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

  bool is_in_chat    = false;
  bool input_focused = false;
  {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyContextState, ModImpactMonitorEnabled());
    is_in_chat    = Hub::IsInChat();
    input_focused = Key::IsInputFocused();
  }
  const auto config = &Config::Get();

#ifdef _WIN32
  if (hotkey_router_quit_action(first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQuitActions)
                                == input_binding::InputActionId::Quit)
      == HotkeyRouterQuitAction::QuitProcess) {
    TerminateProcess(GetCurrentProcess(), 1);
    return false;
  }
#endif

  // ─── Ship selection (1-8 keys) ───────────────────────────────────────────────────────
  const auto ship_select_request           = ship_select_request_from_runtime_bindings(runtime_dispatch_plan);
  auto       ship_select_consumes_original = false;
  if (ship_select_request != -1) {
    ship_select_consumes_original = runtime_binding_consumes_original_key_event(
        runtime_dispatch_plan, kHotkeyShipSelectionActions[ship_select_request], input_binding::InputLayer::Fleet);
    log_runtime_winner("ship-selection", runtime_dispatch_plan, kHotkeyShipSelectionActions[ship_select_request],
                       input_binding::InputLayer::Fleet);
  }

  {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyShipSelection, ModImpactMonitorEnabled());
    if (HandleShipSelection(ship_select_request)) {
      return !ship_select_consumes_original;
    }
  }

  // ─── Escape in chat / input focus ───────────────────────────────────────────────────
  if (hotkey_router_should_clear_input_focus(Key::Pressed(KeyCode::Escape), input_focused, is_in_chat)) {
    Key::ClearInputFocus();
    return false;
  }

  if (!is_in_chat) {
    if (!input_focused) {
      ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyUiRouting, ModImpactMonitorEnabled());

      // SelectCurrent — locate active fleet
      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiSelectCurrent, ModImpactMonitorEnabled());
        if (hotkey_router_select_current_action(
                is_in_chat, input_focused,
                first_runtime_binding_winner(runtime_dispatch_plan, kHotkeySelectCurrentActions)
                    == input_binding::InputActionId::SelectCurrent)
            == HotkeyRouterSelectCurrentAction::ViewActiveFleet) {
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
      }

      // ToggleQueue
      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiQueueToggle, ModImpactMonitorEnabled());
        const auto queue_toggle_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyQueueActions);
        if (hotkey_router_should_toggle_queue(is_in_chat, input_focused,
                                              queue_toggle_action == input_binding::InputActionId::FleetQueueToggle)) {
          config->queue_enabled = !config->queue_enabled;
          return false;
        }
      }

      // ShowChat
      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiChatOpen, ModImpactMonitorEnabled());
        const auto chat_open_action = first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyChatOpenActions);
        if (chat_open_action != input_binding::InputActionId::Max) {
          log_runtime_winner("chat-open", runtime_dispatch_plan, chat_open_action, input_binding::InputLayer::Global);
          auto         chat_open_consumes_original = false;
          auto         chat_open_handled           = false;
          ChatManager* chat_manager                = nullptr;
          {
            ScopedModImpactTimer chat_manager_timer(ModImpactProbe::HotkeyUiChatManagerLookup,
                                                    ModImpactMonitorEnabled());
            chat_manager = ChatManager::Instance();
          }
          if (chat_manager) {
            chat_open_consumes_original = runtime_binding_consumes_original_key_event(
                runtime_dispatch_plan, chat_open_action, input_binding::InputLayer::Global);
            switch (hotkey_router_chat_open_action(is_in_chat, input_focused, chat_manager->IsSideChatOpen,
                                                   chat_open_action)) {
              case HotkeyRouterChatOpenAction::ActivateExistingInput: {
                ScopedModImpactTimer activate_timer(ModImpactProbe::HotkeyUiChatActivateInput,
                                                    ModImpactMonitorEnabled());
                if (auto view_controller = ObjectFinder<FullScreenChatViewController>::Get(); view_controller) {
                  if (auto message_list = view_controller->_messageList; message_list) {
                    if (auto message_field = message_list->_inputField; message_field) {
                      message_field->ActivateInputField();
                    }
                  }
                }
              }
                chat_open_handled = true;
                break;
              case HotkeyRouterChatOpenAction::OpenAllianceSide: {
                ScopedModImpactTimer open_timer(ModImpactProbe::HotkeyUiChatOpenChannel, ModImpactMonitorEnabled());
                chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Side);
              }
                chat_open_handled = true;
                break;
              case HotkeyRouterChatOpenAction::OpenAllianceFullscreen: {
                ScopedModImpactTimer open_timer(ModImpactProbe::HotkeyUiChatOpenChannel, ModImpactMonitorEnabled());
                chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Fullscreen);
              }
                chat_open_handled = true;
                break;
              default:
                break;
            }
          }
          if (chat_open_handled && chat_open_consumes_original) {
            return false;
          }
        }
      }

      // MoveLeft / MoveRight (officer canvas)
      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiOfficerCanvas, ModImpactMonitorEnabled());
        switch (hotkey_router_officer_canvas_action(
            is_in_chat, input_focused,
            first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyOfficerCanvasActions))) {
          case HotkeyRouterOfficerCanvasAction::MoveLeft:
            if (MoveOfficerCanvas(true)) {
              return false;
            }
            break;
          case HotkeyRouterOfficerCanvasAction::MoveRight:
            if (MoveOfficerCanvas(false)) {
              return false;
            }
            break;
          default:
            break;
        }
      }

      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiTableDispatch, ModImpactMonitorEnabled());
        if (const auto action = hotkey_router_table_dispatch_request(
                is_in_chat, input_focused,
                first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyTableDispatchActions));
            action != input_binding::InputActionId::Max) {
          log_runtime_winner("table-dispatch", runtime_dispatch_plan, action, input_binding::InputLayer::Global);
          const auto* table_winner =
              runtime_binding_winner(runtime_dispatch_plan, action, input_binding::InputLayer::Global);
          if (table_winner
              && dispatch_runtime_bound_table_action(*table_winner) == HotkeyRouterDispatchAction::SuppressOriginal) {
            return false;
          }
        }
      }
    }
  } else {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyUiRouting, ModImpactMonitorEnabled());
    ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiChatChannel, ModImpactMonitorEnabled());

    // ─── In-chat channel selection ─────────────────────────────────────────────────────
    if (auto chat_manager = ChatManager::Instance(); chat_manager) {
      switch (hotkey_router_chat_channel_action(
          is_in_chat, first_runtime_binding_winner(runtime_dispatch_plan, kHotkeyChatChannelActions))) {
        case HotkeyRouterChatChannelAction::Global:
          chat_manager->OpenChannel(ChatChannelCategory::Global);
          return false;
        case HotkeyRouterChatChannelAction::Alliance:
          chat_manager->OpenChannel(ChatChannelCategory::Alliance);
          return false;
        case HotkeyRouterChatChannelAction::Private:
          chat_manager->OpenChannel(ChatChannelCategory::Private);
          return false;
        default:
          break;
      }
    }
  }

  if (!input_focused) {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyFleetRouting, ModImpactMonitorEnabled());

    const auto simple_fleet_action = hotkey_router_simple_fleet_action(
        input_focused, first_runtime_binding_winner(runtime_dispatch_plan, kHotkeySimpleFleetActions));
    if (simple_fleet_action == HotkeyRouterSimpleFleetAction::QueueClear) {
      log_runtime_winner("fleet-simple", runtime_dispatch_plan, input_binding::InputActionId::FleetQueueClear,
                         input_binding::InputLayer::Fleet);
    }
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

    if (simple_fleet_action == HotkeyRouterSimpleFleetAction::QueueClear) {
      dispatch_runtime_bound_simple_fleet_action(input_binding::InputActionId::FleetQueueClear);
      if (runtime_binding_consumes_original_key_event(
              runtime_dispatch_plan, input_binding::InputActionId::FleetQueueClear, input_binding::InputLayer::Fleet)) {
        return false;
      }
    }

    // Space actions (engage, scan, recall, repair, queue, etc.)
    if (hotkey_router_should_execute_space_action(space_action_inputs, force_space_action_next_frame)) {
      auto handled_space_action = false;
      if (space_action_inputs.primary) {
        log_runtime_winner("fleet-space", runtime_dispatch_plan, input_binding::InputActionId::FleetPrimary,
                           input_binding::InputLayer::Fleet);
      }
      if (space_action_inputs.secondary) {
        log_runtime_winner("fleet-space", runtime_dispatch_plan, input_binding::InputActionId::FleetSecondary,
                           input_binding::InputLayer::Fleet);
      }
      if (space_action_inputs.recall || space_action_inputs.repair) {
        log_runtime_winner("fleet-space", runtime_dispatch_plan, input_binding::InputActionId::FleetService,
                           input_binding::InputLayer::Fleet);
      }
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat() && !input_focused) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          bool was_forced          = force_space_action_next_frame;
          auto deferred_generation = DeferredSpaceActionGeneration();
          {
            ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeySpaceAction, ModImpactMonitorEnabled());
            ExecuteSpaceAction(fleet_bar, space_action_inputs);
            handled_space_action = true;
          }
          if (hotkey_router_should_clear_deferred_space_action(was_forced, deferred_generation,
                                                               DeferredSpaceActionGeneration())) {
            spdlog::trace("[SpaceActionDiag] cleared-deferred-retry reason=no-generation-advance generation={}",
                          deferred_generation);
            ClearDeferredSpaceAction();
          }
        }
      }
      if (handled_space_action
          && (runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetPrimary, input_binding::InputLayer::Fleet)
              || runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetSecondary, input_binding::InputLayer::Fleet)
              || runtime_binding_consumes_original_key_event(runtime_dispatch_plan,
                                                             input_binding::InputActionId::FleetService,
                                                             input_binding::InputLayer::Fleet))) {
        return false;
      }
    }

    // ActionView — toggle cargo/rewards info panel
    if (simple_fleet_action == HotkeyRouterSimpleFleetAction::ViewInfo) {
      HandleActionView();
      if (runtime_binding_consumes_original_key_event(
              runtime_dispatch_plan, input_binding::InputActionId::FleetViewInfo, input_binding::InputLayer::Fleet)) {
        return false;
      }
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
