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
#include <optional>
#include <string>

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
constexpr bool kEnableShortcutInitializeHook             = true;
constexpr bool kEnableShortcutLateUpdateGuardHook        = true;
constexpr bool kEnableGhostTutorialActiveFlagRepairProbe = true;
constexpr bool kEnableRewardsButtonHook                  = true;
constexpr bool kEnablePreScanTargetHook                  = true;
constexpr bool kEnableSectionManagerBackButtonHook       = true;
constexpr bool kEnableNavigationSetCourseHook            = true;
constexpr bool kEnableFleetBarSelectionGuardHooks        = true;

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

constexpr HookDescriptor kShortcutSetSettingsHook = {
    "ShortcutsManager.SetSettings",
    "observe Scopely shortcut activation lifecycle ordering when hotkey diagnostics are enabled",
    {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", "SetSettings"},
    "cold-start shortcut activation failures may remain indistinguishable from mod input suppression",
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

struct NativeShortcutDiagnosticState {
  void*   actions                          = nullptr;
  void*   action_settings                  = nullptr;
  void*   supported_actions                = nullptr;
  void*   interior_action                  = nullptr;
  void*   galaxy_action                    = nullptr;
  bool    actions_enabled                  = false;
  bool    interior_enabled                 = false;
  bool    galaxy_enabled                   = false;
  bool    actions_initialized              = false;
  bool    bindings_dirty                   = false;
  bool    can_use_shortcuts                = false;
  bool    video_playing                    = false;
  bool    tutorial_active                  = false;
  bool    tutorial_blocking                = false;
  bool    tutorial_ui_open                 = false;
  bool    show_keybindings                 = false;
  bool    message_box_visible              = false;
  bool    touch_blocked                    = false;
  bool    input_focused                    = false;
  bool    ui_popup_visible                 = false;
  bool    plc_offer_open                   = false;
  void*   tutorial_manager                 = nullptr;
  void*   tutorial_mission                 = nullptr;
  void*   tutorial_objective               = nullptr;
  void*   tutorial_data                    = nullptr;
  void*   tutorial_component               = nullptr;
  void*   tutorial_items                   = nullptr;
  void*   tutorial_end_step                = nullptr;
  void*   tutorial_next_step               = nullptr;
  void*   tutorial_step                    = nullptr;
  int     tutorial_step_index              = -1;
  int64_t tutorial_next_action_id          = 0;
  int64_t tutorial_objective_being_cleared = 0;
  int64_t tutorial_target_section          = 0;
};

void populate_native_shortcut_gate_state(NativeShortcutDiagnosticState& state, void* shortcuts_manager)
{
  static auto hub_helper               = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Hub");
  static auto get_video_player_manager = hub_helper.GetMethod<void*()>("get_VideoPlayerManager");
  static auto get_shop_scene_manager   = hub_helper.GetMethod<void*()>("get_ShopSceneManager");
  static auto get_tutorial_manager     = hub_helper.GetMethod<void*()>("get_TutorialManager");

  static auto video_player_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Videos", "VideoPlayerManager");
  static auto get_is_video_playing = video_player_manager_helper.GetMethod<bool(void*)>("get_IsVideoPlaying");

  static auto tutorial_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Tutorial", "TutorialManager");
  static auto get_is_tutorial_active     = tutorial_manager_helper.GetMethod<bool()>("get_IsActive");
  static auto get_is_tutorial_blocking   = tutorial_manager_helper.GetMethod<bool()>("get_IsBlockingInput");
  static auto tutorial_ui_offset         = tutorial_manager_helper.GetField("_loadAndShow").offset();
  static auto tutorial_mission_offset    = tutorial_manager_helper.GetField("_currentMission").offset();
  static auto tutorial_objective_offset  = tutorial_manager_helper.GetField("_currentTutorialObjective").offset();
  static auto tutorial_data_offset       = tutorial_manager_helper.GetField("_currentTutorialData").offset();
  static auto tutorial_component_offset  = tutorial_manager_helper.GetField("_currentComponent").offset();
  static auto tutorial_items_offset      = tutorial_manager_helper.GetField("_currentObjectiveTutorialItems").offset();
  static auto tutorial_step_index_offset = tutorial_manager_helper.GetField("_currentTutorialStepIndex").offset();
  static auto tutorial_end_step_offset   = tutorial_manager_helper.GetField("_endMissionTutorialStep").offset();
  static auto tutorial_next_step_offset  = tutorial_manager_helper.GetField("_nextTutorialStep").offset();
  static auto tutorial_step_offset       = tutorial_manager_helper.GetField("_currentTutorialStep").offset();
  static auto tutorial_next_action_id_offset = tutorial_manager_helper.GetField("_nextTutorialActionID").offset();
  static auto tutorial_objective_being_cleared_offset =
      tutorial_manager_helper.GetField("_currentObjectiveBeingCleared").offset();
  static auto tutorial_target_section_offset = tutorial_manager_helper.GetField("_currentTargetSection").offset();

  static auto message_box_helper         = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "MessageBox");
  static auto get_is_message_box_visible = message_box_helper.GetMethod<bool()>("get_IsMessageBoxVisible");

  static auto touch_kit_helper = il2cpp_get_class_helper("TouchKit", "", "TouchKit");
  static auto block_touch      = touch_kit_helper.GetStaticField("BlockTouch");

  static auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  static auto get_is_input_focused  = screen_manager_helper.GetMethod<bool()>("get_IsInputFocused");

  static auto ui_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SharedFeatures", "UIManager");
  static auto ui_manager_parent = ui_manager_helper.GetParent("MonoSingleton`1");
  static auto ui_manager_instance  = ui_manager_parent.GetProperty("Instance");
  static auto get_is_popup_visible = ui_manager_helper.GetMethod<bool(void*)>("get_IsPopupVisible");

  static auto shop_scene_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager");
  static auto plc_offer_popup_offset = shop_scene_manager_helper.GetField("_plcOfferPopupLoadAndShow").offset();
  static auto generic_load_and_show_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericLoadAndShowUI");
  static auto is_open = generic_load_and_show_helper.GetMethod<bool(void*)>("IsOpen");

  static auto show_keybindings_offset =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager")
          .GetField("_showKeybindings")
          .offset();

  if (get_video_player_manager && get_is_video_playing) {
    if (auto* video_player_manager = get_video_player_manager(); video_player_manager) {
      state.video_playing = get_is_video_playing(video_player_manager);
    }
  }
  if (get_is_tutorial_active) {
    state.tutorial_active = get_is_tutorial_active();
  }
  if (get_is_tutorial_blocking) {
    state.tutorial_blocking = get_is_tutorial_blocking();
  }
  if (get_tutorial_manager) {
    state.tutorial_manager = get_tutorial_manager();
    if (state.tutorial_manager) {
      const auto tutorial_base      = reinterpret_cast<uintptr_t>(state.tutorial_manager);
      state.tutorial_mission        = *reinterpret_cast<void**>(tutorial_base + tutorial_mission_offset);
      state.tutorial_objective      = *reinterpret_cast<void**>(tutorial_base + tutorial_objective_offset);
      state.tutorial_data           = *reinterpret_cast<void**>(tutorial_base + tutorial_data_offset);
      state.tutorial_component      = *reinterpret_cast<void**>(tutorial_base + tutorial_component_offset);
      state.tutorial_items          = *reinterpret_cast<void**>(tutorial_base + tutorial_items_offset);
      state.tutorial_step_index     = *reinterpret_cast<int*>(tutorial_base + tutorial_step_index_offset);
      state.tutorial_end_step       = *reinterpret_cast<void**>(tutorial_base + tutorial_end_step_offset);
      state.tutorial_next_step      = *reinterpret_cast<void**>(tutorial_base + tutorial_next_step_offset);
      state.tutorial_step           = *reinterpret_cast<void**>(tutorial_base + tutorial_step_offset);
      state.tutorial_next_action_id = *reinterpret_cast<int64_t*>(tutorial_base + tutorial_next_action_id_offset);
      state.tutorial_objective_being_cleared =
          *reinterpret_cast<int64_t*>(tutorial_base + tutorial_objective_being_cleared_offset);
      state.tutorial_target_section = *reinterpret_cast<int64_t*>(tutorial_base + tutorial_target_section_offset);
      if (is_open) {
        if (auto* tutorial_ui = *reinterpret_cast<void**>(tutorial_base + tutorial_ui_offset); tutorial_ui) {
          state.tutorial_ui_open = is_open(tutorial_ui);
        }
      }
    }
  }
  if (shortcuts_manager) {
    state.show_keybindings =
        *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(shortcuts_manager) + show_keybindings_offset);
  }
  if (get_is_message_box_visible) {
    state.message_box_visible = get_is_message_box_visible();
  }
  state.touch_blocked = block_touch.Get<bool>();
  if (get_is_input_focused) {
    state.input_focused = get_is_input_focused();
  }
  if (get_is_popup_visible) {
    if (auto* ui_manager = ui_manager_instance.GetRaw<void>(nullptr); ui_manager) {
      state.ui_popup_visible = get_is_popup_visible(ui_manager);
    }
  }
  if (get_shop_scene_manager && is_open) {
    if (auto* shop_scene_manager = get_shop_scene_manager(); shop_scene_manager) {
      auto* plc_offer_popup =
          *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(shop_scene_manager) + plc_offer_popup_offset);
      if (plc_offer_popup) {
        state.plc_offer_open = is_open(plc_offer_popup);
      }
    }
  }
}

