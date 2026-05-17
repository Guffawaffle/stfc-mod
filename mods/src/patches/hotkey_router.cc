/**
 * @file hotkey_router.cc
 * @brief Central hotkey routing logic — called every frame from ScreenManager::Update hook.
 *
 * This is the main keyboard input processing loop for the community patch.
 * It handles enable/disable toggling, ship selection, escape behavior, chat
 * channel shortcuts, space actions (engage/scan/mine/recall/repair), and the
 * table-driven dispatch system. Escape-driven exit suppression is enforced at
 * SectionManager::BackButtonPressed rather than this frame router.
 *
 * The original monolithic implementation has been split across several
 * focused translation units that this file delegates to:
 *   - hotkey_router_action_table.h     — per-frame InputAction grouping tables
 *   - hotkey_router_modifier_query.*   — Unity / Win32 key-state probes
 *   - hotkey_router_dispatch_cache.*   — per-frame DispatchPlan cache
 *   - hotkey_router_native_fleet_guard — singleton guard state + bypass RAII
 *   - hotkey_router_runtime_query.*    — winner queries + handler dispatch
 */
#include "config.h"
#include "errormsg.h"

#include "patches/hotkey_router.h"

#include "patches/cargo_display.h"
#include "patches/fleet_actions.h"
#include "patches/hotkey_router_action_table.h"
#include "patches/hotkey_router_dispatch_cache.h"
#include "patches/hotkey_router_native_fleet_guard.h"
#include "patches/hotkey_router_runtime_query.h"
#include "patches/input_binding/action_registry.h"
#include "patches/input_binding/input_dispatcher.h"
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

namespace
{
namespace actions = hotkey_router_actions;
namespace cache   = hotkey_router_dispatch_cache;
namespace query   = hotkey_router_runtime_query;
} // namespace

// ─── Main Per-Frame Hotkey Router ─────────────────────────────────────────────────────

// Returns true when the original ScreenManager::Update should be called.
bool hotkey_router_screen_update(ScreenManager* _this)
{
  {
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyResetCache, ModImpactMonitorEnabled());
    Key::ResetCache();
  }

  const auto& runtime_dispatch_plan  = cache::frame_runtime_dispatch_plan();
  const auto  scopely_shortcuts      = ScopelyShortcutsPolicy();
  const auto  dispatcher_owns_inputs = hotkey_dispatcher_owns_inputs(Config::Get().hotkeys_enabled, scopely_shortcuts);
  hotkey_router_update_native_fleet_selection_guard(
      runtime_dispatch_plan, cache::frame_runtime_dispatch_cache().key_states, dispatcher_owns_inputs);
  const auto startup_action = query::startup_action_from_runtime_bindings(runtime_dispatch_plan, scopely_shortcuts,
                                                                          Config::Get().hotkeys_enabled);

  switch (startup_action) {
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
  if (hotkey_router_quit_action(query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kQuit)
                                == input_binding::InputActionId::Quit)
      == HotkeyRouterQuitAction::QuitProcess) {
    TerminateProcess(GetCurrentProcess(), 1);
    return false;
  }
