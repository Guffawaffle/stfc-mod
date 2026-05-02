/**
 * @file fleet_actions.cc
 * @brief Ship selection, space actions, and fleet command execution.
 *
 * Implements the core fleet interaction logic: number-key ship selection with
 * double-tap-to-locate, Shift+number tow-to-Discovery, and the contextual
 * space action system that inspects visible object viewers to determine the
 * correct action (engage, scan, mine, warp, join armada, queue, recall, repair).
 */
#include "errormsg.h"
#include "config.h"

#include "patches/fleet_actions.h"
#include "patches/live_debug.h"
#include "patches/viewer_mgmt.h"

#include "prime/ActionQueueManager.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/Hub.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/NavigationSectionManager.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include "patches/key.h"
#include "patches/mapkey.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <span>

// ─── Ship Selection ───────────────────────────────────────────────────────────────────

/** When true, the next frame will re-attempt the primary space action. */
bool force_space_action_next_frame = false;

namespace {
struct DeferredSpaceActionContext {
  uint64_t             fleet_id = 0;
  PreScanTargetWidget* pre_scan_widget = nullptr;
  BattleTargetData*    target_context = nullptr;
};

DeferredSpaceActionContext deferred_space_action_context;
uint64_t                   deferred_space_action_generation = 0;

template <typename T>
bool IsViewerVisible(T* widget)
{
  return widget && widget->_visibilityController
      && (widget->_visibilityController->_state == VisibilityState::Visible
          || widget->_visibilityController->_state == VisibilityState::Show);
}

VisibilityState GetArmadaVisibilityState(ArmadaObjectViewerWidget* armada_widget)
{
  if (!armada_widget) {
    return VisibilityState::Unknown;
  }

  if (armada_widget->_visibilityController) {
    return armada_widget->_visibilityController->State;
  }

  spdlog::warn("ArmadaWidget has no visibility controller, using default Visible state");
  return VisibilityState::Visible;
}

struct SpaceActionDiagnostics {
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
  uint64_t                              fleet_id = 0;
  int                                   fleet_state = -1;
  int                                   previous_state = -1;
  bool                                  physical_primary = false;
  bool                                  deferred_primary_for_fleet = false;
  bool                                  deferred_pending = false;
  bool                                  secondary = false;
  bool                                  queue = false;
  bool                                  queue_clear = false;
  bool                                  recall = false;
  bool                                  repair = false;
  bool                                  recall_cancel = false;
  int                                   visible_pre_scan_count = 0;
  bool                                  mining_visible = false;
  bool                                  star_node_visible = false;
  bool                                  navigation_visible = false;
  const char*                           outcome = "none";
  bool                                  handled = false;

  SpaceActionDiagnostics(FleetPlayerData* fleet, bool has_physical_primary, bool has_deferred_primary_for_fleet,
                         bool has_deferred_pending, bool has_secondary, bool has_queue, bool has_queue_clear,
                         bool has_recall, bool has_repair, bool has_recall_cancel)
      : fleet_id(fleet ? fleet->Id : 0)
      , fleet_state(fleet ? static_cast<int>(fleet->CurrentState) : -1)
      , previous_state(fleet ? static_cast<int>(fleet->PreviousState) : -1)
      , physical_primary(has_physical_primary)
      , deferred_primary_for_fleet(has_deferred_primary_for_fleet)
      , deferred_pending(has_deferred_pending)
      , secondary(has_secondary)
      , queue(has_queue)
      , queue_clear(has_queue_clear)
      , recall(has_recall)
      , repair(has_repair)
      , recall_cancel(has_recall_cancel)
  {
  }

  ~SpaceActionDiagnostics()
  {
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    const auto long_detour = elapsed_us >= 8000;
    const auto had_action_input = physical_primary || deferred_pending || deferred_primary_for_fleet || secondary || queue
        || queue_clear || recall || repair || recall_cancel;
    const auto had_visible_context = visible_pre_scan_count > 0 || mining_visible || star_node_visible || navigation_visible;
    const auto handled_primary = handled && (physical_primary || deferred_primary_for_fleet || deferred_pending);

    if (handled_primary) {
      spdlog::debug(
          "[SpaceActionDiag] handled-primary outcome={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
          "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
          "deferred[fleet={} widget={} target={}]",
          outcome, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
          deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
          visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
          deferred_space_action_context.fleet_id, static_cast<const void*>(deferred_space_action_context.pre_scan_widget),
          static_cast<const void*>(deferred_space_action_context.target_context));
    }

    if (long_detour) {
      spdlog::warn(
          "[SpaceActionDiag] slow outcome={} handled={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
          "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
          "deferred[fleet={} widget={} target={}]",
          outcome, handled, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
          deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
          visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
          deferred_space_action_context.fleet_id, static_cast<const void*>(deferred_space_action_context.pre_scan_widget),
          static_cast<const void*>(deferred_space_action_context.target_context));
    }

    if (!handled && had_action_input && (had_visible_context || deferred_pending)) {
      spdlog::warn(
          "[SpaceActionDiag] unresolved outcome={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
          "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
          "deferred[fleet={} widget={} target={}]",
          outcome, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
          deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
          visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
          deferred_space_action_context.fleet_id, static_cast<const void*>(deferred_space_action_context.pre_scan_widget),
          static_cast<const void*>(deferred_space_action_context.target_context));
    }
  }

  void SetContext(int pre_scan_count, bool has_mining, bool has_star_node, bool has_navigation)
  {
    visible_pre_scan_count = pre_scan_count;
    mining_visible = has_mining;
    star_node_visible = has_star_node;
    navigation_visible = has_navigation;
  }

  void SetOutcome(const char* value)
  {
    outcome = value;
  }

  void Complete(const char* value)
  {
    outcome = value;
    handled = true;
  }
};
}

void ClearDeferredSpaceAction()
{
  force_space_action_next_frame = false;
  deferred_space_action_context = {};
  ++deferred_space_action_generation;
}

uint64_t DeferredSpaceActionGeneration()
{
  return deferred_space_action_generation;
}

namespace {
void ArmDeferredSpaceAction(FleetPlayerData* fleet, PreScanTargetWidget* pre_scan_widget, BattleTargetData* context)
{
  if (!fleet || !pre_scan_widget) {
    ClearDeferredSpaceAction();
    return;
  }

  force_space_action_next_frame = true;
  deferred_space_action_context = {fleet->Id, pre_scan_widget, context};
  ++deferred_space_action_generation;
}

bool DeferredSpaceActionFleetMatches(FleetPlayerData* fleet)
{
  return force_space_action_next_frame && fleet && deferred_space_action_context.fleet_id == fleet->Id;
}

bool DeferredSpaceActionTargetMatches(FleetPlayerData* fleet, PreScanTargetWidget* pre_scan_widget,
                                      BattleTargetData* context)
{
  if (!DeferredSpaceActionFleetMatches(fleet) || deferred_space_action_context.pre_scan_widget != pre_scan_widget) {
    return false;
  }

  return !deferred_space_action_context.target_context || deferred_space_action_context.target_context == context;
}
}

/** Double-tap detection timer for ship selection. */
static std::chrono::time_point<std::chrono::steady_clock> select_clock = std::chrono::steady_clock::now();

/** Last ship key used for double-tap detection. */
static int last_ship_select_request = -1;

