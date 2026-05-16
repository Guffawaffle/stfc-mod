#include "patches/fleet_input_policy.h"

namespace
{
bool is_warping(const FleetInputFleetState state) noexcept
{ return state == FleetInputFleetState::WarpCharging || state == FleetInputFleetState::Warping; }

bool can_recall(const FleetInputFleetState state) noexcept
{
  return state == FleetInputFleetState::IdleInSpace || state == FleetInputFleetState::Impulsing
         || state == FleetInputFleetState::Mining || state == FleetInputFleetState::Capturing;
}

bool can_repair(const FleetInputFleetState state) noexcept
{ return state == FleetInputFleetState::Docked || state == FleetInputFleetState::Destroyed; }

bool has_actionable_target_context(const FleetPrimaryDecisionInput& input) noexcept
{
  return input.visible_prescan_target || input.mining_viewer_visible || input.star_node_visible
         || input.navigation_interaction_visible || input.armada_widget_visible;
}

bool target_can_use_queue(const FleetPrimaryDecisionInput& input) noexcept
{ return input.visible_prescan_target && input.target_hull_type != FleetInputHullType::ArmadaTarget; }

bool target_can_engage(const FleetPrimaryDecisionInput& input) noexcept
{
  return input.visible_prescan_target && input.target_engage_available
         && input.target_hull_type != FleetInputHullType::ArmadaTarget;
}
} // namespace

FleetPrimaryOutcome DecideFleetPrimary(const FleetPrimaryDecisionInput& input) noexcept
{
  if (input.rewards_visible) {
    return FleetPrimaryOutcome::DismissRewards;
  }

  if (is_warping(input.fleet_state) && !input.is_deferred_retry && !has_actionable_target_context(input)) {
    return FleetPrimaryOutcome::CancelWarp;
  }

  if (input.queue_mode_enabled && input.queue_unlocked && target_can_use_queue(input)) {
    if (input.queue_full) {
      return FleetPrimaryOutcome::None;
    }

    if (!input.target_context_resolved && !input.is_deferred_retry) {
      return FleetPrimaryOutcome::DeferUntilTargetResolved;
    }

    if (input.target_context_resolved) {
      return FleetPrimaryOutcome::AddToQueue;
    }
  }

  if (input.mining_viewer_visible) {
    return FleetPrimaryOutcome::Mine;
  }

  if (input.visible_prescan_target && input.target_context_resolved
      && input.target_hull_type == FleetInputHullType::ArmadaTarget && input.armada_attack_available) {
    return FleetPrimaryOutcome::ArmadaAttack;
  }

  if (target_can_engage(input)) {
    return FleetPrimaryOutcome::Engage;
  }

  if (input.navigation_interaction_visible && input.armada_widget_visible && input.armada_join_interactable) {
    return FleetPrimaryOutcome::JoinArmada;
  }

  if (input.navigation_interaction_visible && input.armada_widget_visible) {
    return FleetPrimaryOutcome::ArmadaJoinUnavailable;
  }

  if (input.star_node_visible) {
    return FleetPrimaryOutcome::WarpToNode;
  }

  if (input.navigation_interaction_visible) {
    return FleetPrimaryOutcome::SetCourse;
  }

  if (input.visible_prescan_target && !input.target_context_resolved && !input.is_deferred_retry) {
    return FleetPrimaryOutcome::DeferUntilTargetResolved;
  }

  return FleetPrimaryOutcome::None;
}

FleetSecondaryOutcome DecideFleetSecondary(const FleetSecondaryDecisionInput& input) noexcept
{
  if (input.visible_prescan_target && input.prescan_scan_available) {
    return FleetSecondaryOutcome::ScanPreScan;
  }

  if (input.mining_viewer_visible && input.mining_scan_available) {
    return FleetSecondaryOutcome::ScanMining;
  }

  if (input.star_node_visible) {
    return FleetSecondaryOutcome::ViewStarNode;
  }

  return FleetSecondaryOutcome::None;
}

FleetServiceOutcome DecideFleetService(const FleetServiceDecisionInput& input) noexcept
{
  if (input.recall_allowed && can_recall(input.fleet_state)) {
    return FleetServiceOutcome::Recall;
  }

  if (input.repair_allowed && can_repair(input.fleet_state)) {
    return FleetServiceOutcome::Repair;
  }

  return FleetServiceOutcome::None;
}

std::string_view FleetPrimaryOutcomeName(const FleetPrimaryOutcome outcome) noexcept
{
  switch (outcome) {
    case FleetPrimaryOutcome::None:
      return "none";
    case FleetPrimaryOutcome::DismissRewards:
      return "dismiss-rewards";
    case FleetPrimaryOutcome::CancelWarp:
      return "cancel-warp";
    case FleetPrimaryOutcome::AddToQueue:
      return "add-to-queue";
    case FleetPrimaryOutcome::Mine:
      return "mine";
    case FleetPrimaryOutcome::Engage:
      return "engage";
    case FleetPrimaryOutcome::ArmadaAttack:
      return "armada-attack";
    case FleetPrimaryOutcome::JoinArmada:
      return "join-armada";
    case FleetPrimaryOutcome::ArmadaJoinUnavailable:
      return "join-armada-unavailable";
    case FleetPrimaryOutcome::WarpToNode:
      return "warp-to-node";
    case FleetPrimaryOutcome::SetCourse:
      return "set-course";
    case FleetPrimaryOutcome::DeferUntilTargetResolved:
      return "defer-until-target-resolved";
    default:
      return "unknown";
  }
}

std::string_view FleetSecondaryOutcomeName(const FleetSecondaryOutcome outcome) noexcept
{
  switch (outcome) {
    case FleetSecondaryOutcome::None:
      return "none";
    case FleetSecondaryOutcome::ScanPreScan:
      return "scan-prescan";
    case FleetSecondaryOutcome::ScanMining:
      return "scan-mining";
    case FleetSecondaryOutcome::ViewStarNode:
      return "view-star-node";
    default:
      return "unknown";
  }
}

std::string_view FleetServiceOutcomeName(const FleetServiceOutcome outcome) noexcept
{
  switch (outcome) {
    case FleetServiceOutcome::None:
      return "none";
    case FleetServiceOutcome::Recall:
      return "recall";
    case FleetServiceOutcome::Repair:
      return "repair";
    default:
      return "unknown";
  }
}