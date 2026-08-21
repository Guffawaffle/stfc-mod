/**
 * @file hotkeys.cc
 * @brief Thin SPUD hook layer for keyboard shortcut processing.
 *
 * This file installs game-method detours that delegate all actual input
 * handling to the hotkey_router (hotkey_router.h / hotkey_router.cc) or
 * adjacent input policy helpers at the correct game seam. Each hook is
 * intentionally minimal — it captures the call, forwards context to the
 * router or policy helper, and invokes the original method when allowed.
 */

#include "config.h"

#include <chrono>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include "patches/hook_registry.h"
#include "patches/hotkey_router.h"

#include "patches/key.h"
#include "prime/FleetBarViewController.h"
#include "prime/KeyCode.h"
#include "testable_functions.h"

#include "prime/ChatMessageListLocalViewController.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanTargetWidget.h"

namespace
{
constexpr bool     kEnableShortcutInitializeHook             = true;
constexpr bool     kEnableShortcutLateUpdateGuardHook        = true;
constexpr bool     kEnableRewardsButtonHook                  = true;
constexpr bool     kEnablePreScanTargetHook                  = true;
constexpr bool     kEnableSectionManagerBackButtonHook       = true;
constexpr bool     kEnableNavigationSetCourseHook            = true;
constexpr bool     kEnableFleetBarSelectionGuardHooks        = true;
// Require two stable samples while keeping the player-visible delay near one second at 60 FPS.
constexpr uint64_t kOrphanedTutorialShortcutGateSampleFrames = 60;

const char* initialize_actions_reason()
{ return scopely_shortcut_policy_name(ScopelyShortcutsPolicy()); }

constexpr HookDescriptor kInitializeActionsHook = {
    "ShortcutsManager.InitializeActions",
    "decide whether Scopely shortcut actions initialize alongside mod hotkeys",
    {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", "InitializeActions"},
    "Scopely shortcuts may be unavailable or may double-handle inputs",
};

constexpr HookDescriptor kShortcutLateUpdateHook = {
    "ShortcutsManager.LateUpdate",
    "skip native shortcut processing on frames consumed by the unified input dispatcher",
    {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", "LateUpdate"},
    "modified shortcut chords may still trigger native shortcut actions",
};

constexpr HookDescriptor kShortcutSelectShipHook = {
    "ShortcutsManager.SelectShip",
    "skip native ship selection when the unified input dispatcher consumed the current chord",
    {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", "SelectShip"},
    "modified shortcut chords may still select ships through native shortcut callbacks",
};

// Broad generated ShortcutsManager.On*Action(InputAction.CallbackContext) guard families are intentionally absent.
// The Shift+Space locate-callback RCA showed this opaque native callback seam is not product-safe; any future callback
// guard must be one-callback-at-a-time and promoted through the native seam ledger first.

constexpr HookDescriptor kRewardsButtonBindHook = {
    "RewardsButtonWidget.OnDidBindContext",
    "track combat reward buttons for hotkey-driven reward collection",
    {"Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget", "OnDidBindContext"},
    "reward collection hotkeys may not find the active reward button",
};

constexpr HookDescriptor kPreScanTargetShowHook = {
    "PreScanTargetWidget.ShowWithFleet",
    "track pre-scan targets for hotkey-driven scan actions",
    {"Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget", "ShowWithFleet"},
    "scan/action hotkeys may miss the currently shown target",
};

constexpr HookDescriptor kFleetBarRequestSelectHook = {
    "FleetBarViewController.RequestSelect(int)",
    "prevent modified number chords from falling through to native bare ship selection",
    {"Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController", "RequestSelect"},
    "modified number shortcuts may also select ships",
};

constexpr HookDescriptor kFleetBarRequestSelectComponentHook = {
    "FleetBarViewController.RequestSelect(Component)",
    "prevent modified number chords from falling through to native component-based ship selection",
    {"Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController", "RequestSelect"},
    "modified number shortcuts may also select ships through the component overload",
};

constexpr HookDescriptor kFleetBarElementActionHook = {
    "FleetBarViewController.ElementAction",
    "prevent modified number chords from falling through to native bare ship element actions",
    {"Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController", "ElementAction"},
    "modified number shortcuts may also execute fleet-bar element actions",
};

constexpr HookDescriptor kSectionManagerBackButtonPressedHook = {
    "SectionManager.BackButtonPressed",
    "suppress Escape-driven exit back-button handling at the game back-button seam",
    {"Assembly-CSharp", "Digit.Client.Sections", "SectionManager", "BackButtonPressed"},
    "disable_escape_exit may fail to stop the game's exit prompt",
};

constexpr HookDescriptor kSectionManagerExitSectionDependency = {
    "SectionManager.InBackButtonExitSection",
    "identify whether the current section should open the exit prompt on back-button press",
    {"Assembly-CSharp", "Digit.Client.Sections", "SectionManager", "InBackButtonExitSection"},
    "disable_escape_exit may suppress non-exit back navigation or fail open",
};

constexpr HookDescriptor kNavigationSetCourseHook = {
    "NavigationInteractionUIViewController.OnSetCourseButtonClick",
    "suppress rapid duplicate set-course submissions for the same navigation target",
    {"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController", "OnSetCourseButtonClick"},
    "rapid repeated location actions may trigger server 429 errors",
};

struct SetCourseSubmission {
  uintptr_t                             target_identity = 0;
  std::chrono::steady_clock::time_point submitted_at{};
};

constexpr auto kDuplicateSetCourseSuppressionWindow = std::chrono::milliseconds(750);
constexpr auto kSetCourseSubmissionSource           = uint64_t{1};

SetCourseSubmission last_set_course_submission;

uintptr_t navigation_target_identity(NavigationInteractionUIViewController* navigation_ui_controller)
{
  if (!navigation_ui_controller) {
    return 0;
  }

  auto context = navigation_ui_controller->CanvasContext;
  if (!context) {
    return reinterpret_cast<uintptr_t>(navigation_ui_controller);
  }

  if (context->Poi) {
    return reinterpret_cast<uintptr_t>(context->Poi);
  }

  if (context->LocationTranslationId > 0) {
    return static_cast<uintptr_t>(context->LocationTranslationId);
  }

  return reinterpret_cast<uintptr_t>(context);
}

bool should_suppress_duplicate_set_course(NavigationInteractionUIViewController* navigation_ui_controller)
{
  const auto target_identity = navigation_target_identity(navigation_ui_controller);
  const auto now             = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      last_set_course_submission.submitted_at == std::chrono::steady_clock::time_point{}
          ? int64_t{-1}
          : std::chrono::duration_cast<std::chrono::milliseconds>(now - last_set_course_submission.submitted_at)
                .count();

  if (space_action_duplicate_submission_should_suppress(
          kSetCourseSubmissionSource, last_set_course_submission.target_identity, kSetCourseSubmissionSource,
          target_identity, elapsed_ms, kDuplicateSetCourseSuppressionWindow.count())) {
    spdlog::debug("[Hotkeys] Suppressed duplicate set-course target={} elapsed_ms={}", target_identity, elapsed_ms);
    return true;
  }

  last_set_course_submission = {target_identity, now};
  return false;
}

bool is_back_button_exit_section(void* section_manager)
{
  static auto section_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  static auto in_back_button_exit_section = section_manager_helper.GetMethod<bool(void*)>("InBackButtonExitSection");
  return section_manager != nullptr && in_back_button_exit_section && in_back_button_exit_section(section_manager);
}

bool should_suppress_escape_exit_back_button(void* section_manager)
{
  if (!Config::Get().disable_escape_exit || !Key::Pressed(KeyCode::Escape)
      || !is_back_button_exit_section(section_manager)) {
    return false;
  }

  static auto last_escape_back_button_press = std::chrono::steady_clock::time_point{};
  const auto  now                           = std::chrono::steady_clock::now();
  auto        elapsed_ms_since_last_press   = int64_t{-1};
  if (last_escape_back_button_press != std::chrono::steady_clock::time_point{}) {
    elapsed_ms_since_last_press =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_escape_back_button_press).count();
  }
  last_escape_back_button_press = now;

  return should_suppress_escape_exit(Config::Get().disable_escape_exit, true, Config::Get().escape_exit_timer,
                                     elapsed_ms_since_last_press);
}

bool inspect_native_fleet_selection(const char* method, const int32_t index, const bool simulated)
{
  const auto suppress = hotkey_router_should_suppress_native_fleet_selection(index);
  if (suppress) {
    spdlog::debug("[Hotkeys] suppressed native fleet selection method={} index={} simulated={}", method, index,
                  simulated);
  }

  return suppress;
}

bool inspect_native_fleet_selection_component(const char* method, void* element, const bool simulated)
{
  const auto suppress_any = hotkey_router_should_suppress_any_native_fleet_selection();
  if (suppress_any) {
    spdlog::debug("[Hotkeys] suppressed native fleet selection method={} element={:p} simulated={}", method, element,
                  simulated);
  }

  return suppress_any;
}

template <typename T> bool read_instance_field(void* instance, FieldInfo* field, T& value)
{
  if (!instance || !field) {
    return false;
  }

  value = *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(instance) + il2cpp_field_get_offset(field));
  return true;
}

template <typename T> bool read_object_field(void* object, const char* field_name, T& value)
{
  if (!object) {
    return false;
  }

  auto* klass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(object));
  auto* field = klass ? il2cpp_class_get_field_from_name(klass, field_name) : nullptr;
  return read_instance_field(object, field, value);
}

struct OrphanedTutorialShortcutRuntimeEvidence {
  OrphanedTutorialShortcutGateState state;
  void*                             mission         = nullptr;
  void*                             step            = nullptr;
  FieldInfo*                        is_active_field = nullptr;
  bool (*get_is_active)()                           = nullptr;
  bool (*get_can_use_shortcuts)()                   = nullptr;
};

OrphanedTutorialShortcutRuntimeEvidence collect_orphaned_tutorial_shortcut_evidence(void* shortcuts_manager)
{
  OrphanedTutorialShortcutRuntimeEvidence evidence;
  auto&                                   state = evidence.state;
  state.repair_enabled                          = RepairOrphanedTutorialShortcutGate();
  state.native_shortcuts_enabled                = ScopelyShortcutsPolicy() != ScopelyShortcutPolicy::Off;

  if (!shortcuts_manager || !state.repair_enabled || !state.native_shortcuts_enabled) {
    return evidence;
  }

  static auto shortcuts_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  static auto* actions_field = il2cpp_class_get_field_from_name(shortcuts_helper.get_cls(), "_actions");
  static auto* show_keybindings_field =
      il2cpp_class_get_field_from_name(shortcuts_helper.get_cls(), "_showKeybindings");
  static auto get_can_use_shortcuts = shortcuts_helper.GetMethod<bool()>("get_CanUseShortcuts");

  static auto hub_helper               = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Hub");
  static auto get_video_player_manager = hub_helper.GetMethod<void*()>("get_VideoPlayerManager");
  static auto get_shop_scene_manager   = hub_helper.GetMethod<void*()>("get_ShopSceneManager");
  static auto get_tutorial_manager     = hub_helper.GetMethod<void*()>("get_TutorialManager");

  static auto video_player_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Videos", "VideoPlayerManager");
  static auto get_is_video_playing = video_player_manager_helper.GetMethod<bool(void*)>("get_IsVideoPlaying");

  static auto tutorial_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Tutorial", "TutorialManager");
  static auto  get_is_tutorial_active   = tutorial_manager_helper.GetMethod<bool()>("get_IsActive");
  static auto  get_is_tutorial_blocking = tutorial_manager_helper.GetMethod<bool()>("get_IsBlockingInput");
  static auto* tutorial_is_active_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_isActive");
  static auto* tutorial_ui_field = il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_loadAndShow");
  static auto* tutorial_mission_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentMission");
  static auto* tutorial_objective_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentTutorialObjective");
  static auto* tutorial_data_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentTutorialData");
  static auto* tutorial_component_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentComponent");
  static auto* tutorial_items_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentObjectiveTutorialItems");
  static auto* tutorial_end_step_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_endMissionTutorialStep");
  static auto* tutorial_next_step_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_nextTutorialStep");
  static auto* tutorial_step_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentTutorialStep");
  static auto* tutorial_step_index_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentTutorialStepIndex");
  static auto* tutorial_next_action_id_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_nextTutorialActionID");
  static auto* tutorial_objective_being_cleared_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentObjectiveBeingCleared");
  static auto* tutorial_target_section_field =
      il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_currentTargetSection");

  static auto message_box_helper         = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "MessageBox");
  static auto get_is_message_box_visible = message_box_helper.GetMethod<bool()>("get_IsMessageBoxVisible");

