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
constexpr bool kEnableShortcutInitializeHook              = true;
constexpr bool kEnableShortcutLateUpdateGuardHook         = true;
constexpr bool kEnableRewardsButtonHook                   = true;
constexpr bool kEnablePreScanTargetHook                   = true;
constexpr bool kEnableSectionManagerBackButtonHook        = true;
constexpr bool kEnableNavigationSetCourseHook             = true;
constexpr bool kEnableFleetBarSelectionGuardHooks         = true;
constexpr bool kEnableNativeShortcutPointerCallbackGuards = true;

const char* initialize_actions_reason()
{ return scopely_shortcut_policy_name(ScopelyShortcutsPolicy()); }

#define SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS(X)                                                                   \
  X(ShipManage, OnShipManageAction)                                                                                    \
  X(ShipLocate, OnShipLocateAction)                                                                                    \
  X(ShipRecall, OnShipRecallAction)                                                                                    \
  X(Interior, OnInteriorAction)                                                                                        \
  X(Exterior, OnExteriorAction)                                                                                        \
  X(System, OnSystemAction)                                                                                            \
  X(Galaxy, OnGalaxyAction)                                                                                            \
  X(Events, OnEventsAction)                                                                                            \
  X(ShopOffers, OnShopOffersAction)                                                                                    \
  X(Gifts, OnGiftsAction)                                                                                              \
  X(HelpAlliance, OnHelpAllianceAction)                                                                                \
  X(Ships, OnShipsAction)                                                                                              \
  X(Officers, OnOfficersAction)                                                                                        \
  X(Command, OnCommandAction)                                                                                          \
  X(Factions, OnFactionsAction)                                                                                        \
  X(Items, OnItemsAction)                                                                                              \
  X(Refinery, OnRefineryAction)                                                                                        \
  X(Alliance, OnAllianceAction)                                                                                        \
  X(Chat, OnChatAction)                                                                                                \
  X(Inbox, OnInboxAction)                                                                                              \
  X(Missions, OnMissionsAction)                                                                                        \
  X(ChallengeTrack, OnChallengeTrackAction)                                                                            \
  X(AwayTeams, OnAwayTeamsAction)                                                                                      \
  X(DailyGoals, OnDailyGoalsAction)                                                                                    \
  X(FieldTrainings, OnFieldTrainingsAction)                                                                            \
  X(Challenges, OnChallengesAction)                                                                                    \
  X(ResearchLanding, OnResearchLandingAction)                                                                          \
  X(Consumables, OnConsumablesAction)                                                                                  \
  X(PeaceShield, OnPeaceShieldAction)                                                                                  \
  X(ToggleBattleView, OnToggleBattleViewAction)                                                                        \
  X(ShowKeybindings, OnShowKeybindingsAction)                                                                          \
  X(SideChat, OnSideChat)                                                                                              \
  X(UseFleetCommanderAbility, OnUseFleetCommanderAbility)                                                              \
  X(ToggleArenaScores, OnToggleArenaScores)                                                                            \
  X(Archives, OnArchivesAction)

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

