#include "test_pure_common.h"

// ===========================================================================
// fleet_input_policy
// ===========================================================================

TEST_SUITE("fleet_input_policy")
{
  TEST_CASE("primary dismisses rewards before other outcomes")
  {
    FleetPrimaryDecisionInput input;
    input.rewards_visible         = true;
    input.fleet_state             = FleetInputFleetState::Warping;
    input.queue_mode_enabled      = true;
    input.queue_unlocked          = true;
    input.visible_prescan_target  = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::Destroyer;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::DismissRewards);
  }

  TEST_CASE("primary cancels warp unless mouse target context should consume the click")
  {
    FleetPrimaryDecisionInput input;
    input.fleet_state = FleetInputFleetState::WarpCharging;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::CancelWarp);

    input.primary_is_mouse        = true;
    input.visible_prescan_target  = true;
    input.target_engage_available = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::Battleship;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::Engage);
  }

  TEST_CASE("primary queue mode handles queue add defer and full queue")
  {
    FleetPrimaryDecisionInput input;
    input.queue_mode_enabled      = true;
    input.queue_unlocked          = true;
    input.visible_prescan_target  = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::Explorer;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::AddToQueue);

    input.queue_full = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::None);

    input.queue_full              = false;
    input.target_context_resolved = false;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::DeferUntilTargetResolved);

    input.is_deferred_retry = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::None);
  }

  TEST_CASE("primary skips normal queue for armada targets")
  {
    FleetPrimaryDecisionInput input;
    input.queue_mode_enabled      = true;
    input.queue_unlocked          = true;
    input.visible_prescan_target  = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::ArmadaTarget;
    input.armada_attack_available = true;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::ArmadaAttack);
  }

  TEST_CASE("primary resolves normal context outcomes")
  {
    FleetPrimaryDecisionInput input;
    input.mining_viewer_visible = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::Mine);

    input                         = {};
    input.visible_prescan_target  = true;
    input.target_engage_available = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::Survey;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::Engage);

    input                         = {};
    input.visible_prescan_target  = true;
    input.target_context_resolved = true;
    input.target_hull_type        = FleetInputHullType::ArmadaTarget;
    input.armada_attack_available = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::ArmadaAttack);

    input                                = {};
    input.navigation_interaction_visible = true;
    input.armada_widget_visible          = true;
    input.armada_join_interactable       = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::JoinArmada);

    input                                = {};
    input.navigation_interaction_visible = true;
    input.armada_widget_visible          = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::ArmadaJoinUnavailable);

    input                          = {};
    input.armada_widget_visible    = true;
    input.armada_join_interactable = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::None);

    input                   = {};
    input.star_node_visible = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::WarpToNode);

    input                                = {};
    input.navigation_interaction_visible = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::SetCourse);
  }

  TEST_CASE("primary defers unresolved prescan target once")
  {
    FleetPrimaryDecisionInput input;
    input.visible_prescan_target  = true;
    input.target_context_resolved = false;
    input.target_hull_type        = FleetInputHullType::Any;

    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::DeferUntilTargetResolved);

    input.is_deferred_retry = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::None);

    input.target_engage_available = true;
    CHECK(DecideFleetPrimary(input) == FleetPrimaryOutcome::Engage);
  }

  TEST_CASE("secondary scans or views by context priority")
  {
    FleetSecondaryDecisionInput input;
    input.visible_prescan_target = true;
    input.prescan_scan_available = true;
    input.mining_viewer_visible  = true;
    input.mining_scan_available  = true;
    input.star_node_visible      = true;
    CHECK(DecideFleetSecondary(input) == FleetSecondaryOutcome::ScanPreScan);

    input                       = {};
    input.mining_viewer_visible = true;
    input.mining_scan_available = true;
    CHECK(DecideFleetSecondary(input) == FleetSecondaryOutcome::ScanMining);

    input                   = {};
    input.star_node_visible = true;
    CHECK(DecideFleetSecondary(input) == FleetSecondaryOutcome::ViewStarNode);

    input = {};
    CHECK(DecideFleetSecondary(input) == FleetSecondaryOutcome::None);
  }

  TEST_CASE("service recalls away fleets and repairs docked or destroyed fleets")
  {
    for (const auto state : {FleetInputFleetState::IdleInSpace, FleetInputFleetState::Impulsing,
                             FleetInputFleetState::Mining, FleetInputFleetState::Capturing}) {
      FleetServiceDecisionInput input;
      input.fleet_state    = state;
      input.recall_allowed = true;
      input.repair_allowed = true;
      CHECK(DecideFleetService(input) == FleetServiceOutcome::Recall);
    }

    for (const auto state : {FleetInputFleetState::Docked, FleetInputFleetState::Destroyed}) {
      FleetServiceDecisionInput input;
      input.fleet_state    = state;
      input.recall_allowed = true;
      input.repair_allowed = true;
      CHECK(DecideFleetService(input) == FleetServiceOutcome::Repair);
    }

    FleetServiceDecisionInput input;
    input.fleet_state = FleetInputFleetState::IdleInSpace;
    CHECK(DecideFleetService(input) == FleetServiceOutcome::None);
  }

  TEST_CASE("outcome names are stable for diagnostics")
  {
    CHECK(FleetPrimaryOutcomeName(FleetPrimaryOutcome::AddToQueue) == "add-to-queue");
    CHECK(FleetPrimaryOutcomeName(FleetPrimaryOutcome::ArmadaJoinUnavailable) == "join-armada-unavailable");
    CHECK(FleetSecondaryOutcomeName(FleetSecondaryOutcome::ScanMining) == "scan-mining");
    CHECK(FleetServiceOutcomeName(FleetServiceOutcome::Repair) == "repair");
  }
}