std::string native_object_type_name(void* value)
{
  if (!value) {
    return "<null>";
  }

  auto* klass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(value));
  if (!klass) {
    return "<unknown>";
  }

  const auto* class_namespace = il2cpp_class_get_namespace(klass);
  const auto* class_name      = il2cpp_class_get_name(klass);
  if (!class_name) {
    return "<unnamed>";
  }
  if (!class_namespace || !*class_namespace) {
    return class_name;
  }
  return std::string(class_namespace) + "." + class_name;
}

void* native_reference_field(void* value, const char* field_name)
{
  if (!value) {
    return nullptr;
  }
  auto* object = reinterpret_cast<Il2CppObject*>(value);
  auto* field  = il2cpp_class_get_field_from_name(il2cpp_object_get_class(object), field_name);
  if (!field) {
    return nullptr;
  }
  return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(value) + il2cpp_field_get_offset(field));
}

template <typename T> std::optional<T> native_value_field(void* value, const char* field_name)
{
  if (!value) {
    return std::nullopt;
  }
  auto* object = reinterpret_cast<Il2CppObject*>(value);
  auto* field  = il2cpp_class_get_field_from_name(il2cpp_object_get_class(object), field_name);
  if (!field) {
    return std::nullopt;
  }
  return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(value) + il2cpp_field_get_offset(field));
}