// Returns true if ship selection was handled. The caller should still allow
// ScreenManager::Update so the fleet panel can finish opening this frame.
bool HandleShipSelection(int ship_select_request)
{
  if (ship_select_request == -1 || Key::IsInputFocused()) {
    return false;
  }

  // A deferred primary action belongs to the previous target/ship context.
  // Switching ships should not let that retry fire against the next selection.
  ClearDeferredSpaceAction();

  auto config = &Config::Get();

  if (Key::HasShift()) {
    FleetPlayerData* foundDisco = nullptr;
    for (int discoIdx = 0; discoIdx < 10; ++discoIdx) {
      auto fleetPlayerData = FleetsManager::Instance()->GetFleetPlayerData(discoIdx);
      if (fleetPlayerData && fleetPlayerData->Hull && fleetPlayerData->Hull->Id == 1307832955) {
        foundDisco = fleetPlayerData;
        break;
      }
    }

    if (foundDisco) {
      auto towedFleetId = FleetsManager::Instance()->GetFleetPlayerData(ship_select_request)->Id;
      auto plannedCourse =
          DeploymentManger::Instance()->PlanCourse(FleetsManager::Instance()->GetFleetPlayerData(ship_select_request),
                                                   foundDisco->Address, Vector3::zero(), nullptr, nullptr, nullptr);
      while (plannedCourse->MoveNext()) {
        ;
      }
      DeploymentManger::Instance()->SetTowRequest(towedFleetId, foundDisco->Id);
    }
  } else {
    auto fleet_bar  = ObjectFinder<FleetBarViewController>::Get();
    auto can_locate = !config->disable_preview_locate || !CanHideViewers();
    if (fleet_bar) {
      std::chrono::time_point<std::chrono::steady_clock> select_now = std::chrono::steady_clock::now();
      std::chrono::milliseconds                          select_diff =
          std::chrono::duration_cast<std::chrono::milliseconds>(select_now - select_clock);
      spdlog::debug("select_diff was {}ms", select_diff.count());
      if (can_locate && ship_select_request == last_ship_select_request && fleet_bar->IsIndexSelected(ship_select_request)
          && select_diff < std::chrono::milliseconds((int)Config::Get().select_timer)) {
        auto fleet = fleet_bar->_fleetPanelController->fleet;
        if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
          NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
        }
        FleetsManager::Instance()->RequestViewFleet(fleet, true);
      } else {
        fleet_bar->RequestSelect(ship_select_request);
        fleet_bar->ElementAction(ship_select_request);
        fleet_bar->TogglePanel();
      }

      last_ship_select_request = ship_select_request;
      select_clock = select_now;
      return true;  // handled — skip original
    }
  }

  return false;
}

// ─── Fleet Action Helpers ─────────────────────────────────────────────────────────────

#define FleetAction_Format "Fleet {} ({}) #{} - State: {}, previous {} - canAction {}, canState {} - didAction: {}"

/**
 * @brief Generic fleet action executor — checks fleet state and requests an action.
 *
 * @tparam T Unused (originally intended for requirement checking; kept for signature compat).
 * @param actionText Human-readable action name for trace logging.
 * @param actionType The ActionType enum value to request.
 * @param fleet_bar The active FleetBarViewController.
 * @param wantedStates States in which the action is valid.
 * @param helpState If set and fleet enters this state, re-request with AskHelp behavior.
 * @return true if the action was successfully requested.
 */
template <typename T>
inline bool DidExecuteFleetAction(std::string_view actionText, ActionType actionType, FleetBarViewController* fleet_bar,
                                  const std::span<const FleetState> wantedStates,
                                  FleetState                        helpState = FleetState::Unknown)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_bar->_fleetPanelController->fleet;
  auto fleet_state      = fleet->CurrentState;

  auto       fleet_id   = fleet->Id;
  auto       prev_state = fleet->PreviousState;
  auto       canAction  = true; // actionRequired->CheckIsMet();
  FleetState canState   = FleetState::Unknown;
  auto       didAction  = false;

  if (std::find(std::begin(wantedStates), std::end(wantedStates), fleet_state) != std::end(wantedStates)) {
    canState = fleet_state;
  }

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, "[start]");

  if (canState != FleetState::Unknown && canAction) {
    if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
      NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
    }

    didAction = fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::Default);
  }

  if (helpState != FleetState::Unknown && (didAction || helpState == fleet->CurrentState)) {
    didAction = didAction || fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::AskHelp);
  }

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, didAction);

  return didAction;
}

bool DidExecuteRecall(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::IdleInSpace, FleetState::Impulsing, FleetState::Mining,
                                          FleetState::Capturing};

  auto fleet_controller = fleet_bar->_fleetPanelController;

  return DidExecuteFleetAction<RecallRequirement>("Recall", ActionType::Recall, fleet_bar, states);
}

bool DidExecuteRepair(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::Docked, FleetState::Destroyed};

  return DidExecuteFleetAction<CanRepairRequirement>("Repair", ActionType::Repair, fleet_bar, states,
                                                     FleetState::Repairing);
}

// ─── Space Action Execution ───────────────────────────────────────────────────────────

void ExecuteSpaceAction(FleetBarViewController* fleet_bar)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller->fleet;

  auto action_queue = ActionQueueManager::Instance();

  auto has_physical_primary = MapKey::IsDown(GameFunction::ActionPrimary);
  if (has_physical_primary && force_space_action_next_frame) {
    ClearDeferredSpaceAction();
  }

  auto deferred_primary_for_fleet = DeferredSpaceActionFleetMatches(fleet);
  auto has_primary       = has_physical_primary || deferred_primary_for_fleet;
  auto has_repair        = MapKey::IsDown(GameFunction::ActionRepair);
  auto has_recall_cancel = MapKey::IsDown(GameFunction::ActionRecallCancel);
  auto has_secondary     = MapKey::IsDown(GameFunction::ActionSecondary);
  auto has_queue         = MapKey::IsDown(GameFunction::ActionQueue);
  auto has_queue_clear   = MapKey::IsDown(GameFunction::ActionQueueClear);
  auto has_recall =
      MapKey::IsDown(GameFunction::ActionRecall) && (!Config::Get().disable_preview_recall || !CanHideViewers());

  SpaceActionDiagnostics diagnostics(fleet, has_physical_primary, deferred_primary_for_fleet,
                                     force_space_action_next_frame, has_secondary, has_queue, has_queue_clear,
                                     has_recall, has_repair, has_recall_cancel);

  const auto fleet_is_warping =
      fleet->CurrentState == FleetState::WarpCharging || fleet->CurrentState == FleetState::Warping;

  auto visible_pre_scan_target_count = 0;
  auto mining_viewer_visible = false;
  auto star_node_viewer_visible = false;
  auto navigation_interaction_visible = false;

  const auto collect_warp_cancel_context = [&]() {
    for (auto pre_scan_widget : ObjectFinder<PreScanTargetWidget>::GetAll()) {
      if (IsViewerVisible(pre_scan_widget)) {
        ++visible_pre_scan_target_count;
      }
    }

    auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
    mining_viewer_visible = IsViewerVisible(mine_object_viewer_widget);

    auto star_node_object_viewer_widget = ObjectFinder<StarNodeObjectViewerWidget>::Get();
    star_node_viewer_visible = star_node_object_viewer_widget && star_node_object_viewer_widget->Context;

    navigation_interaction_visible = ObjectFinder<NavigationInteractionUIViewController>::Get() != nullptr;
    diagnostics.SetContext(visible_pre_scan_target_count, mining_viewer_visible, star_node_viewer_visible,
                           navigation_interaction_visible);
  };

  if (has_queue_clear) {
    action_queue->ClearQueue(fleet);
    diagnostics.Complete("queue-clear");
  } else if (has_recall_cancel && fleet_is_warping) {
    collect_warp_cancel_context();
    const auto suppress_mouse_warp_cancel =
        Key::Down(KeyCode::Mouse1)
        && (visible_pre_scan_target_count > 0 || mining_viewer_visible || star_node_viewer_visible
            || navigation_interaction_visible);
    if (suppress_mouse_warp_cancel) {
      live_debug_record_space_action_warp_cancel_suppressed(
          fleet_bar, fleet, has_primary, has_secondary, has_queue, has_queue_clear, has_recall, has_repair,
          has_recall_cancel, force_space_action_next_frame, visible_pre_scan_target_count, mining_viewer_visible,
          star_node_viewer_visible, navigation_interaction_visible);
      diagnostics.Complete("warp-cancel-suppressed");
    } else {
      live_debug_record_space_action_warp_cancel(
          fleet_bar, fleet, has_primary, has_secondary, has_queue, has_queue_clear, has_recall, has_repair,
          has_recall_cancel, force_space_action_next_frame, visible_pre_scan_target_count, mining_viewer_visible,
          star_node_viewer_visible, navigation_interaction_visible);
      fleet_controller->CancelButtonClicked();
      diagnostics.Complete("warp-cancel");
    }
  } else {
    auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();
    auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
    auto mine_object_viewer_visible = IsViewerVisible(mine_object_viewer_widget);
    auto armada_widget = ObjectFinder<ArmadaObjectViewerWidget>::Get();
    auto armada_state = VisibilityState::Unknown;
    auto armada_state_loaded = false;
    const auto get_armada_state = [&]() {
      if (!armada_state_loaded) {
        armada_state = GetArmadaVisibilityState(armada_widget);
        armada_state_loaded = true;
      }
      return armada_state;
    };
    auto queue_unlocked = has_queue && action_queue->IsQueueUnlocked();
    diagnostics.SetContext(visible_pre_scan_target_count, mine_object_viewer_visible, star_node_viewer_visible,
                           navigation_interaction_visible);

    for (auto pre_scan_widget : all_pre_scan_widgets) {
      if (IsViewerVisible(pre_scan_widget)) {
        ++visible_pre_scan_target_count;
        diagnostics.SetContext(visible_pre_scan_target_count, mine_object_viewer_visible, star_node_viewer_visible,
                               navigation_interaction_visible);
        auto scan_engage_buttons_widget = pre_scan_widget->_scanEngageButtonsWidget;
        auto context = scan_engage_buttons_widget ? scan_engage_buttons_widget->Context : nullptr;
        auto type = GetHullTypeFromBattleTarget(context);
        auto deferred_primary_for_target = DeferredSpaceActionTargetMatches(fleet, pre_scan_widget, context);
        auto has_primary_for_target = has_physical_primary || deferred_primary_for_target;

        if (!has_physical_primary && force_space_action_next_frame && deferred_primary_for_fleet
            && !deferred_primary_for_target) {
          diagnostics.SetOutcome(deferred_space_action_context.pre_scan_widget == pre_scan_widget
                                     ? "deferred-target-context-mismatch"
                                     : "deferred-target-widget-mismatch");
        }

        if (mine_object_viewer_visible) {
          if (has_secondary && scan_engage_buttons_widget) {
            diagnostics.Complete("scan-prescan-mining-viewer");
            scan_engage_buttons_widget->OnScanButtonClicked();
            return;
          } else if (has_primary_for_target) {
            diagnostics.Complete("mine-prescan-viewer");
            mine_object_viewer_widget->MineClicked();
            return;
          }
        }

        if (queue_unlocked && pre_scan_widget->_addToQueueButtonWidget && scan_engage_buttons_widget) {
          if (type != HullType::ArmadaTarget && (type != HullType::Any || deferred_primary_for_target)) {
            if (pre_scan_widget->_addToQueueButtonWidget->isActiveAndEnabled) {
              auto listener = pre_scan_widget->_addToQueueButtonWidget->SemaphoreListener;
              const auto queue_full = action_queue->IsQueueFull(fleet);
              if (listener && !queue_full) {
                auto button = listener->TheButton;
                if (button) {
                  diagnostics.Complete("queue-add");
                  button->Press();
                  DidHideViewers();
                  return;
                }
                diagnostics.SetOutcome("queue-button-missing");
              } else if (!listener) {
                diagnostics.SetOutcome("queue-listener-missing");
              } else {
                diagnostics.SetOutcome("queue-full");
              }
              return;
            }

            if (type == HullType::Any) {
              ArmDeferredSpaceAction(fleet, pre_scan_widget, context);
              diagnostics.Complete("defer-queue-any-target");
              return;
            }
          }
        }

        if (has_secondary && scan_engage_buttons_widget) {
          diagnostics.Complete("scan-prescan");
          scan_engage_buttons_widget->OnScanButtonClicked();
          return;
        }

        if (has_primary_for_target && scan_engage_buttons_widget && scan_engage_buttons_widget->enabled) {
          auto canActionPrimary = type != HullType::Any;
          const auto current_armada_state = get_armada_state();
          if (type == HullType::ArmadaTarget
              && (current_armada_state == VisibilityState::Visible || current_armada_state == VisibilityState::Show)) {
            canActionPrimary = false;
          } else if (deferred_primary_for_target) {
            canActionPrimary = true;
          }

          if (canActionPrimary) {
            if (type == HullType::ArmadaTarget) {
              if (pre_scan_widget->_armadaAttackButton && pre_scan_widget->_armadaAttackButton->isActiveAndEnabled) {
                auto listener = pre_scan_widget->_armadaAttackButton->SemaphoreListener;
                if (listener) {
                  auto button = listener->TheButton;
                  if (button) {
                    diagnostics.Complete("armada-attack-button");
                    button->Press();
                    return;
                  }
                  diagnostics.SetOutcome("armada-attack-button-missing");
                } else {
                  diagnostics.SetOutcome("armada-attack-listener-missing");
                }
                return;
              }
              diagnostics.Complete("armada-button-clicked");
              scan_engage_buttons_widget->OnArmadaButtonClicked();
            } else {
              diagnostics.Complete("engage-prescan");
              scan_engage_buttons_widget->OnEngageButtonClicked();
            }
            return;
          } else if (type == HullType::Any && has_physical_primary) {
            ArmDeferredSpaceAction(fleet, pre_scan_widget, context);
            diagnostics.Complete("defer-primary-any-target");
            return;
          } else if (type == HullType::ArmadaTarget) {
            diagnostics.SetOutcome("armada-primary-blocked-by-visible-widget");
          }
        } else if ((has_primary_for_target || has_secondary || has_queue) && !scan_engage_buttons_widget) {
          diagnostics.SetOutcome("prescan-scan-engage-missing");
        } else if (has_primary_for_target && scan_engage_buttons_widget && !scan_engage_buttons_widget->enabled) {
          diagnostics.SetOutcome("prescan-scan-engage-disabled");
        }
      }
    }

    if (mine_object_viewer_visible) {
      if (has_secondary) {
        auto scan_engage_buttons_widget = mine_object_viewer_widget->_scanEngageButtonsWidget;
        if (scan_engage_buttons_widget && scan_engage_buttons_widget->Context) {
          diagnostics.Complete("scan-mining-viewer");
          scan_engage_buttons_widget->OnScanButtonClicked();
          return;
        }
        diagnostics.SetOutcome("mining-viewer-scan-context-missing");
      } else if (has_physical_primary) {
        diagnostics.Complete("mine-viewer");
        mine_object_viewer_widget->MineClicked();
        return;
      }
    } else if (auto star_node_object_viewer_widget = ObjectFinder<StarNodeObjectViewerWidget>::Get();
               star_node_object_viewer_widget && star_node_object_viewer_widget->Context) {
      star_node_viewer_visible = true;
      diagnostics.SetContext(visible_pre_scan_target_count, mine_object_viewer_visible, star_node_viewer_visible,
                             navigation_interaction_visible);
      if (has_secondary) {
        star_node_object_viewer_widget->OnViewButtonActivation();
        diagnostics.Complete("view-star-node");
        return;
      } else if (has_physical_primary) {
        star_node_object_viewer_widget->InitiateWarp();
        diagnostics.Complete("warp-star-node");
        return;
      }
    } else if (auto navigation_ui_controller = ObjectFinder<NavigationInteractionUIViewController>::Get();
               navigation_ui_controller && has_physical_primary) {
      navigation_interaction_visible = true;
      diagnostics.SetContext(visible_pre_scan_target_count, mine_object_viewer_visible, star_node_viewer_visible,
                             navigation_interaction_visible);
      const auto current_armada_state = get_armada_state();
      if (armada_widget
          && (current_armada_state == VisibilityState::Visible || current_armada_state == VisibilityState::Show)) {
        auto button = armada_widget->__get__joinContext();
        if (button && button->Interactable) {
          diagnostics.Complete("join-armada");
          armada_widget->ValidateThenJoinArmada();
          return;
        }
        diagnostics.SetOutcome(button ? "join-armada-not-interactable" : "join-armada-button-missing");
      } else {
        diagnostics.Complete("set-course");
        navigation_ui_controller->OnSetCourseButtonClick();
        return;
      }
    } else if (has_recall && DidExecuteRecall(fleet_bar)) {
      diagnostics.Complete("recall");
      return;
    } else if (has_repair && DidExecuteRepair(fleet_bar)) {
      diagnostics.Complete("repair");
      return;
    }
  }
}

// ─── Hull Type Resolution ─────────────────────────────────────────────────────────────

HullType GetHullTypeFromBattleTarget(BattleTargetData* context)
{
  if (!context) {
    return HullType::Any;
  }
  auto deployed_data = context->TargetFleetDeployedData;
  if (!deployed_data) {
    return HullType::Any;
  }
  auto hull_spec = deployed_data->Hull;
  if (!hull_spec) {
    return HullType::Any;
  }
  return hull_spec->Type;
}