  static auto  touch_kit_helper  = il2cpp_get_class_helper("TouchKit", "", "TouchKit");
  static auto* block_touch_field = il2cpp_class_get_field_from_name(touch_kit_helper.get_cls(), "BlockTouch");

  static auto ui_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SharedFeatures", "UIManager");
  static auto ui_manager_parent = ui_manager_helper.GetParent("MonoSingleton`1");
  static auto ui_manager_instance  = ui_manager_parent.GetProperty("Instance");
  static auto get_is_popup_visible = ui_manager_helper.GetMethod<bool(void*)>("get_IsPopupVisible");

  static auto shop_scene_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager");
  static auto* plc_offer_popup_field =
      il2cpp_class_get_field_from_name(shop_scene_manager_helper.get_cls(), "_plcOfferPopupLoadAndShow");
  static auto generic_load_and_show_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericLoadAndShowUI");
  static auto is_open = generic_load_and_show_helper.GetMethod<bool(void*)>("IsOpen");

  const auto bindings_complete =
      shortcuts_helper.isValidHelper() && actions_field && show_keybindings_field && get_can_use_shortcuts
      && hub_helper.isValidHelper() && get_video_player_manager && get_shop_scene_manager && get_tutorial_manager
      && video_player_manager_helper.isValidHelper() && get_is_video_playing && tutorial_manager_helper.isValidHelper()
      && get_is_tutorial_active && get_is_tutorial_blocking && tutorial_is_active_field && tutorial_ui_field
      && tutorial_mission_field && tutorial_objective_field && tutorial_data_field && tutorial_component_field
      && tutorial_items_field && tutorial_end_step_field && tutorial_next_step_field && tutorial_step_field
      && tutorial_step_index_field && tutorial_next_action_id_field && tutorial_objective_being_cleared_field
      && tutorial_target_section_field && message_box_helper.isValidHelper() && get_is_message_box_visible
      && touch_kit_helper.isValidHelper() && block_touch_field && ui_manager_helper.isValidHelper()
      && ui_manager_parent.isValidHelper() && ui_manager_instance.isValidHelper() && get_is_popup_visible
      && shop_scene_manager_helper.isValidHelper() && plc_offer_popup_field
      && generic_load_and_show_helper.isValidHelper() && is_open;
  if (!bindings_complete) {
    return evidence;
  }

  evidence.is_active_field       = tutorial_is_active_field;
  evidence.get_is_active         = get_is_tutorial_active;
  evidence.get_can_use_shortcuts = get_can_use_shortcuts;
  state.can_use_shortcuts        = get_can_use_shortcuts();
  state.tutorial_active          = get_is_tutorial_active();
  state.tutorial_blocking        = get_is_tutorial_blocking();
  state.message_box_visible      = get_is_message_box_visible();
  state.input_focused            = Key::IsInputFocused();
  il2cpp_field_static_get_value(block_touch_field, &state.touch_blocked);

  void* actions        = nullptr;
  auto  reads_complete = read_instance_field(shortcuts_manager, actions_field, actions)
                         && read_instance_field(shortcuts_manager, show_keybindings_field, state.show_keybindings);

  static auto input_action_asset_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionAsset");
  static auto input_action_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputAction");
  static auto asset_enabled  = input_action_asset_helper.GetMethod<bool(void*)>("get_enabled");
  static auto find_action    = input_action_asset_helper.GetMethod<void*(void*, Il2CppString*, bool)>("FindAction");
  static auto action_enabled = input_action_helper.GetMethod<bool(void*)>("get_enabled");
  if (!input_action_asset_helper.isValidHelper() || !input_action_helper.isValidHelper() || !asset_enabled
      || !find_action || !action_enabled) {
    return evidence;
  }
  if (actions) {
    state.actions_enabled         = asset_enabled(actions);
    auto* interior_action         = find_action(actions, il2cpp_string_new("General/interior_view"), false);
    auto* galaxy_action           = find_action(actions, il2cpp_string_new("General/galaxy_view"), false);
    state.interior_action_enabled = interior_action && action_enabled(interior_action);
    state.galaxy_action_enabled   = galaxy_action && action_enabled(galaxy_action);
  }

  if (auto* video_player_manager = get_video_player_manager(); video_player_manager) {
    state.video_playing = get_is_video_playing(video_player_manager);
  }
  if (auto* ui_manager = ui_manager_instance.GetRaw<void>(nullptr); ui_manager) {
    state.popup_visible = get_is_popup_visible(ui_manager);
  }
  if (auto* shop_scene_manager = get_shop_scene_manager(); shop_scene_manager) {
    void* plc_offer_popup = nullptr;
    reads_complete = read_instance_field(shop_scene_manager, plc_offer_popup_field, plc_offer_popup) && reads_complete;
    if (plc_offer_popup) {
      state.plc_offer_open = is_open(plc_offer_popup);
    }
  }

  auto* tutorial_manager     = get_tutorial_manager();
  state.has_tutorial_manager = tutorial_manager != nullptr;
  if (!tutorial_manager) {
    return evidence;
  }

  void* tutorial_ui     = nullptr;
  void* objective       = nullptr;
  void* data            = nullptr;
  void* component       = nullptr;
  void* objective_items = nullptr;
  void* end_step        = nullptr;
  void* next_step       = nullptr;
  reads_complete        = read_instance_field(tutorial_manager, tutorial_ui_field, tutorial_ui) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_mission_field, evidence.mission) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_objective_field, objective) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_data_field, data) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_component_field, component) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_items_field, objective_items) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_end_step_field, end_step) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_next_step_field, next_step) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_step_field, evidence.step) && reads_complete;
  reads_complete = read_instance_field(tutorial_manager, tutorial_step_index_field, state.step_index) && reads_complete;
  reads_complete =
      read_instance_field(tutorial_manager, tutorial_next_action_id_field, state.next_action_id) && reads_complete;
  reads_complete =
      read_instance_field(tutorial_manager, tutorial_objective_being_cleared_field, state.objective_being_cleared)
      && reads_complete;
  reads_complete =
      read_instance_field(tutorial_manager, tutorial_target_section_field, state.target_section) && reads_complete;

  state.tutorial_ui_open    = tutorial_ui && is_open(tutorial_ui);
  state.has_mission         = evidence.mission != nullptr;
  state.has_objective       = objective != nullptr;
  state.has_data            = data != nullptr;
  state.has_component       = component != nullptr;
  state.has_objective_items = objective_items != nullptr;
  state.has_end_step        = end_step != nullptr;
  state.has_next_step       = next_step != nullptr;
  state.step_identity       = reinterpret_cast<uintptr_t>(evidence.step);
  reads_complete            = read_object_field(evidence.step, "type_", state.step_type) && reads_complete;
  reads_complete            = read_object_field(evidence.mission, "missionId_", state.mission_id) && reads_complete;
  reads_complete            = read_object_field(evidence.step, "actionId_", state.action_id) && reads_complete;

  state.evidence_complete = reads_complete;
  return evidence;
}