void log_ghost_tutorial_object(const char* role, void* value)
{
  spdlog::info("[Hotkeys] ghost tutorial object role={} address={:p} type={}", role, value,
               native_object_type_name(value));
}

void log_ghost_tutorial_objects_once(const NativeShortcutDiagnosticState& state)
{
  static void* logged_step = nullptr;
  const auto   stranded    = !state.can_use_shortcuts && state.tutorial_active && !state.tutorial_blocking
                             && !state.tutorial_ui_open && state.tutorial_manager && state.tutorial_mission
                             && state.tutorial_data && !state.tutorial_component && state.tutorial_step;
  if (!stranded || logged_step == state.tutorial_step) {
    return;
  }
  logged_step = state.tutorial_step;

  spdlog::info("[Hotkeys] ghost tutorial snapshot manager={:p} mission={:p} objective={:p} data={:p} items={:p} "
               "component={:p} end_step={:p} next_step={:p} current_step={:p} step_index={} next_action_id={} "
               "objective_being_cleared={} target_section={}",
               state.tutorial_manager, state.tutorial_mission, state.tutorial_objective, state.tutorial_data,
               state.tutorial_items, state.tutorial_component, state.tutorial_end_step, state.tutorial_next_step,
               state.tutorial_step, state.tutorial_step_index, state.tutorial_next_action_id,
               state.tutorial_objective_being_cleared, state.tutorial_target_section);
  log_ghost_tutorial_object("mission", state.tutorial_mission);
  log_ghost_tutorial_object("objective", state.tutorial_objective);
  log_ghost_tutorial_object("data", state.tutorial_data);
  log_ghost_tutorial_object("end-step", state.tutorial_end_step);
  log_ghost_tutorial_object("next-step", state.tutorial_next_step);
  log_ghost_tutorial_object("current-step", state.tutorial_step);

  const auto mission_spec     = native_reference_field(state.tutorial_mission, "_spec");
  const auto active_objective = native_reference_field(state.tutorial_mission, "activeObjective_");
  const auto action_spec      = native_reference_field(state.tutorial_data, "<ActionSpec>k__BackingField");
  const auto step_params      = native_reference_field(state.tutorial_step, "params_");
  spdlog::info("[Hotkeys] ghost tutorial mission fields id={} mission_id={} assigned_fleet_id={} state={} "
               "previous_objective_index={} closed={} spec={:p} spec_type={} active_objective={:p} "
               "active_objective_type={}",
               native_value_field<int64_t>(state.tutorial_mission, "id_").value_or(0),
               native_value_field<int64_t>(state.tutorial_mission, "missionId_").value_or(0),
               native_value_field<int64_t>(state.tutorial_mission, "assignedFleetId_").value_or(0),
               native_value_field<int32_t>(state.tutorial_mission, "state_").value_or(-1),
               native_value_field<int32_t>(state.tutorial_mission, "_previousObjectiveIndex").value_or(-1),
               native_value_field<bool>(state.tutorial_mission, "_isClosed").value_or(false), mission_spec,
               native_object_type_name(mission_spec), active_objective, native_object_type_name(active_objective));
  spdlog::info("[Hotkeys] ghost tutorial data fields action_spec_id={} trans_id={} section_id={} sticky_toast={} "
               "action_spec={:p} action_spec_type={}",
               native_value_field<int64_t>(state.tutorial_data, "actionSpecId_").value_or(0),
               native_value_field<int64_t>(state.tutorial_data, "transId_").value_or(0),
               native_value_field<int64_t>(state.tutorial_data, "sectionId_").value_or(0),
               native_value_field<bool>(state.tutorial_data, "hasStickyToast_").value_or(false), action_spec,
               native_object_type_name(action_spec));
  spdlog::info("[Hotkeys] ghost tutorial action spec fields id={} domain={} action_type={} behaviours={}",
               native_value_field<int64_t>(action_spec, "id_").value_or(0),
               native_value_field<int32_t>(action_spec, "domain_").value_or(-1),
               native_value_field<int32_t>(action_spec, "actionType_").value_or(-1),
               native_value_field<int32_t>(action_spec, "behaviours_").value_or(-1));
  spdlog::info("[Hotkeys] ghost tutorial step fields type={} action_id={} trans_id={} has_background={} "
               "params_case={} params={:p} params_type={}",
               native_value_field<int64_t>(state.tutorial_step, "type_").value_or(0),
               native_value_field<int64_t>(state.tutorial_step, "actionId_").value_or(0),
               native_value_field<int64_t>(state.tutorial_step, "transId_").value_or(0),
               native_value_field<bool>(state.tutorial_step, "hasBackground_").value_or(false),
               native_value_field<int32_t>(state.tutorial_step, "paramsCase_").value_or(-1), step_params,
               native_object_type_name(step_params));
  if (auto* params = native_reference_field(state.tutorial_step, "params_")) {
    log_ghost_tutorial_object("current-step-params", params);
  }
}

