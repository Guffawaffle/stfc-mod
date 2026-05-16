/**
 * @file fleet_actions.cc
 * @brief Ship selection, space actions, and fleet command execution.
 *
 * Implements the core fleet interaction logic: number-key ship selection with
 * double-tap-to-locate, Shift+number tow-to-Discovery, and the contextual
 * space action system that inspects visible object viewers to determine the
 * correct action (engage, scan, mine, warp, join armada, queue, recall, repair).
 */
#include "config.h"
#include "errormsg.h"

#include "patches/fleet_actions.h"
#include "patches/fleet_deferred_action.h"
#include "patches/fleet_input_policy.h"
#include "patches/hotkey_router.h"
#include "patches/hotkey_router_trace_log.h"
#include "patches/live_debug.h"
#include "patches/mod_impact_monitor.h"
#include "patches/viewer_mgmt.h"
#include "testable_functions.h"

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
#include <vector>

// ─── Ship Selection ───────────────────────────────────────────────────────────────────

/** When true, the next frame will re-attempt the primary space action. */
bool force_space_action_next_frame = false;

struct FleetActionExecutionResult {
  FleetActionRequestMode request_mode = FleetActionRequestMode::None;
  bool                   did_action   = false;
};

FleetActionExecutionResult TryExecuteRecall(FleetBarViewController* fleet_bar);
FleetActionExecutionResult TryExecuteRepair(FleetBarViewController* fleet_bar);

namespace
{
fleet_deferred_action::State deferred_space_action_state;

struct ScanSubmission {
  uint64_t                              fleet_id        = 0;
  uintptr_t                             target_identity = 0;
  std::chrono::steady_clock::time_point submitted_at{};
};

constexpr auto kDuplicateScanSuppressionWindow = std::chrono::milliseconds(750);

ScanSubmission last_scan_submission;

template <typename T> bool IsViewerVisible(T* widget)
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

FleetInputFleetState ToFleetInputState(const FleetState state)
{
  switch (state) {
    case FleetState::IdleInSpace:
      return FleetInputFleetState::IdleInSpace;
    case FleetState::Docked:
      return FleetInputFleetState::Docked;
    case FleetState::Mining:
      return FleetInputFleetState::Mining;
    case FleetState::Destroyed:
      return FleetInputFleetState::Destroyed;
    case FleetState::Repairing:
      return FleetInputFleetState::Repairing;
    case FleetState::WarpCharging:
      return FleetInputFleetState::WarpCharging;
    case FleetState::Warping:
      return FleetInputFleetState::Warping;
    case FleetState::Impulsing:
      return FleetInputFleetState::Impulsing;
    case FleetState::Capturing:
      return FleetInputFleetState::Capturing;
    default:
      return FleetInputFleetState::Unknown;
  }
}

FleetInputHullType ToFleetInputHullType(const HullType type)
{
  switch (type) {
    case HullType::Destroyer:
      return FleetInputHullType::Destroyer;
    case HullType::Survey:
      return FleetInputHullType::Survey;
    case HullType::Explorer:
      return FleetInputHullType::Explorer;
    case HullType::Battleship:
      return FleetInputHullType::Battleship;
    case HullType::Defense:
      return FleetInputHullType::Defense;
    case HullType::ArmadaTarget:
      return FleetInputHullType::ArmadaTarget;
    default:
      return FleetInputHullType::Any;
  }
}

struct SpaceActionDiagnostics {
  std::chrono::steady_clock::time_point started_at                 = std::chrono::steady_clock::now();
  uint64_t                              fleet_id                   = 0;
  int                                   fleet_state                = -1;
  int                                   previous_state             = -1;
  bool                                  physical_primary           = false;
  bool                                  deferred_primary_for_fleet = false;
  bool                                  deferred_pending           = false;
  bool                                  secondary                  = false;
  bool                                  queue                      = false;
  bool                                  queue_clear                = false;
  bool                                  recall                     = false;
  bool                                  repair                     = false;
  bool                                  recall_cancel              = false;
  int                                   visible_pre_scan_count     = 0;
  bool                                  mining_visible             = false;
  bool                                  star_node_visible          = false;
  bool                                  navigation_visible         = false;
  const char*                           outcome                    = "none";
  bool                                  handled                    = false;

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
    const auto elapsed           = std::chrono::steady_clock::now() - started_at;
    const auto elapsed_us        = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    const auto slow_threshold_us = ModImpactMonitorEnabled() ? 1000 : 8000;
    const auto long_detour       = elapsed_us >= slow_threshold_us;
    const auto had_action_input  = physical_primary || deferred_pending || deferred_primary_for_fleet || secondary
                                   || queue || queue_clear || recall || repair || recall_cancel;
    const auto had_visible_context =
        visible_pre_scan_count > 0 || mining_visible || star_node_visible || navigation_visible;
    const auto handled_primary = handled && (physical_primary || deferred_primary_for_fleet || deferred_pending);