void repair_orphaned_tutorial_shortcut_gate_if_needed(void* shortcuts_manager)
{
  static uint64_t  frame                   = 0;
  static uintptr_t candidate_step_identity = 0;
  static int       consecutive_samples     = 0;

  if (!RepairOrphanedTutorialShortcutGate() || ScopelyShortcutsPolicy() == ScopelyShortcutPolicy::Off) {
    frame                   = 0;
    candidate_step_identity = 0;
    consecutive_samples     = 0;
    return;
  }

  ++frame;
  if (frame != 1 && frame % kOrphanedTutorialShortcutGateSampleFrames != 0) {
    return;
  }

  auto  evidence = collect_orphaned_tutorial_shortcut_evidence(shortcuts_manager);
  auto& state    = evidence.state;
  if (!should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2)) {
    candidate_step_identity = 0;
    consecutive_samples     = 0;
    return;
  }

  if (candidate_step_identity != state.step_identity) {
    candidate_step_identity = state.step_identity;
    consecutive_samples     = 1;
  } else {
    ++consecutive_samples;
  }

  if (!should_repair_orphaned_tutorial_shortcut_gate(state, candidate_step_identity, consecutive_samples)) {
    return;
  }

  bool inactive = false;
  il2cpp_field_static_set_value(evidence.is_active_field, &inactive);
  const auto tutorial_active_after   = evidence.get_is_active();
  const auto can_use_shortcuts_after = evidence.get_can_use_shortcuts();
  if (tutorial_active_after || !can_use_shortcuts_after) {
    bool active = true;
    il2cpp_field_static_set_value(evidence.is_active_field, &active);
    spdlog::error("[Hotkeys] orphaned tutorial shortcut repair rolled back mission_id={} action_id={} "
                  "tutorial_active_after={} can_use_shortcuts_after={}",
                  state.mission_id, state.action_id, tutorial_active_after, can_use_shortcuts_after);
  } else {
    spdlog::warn("[Hotkeys] repaired orphaned tutorial shortcut gate mission_id={} action_id={} "
                 "tutorial_active_after={} can_use_shortcuts_after={}",
                 state.mission_id, state.action_id, tutorial_active_after, can_use_shortcuts_after);
  }

  candidate_step_identity = 0;
  consecutive_samples     = 0;
}

} // namespace