void repair_ghost_tutorial_active_flag_probe(const NativeShortcutDiagnosticState& state, void* shortcuts_manager)
{
  static void* candidate_step    = nullptr;
  static int   candidate_samples = 0;

  const auto step_type = native_value_field<int64_t>(state.tutorial_step, "type_").value_or(-1);
  const auto exact_orphan =
      !state.can_use_shortcuts && state.actions_enabled && state.interior_enabled && state.galaxy_enabled
      && !state.video_playing && state.tutorial_active && !state.tutorial_blocking && !state.tutorial_ui_open
      && state.tutorial_manager && state.tutorial_mission && !state.tutorial_objective && state.tutorial_data
      && !state.tutorial_component && !state.tutorial_items && !state.tutorial_end_step && !state.tutorial_next_step
      && state.tutorial_step && state.tutorial_step_index == 0 && state.tutorial_target_section == -1 && step_type == 0
      && !state.show_keybindings && !state.message_box_visible && !state.touch_blocked && !state.input_focused
      && !state.ui_popup_visible && !state.plc_offer_open;
  if (!kEnableGhostTutorialActiveFlagRepairProbe || !AdvancedDiagnosticsSettings().hotkey_suppression_logging
      || !exact_orphan) {
    candidate_step    = nullptr;
    candidate_samples = 0;
    return;
  }

  if (candidate_step != state.tutorial_step) {
    candidate_step    = state.tutorial_step;
    candidate_samples = 1;
    return;
  }
  if (++candidate_samples < 2) {
    return;
  }

  auto  tutorial_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Tutorial", "TutorialManager");
  auto* is_active_field         = il2cpp_class_get_field_from_name(tutorial_manager_helper.get_cls(), "_isActive");
  if (!is_active_field) {
    spdlog::error("[Hotkeys] ghost tutorial active-flag repair failed: _isActive field not found");
    return;
  }

  bool inactive = false;
  il2cpp_field_static_set_value(is_active_field, &inactive);

  static auto get_is_active = tutorial_manager_helper.GetMethod<bool()>("get_IsActive");
  static auto get_can_use_shortcuts =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager")
          .GetMethod<bool()>("get_CanUseShortcuts");
  const auto tutorial_active_after   = get_is_active && get_is_active();
  const auto can_use_shortcuts_after = get_can_use_shortcuts && get_can_use_shortcuts();
  spdlog::warn("[Hotkeys] ghost tutorial active-flag repair applied step={:p} mission_id={} action_id={} "
               "tutorial_active_after={} can_use_shortcuts_after={}",
               state.tutorial_step, native_value_field<int64_t>(state.tutorial_mission, "missionId_").value_or(0),
               native_value_field<int64_t>(state.tutorial_step, "actionId_").value_or(0), tutorial_active_after,
               can_use_shortcuts_after);

  candidate_step    = nullptr;
  candidate_samples = 0;
}

