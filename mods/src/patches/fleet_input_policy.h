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
  ArmadaJoinUnavailable,
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
  FleetInputFleetState fleet_state      = FleetInputFleetState::Unknown;
  FleetInputHullType   target_hull_type = FleetInputHullType::Any;

  bool rewards_visible                = false;
  bool queue_mode_enabled             = false;
  bool queue_unlocked                 = false;
  bool queue_full                     = false;
  bool visible_prescan_target         = false;
  bool mining_viewer_visible          = false;
  bool star_node_visible              = false;
  bool navigation_interaction_visible = false;
  bool armada_widget_visible          = false;
  bool armada_join_interactable       = false;
  bool armada_attack_available        = false;
  bool target_engage_available        = false;
  bool target_context_resolved        = false;
  bool is_deferred_retry              = false;
};

struct FleetSecondaryDecisionInput {
  bool visible_prescan_target = false;
  bool prescan_scan_available = false;
  bool mining_viewer_visible  = false;
  bool mining_scan_available  = false;
  bool star_node_visible      = false;
};

struct FleetServiceDecisionInput {
  FleetInputFleetState fleet_state    = FleetInputFleetState::Unknown;
  bool                 recall_allowed = false;
  bool                 repair_allowed = false;
};

FleetPrimaryOutcome   DecideFleetPrimary(const FleetPrimaryDecisionInput& input) noexcept;
FleetSecondaryOutcome DecideFleetSecondary(const FleetSecondaryDecisionInput& input) noexcept;
FleetServiceOutcome   DecideFleetService(const FleetServiceDecisionInput& input) noexcept;

std::string_view FleetPrimaryOutcomeName(FleetPrimaryOutcome outcome) noexcept;
std::string_view FleetSecondaryOutcomeName(FleetSecondaryOutcome outcome) noexcept;
std::string_view FleetServiceOutcomeName(FleetServiceOutcome outcome) noexcept;

// ---------------------------------------------------------------------------
// Ship-selection (numeric hotkey) decision — issue #93.
//
// Pure helpers extracted from `HandleShipSelection` so the open-vs-locate
// branch and the open-branch call sequence can be pinned by unit tests
// without mocking IL2CPP. The imperative hook code in `fleet_actions.cc`
// must reach the same conclusions via the same helpers.
// ---------------------------------------------------------------------------

enum class FleetSelectAction {
  Open,   // RequestSelect + ElementAction (open the FleetPanel)
  Locate, // HideInteraction + RequestViewFleet (re-center on already-open ship)
};

struct FleetSelectOpenPlan {
  bool call_request_select = false;
  bool call_element_action = false;
  bool call_toggle_panel   = false;
};

constexpr FleetSelectAction DecideFleetSelectAction(const bool can_locate, const bool same_request_as_last,
                                                    const bool index_already_selected,
                                                    const bool within_select_timer) noexcept
{
  if (can_locate && same_request_as_last && index_already_selected && within_select_timer) {
    return FleetSelectAction::Locate;
  }
  return FleetSelectAction::Open;
}

// The Open branch must call exactly RequestSelect + ElementAction, and must
// NOT call TogglePanel. ElementAction is the game's own click handler and
// already toggles the panel; an extra TogglePanel produces the double-toggle
// regression that closed the panel immediately after opening it.
constexpr FleetSelectOpenPlan FleetSelectOpenBranchPlan() noexcept
{
  return FleetSelectOpenPlan{
      .call_request_select = true,
      .call_element_action = true,
      .call_toggle_panel   = false,
  };
}