    if (handled_primary) {
      spdlog::debug(
          "[SpaceActionDiag] handled-primary outcome={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
          "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
          "deferred[fleet={} widget={} target={}]",
          outcome, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
          deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
          visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
          deferred_space_action_state.fleet_id,
          reinterpret_cast<const void*>(deferred_space_action_state.widget_identity),
          reinterpret_cast<const void*>(deferred_space_action_state.target_identity));
    }

    if (long_detour) {
      spdlog::warn(
          "[SpaceActionDiag] slow outcome={} handled={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
          "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
          "deferred[fleet={} widget={} target={}]",
          outcome, handled, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
          deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
          visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
          deferred_space_action_state.fleet_id,
          reinterpret_cast<const void*>(deferred_space_action_state.widget_identity),
          reinterpret_cast<const void*>(deferred_space_action_state.target_identity));
    }

    if (!handled && had_action_input && (had_visible_context || deferred_pending)) {
      spdlog::warn("[SpaceActionDiag] unresolved outcome={} duration_us={} fleet={} state={} prev={} inputs[p={} dp={} "
                   "df={} s={} q={} qc={} r={} repair={} rc={}] context[preScan={} mining={} star={} nav={}] "
                   "deferred[fleet={} widget={} target={}]",
                   outcome, elapsed_us, fleet_id, fleet_state, previous_state, physical_primary, deferred_pending,
                   deferred_primary_for_fleet, secondary, queue, queue_clear, recall, repair, recall_cancel,
                   visible_pre_scan_count, mining_visible, star_node_visible, navigation_visible,
                   deferred_space_action_state.fleet_id,
                   reinterpret_cast<const void*>(deferred_space_action_state.widget_identity),
                   reinterpret_cast<const void*>(deferred_space_action_state.target_identity));
    }
  }

  void SetContext(int pre_scan_count, bool has_mining, bool has_star_node, bool has_navigation)
  {
    visible_pre_scan_count = pre_scan_count;
    mining_visible         = has_mining;
    star_node_visible      = has_star_node;
    navigation_visible     = has_navigation;
  }

  void SetOutcome(const char* value)
  { outcome = value; }

  void Complete(const char* value)
  {
    outcome = value;
    handled = true;
  }
};

struct VisiblePreScanTargetContext {
  PreScanTargetWidget*     widget                      = nullptr;
  ScanEngageButtonsWidget* scan_engage_buttons_widget  = nullptr;
  BattleTargetData*        target_context              = nullptr;
  HullType                 target_hull_type            = HullType::Any;
  bool                     deferred_primary_for_target = false;
};

struct SpaceActionRuntimeContext {
  std::vector<VisiblePreScanTargetContext> visible_pre_scan_targets;
  int                                      visible_pre_scan_target_count  = 0;
  MiningObjectViewerWidget*                mining_viewer_widget           = nullptr;
  bool                                     mining_viewer_visible          = false;
  bool                                     mining_scan_available          = false;
  StarNodeObjectViewerWidget*              star_node_viewer_widget        = nullptr;
  bool                                     star_node_visible              = false;
  NavigationInteractionUIViewController*   navigation_ui_controller       = nullptr;
  bool                                     navigation_interaction_visible = false;
  ArmadaObjectViewerWidget*                armada_widget                  = nullptr;
  bool                                     armada_visible                 = false;
};

bool TryExecuteQueueAdd(PreScanTargetWidget* pre_scan_widget, SpaceActionDiagnostics& diagnostics)
{
  if (!pre_scan_widget || !pre_scan_widget->_addToQueueButtonWidget) {
    return false;
  }

  if (!pre_scan_widget->_addToQueueButtonWidget->isActiveAndEnabled) {
    return false;
  }

  auto listener = pre_scan_widget->_addToQueueButtonWidget->SemaphoreListener;
  if (!listener) {
    diagnostics.SetOutcome("queue-listener-missing");
    return true;
  }

  auto button = listener->TheButton;
  if (!button) {
    diagnostics.SetOutcome("queue-button-missing");
    return true;
  }

  diagnostics.Complete("queue-add");
  button->Press();
  DidHideViewers();
  return true;
}

uintptr_t scan_target_identity(ScanEngageButtonsWidget* scan_engage_buttons_widget, BattleTargetData* context)
{
  if (context) {
    return reinterpret_cast<uintptr_t>(context);
  }

  if (scan_engage_buttons_widget && scan_engage_buttons_widget->Context) {
    return reinterpret_cast<uintptr_t>(scan_engage_buttons_widget->Context);
  }

  return reinterpret_cast<uintptr_t>(scan_engage_buttons_widget);
}