TEST_SUITE("fleet_deferred_action")
{
  TEST_CASE("arm stores identities and increments generation")
  {
    fleet_deferred_action::State state;

    fleet_deferred_action::Arm(state, 77, 0x1234, 0x5678);

    CHECK(state.pending);
    CHECK(state.generation == 1);
    CHECK(fleet_deferred_action::MatchesFleet(state, 77));
    CHECK(fleet_deferred_action::MatchesTarget(state, 77, 0x1234, 0x5678));
  }

  TEST_CASE("clear resets pending state and bumps generation")
  {
    fleet_deferred_action::State state;
    fleet_deferred_action::Arm(state, 77, 0x1234, 0x5678);

    fleet_deferred_action::Clear(state);

    CHECK_FALSE(state.pending);
    CHECK(state.generation == 2);
    CHECK_FALSE(fleet_deferred_action::MatchesFleet(state, 77));
  }

  TEST_CASE("arming without fleet or widget clears instead of arming")
  {
    fleet_deferred_action::State state;

    fleet_deferred_action::Arm(state, 0, 0x1234, 0x5678);
    CHECK_FALSE(state.pending);
    CHECK(state.generation == 1);

    fleet_deferred_action::Arm(state, 77, 0, 0x5678);
    CHECK_FALSE(state.pending);
    CHECK(state.generation == 2);
  }

  TEST_CASE("target matching allows unknown target identity but requires matching widget")
  {
    fleet_deferred_action::State state;
    fleet_deferred_action::Arm(state, 77, 0x1234, 0);

    CHECK(fleet_deferred_action::MatchesTarget(state, 77, 0x1234, 0x9999));
    CHECK_FALSE(fleet_deferred_action::MatchesTarget(state, 77, 0x9998, 0x9999));

    fleet_deferred_action::Arm(state, 77, 0x1234, 0x5678);
    CHECK(fleet_deferred_action::MatchesTarget(state, 77, 0x1234, 0x5678));
    CHECK_FALSE(fleet_deferred_action::MatchesTarget(state, 77, 0x1234, 0x9999));
  }
}

// ===========================================================================
// config_schema
// ===========================================================================