// ─── SPUD Hook Delegates ─────────────────────────────────────────────────────

/**
 * @brief Hook: ShortcutsManager::InitializeActions
 *
 * Intercepts the game's shortcut initialization to let the router
 * register its own action bindings first.
 * Original method: populates the game's default shortcut map.
 * Our modification: router may suppress original initialization.
 */
void InitializeActions_Hook(auto original, void* _this)
{
  const auto requested_policy     = ScopelyShortcutsPolicy();
  const auto should_call_original = should_call_original_initialize_actions(requested_policy);
  spdlog::info("[Hotkeys] ShortcutsManager.InitializeActions original={} reason={} use_scopely_hotkeys={} "
               "allow_key_fallthrough={} scopely_shortcuts={} original_frame_policy={}",
               should_call_original ? "called" : "suppressed", initialize_actions_reason(),
               Config::Get().use_scopely_hotkeys, AllowKeyFallthrough(),
               scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
               original_frame_policy_name(OriginalFramePolicySetting()));

  if (should_call_original) {
    return original(_this);
  }
}

void ShortcutsManager_LateUpdate_Hook(auto original, void* _this)
{
  repair_orphaned_tutorial_shortcut_gate_if_needed(_this);
  hotkey_router_refresh_native_shortcut_suppression();
  const auto suppress = hotkey_router_should_suppress_native_shortcuts();
  if (suppress) {
    if (AdvancedDiagnosticsSettings().hotkey_suppression_logging) {
      spdlog::debug("[Hotkeys] suppressed native shortcut update method=ShortcutsManager.LateUpdate");
    }
    return;
  }

  original(_this);
}

void ShortcutsManager_SelectShip_Hook(auto original, void* _this, int32_t index)
{
  hotkey_router_refresh_native_shortcut_suppression();
  const auto suppress = hotkey_router_should_suppress_native_shortcuts();

  if (suppress) {
    spdlog::debug("[Hotkeys] suppressed native shortcut ship selection method=ShortcutsManager.SelectShip index={}",
                  index);
    return;
  }

  original(_this, index);
}

/**
 * @brief Hook: RewardsButtonWidget::OnDidBindContext
 *
 * Intercepts the combat-rewards UI binding to let the router
 * capture the widget reference for hotkey-driven reward collection.
 * Original method: binds data context to the rewards button.
 * Our modification: notifies the router after binding completes.
 */
void OnDidBindContext_Hook(auto original, RewardsButtonWidget* _this)
{
  original(_this);
  hotkey_router_bind_context(_this);
}

/**
 * @brief Hook: PreScanTargetWidget::ShowWithFleet
 *
 * Intercepts the pre-scan target overlay to let the router
 * capture fleet data for hotkey-driven scan actions.
 * Original method: displays the pre-scan UI for a fleet.
 * Our modification: notifies the router after the widget is shown.
 */
void ShowWithFleet_Hook(auto original, PreScanTargetWidget* _this, void* a1)
{
  original(_this, a1);
  hotkey_router_show_fleet(_this);
}

void FleetBarViewController_RequestSelect_Hook(auto original, FleetBarViewController* _this, int32_t index,
                                               bool simulated)
{
  if (inspect_native_fleet_selection("RequestSelect", index, simulated)) {
    return;
  }

  original(_this, index, simulated);
}

void FleetBarViewController_RequestSelectComponent_Hook(auto original, FleetBarViewController* _this, void* element,
                                                        bool simulated)
{
  if (inspect_native_fleet_selection_component("RequestSelectComponent", element, simulated)) {
    return;
  }

  original(_this, element, simulated);
}

void FleetBarViewController_ElementAction_Hook(auto original, FleetBarViewController* _this, int32_t index)
{
  if (inspect_native_fleet_selection("ElementAction", index, false)) {
    return;
  }

  original(_this, index);
}

void SectionManager_BackButtonPressed_Hook(auto original, void* _this)
{
  if (should_suppress_escape_exit_back_button(_this)) {
    return;
  }

  original(_this);
}