bool TryExecuteScanAction(FleetPlayerData* fleet, ScanEngageButtonsWidget* scan_engage_buttons_widget,
                          BattleTargetData* context, const char* success_outcome, const char* duplicate_outcome,
                          SpaceActionDiagnostics& diagnostics)
{
  if (!scan_engage_buttons_widget) {
    return false;
  }

  const auto fleet_id        = fleet ? fleet->Id : 0;
  const auto target_identity = scan_target_identity(scan_engage_buttons_widget, context);
  const auto now             = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      last_scan_submission.submitted_at == std::chrono::steady_clock::time_point{}
          ? int64_t{-1}
          : std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scan_submission.submitted_at).count();

  if (space_action_duplicate_submission_should_suppress(last_scan_submission.fleet_id,
                                                        last_scan_submission.target_identity, fleet_id, target_identity,
                                                        elapsed_ms, kDuplicateScanSuppressionWindow.count())) {
    spdlog::trace("[SpaceActionDiag] suppressed duplicate scan outcome={} fleet={} target={} elapsed_ms={}",
                  duplicate_outcome, fleet_id, target_identity, elapsed_ms);
    diagnostics.Complete(duplicate_outcome);
    return true;
  }

  last_scan_submission = {fleet_id, target_identity, now};
  scan_engage_buttons_widget->OnScanButtonClicked();
  diagnostics.Complete(success_outcome);
  return true;
}

bool TryExecuteArmadaAttack(PreScanTargetWidget* pre_scan_widget, ScanEngageButtonsWidget* scan_engage_buttons_widget,
                            SpaceActionDiagnostics& diagnostics)
{
  if (pre_scan_widget && pre_scan_widget->_armadaAttackButton
      && pre_scan_widget->_armadaAttackButton->isActiveAndEnabled) {
    auto listener = pre_scan_widget->_armadaAttackButton->SemaphoreListener;
    if (!listener) {
      diagnostics.SetOutcome("armada-attack-listener-missing");
      return true;
    }

    auto button = listener->TheButton;
    if (!button) {
      diagnostics.SetOutcome("armada-attack-button-missing");
      return true;
    }

    diagnostics.Complete("armada-attack-button");
    button->Press();
    return true;
  }

  if (!scan_engage_buttons_widget) {
    return false;
  }

  diagnostics.Complete("armada-button-clicked");
  scan_engage_buttons_widget->OnArmadaButtonClicked();
  return true;
}

bool DeferredSpaceActionTargetMatches(FleetPlayerData* fleet, PreScanTargetWidget* widget, BattleTargetData* target);

SpaceActionRuntimeContext GatherSpaceActionRuntimeContext(FleetPlayerData* fleet)
{
  SpaceActionRuntimeContext runtime_context;

  const auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAllNonNull();
  runtime_context.visible_pre_scan_targets.reserve(all_pre_scan_widgets.size());
  for (auto pre_scan_widget : all_pre_scan_widgets) {
    if (!IsViewerVisible(pre_scan_widget)) {
      continue;
    }

    ++runtime_context.visible_pre_scan_target_count;

    auto scan_engage_buttons_widget = pre_scan_widget->_scanEngageButtonsWidget;
    auto target_context             = scan_engage_buttons_widget ? scan_engage_buttons_widget->Context : nullptr;
    runtime_context.visible_pre_scan_targets.push_back(
        {pre_scan_widget, scan_engage_buttons_widget, target_context, GetHullTypeFromBattleTarget(target_context),
         DeferredSpaceActionTargetMatches(fleet, pre_scan_widget, target_context)});
  }

  runtime_context.mining_viewer_widget  = ObjectFinder<MiningObjectViewerWidget>::Get();
  runtime_context.mining_viewer_visible = IsViewerVisible(runtime_context.mining_viewer_widget);
  runtime_context.mining_scan_available = runtime_context.mining_viewer_widget
                                          && runtime_context.mining_viewer_widget->_scanEngageButtonsWidget
                                          && runtime_context.mining_viewer_widget->_scanEngageButtonsWidget->Context;

  runtime_context.star_node_viewer_widget = ObjectFinder<StarNodeObjectViewerWidget>::Get();
  runtime_context.star_node_visible =
      runtime_context.star_node_viewer_widget && runtime_context.star_node_viewer_widget->Context;

  runtime_context.navigation_ui_controller = ObjectFinder<NavigationInteractionUIViewController>::Get();
  if (runtime_context.navigation_ui_controller) {
    if (auto navigation_context = runtime_context.navigation_ui_controller->CanvasContext; navigation_context) {
      runtime_context.navigation_interaction_visible =
          navigation_context->ValidNavigationInput || navigation_context->ShowSetCourseArm;
    }
  }

  runtime_context.armada_widget  = ObjectFinder<ArmadaObjectViewerWidget>::Get();
  const auto armada_state        = GetArmadaVisibilityState(runtime_context.armada_widget);
  runtime_context.armada_visible = armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show;

  return runtime_context;
}
} // namespace

void ClearDeferredSpaceAction()
{
  fleet_deferred_action::Clear(deferred_space_action_state);
  force_space_action_next_frame = deferred_space_action_state.pending;
}