NativeShortcutDiagnosticState native_shortcut_diagnostic_state(void* shortcuts_manager)
{
  NativeShortcutDiagnosticState state;
  if (!shortcuts_manager) {
    return state;
  }

  static auto shortcuts_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  static auto actions_offset             = shortcuts_manager_helper.GetField("_actions").offset();
  static auto action_settings_offset     = shortcuts_manager_helper.GetField("_actionSettings").offset();
  static auto supported_actions_offset   = shortcuts_manager_helper.GetField("_supportedActions").offset();
  static auto actions_initialized_offset = shortcuts_manager_helper.GetField("_areActionsInitialized").offset();
  static auto bindings_dirty_offset      = shortcuts_manager_helper.GetField("_bindingsDirty").offset();
  static auto can_use_shortcuts          = shortcuts_manager_helper.GetMethod<bool()>("get_CanUseShortcuts");

  const auto base           = reinterpret_cast<uintptr_t>(shortcuts_manager);
  state.actions             = *reinterpret_cast<void**>(base + actions_offset);
  state.action_settings     = *reinterpret_cast<void**>(base + action_settings_offset);
  state.supported_actions   = *reinterpret_cast<void**>(base + supported_actions_offset);
  state.actions_initialized = *reinterpret_cast<bool*>(base + actions_initialized_offset);
  state.bindings_dirty      = *reinterpret_cast<bool*>(base + bindings_dirty_offset);
  if (can_use_shortcuts) {
    state.can_use_shortcuts = can_use_shortcuts();
  }
  populate_native_shortcut_gate_state(state, shortcuts_manager);

  if (!state.actions) {
    return state;
  }

  static auto input_action_asset_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionAsset");
  static auto input_action_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputAction");
  static auto asset_enabled  = input_action_asset_helper.GetMethod<bool(void*)>("get_enabled");
  static auto find_action    = input_action_asset_helper.GetMethod<void*(void*, Il2CppString*, bool)>("FindAction");
  static auto action_enabled = input_action_helper.GetMethod<bool(void*)>("get_enabled");

  if (asset_enabled) {
    state.actions_enabled = asset_enabled(state.actions);
  }
  if (!find_action || !action_enabled) {
    return state;
  }

  state.interior_action = find_action(state.actions, il2cpp_string_new("General/interior_view"), false);
  state.galaxy_action   = find_action(state.actions, il2cpp_string_new("General/galaxy_view"), false);
  if (state.interior_action) {
    state.interior_enabled = action_enabled(state.interior_action);
  }
  if (state.galaxy_action) {
    state.galaxy_enabled = action_enabled(state.galaxy_action);
  }

  return state;
}

