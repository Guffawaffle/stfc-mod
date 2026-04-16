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
#include <span>

// ─── Ship Selection ───────────────────────────────────────────────────────────────────

/** When true, the next frame will re-attempt the primary space action. */
bool force_space_action_next_frame = false;

/** Double-tap detection timer for ship selection. */
static std::chrono::time_point<std::chrono::steady_clock> select_clock = std::chrono::steady_clock::now();

// Returns true if the hook should return early (skip original)
bool HandleShipSelection(int ship_select_request)
{
  if (ship_select_request == -1 || Key::IsInputFocused()) {
    return false;
  }

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
      if (can_locate && fleet_bar->IsIndexSelected(ship_select_request)
          && select_diff < std::chrono::milliseconds((int)Config::Get().select_timer)) {
        auto fleet = fleet_bar->_fleetPanelController->fleet;
        if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
          NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
        }
        FleetsManager::Instance()->RequestViewFleet(fleet, true);
      } else {
        fleet_bar->RequestSelect(ship_select_request);
      }

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

  auto has_primary       = MapKey::IsDown(GameFunction::ActionPrimary) || force_space_action_next_frame;
  auto has_repair        = MapKey::IsDown(GameFunction::ActionRepair);
  auto has_recall_cancel = MapKey::IsDown(GameFunction::ActionRecallCancel);
  auto has_secondary     = MapKey::IsDown(GameFunction::ActionSecondary);
  auto has_queue         = MapKey::IsDown(GameFunction::ActionQueue);
  auto has_queue_clear   = MapKey::IsDown(GameFunction::ActionQueueClear);
  auto has_recall =
      MapKey::IsDown(GameFunction::ActionRecall) && (!Config::Get().disable_preview_recall || !CanHideViewers());

  if (has_queue_clear) {
    action_queue->ClearQueue(fleet);
  } else if (has_recall_cancel
             && (fleet->CurrentState == FleetState::WarpCharging || fleet->CurrentState == FleetState::Warping)) {
    fleet_controller->CancelButtonClicked();
  } else {
    auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();
    for (auto pre_scan_widget : all_pre_scan_widgets) {
      if (pre_scan_widget
          && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
              || pre_scan_widget->_visibilityController->_state == VisibilityState::Show)) {

        if (auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
            mine_object_viewer_widget
            && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
                || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
          if (has_secondary) {
            return pre_scan_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
          } else if (has_primary) {
            return mine_object_viewer_widget->MineClicked();
          }
        }

        if (has_queue && action_queue->IsQueueUnlocked() && pre_scan_widget->_addToQueueButtonWidget
            && pre_scan_widget->_scanEngageButtonsWidget) {
          auto context = pre_scan_widget->_scanEngageButtonsWidget->Context;
          auto type    = GetHullTypeFromBattleTarget(context);

          if (type != HullType::ArmadaTarget && (type != HullType::Any || force_space_action_next_frame)) {
            if (pre_scan_widget->_addToQueueButtonWidget->isActiveAndEnabled) {
              auto listener = pre_scan_widget->_addToQueueButtonWidget->SemaphoreListener;
              if (listener && !action_queue->IsQueueFull(fleet)) {
                auto button = listener->TheButton;
                if (button) {
                  button->Press();
                  DidHideViewers();
                }
              }
              return;
            }

            if (type == HullType::Any) {
              force_space_action_next_frame = true;
              return;
            }
          }
        }

        if (has_secondary) {
          return pre_scan_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
        }

        if (has_primary && pre_scan_widget->_scanEngageButtonsWidget
            && pre_scan_widget->_scanEngageButtonsWidget->enabled) {
          auto context = pre_scan_widget->_scanEngageButtonsWidget->Context;
          auto type    = GetHullTypeFromBattleTarget(context);

          auto armada_widget = ObjectFinder<ArmadaObjectViewerWidget>::Get();
          auto armada_state  = VisibilityState::Unknown;

          if (armada_widget) {
            if (armada_widget->_visibilityController) {
              armada_state = armada_widget->_visibilityController->State;
            } else {
              spdlog::warn("ArmadaWidget has no visibility controller, using default Visible state");
              armada_state = VisibilityState::Visible;
            }
          }

          auto canActionPrimary = type != HullType::Any;
          if (type == HullType::ArmadaTarget
              && (armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show)) {
            canActionPrimary = false;
          } else if (force_space_action_next_frame) {
            canActionPrimary = true;
          }

          if (canActionPrimary) {
            if (type == HullType::ArmadaTarget) {
              if (pre_scan_widget->_armadaAttackButton && pre_scan_widget->_armadaAttackButton->isActiveAndEnabled) {
                auto listener = pre_scan_widget->_armadaAttackButton->SemaphoreListener;
                if (listener) {
                  auto button = listener->TheButton;
                  if (button) {
                    button->Press();
                  }
                }
                return;
              }
              pre_scan_widget->_scanEngageButtonsWidget->OnArmadaButtonClicked();
            } else {
              pre_scan_widget->_scanEngageButtonsWidget->OnEngageButtonClicked();
            }
            return;
          } else if (type == HullType::Any) {
            force_space_action_next_frame = true;
            return;
          }
        }
      }
    }

    if (auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
        mine_object_viewer_widget
        && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
            || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
      if (has_secondary) {
        if (mine_object_viewer_widget->_scanEngageButtonsWidget->Context) {
          return mine_object_viewer_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
        }
      } else if (has_primary) {
        return mine_object_viewer_widget->MineClicked();
      }
    } else if (auto star_node_object_viewer_widget = ObjectFinder<StarNodeObjectViewerWidget>::Get();
               star_node_object_viewer_widget && star_node_object_viewer_widget->Context) {
      if (has_secondary) {
        star_node_object_viewer_widget->OnViewButtonActivation();
      } else if (has_primary) {
        star_node_object_viewer_widget->InitiateWarp();
      }
    } else if (auto navigation_ui_controller = ObjectFinder<NavigationInteractionUIViewController>::Get();
               navigation_ui_controller && has_primary) {
      auto armada_widget = ObjectFinder<ArmadaObjectViewerWidget>::Get();
      auto armada_state  = VisibilityState::Unknown;

      if (armada_widget) {
        if (armada_widget->_visibilityController) {
          armada_state = armada_widget->_visibilityController->State;
        } else {
          spdlog::warn("ArmadaWidget has no visibility controller, using default Visible state");
          armada_state = VisibilityState::Visible;
        }
      }

      spdlog::info("have armada? {}, State {}", (armada_widget ? "Yes" : "No"), (int)armada_state);
      if (armada_widget && (armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show)) {
        auto button = armada_widget->__get__joinContext();
        if (button && button->Interactable) {
          armada_widget->ValidateThenJoinArmada();
          return;
        }
      } else {
        navigation_ui_controller->OnSetCourseButtonClick();
        return;
      }
    } else if (has_recall && DidExecuteRecall(fleet_bar)) {
      return;
    } else if (has_repair && DidExecuteRepair(fleet_bar)) {
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