uint64_t DeferredSpaceActionGeneration()
{ return deferred_space_action_state.generation; }

namespace
{
void ArmDeferredSpaceAction(FleetPlayerData* fleet, PreScanTargetWidget* pre_scan_widget, BattleTargetData* context)
{
  fleet_deferred_action::Arm(deferred_space_action_state, fleet ? fleet->Id : 0,
                             reinterpret_cast<uintptr_t>(pre_scan_widget), reinterpret_cast<uintptr_t>(context));
  force_space_action_next_frame = deferred_space_action_state.pending;
}

bool DeferredSpaceActionFleetMatches(FleetPlayerData* fleet)
{ return fleet_deferred_action::MatchesFleet(deferred_space_action_state, fleet ? fleet->Id : 0); }

bool DeferredSpaceActionTargetMatches(FleetPlayerData* fleet, PreScanTargetWidget* pre_scan_widget,
                                      BattleTargetData* context)
{
  return fleet_deferred_action::MatchesTarget(deferred_space_action_state, fleet ? fleet->Id : 0,
                                              reinterpret_cast<uintptr_t>(pre_scan_widget),
                                              reinterpret_cast<uintptr_t>(context));
}

bool TryHandlePreScanQueueOutcome(FleetPrimaryOutcome outcome, FleetPlayerData* fleet,
                                  PreScanTargetWidget* pre_scan_widget, BattleTargetData* context, bool queue_full,
                                  SpaceActionDiagnostics& diagnostics)
{
  switch (outcome) {
    case FleetPrimaryOutcome::AddToQueue:
      return TryExecuteQueueAdd(pre_scan_widget, diagnostics);
    case FleetPrimaryOutcome::DeferUntilTargetResolved:
      ArmDeferredSpaceAction(fleet, pre_scan_widget, context);
      diagnostics.Complete("defer-queue-any-target");
      return true;
    case FleetPrimaryOutcome::None:
      if (queue_full) {
        diagnostics.SetOutcome("queue-full");
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool TryHandlePreScanPrimaryOutcome(FleetPrimaryOutcome outcome, FleetPlayerData* fleet,
                                    PreScanTargetWidget*     pre_scan_widget,
                                    ScanEngageButtonsWidget* scan_engage_buttons_widget, BattleTargetData* context,
                                    HullType type, bool armada_visible, SpaceActionDiagnostics& diagnostics)
{
  switch (outcome) {
    case FleetPrimaryOutcome::ArmadaAttack:
      return TryExecuteArmadaAttack(pre_scan_widget, scan_engage_buttons_widget, diagnostics);
    case FleetPrimaryOutcome::Engage:
      diagnostics.Complete("engage-prescan");
      scan_engage_buttons_widget->OnEngageButtonClicked();
      return true;
    case FleetPrimaryOutcome::DeferUntilTargetResolved:
      ArmDeferredSpaceAction(fleet, pre_scan_widget, context);
      diagnostics.Complete("defer-primary-any-target");
      return true;
    case FleetPrimaryOutcome::None:
      if (type == HullType::ArmadaTarget && armada_visible) {
        diagnostics.SetOutcome("armada-primary-blocked-by-visible-widget");
        return true;
      }
      return false;
    default:
      return false;
  }
}

void RecordPreScanWidgetReadinessOutcome(bool has_primary_for_target, bool has_secondary, bool has_queue,
                                         ScanEngageButtonsWidget* scan_engage_buttons_widget,
                                         SpaceActionDiagnostics&  diagnostics)
{
  if ((has_primary_for_target || has_secondary || has_queue) && !scan_engage_buttons_widget) {
    diagnostics.SetOutcome("prescan-scan-engage-missing");
    return;
  }

  if (has_primary_for_target && scan_engage_buttons_widget && !scan_engage_buttons_widget->enabled) {
    diagnostics.SetOutcome("prescan-scan-engage-disabled");
  }
}

bool TryExecuteWarpCancel(FleetBarViewController* fleet_bar, FleetPlayerData* fleet,
                          const SpaceActionRuntimeContext& runtime_context, bool has_primary, bool has_secondary,
                          bool has_queue, bool has_queue_clear, bool has_recall, bool has_repair,
                          bool has_recall_cancel, bool suppress_warp_cancel, SpaceActionDiagnostics& diagnostics)
{
  if (suppress_warp_cancel) {
    live_debug_record_space_action_warp_cancel_suppressed(
        fleet_bar, fleet, has_primary, has_secondary, has_queue, has_queue_clear, has_recall, has_repair,
        has_recall_cancel, force_space_action_next_frame, runtime_context.visible_pre_scan_target_count,
        runtime_context.mining_viewer_visible, runtime_context.star_node_visible,
        runtime_context.navigation_interaction_visible);
    diagnostics.SetOutcome("warp-cancel-suppressed");
    return false;
  }

  live_debug_record_space_action_warp_cancel(fleet_bar, fleet, has_primary, has_secondary, has_queue, has_queue_clear,
                                             has_recall, has_repair, has_recall_cancel, force_space_action_next_frame,
                                             runtime_context.visible_pre_scan_target_count,
                                             runtime_context.mining_viewer_visible, runtime_context.star_node_visible,
                                             runtime_context.navigation_interaction_visible);
  fleet_bar->_fleetPanelController->CancelButtonClicked();
  diagnostics.Complete("warp-cancel");
  return true;
}

bool ShouldPreferContextActionOverWarpCancel(const SpaceActionRuntimeContext& runtime_context, bool has_primary,
                                             bool has_secondary, bool has_queue)
{
  const auto has_visible_pre_scan_target = runtime_context.visible_pre_scan_target_count > 0;
  const auto has_primary_context         = has_visible_pre_scan_target || runtime_context.mining_viewer_visible
                                           || runtime_context.star_node_visible
                                           || runtime_context.navigation_interaction_visible;
  const auto has_secondary_context =
      has_visible_pre_scan_target || runtime_context.mining_viewer_visible || runtime_context.star_node_visible;
  const auto has_queue_context = has_visible_pre_scan_target;

  return (has_primary && has_primary_context) || (has_secondary && has_secondary_context)
         || (has_queue && has_queue_context);
}

bool TryHandleNoPreScanSecondaryOutcome(FleetSecondaryOutcome outcome, FleetPlayerData* fleet,
                                        const SpaceActionRuntimeContext& runtime_context,
                                        SpaceActionDiagnostics&          diagnostics)
{
  switch (outcome) {
    case FleetSecondaryOutcome::ScanMining:
      return TryExecuteScanAction(fleet, runtime_context.mining_viewer_widget->_scanEngageButtonsWidget, nullptr,
                                  "scan-mining-viewer", "scan-mining-viewer-suppressed-duplicate", diagnostics);
    case FleetSecondaryOutcome::ViewStarNode:
      runtime_context.star_node_viewer_widget->OnViewButtonActivation();
      diagnostics.Complete("view-star-node");
      return true;
    case FleetSecondaryOutcome::None:
      return false;
    default:
      return false;
  }
}

bool TryHandleNoPreScanPrimaryOutcome(FleetPrimaryOutcome outcome, const SpaceActionRuntimeContext& runtime_context,
                                      bool armada_join_button_present, SpaceActionDiagnostics& diagnostics)
{
  switch (outcome) {
    case FleetPrimaryOutcome::Mine:
      diagnostics.Complete("mine-viewer");
      runtime_context.mining_viewer_widget->MineClicked();
      return true;
    case FleetPrimaryOutcome::JoinArmada:
      diagnostics.Complete("join-armada");
      runtime_context.armada_widget->ValidateThenJoinArmada();
      return true;
    case FleetPrimaryOutcome::ArmadaJoinUnavailable:
      diagnostics.SetOutcome(armada_join_button_present ? "join-armada-not-interactable"
                                                        : "join-armada-button-missing");
      return false;
    case FleetPrimaryOutcome::WarpToNode:
      runtime_context.star_node_viewer_widget->InitiateWarp();
      diagnostics.Complete("warp-star-node");
      return true;
    case FleetPrimaryOutcome::SetCourse:
      if (!runtime_context.navigation_ui_controller) {
        diagnostics.SetOutcome("set-course-controller-missing");
        return false;
      }
      diagnostics.Complete("set-course");
      runtime_context.navigation_ui_controller->OnSetCourseButtonClick();
      return true;
    default:
      return false;
  }
}

bool TryHandleFleetServiceOutcome(FleetServiceOutcome outcome, FleetBarViewController* fleet_bar,
                                  SpaceActionDiagnostics& diagnostics)
{
  switch (outcome) {
    case FleetServiceOutcome::Recall:
      if (const auto result = TryExecuteRecall(fleet_bar); result.did_action) {
        diagnostics.Complete("recall-default");
        return true;
      } else if (result.request_mode == FleetActionRequestMode::Default) {
        diagnostics.SetOutcome("recall-default-not-executed");
      } else {
        diagnostics.SetOutcome("recall-not-eligible");
      }
      return false;
    case FleetServiceOutcome::Repair:
      if (const auto result = TryExecuteRepair(fleet_bar); result.did_action) {
        diagnostics.Complete(result.request_mode == FleetActionRequestMode::AskHelp ? "repair-ask-help"
                                                                                    : "repair-default");
        return true;
      } else if (result.request_mode == FleetActionRequestMode::AskHelp) {
        diagnostics.SetOutcome("repair-ask-help-not-executed");
      } else if (result.request_mode == FleetActionRequestMode::Default) {
        diagnostics.SetOutcome("repair-default-not-executed");
      } else {
        diagnostics.SetOutcome("repair-not-eligible");
      }
      return false;
    case FleetServiceOutcome::None:
      return false;
    default:
      return false;
  }
}
} // namespace

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
    ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyShipTow, ModImpactMonitorEnabled());

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
    FleetBarViewController* fleet_bar  = nullptr;
    bool                    can_locate = false;
    {
      ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyShipFleetBarLookup, ModImpactMonitorEnabled());
      fleet_bar  = ObjectFinder<FleetBarViewController>::Get();
      can_locate = !config->disable_preview_locate || !CanHideViewers();
    }
    if (fleet_bar) {
      std::chrono::time_point<std::chrono::steady_clock> select_now = std::chrono::steady_clock::now();
      std::chrono::milliseconds                          select_diff =
          std::chrono::duration_cast<std::chrono::milliseconds>(select_now - select_clock);
      spdlog::debug("select_diff was {}ms", select_diff.count());
      const bool same_request_as_last   = ship_select_request == last_ship_select_request;
      const bool index_already_selected = fleet_bar->IsIndexSelected(ship_select_request);
      const bool within_select_timer    = select_diff < std::chrono::milliseconds((int)Config::Get().select_timer);
      const FleetSelectAction action =
          DecideFleetSelectAction(can_locate, same_request_as_last, index_already_selected, within_select_timer);
      if (action == FleetSelectAction::Locate) {
        ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyShipLocate, ModImpactMonitorEnabled());

        auto fleet = fleet_bar->_fleetPanelController->fleet;
        {
          ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyShipLocateHideInteraction, ModImpactMonitorEnabled());
          if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
            NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
          }
        }
        {
          ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyShipLocateRequestView, ModImpactMonitorEnabled());
          FleetsManager::Instance()->RequestViewFleet(fleet, true);
        }
      } else {
        ScopedModImpactTimer impact_timer(ModImpactProbe::HotkeyShipSelectPanel, ModImpactMonitorEnabled());
        HotkeyRouterNativeFleetSelectionBypass fleet_selection_bypass;

        constexpr auto plan = FleetSelectOpenBranchPlan();
        static_assert(plan.call_request_select && plan.call_element_action && !plan.call_toggle_panel,
                      "Open-branch plan changed; HandleShipSelection must mirror it.");

        if (plan.call_request_select) {
          ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyShipRequestSelect, ModImpactMonitorEnabled());
          fleet_bar->RequestSelect(ship_select_request);
        }
        if (plan.call_element_action) {
          // ElementAction is the game's own fleet-bar click handler; it both selects
          // the ship and toggles the FleetPanel. A previous explicit fleet_bar->TogglePanel()
          // call here produced a double-toggle that briefly opened then immediately closed
          // the panel. ElementAction alone is sufficient to surface the panel.
          ScopedModImpactTimer sub_timer(ModImpactProbe::HotkeyShipElementAction, ModImpactMonitorEnabled());
          fleet_bar->ElementAction(ship_select_request);
        }
        // plan.call_toggle_panel is intentionally false — see FleetSelectOpenBranchPlan().
      }

      last_ship_select_request = ship_select_request;
      select_clock             = select_now;
      return true; // handled — skip original
    }
  }

  return false;
}