void log_native_shortcut_diagnostic_state(const char* phase, void* shortcuts_manager, void* settings = nullptr)
{
  const auto state = native_shortcut_diagnostic_state(shortcuts_manager);
  spdlog::info("[Hotkeys] native shortcut state phase={} manager={:p} settings={:p} actions={:p} action_settings={:p} "
               "supported_actions={:p} actions_initialized={} bindings_dirty={} asset_enabled={} interior={:p} "
               "interior_enabled={} galaxy={:p} galaxy_enabled={} can_use_shortcuts={} video_playing={} "
               "tutorial_active={} tutorial_blocking={} tutorial_ui_open={} tutorial_manager={:p} "
               "tutorial_mission={:p} tutorial_data={:p} tutorial_component={:p} tutorial_step={:p} "
               "show_keybindings={} message_box_visible={} touch_blocked={} input_focused={} ui_popup_visible={} "
               "plc_offer_open={}",
               phase, shortcuts_manager, settings, state.actions, state.action_settings, state.supported_actions,
               state.actions_initialized, state.bindings_dirty, state.actions_enabled, state.interior_action,
               state.interior_enabled, state.galaxy_action, state.galaxy_enabled, state.can_use_shortcuts,
               state.video_playing, state.tutorial_active, state.tutorial_blocking, state.tutorial_ui_open,
               state.tutorial_manager, state.tutorial_mission, state.tutorial_data, state.tutorial_component,
               state.tutorial_step, state.show_keybindings, state.message_box_visible, state.touch_blocked,
               state.input_focused, state.ui_popup_visible, state.plc_offer_open);
  log_ghost_tutorial_objects_once(state);
  repair_ghost_tutorial_active_flag_probe(state, shortcuts_manager);
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
    if (AdvancedDiagnosticsSettings().hotkey_suppression_logging) {
      log_native_shortcut_diagnostic_state("initialize-actions-before", _this);
    }
    original(_this);
    if (AdvancedDiagnosticsSettings().hotkey_suppression_logging) {
      log_native_shortcut_diagnostic_state("initialize-actions-after", _this);
    }
  }
}

void ShortcutsManager_SetSettings_Hook(auto original, void* _this, void* settings)
{
  log_native_shortcut_diagnostic_state("set-settings-before", _this, settings);
  original(_this, settings);
  log_native_shortcut_diagnostic_state("set-settings-after", _this, settings);
}

void ShortcutsManager_LateUpdate_Hook(auto original, void* _this)
{
  if (AdvancedDiagnosticsSettings().hotkey_suppression_logging) {
    static uint64_t diagnostic_frame = 0;
    ++diagnostic_frame;
    if (diagnostic_frame == 1 || diagnostic_frame % 300 == 0) {
      log_native_shortcut_diagnostic_state("late-update-sample", _this);
    }
  }

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
               "allow_key_fallthrough={} scopely_shortcuts={} original_frame_policy={} frame_owner=FrameTickHooks "
               "initialize_actions_hook={}",
               Config::Get().installHotkeyHooks, Config::Get().hotkeys_enabled, Config::Get().use_scopely_hotkeys,
               AllowKeyFallthrough(), scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
               original_frame_policy_name(OriginalFramePolicySetting()), kEnableShortcutInitializeHook);

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

  if (AdvancedDiagnosticsSettings().hotkey_suppression_logging) {
    auto shortcuts_manager_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
    if (!shortcuts_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kShortcutSetSettingsHook);
    } else {
      auto set_settings = shortcuts_manager_helper.GetMethod("SetSettings", 1);
      if (set_settings == nullptr) {
        hooks.record_missing_method(kShortcutSetSettingsHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShortcutSetSettingsHook, set_settings,
                                         ShortcutsManager_SetSettings_Hook);
      }
    }
  } else {
    hooks.record_skipped(kShortcutSetSettingsHook, "hotkey suppression logging disabled");
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