void NavigationInteractionUIViewController_OnSetCourseButtonClick_Hook(auto                                   original,
                                                                       NavigationInteractionUIViewController* _this)
{
  if (should_suppress_duplicate_set_course(_this)) {
    return;
  }

  original(_this);
}

// ─── Manual (Non-SPUD) Hook ─────────────────────────────────────────────────

/**
 * @brief Hook: ChatMessageListLocalViewController::AboutToShow
 *
 * Intercepts chat panel display to auto-focus the input field.
 * This is a manual trampoline hook (not SPUD) because the class
 * requires a different hooking approach.
 * Original method: prepares the chat message list for display.
 * Our modification: sends focus to the input field after show.
 */
void ChatMessageListLocalViewController_AboutToShow_Hook(ChatMessageListLocalViewController* _this);
decltype(ChatMessageListLocalViewController_AboutToShow_Hook)* oChatMessageListLocalViewController_AboutToShow =
    nullptr;
void ChatMessageListLocalViewController_AboutToShow_Hook(ChatMessageListLocalViewController* _this)
{
  oChatMessageListLocalViewController_AboutToShow(_this);
  if (_this->_inputField) {
    _this->_inputField->SendOnFocus();
  }
}

// ─── Hook Installation ──────────────────────────────────────────────────────