// ─── Fleet Action Helpers ─────────────────────────────────────────────────────────────

#define FleetAction_Format                                                                                             \
  "Fleet {} ({}) #{} - State: {}, previous {} - canAction {}, canState {} - requestMode {} - didAction: {}"

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
inline FleetActionExecutionResult
DidExecuteFleetAction(std::string_view actionText, ActionType actionType, FleetBarViewController* fleet_bar,
                      const std::span<const FleetState> wantedStates, FleetState helpState = FleetState::Unknown)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_bar->_fleetPanelController->fleet;
  auto fleet_state      = fleet->CurrentState;

  auto       fleet_id   = fleet->Id;
  auto       prev_state = fleet->PreviousState;
  auto       canAction  = true; // actionRequired->CheckIsMet();
  FleetState canState   = FleetState::Unknown;
  auto       result     = FleetActionExecutionResult{};

  if (std::find(std::begin(wantedStates), std::end(wantedStates), fleet_state) != std::end(wantedStates)) {
    canState = fleet_state;
  }

  result.request_mode = fleet_action_request_mode(canState != FleetState::Unknown,
                                                  helpState != FleetState::Unknown && helpState == fleet_state);

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, fleet_action_request_mode_name(result.request_mode), "[start]");

  if (canAction) {
    switch (result.request_mode) {
      case FleetActionRequestMode::Default:
        if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
          NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
        }

        result.did_action = fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::Default);
        break;
      case FleetActionRequestMode::AskHelp:
        result.did_action = fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::AskHelp);
        break;
      case FleetActionRequestMode::None:
        break;
    }
  }

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, fleet_action_request_mode_name(result.request_mode), result.did_action);

  return result;
}