TEST_SUITE("hotkey_decisions")
{
  TEST_CASE("Scopely shortcut initialization runs for Scopely mode or fallthrough")
  {
    CHECK_FALSE(should_call_original_initialize_actions(false, false));
    CHECK(should_call_original_initialize_actions(false, true));
    CHECK(should_call_original_initialize_actions(true, false));
    CHECK(should_call_original_initialize_actions(true, true));

    CHECK_FALSE(should_call_original_initialize_actions(ScopelyShortcutPolicy::Off));
    CHECK(should_call_original_initialize_actions(ScopelyShortcutPolicy::Native));
    CHECK(should_call_original_initialize_actions(ScopelyShortcutPolicy::Fallback));
  }

  TEST_CASE("per-frame fallthrough can allow original ScreenManager update")
  {
    CHECK_FALSE(should_call_original_screen_update(false, false));
    CHECK_FALSE(should_call_original_screen_update(false, true));
    CHECK(should_call_original_screen_update(true, false));
    CHECK(should_call_original_screen_update(true, true));

    CHECK_FALSE(should_call_original_screen_update(false, OriginalFramePolicy::Mod));
    CHECK(should_call_original_screen_update(true, OriginalFramePolicy::Mod));
    CHECK_FALSE(should_call_original_screen_update(false, OriginalFramePolicy::FallthroughUnhandled));
    CHECK(should_call_original_screen_update(true, OriginalFramePolicy::FallthroughUnhandled));
    CHECK(should_call_original_screen_update(false, OriginalFramePolicy::FallthroughAll));
    CHECK(should_call_original_screen_update(true, OriginalFramePolicy::FallthroughAll));
  }

  TEST_CASE("legacy fallthrough booleans map to split policy lanes")
  {
    CHECK(resolve_scopely_shortcut_policy(false, false) == ScopelyShortcutPolicy::Off);
    CHECK(resolve_scopely_shortcut_policy(true, false) == ScopelyShortcutPolicy::Native);
    CHECK(resolve_scopely_shortcut_policy(false, true) == ScopelyShortcutPolicy::Fallback);
    CHECK(resolve_scopely_shortcut_policy(true, true) == ScopelyShortcutPolicy::Native);

    CHECK(resolve_original_frame_policy(false) == OriginalFramePolicy::Mod);
    CHECK(resolve_original_frame_policy(true) == OriginalFramePolicy::FallthroughUnhandled);

    CHECK(std::string(scopely_shortcut_policy_name(ScopelyShortcutPolicy::Fallback)) == "fallback");
    CHECK(std::string(original_frame_policy_name(OriginalFramePolicy::FallthroughUnhandled))
          == "fallthrough_unhandled");
  }

  TEST_CASE("Escape exit suppression only blocks Escape-triggered exit outside the double-tap window")
  {
    CHECK_FALSE(should_suppress_escape_exit(false, true, 500, -1));
    CHECK_FALSE(should_suppress_escape_exit(true, false, 500, -1));

    CHECK(should_suppress_escape_exit(true, true, 0, -1));
    CHECK(should_suppress_escape_exit(true, true, 500, -1));
    CHECK(should_suppress_escape_exit(true, true, 500, 750));

    CHECK_FALSE(should_suppress_escape_exit(true, true, 500, 500));
    CHECK_FALSE(should_suppress_escape_exit(true, true, 500, 250));
  }

  TEST_CASE("startup router gates hotkey toggles and Scopely fallthrough")
  {
    CHECK(hotkey_router_startup_action(true, false, false, true) == HotkeyRouterStartupAction::DisableHotkeys);
    CHECK(hotkey_router_startup_action(false, true, false, false) == HotkeyRouterStartupAction::EnableHotkeys);
    CHECK(hotkey_router_startup_action(false, false, true, true) == HotkeyRouterStartupAction::AllowOriginal);
    CHECK(hotkey_router_startup_action(false, false, false, false) == HotkeyRouterStartupAction::SuppressOriginal);
    CHECK(hotkey_router_startup_action(false, false, false, true) == HotkeyRouterStartupAction::Continue);

    CHECK(hotkey_router_startup_action(false, false, ScopelyShortcutPolicy::Native, true)
          == HotkeyRouterStartupAction::AllowOriginal);
    CHECK(hotkey_router_startup_action(false, false, ScopelyShortcutPolicy::Fallback, true)
          == HotkeyRouterStartupAction::Continue);
    CHECK(hotkey_router_startup_action(false, false, ScopelyShortcutPolicy::Fallback, false)
          == HotkeyRouterStartupAction::SuppressOriginal);
  }

  TEST_CASE("ship selection returns the first active fleet hotkey")
  {
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{}) == -1);
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{false, false, true, false, false, false, false, false})
          == 2);
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{true, false, true, false, false, false, false, false})
          == 0);
  }

  TEST_CASE("escape clears focused chat or input without falling through")
  {
    CHECK(hotkey_router_should_clear_input_focus(true, true, false));
    CHECK(hotkey_router_should_clear_input_focus(true, false, true));
    CHECK_FALSE(hotkey_router_should_clear_input_focus(true, false, false));
    CHECK_FALSE(hotkey_router_should_clear_input_focus(false, true, true));
  }

  TEST_CASE("queue toggle is only a gameplay-surface action")
  {
    CHECK(hotkey_router_should_toggle_queue(false, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(true, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(false, true, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(false, false, false));
  }

  TEST_CASE("space action execution is driven by sampled inputs or deferred retry")
  {
    SpaceActionInputs inputs;
    CHECK_FALSE(hotkey_router_should_execute_space_action(inputs, false));

    inputs.secondary = true;
    CHECK(hotkey_router_should_execute_space_action(inputs, false));

    inputs = {};
    CHECK(hotkey_router_should_execute_space_action(inputs, true));
  }

  TEST_CASE("fleet service requests choose one mode without default fallthrough")
  {
    CHECK(fleet_action_request_mode(false, false) == FleetActionRequestMode::None);
    CHECK(fleet_action_request_mode(true, false) == FleetActionRequestMode::Default);
    CHECK(fleet_action_request_mode(false, true) == FleetActionRequestMode::AskHelp);
    CHECK(fleet_action_request_mode(true, true) == FleetActionRequestMode::AskHelp);

    CHECK(std::string_view(fleet_action_request_mode_name(FleetActionRequestMode::None)) == "none");
    CHECK(std::string_view(fleet_action_request_mode_name(FleetActionRequestMode::Default)) == "default");
    CHECK(std::string_view(fleet_action_request_mode_name(FleetActionRequestMode::AskHelp)) == "ask-help");
  }

  TEST_CASE("forced deferred retry only clears when the generation does not advance")
  {
    CHECK(hotkey_router_should_clear_deferred_space_action(true, 7, 7));

    CHECK_FALSE(hotkey_router_should_clear_deferred_space_action(false, 7, 7));
    CHECK_FALSE(hotkey_router_should_clear_deferred_space_action(true, 7, 8));
  }

  TEST_CASE("space action duplicate guard only suppresses same target inside window")
  {
    CHECK(space_action_duplicate_submission_should_suppress(10, 20, 10, 20, 250, 750));

    CHECK_FALSE(space_action_duplicate_submission_should_suppress(0, 20, 10, 20, 250, 750));
    CHECK_FALSE(space_action_duplicate_submission_should_suppress(10, 0, 10, 20, 250, 750));
    CHECK_FALSE(space_action_duplicate_submission_should_suppress(10, 20, 11, 20, 250, 750));
    CHECK_FALSE(space_action_duplicate_submission_should_suppress(10, 20, 10, 21, 250, 750));
    CHECK_FALSE(space_action_duplicate_submission_should_suppress(10, 20, 10, 20, -1, 750));
    CHECK_FALSE(space_action_duplicate_submission_should_suppress(10, 20, 10, 20, 750, 750));
  }

  TEST_CASE("runtime fleet action winners map to compatibility space inputs")
  {
    auto inputs = hotkey_router_runtime_space_action_inputs(false, false, false);
    CHECK_FALSE(inputs.any_requested());

    inputs = hotkey_router_runtime_space_action_inputs(true, false, false);
    CHECK(inputs.primary);
    CHECK(inputs.queue);
    CHECK(inputs.recall_cancel);
    CHECK_FALSE(inputs.secondary);
    CHECK_FALSE(inputs.recall);
    CHECK_FALSE(inputs.repair);

    inputs = hotkey_router_runtime_space_action_inputs(false, true, true);
    CHECK_FALSE(inputs.primary);
    CHECK(inputs.secondary);
    CHECK_FALSE(inputs.queue);
    CHECK_FALSE(inputs.recall_cancel);
    CHECK(inputs.recall);
    CHECK(inputs.repair);
  }

  TEST_CASE("dispatch decisions preserve explicit action fallthrough")
  {
    CHECK(hotkey_router_dispatch_action(false, false, false) == HotkeyRouterDispatchAction::Continue);
    CHECK(hotkey_router_dispatch_action(true, true, false) == HotkeyRouterDispatchAction::SuppressOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, true) == HotkeyRouterDispatchAction::AllowOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, false) == HotkeyRouterDispatchAction::Continue);
    CHECK(hotkey_router_dispatch_action(true, false, true, true) == HotkeyRouterDispatchAction::SuppressOriginal);
  }
}