#endif

  // ─── Ship selection (1-8 keys) ───────────────────────────────────────────────────────
  const auto ship_select_request           = query::ship_select_request_from_runtime_bindings(runtime_dispatch_plan);
  auto       ship_select_consumes_original = false;
  if (ship_select_request != -1) {
    ship_select_consumes_original = query::runtime_binding_consumes_original_key_event(
        runtime_dispatch_plan, actions::kShipSelection[ship_select_request], input_binding::InputLayer::Fleet);
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
                query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kSelectCurrent)
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
        const auto queue_toggle_action = query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kQueue);
        if (hotkey_router_should_toggle_queue(is_in_chat, input_focused,
                                              queue_toggle_action == input_binding::InputActionId::FleetQueueToggle)) {
          config->queue_enabled = !config->queue_enabled;
          return false;
        }
      }

      // ShowChat
      {
        ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyUiChatOpen, ModImpactMonitorEnabled());
        const auto chat_open_action = query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kChatOpen);
        if (chat_open_action != input_binding::InputActionId::Max) {
          auto         chat_open_consumes_original = false;
          auto         chat_open_handled           = false;
          ChatManager* chat_manager                = nullptr;
          {
            ScopedModImpactTimer chat_manager_timer(ModImpactProbe::HotkeyUiChatManagerLookup,
                                                    ModImpactMonitorEnabled());
            chat_manager = ChatManager::Instance();
          }
          if (chat_manager) {
            chat_open_consumes_original = query::runtime_binding_consumes_original_key_event(
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
            query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kOfficerCanvas))) {
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
        const auto           table_dispatch_winner =
            query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kTableDispatch);
        const auto action = hotkey_router_table_dispatch_request(is_in_chat, input_focused, table_dispatch_winner);
        if (action != input_binding::InputActionId::Max) {
          const auto* table_winner =
              query::runtime_binding_winner(runtime_dispatch_plan, action, input_binding::InputLayer::Global);
          if (table_winner
              && query::dispatch_runtime_bound_table_action(*table_winner)
                     == HotkeyRouterDispatchAction::SuppressOriginal) {
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
          is_in_chat, query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kChatChannel))) {
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
        input_focused, query::first_runtime_binding_winner(runtime_dispatch_plan, actions::kSimpleFleet));

    const auto* fleet_primary_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetPrimary, input_binding::InputLayer::Fleet);
    const auto* fleet_secondary_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetSecondary, input_binding::InputLayer::Fleet);
    const auto* fleet_service_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetService, input_binding::InputLayer::Fleet);
    const auto* fleet_queue_add_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetQueueAdd, input_binding::InputLayer::Fleet);
    const auto* fleet_recall_cancel_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetRecallCancel, input_binding::InputLayer::Fleet);
    const auto* fleet_recall_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetRecall, input_binding::InputLayer::Fleet);
    const auto* fleet_repair_winner = query::runtime_binding_winner(
        runtime_dispatch_plan, input_binding::InputActionId::FleetRepair, input_binding::InputLayer::Fleet);

    auto space_action_inputs = hotkey_router_runtime_space_action_inputs(
        fleet_primary_winner != nullptr, fleet_secondary_winner != nullptr, fleet_queue_add_winner != nullptr,
        fleet_recall_cancel_winner != nullptr, fleet_recall_winner != nullptr, fleet_repair_winner != nullptr,
        fleet_service_winner != nullptr);

    if (fleet_primary_winner) {
      space_action_inputs.primary_key = fleet_primary_winner->key;
    }
    if (fleet_secondary_winner) {
      space_action_inputs.secondary_key = fleet_secondary_winner->key;
    }
    if (fleet_queue_add_winner) {
      space_action_inputs.queue_key = fleet_queue_add_winner->key;
    }
    if (fleet_recall_cancel_winner) {
      space_action_inputs.recall_cancel_key = fleet_recall_cancel_winner->key;
    }
    if (fleet_recall_winner) {
      space_action_inputs.recall_key = fleet_recall_winner->key;
    }
    if (fleet_repair_winner) {
      space_action_inputs.repair_key = fleet_repair_winner->key;
    }
    if (fleet_service_winner) {
      space_action_inputs.recall_key = fleet_service_winner->key;
      space_action_inputs.repair_key = fleet_service_winner->key;
    }

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
      query::dispatch_runtime_bound_simple_fleet_action(input_binding::InputActionId::FleetQueueClear);
      if (query::runtime_binding_consumes_original_key_event(
              runtime_dispatch_plan, input_binding::InputActionId::FleetQueueClear, input_binding::InputLayer::Fleet)) {
        return false;
      }
    }

    // Space actions (engage, scan, recall, repair, queue, etc.)
    if (hotkey_router_should_execute_space_action(space_action_inputs, force_space_action_next_frame)) {
      auto handled_space_action = false;
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
          && (query::runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetPrimary, input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetSecondary, input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetService, input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetQueueAdd, input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(runtime_dispatch_plan,
                                                                    input_binding::InputActionId::FleetRecallCancel,
                                                                    input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(
                  runtime_dispatch_plan, input_binding::InputActionId::FleetRecall, input_binding::InputLayer::Fleet)
              || query::runtime_binding_consumes_original_key_event(runtime_dispatch_plan,
                                                                    input_binding::InputActionId::FleetRepair,
                                                                    input_binding::InputLayer::Fleet))) {
        return false;
      }
    }

    // ActionView — toggle cargo/rewards info panel
    if (simple_fleet_action == HotkeyRouterSimpleFleetAction::ViewInfo) {
      HandleActionView();
      if (query::runtime_binding_consumes_original_key_event(
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