FleetActionExecutionResult TryExecuteRecall(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::IdleInSpace, FleetState::Impulsing, FleetState::Mining,
                                          FleetState::Capturing};

  return DidExecuteFleetAction<RecallRequirement>("Recall", ActionType::Recall, fleet_bar, states);
}

FleetActionExecutionResult TryExecuteRepair(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::Docked, FleetState::Destroyed};

  return DidExecuteFleetAction<CanRepairRequirement>("Repair", ActionType::Repair, fleet_bar, states,
                                                     FleetState::Repairing);
}

void ClearFleetActionQueue(FleetBarViewController* fleet_bar)
{
  if (!fleet_bar || !fleet_bar->_fleetPanelController || !fleet_bar->_fleetPanelController->fleet) {
    return;
  }

  if (auto action_queue = ActionQueueManager::Instance(); action_queue) {
    action_queue->ClearQueue(fleet_bar->_fleetPanelController->fleet);
  }
}

// ─── Space Action Execution ───────────────────────────────────────────────────────────

void ExecuteSpaceAction(FleetBarViewController* fleet_bar, const SpaceActionInputs& inputs)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller->fleet;

  auto action_queue = ActionQueueManager::Instance();

  const auto has_dispatched_primary = inputs.primary;
  if (has_dispatched_primary && force_space_action_next_frame) {
    ClearDeferredSpaceAction();
  }

  auto       deferred_primary_for_fleet = DeferredSpaceActionFleetMatches(fleet);
  auto       has_primary                = has_dispatched_primary || deferred_primary_for_fleet;
  const auto has_repair                 = inputs.repair;
  const auto has_recall_cancel          = inputs.recall_cancel;
  const auto has_secondary              = inputs.secondary;
  const auto has_queue                  = inputs.queue;
  const auto has_queue_clear            = inputs.queue_clear;
  const auto has_recall                 = inputs.recall && (!Config::Get().disable_preview_recall || !CanHideViewers());

  SpaceActionDiagnostics diagnostics(fleet, has_dispatched_primary, deferred_primary_for_fleet,
                                     force_space_action_next_frame, has_secondary, has_queue, has_queue_clear,
                                     has_recall, has_repair, has_recall_cancel);

  const auto fleet_is_warping =
      fleet->CurrentState == FleetState::WarpCharging || fleet->CurrentState == FleetState::Warping;

  if (has_queue_clear) {
    action_queue->ClearQueue(fleet);
    diagnostics.Complete("queue-clear");
  } else {
    const auto runtime_context = GatherSpaceActionRuntimeContext(fleet);
    diagnostics.SetContext(runtime_context.visible_pre_scan_target_count, runtime_context.mining_viewer_visible,
                           runtime_context.star_node_visible, runtime_context.navigation_interaction_visible);

    if (has_recall_cancel && fleet_is_warping) {
      if (TryExecuteWarpCancel(
              fleet_bar, fleet, runtime_context, has_primary, has_secondary, has_queue, has_queue_clear, has_recall,
              has_repair, has_recall_cancel,
              ShouldPreferContextActionOverWarpCancel(runtime_context, has_primary, has_secondary, has_queue),
              diagnostics)) {
        return;
      }
    }

    auto queue_unlocked = has_queue && action_queue->IsQueueUnlocked();

    for (const auto& pre_scan_target : runtime_context.visible_pre_scan_targets) {
      auto pre_scan_widget             = pre_scan_target.widget;
      auto scan_engage_buttons_widget  = pre_scan_target.scan_engage_buttons_widget;
      auto context                     = pre_scan_target.target_context;
      auto type                        = pre_scan_target.target_hull_type;
      auto deferred_primary_for_target = pre_scan_target.deferred_primary_for_target;
      auto has_primary_for_target      = has_dispatched_primary || deferred_primary_for_target;
      auto add_to_queue_button         = pre_scan_widget->_addToQueueButtonWidget;

      if (!has_dispatched_primary && force_space_action_next_frame && deferred_primary_for_fleet
          && !deferred_primary_for_target) {
        diagnostics.SetOutcome(deferred_space_action_state.widget_identity
                                       == reinterpret_cast<uintptr_t>(pre_scan_widget)
                                   ? "deferred-target-context-mismatch"
                                   : "deferred-target-widget-mismatch");
      }

      if (runtime_context.mining_viewer_visible) {
        if (has_secondary && scan_engage_buttons_widget) {
          if (TryExecuteScanAction(fleet, scan_engage_buttons_widget, context, "scan-prescan-mining-viewer",
                                   "scan-prescan-mining-viewer-suppressed-duplicate", diagnostics)) {
            return;
          }
        } else if (has_primary_for_target) {
          diagnostics.Complete("mine-prescan-viewer");
          runtime_context.mining_viewer_widget->MineClicked();
          return;
        }
      }

      FleetPrimaryDecisionInput primary_input;
      primary_input.fleet_state             = ToFleetInputState(fleet->CurrentState);
      primary_input.target_hull_type        = ToFleetInputHullType(type);
      primary_input.visible_prescan_target  = true;
      primary_input.armada_attack_available = type == HullType::ArmadaTarget && !runtime_context.armada_visible;
      primary_input.target_engage_available =
          type != HullType::ArmadaTarget && (type != HullType::Any || deferred_primary_for_target);
      primary_input.target_context_resolved = type != HullType::Any;
      primary_input.is_deferred_retry       = deferred_primary_for_target;

      if ((has_primary_for_target || has_secondary || has_queue)
          && hotkey_router_trace_log::hotkey_trace_space_action_probe_enabled()) {
        spdlog::debug(
            "[SpaceActionProbe] prescan-gate inputs[p={} s={} q={}] queue_unlocked={} add_queue_widget={} "
            "add_queue_active={} scan_engage={} scan_engage_enabled={} target_type={} context_resolved={} "
            "engage_available={} deferred_target={}",
            has_primary_for_target, has_secondary, has_queue, queue_unlocked, add_to_queue_button != nullptr,
            add_to_queue_button && add_to_queue_button->isActiveAndEnabled, scan_engage_buttons_widget != nullptr,
            scan_engage_buttons_widget && scan_engage_buttons_widget->enabled, static_cast<int>(type),
            primary_input.target_context_resolved, primary_input.target_engage_available, deferred_primary_for_target);
      }

      if (queue_unlocked && add_to_queue_button && scan_engage_buttons_widget) {
        auto queue_input               = primary_input;
        queue_input.queue_mode_enabled = true;
        queue_input.queue_unlocked     = true;
        queue_input.queue_full         = action_queue->IsQueueFull(fleet);
        const auto queue_outcome       = DecideFleetPrimary(queue_input);

        if (hotkey_router_trace_log::hotkey_trace_space_action_probe_enabled()) {
          spdlog::debug("[SpaceActionProbe] queue-outcome outcome={} queue_full={} queue_widget_active={} "
                        "target_type={} context_resolved={}",
                        FleetPrimaryOutcomeName(queue_outcome), queue_input.queue_full,
                        add_to_queue_button->isActiveAndEnabled, static_cast<int>(type),
                        queue_input.target_context_resolved);
        }

        if (TryHandlePreScanQueueOutcome(queue_outcome, fleet, pre_scan_widget, context, queue_input.queue_full,
                                         diagnostics)) {
          return;
        }
      }

      if (has_secondary && scan_engage_buttons_widget) {
        if (TryExecuteScanAction(fleet, scan_engage_buttons_widget, context, "scan-prescan",
                                 "scan-prescan-suppressed-duplicate", diagnostics)) {
          return;
        }
      }

      if (has_primary_for_target && scan_engage_buttons_widget && scan_engage_buttons_widget->enabled) {
        if (TryHandlePreScanPrimaryOutcome(DecideFleetPrimary(primary_input), fleet, pre_scan_widget,
                                           scan_engage_buttons_widget, context, type, runtime_context.armada_visible,
                                           diagnostics)) {
          return;
        }
      }

      RecordPreScanWidgetReadinessOutcome(has_primary_for_target, has_secondary, has_queue, scan_engage_buttons_widget,
                                          diagnostics);
    }

    if (runtime_context.visible_pre_scan_target_count == 0 && has_secondary) {
      if (TryHandleNoPreScanSecondaryOutcome(
              DecideFleetSecondary({false, false, runtime_context.mining_viewer_visible,
                                    runtime_context.mining_scan_available, runtime_context.star_node_visible}),
              fleet, runtime_context, diagnostics)) {
        return;
      }
    }

    if (runtime_context.visible_pre_scan_target_count == 0 && has_dispatched_primary) {
      auto       armada_join_button         = runtime_context.armada_widget && runtime_context.armada_visible
                                                  ? runtime_context.armada_widget->__get__joinContext()
                                                  : nullptr;
      const auto armada_join_button_present = armada_join_button != nullptr;

      FleetPrimaryDecisionInput primary_input;
      primary_input.fleet_state                    = ToFleetInputState(fleet->CurrentState);
      primary_input.mining_viewer_visible          = runtime_context.mining_viewer_visible;
      primary_input.star_node_visible              = runtime_context.star_node_visible;
      primary_input.navigation_interaction_visible = runtime_context.navigation_interaction_visible;
      primary_input.armada_widget_visible          = runtime_context.armada_widget && runtime_context.armada_visible;
      primary_input.armada_join_interactable       = armada_join_button_present && armada_join_button->Interactable;

      if (TryHandleNoPreScanPrimaryOutcome(DecideFleetPrimary(primary_input), runtime_context,
                                           armada_join_button_present, diagnostics)) {
        return;
      }
    }

    if (TryHandleFleetServiceOutcome(
            DecideFleetService({ToFleetInputState(fleet->CurrentState), has_recall, has_repair}), fleet_bar,
            diagnostics)) {
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
