#pragma once

#include <string_view>

enum class FleetInputFleetState {
  Unknown      = 0,
  IdleInSpace  = 1,
  Docked       = 2,
  Mining       = 4,
  Destroyed    = 8,
  Repairing    = 32,
  WarpCharging = 128,
  Warping      = 256,
  Impulsing    = 512,
  Capturing    = 1024,
};

enum class FleetInputHullType {
  Any          = -1,
  Destroyer    = 0,
  Survey       = 1,
  Explorer     = 2,
  Battleship   = 3,
  Defense      = 4,
  ArmadaTarget = 5,
};

enum class FleetPrimaryOutcome {
  None = 0,
  DismissRewards,
  CancelWarp,
  AddToQueue,
  Mine,
  Engage,
  ArmadaAttack,
  JoinArmada,
  WarpToNode,
  SetCourse,
  DeferUntilTargetResolved,
};

enum class FleetSecondaryOutcome {
  None = 0,
  ScanPreScan,
  ScanMining,
  ViewStarNode,
};

enum class FleetServiceOutcome {
  None = 0,
  Recall,
  Repair,
};

struct FleetPrimaryDecisionInput {
  FleetInputFleetState fleet_state = FleetInputFleetState::Unknown;
  FleetInputHullType   target_hull_type = FleetInputHullType::Any;

  bool primary_is_mouse = false;
  bool rewards_visible = false;
  bool queue_mode_enabled = false;
  bool queue_unlocked = false;
  bool queue_full = false;
  bool visible_prescan_target = false;
  bool mining_viewer_visible = false;
  bool star_node_visible = false;
  bool navigation_interaction_visible = false;
  bool armada_widget_visible = false;
  bool armada_join_interactable = false;
  bool armada_attack_available = false;
  bool target_context_resolved = false;
  bool is_deferred_retry = false;
};

struct FleetSecondaryDecisionInput {
  bool visible_prescan_target = false;
  bool prescan_scan_available = false;
  bool mining_viewer_visible = false;
  bool mining_scan_available = false;
  bool star_node_visible = false;
};

struct FleetServiceDecisionInput {
  FleetInputFleetState fleet_state = FleetInputFleetState::Unknown;
  bool                 recall_allowed = false;
  bool                 repair_allowed = false;
};

FleetPrimaryOutcome   DecideFleetPrimary(const FleetPrimaryDecisionInput& input) noexcept;
FleetSecondaryOutcome DecideFleetSecondary(const FleetSecondaryDecisionInput& input) noexcept;
FleetServiceOutcome   DecideFleetService(const FleetServiceDecisionInput& input) noexcept;

std::string_view FleetPrimaryOutcomeName(FleetPrimaryOutcome outcome) noexcept;
std::string_view FleetSecondaryOutcomeName(FleetSecondaryOutcome outcome) noexcept;
std::string_view FleetServiceOutcomeName(FleetServiceOutcome outcome) noexcept;