/** @brief Resolves IL2CPP class/method pointers and installs all hotkey hooks. */
void InstallHotkeyHooks()
{
  HookModuleHealth hooks("HotkeyHooks");

  spdlog::info("[Hotkeys] startup config installHotkeyHooks={} hotkeys_enabled={} use_scopely_hotkeys={} "
               "allow_key_fallthrough={} scopely_shortcuts={} original_frame_policy={} "
               "repair_orphaned_tutorial_shortcut_gate={} frame_owner=FrameTickHooks initialize_actions_hook={}",
               Config::Get().installHotkeyHooks, Config::Get().hotkeys_enabled, Config::Get().use_scopely_hotkeys,
               AllowKeyFallthrough(), scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
               original_frame_policy_name(OriginalFramePolicySetting()), RepairOrphanedTutorialShortcutGate(),
               kEnableShortcutInitializeHook);

  if (kEnableShortcutInitializeHook) {
    auto shortcuts_manager_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
    if (!shortcuts_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kInitializeActionsHook);
    } else {
      auto ptr_can_user_shortcuts = shortcuts_manager_helper.GetMethod("InitializeActions");
      if (ptr_can_user_shortcuts == nullptr) {
        hooks.record_missing_method(kInitializeActionsHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kInitializeActionsHook, ptr_can_user_shortcuts, InitializeActions_Hook);
      }
    }
  } else {
    hooks.record_skipped(kInitializeActionsHook, "compile-time disabled");
  }

  if (kEnableShortcutLateUpdateGuardHook) {
    auto shortcuts_manager_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
    if (!shortcuts_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kShortcutLateUpdateHook);
    } else {
      auto late_update = shortcuts_manager_helper.GetMethod("LateUpdate");
      if (late_update == nullptr) {
        hooks.record_missing_method(kShortcutLateUpdateHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShortcutLateUpdateHook, late_update, ShortcutsManager_LateUpdate_Hook);
      }
    }
  } else {
    hooks.record_skipped(kShortcutLateUpdateHook, "compile-time disabled");
  }

  {
    auto shortcuts_manager_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
    if (!shortcuts_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kShortcutSelectShipHook);
    } else {
      auto select_ship = shortcuts_manager_helper.GetMethod("SelectShip", 1);
      if (select_ship == nullptr) {
        hooks.record_missing_method(kShortcutSelectShipHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShortcutSelectShipHook, select_ship, ShortcutsManager_SelectShip_Hook);
      }
    }
  }

  if (kEnableRewardsButtonHook) {
    static auto rewards_button_widget =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget");
    if (!rewards_button_widget.isValidHelper()) {
      hooks.record_missing_helper(kRewardsButtonBindHook);
    } else {
      auto on_did_bind_context_ptr = rewards_button_widget.GetMethod("OnDidBindContext");
      if (on_did_bind_context_ptr == nullptr) {
        hooks.record_missing_method(kRewardsButtonBindHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRewardsButtonBindHook, on_did_bind_context_ptr, OnDidBindContext_Hook);
      }
    }
  } else {
    hooks.record_skipped(kRewardsButtonBindHook, "compile-time disabled");
  }

  if (kEnablePreScanTargetHook) {
    static auto pre_scan_target_widget =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget");
    if (!pre_scan_target_widget.isValidHelper()) {
      hooks.record_missing_helper(kPreScanTargetShowHook);
    } else {
      auto show_with_fleet_ptr = pre_scan_target_widget.GetMethod("ShowWithFleet");
      if (show_with_fleet_ptr == nullptr) {
        hooks.record_missing_method(kPreScanTargetShowHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kPreScanTargetShowHook, show_with_fleet_ptr, ShowWithFleet_Hook);
      }
    }
  } else {
    hooks.record_skipped(kPreScanTargetShowHook, "compile-time disabled");
  }

  if (kEnableFleetBarSelectionGuardHooks) {
    auto fleet_bar_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");
    if (!fleet_bar_helper.isValidHelper()) {
      hooks.record_missing_helper(kFleetBarRequestSelectHook);
      hooks.record_missing_helper(kFleetBarRequestSelectComponentHook);
      hooks.record_missing_helper(kFleetBarElementActionHook);
    } else {
      auto request_select =
          fleet_bar_helper.GetMethodSpecial("RequestSelect", [](auto count, const Il2CppType** params) {
            return count == 2 && params[0]->type == IL2CPP_TYPE_I4 && params[1]->type == IL2CPP_TYPE_BOOLEAN;
          });
      if (request_select == nullptr) {
        hooks.record_missing_method(kFleetBarRequestSelectHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kFleetBarRequestSelectHook, request_select,
                                         FleetBarViewController_RequestSelect_Hook);
      }

      auto request_select_component =
          fleet_bar_helper.GetMethodSpecial("RequestSelect", [](auto count, const Il2CppType** params) {
            return count == 2 && params[0]->type != IL2CPP_TYPE_I4 && params[1]->type == IL2CPP_TYPE_BOOLEAN;
          });
      if (request_select_component == nullptr) {
        hooks.record_missing_method(kFleetBarRequestSelectComponentHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kFleetBarRequestSelectComponentHook, request_select_component,
                                         FleetBarViewController_RequestSelectComponent_Hook);
      }

      auto element_action = fleet_bar_helper.GetMethod("ElementAction", 1);
      if (element_action == nullptr) {
        hooks.record_missing_method(kFleetBarElementActionHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kFleetBarElementActionHook, element_action,
                                         FleetBarViewController_ElementAction_Hook);
      }
    }
  } else {
    hooks.record_skipped(kFleetBarRequestSelectHook, "compile-time disabled");
    hooks.record_skipped(kFleetBarRequestSelectComponentHook, "compile-time disabled");
    hooks.record_skipped(kFleetBarElementActionHook, "compile-time disabled");
  }

  if (kEnableSectionManagerBackButtonHook) {
    auto section_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
    if (!section_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kSectionManagerBackButtonPressedHook);
    } else {
      auto back_button_pressed         = section_manager_helper.GetMethod("BackButtonPressed");
      auto in_back_button_exit_section = section_manager_helper.GetMethod("InBackButtonExitSection");
      if (back_button_pressed == nullptr) {
        hooks.record_missing_method(kSectionManagerBackButtonPressedHook);
      } else if (in_back_button_exit_section == nullptr) {
        hooks.record_missing_method(kSectionManagerExitSectionDependency);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSectionManagerBackButtonPressedHook, back_button_pressed,
                                         SectionManager_BackButtonPressed_Hook);
      }
    }
  } else {
    hooks.record_skipped(kSectionManagerBackButtonPressedHook, "compile-time disabled");
  }

  if (kEnableNavigationSetCourseHook) {
    auto navigation_interaction_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController");
    if (!navigation_interaction_helper.isValidHelper()) {
      hooks.record_missing_helper(kNavigationSetCourseHook);
    } else {
      auto set_course_button_click = navigation_interaction_helper.GetMethod("OnSetCourseButtonClick");
      if (set_course_button_click == nullptr) {
        hooks.record_missing_method(kNavigationSetCourseHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kNavigationSetCourseHook, set_course_button_click,
                                         NavigationInteractionUIViewController_OnSetCourseButtonClick_Hook);
      }
    }
  } else {
    hooks.record_skipped(kNavigationSetCourseHook, "compile-time disabled");
  }

  hooks.log_summary();
}