#define DEFINE_SHORTCUT_POINTER_CALLBACK_DESCRIPTOR(token, method)                                                     \
  constexpr HookDescriptor kShortcut##token##GuardHook = {                                                             \
      "ShortcutsManager." #method,                                                                                     \
      "guard native shortcut callbacks when the unified input dispatcher owns the current chord",                      \
      {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", #method},                                       \
      "native shortcut callback guard unavailable",                                                                    \
  };

SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS(DEFINE_SHORTCUT_POINTER_CALLBACK_DESCRIPTOR)

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

// Mirrors UnityEngine.InputSystem.InputAction+CallbackContext (16-byte struct, x64).
// Layout per il2cpp dump: { InputActionState* m_State; int m_ActionIndex; int m_BindingIndex; }.
// On the Windows x64 ABI a 16-byte struct argument is passed by reference
// (caller-allocated storage), so probe hooks receive a `void*` that points
// to an instance of this layout.
struct InputActionCallbackContext {
  void*   state         = nullptr;
  int32_t action_index  = -1;
  int32_t binding_index = -1;
};

static_assert(sizeof(InputActionCallbackContext) == 16);

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

bool native_shortcut_probe_trace_enabled()
{
  const auto level = RuntimeTraceLevelSetting();
  return level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
}

void log_native_shortcut_probe(const char* callback, const InputActionCallbackContext& context)
{
  if (!native_shortcut_probe_trace_enabled()) {
    return;
  }

  spdlog::info("[HotkeyProbe] native-shortcut callback={} suppress_native_shortcuts={} context_state={:p} "
               "action_index={} hotkeys_enabled={} scopely_shortcuts={} original_frame_policy={}",
               callback, hotkey_router_should_suppress_native_shortcuts(), context.state, context.action_index,
               Config::Get().hotkeys_enabled, scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
               original_frame_policy_name(OriginalFramePolicySetting()));
}

void log_native_shortcut_probe_pointer(const char* callback, const void* context)
{
  if (!native_shortcut_probe_trace_enabled()) {
    return;
  }

  // The void* is a pointer to a 16-byte InputAction.CallbackContext. UI-button-driven
  // invocations pass a default-constructed (zeroed) context with state == nullptr;
  // keyboard-driven invocations from Unity's InputSystem carry a non-null state pointer.
  // Logging all three fields lets us validate that discriminator before we wire it into
  // suppression decisions.
  const auto* typed       = static_cast<const InputActionCallbackContext*>(context);
  const void* state_ptr   = typed ? typed->state : nullptr;
  const auto  action_idx  = typed ? typed->action_index : -1;
  const auto  binding_idx = typed ? typed->binding_index : -1;
  const char* source      = (state_ptr == nullptr) ? "ui-or-direct" : "keyboard-or-input-system";
  spdlog::info("[HotkeyProbe] native-shortcut callback={} suppress_native_shortcuts={} context_ptr={:p} "
               "state_ptr={:p} action_index={} binding_index={} source={} "
               "hotkeys_enabled={} scopely_shortcuts={} original_frame_policy={}",
               callback, hotkey_router_should_suppress_native_shortcuts(), context, state_ptr, action_idx, binding_idx,
               source, Config::Get().hotkeys_enabled, scopely_shortcut_policy_name(ScopelyShortcutsPolicy()),
               original_frame_policy_name(OriginalFramePolicySetting()));
}

} // namespace

#define INSTALL_SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARD(hooks, descriptor, hook_fn)                                   \
  do {                                                                                                                 \
    auto shortcuts_manager_helper =                                                                                    \
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");                       \
    if (!shortcuts_manager_helper.isValidHelper()) {                                                                   \
      (hooks).record_missing_helper((descriptor));                                                                     \
    } else {                                                                                                           \
      auto method = shortcuts_manager_helper.GetMethod((descriptor).target.method_name.data(), 1);                     \
      if (method == nullptr) {                                                                                         \
        (hooks).record_missing_method((descriptor));                                                                   \
      } else {                                                                                                         \
        HOOK_REGISTRY_SPUD_STATIC_DETOUR((hooks), (descriptor), method, hook_fn);                                      \
      }                                                                                                                \
    }                                                                                                                  \
  } while (false)

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
  const auto should_call_original = hotkey_router_should_call_original_initialize_actions();
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
  hotkey_router_refresh_native_shortcut_suppression();
  const auto suppress = hotkey_router_should_suppress_native_shortcuts();
  if (suppress) {
    spdlog::debug("[Hotkeys] suppressed native shortcut update method=ShortcutsManager.LateUpdate");
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

template <typename OriginalFn>
void HandleNativeShortcutPointerCallback(OriginalFn original, void* _this, void* context, const char* callback)
{
  hotkey_router_refresh_native_shortcut_suppression();
  log_native_shortcut_probe_pointer(callback, context);
  const auto* typed_context         = static_cast<const InputActionCallbackContext*>(context);
  const auto  input_system_callback = typed_context && typed_context->state != nullptr;
  if (input_system_callback && hotkey_router_should_suppress_native_shortcuts()) {
    spdlog::debug("[Hotkeys] suppressed native shortcut callback={} context_ptr={:p}", callback, context);
    return;
  }

  original(_this, context);
}

#define DEFINE_SHORTCUT_POINTER_CALLBACK_HOOK(token, method)                                                           \
  void ShortcutsManager_##method##_GuardHook(auto original, void* _this, void* context)                                \
  { HandleNativeShortcutPointerCallback(original, _this, context, #method); }

SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS(DEFINE_SHORTCUT_POINTER_CALLBACK_HOOK)

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

#define INSTALL_SHORTCUT_POINTER_CALLBACK_GUARD(token, method)                                                         \
  do {                                                                                                                 \
    if (kEnableNativeShortcutPointerCallbackGuards) {                                                                  \
      INSTALL_SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARD(hooks, kShortcut##token##GuardHook,                             \
                                                       ShortcutsManager_##method##_GuardHook);                         \
    } else {                                                                                                           \
      hooks.record_skipped(kShortcut##token##GuardHook, "compile-time disabled");                                      \
    }                                                                                                                  \
  } while (false);

  SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS(INSTALL_SHORTCUT_POINTER_CALLBACK_GUARD)

#undef INSTALL_SHORTCUT_POINTER_CALLBACK_GUARD

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

#undef INSTALL_SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARD
