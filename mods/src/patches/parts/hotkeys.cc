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
#include "prime/KeyCode.h"
#include "testable_functions.h"

#include "prime/ChatMessageListLocalViewController.h"
#include "prime/PreScanTargetWidget.h"

namespace {
constexpr bool kEnableShortcutInitializeHook = true;
constexpr bool kEnableRewardsButtonHook = true;
constexpr bool kEnablePreScanTargetHook = true;
constexpr bool kEnableSectionManagerBackButtonHook = true;

const char* initialize_actions_reason()
{
  if (Config::Get().use_scopely_hotkeys) {
    return "use_scopely_hotkeys";
  }

  if (AllowKeyFallthrough()) {
    return "allow_key_fallthrough";
  }

  return "mod-hotkeys-only";
}

constexpr HookDescriptor kInitializeActionsHook = {
  "ShortcutsManager.InitializeActions",
  "decide whether Scopely shortcut actions initialize alongside mod hotkeys",
  {"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager", "InitializeActions"},
  "Scopely shortcuts may be unavailable or may double-handle inputs",
};

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

bool is_back_button_exit_section(void* section_manager)
{
  static auto section_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  static auto in_back_button_exit_section = section_manager_helper.GetMethod<bool(void*)>("InBackButtonExitSection");
  return section_manager != nullptr && in_back_button_exit_section && in_back_button_exit_section(section_manager);
}

bool should_suppress_escape_exit_back_button(void* section_manager)
{
  if (!Config::Get().disable_escape_exit || !Key::Pressed(KeyCode::Escape) || !is_back_button_exit_section(section_manager)) {
    return false;
  }

  static auto last_escape_back_button_press = std::chrono::steady_clock::time_point{};
  const auto now = std::chrono::steady_clock::now();
  auto elapsed_ms_since_last_press = int64_t{-1};
  if (last_escape_back_button_press != std::chrono::steady_clock::time_point{}) {
    elapsed_ms_since_last_press = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_escape_back_button_press).count();
  }
  last_escape_back_button_press = now;

  return should_suppress_escape_exit(Config::Get().disable_escape_exit,
                                     true,
                                     Config::Get().escape_exit_timer,
                                     elapsed_ms_since_last_press);
}
}

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
  spdlog::info("[Hotkeys] ShortcutsManager.InitializeActions original={} reason={} use_scopely_hotkeys={} allow_key_fallthrough={}",
               should_call_original ? "called" : "suppressed",
               initialize_actions_reason(),
               Config::Get().use_scopely_hotkeys,
               AllowKeyFallthrough());

  if (should_call_original) {
    return original(_this);
  }
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

void SectionManager_BackButtonPressed_Hook(auto original, void* _this)
{
  if (should_suppress_escape_exit_back_button(_this)) {
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

  spdlog::info("[Hotkeys] startup config installHotkeyHooks={} hotkeys_enabled={} use_scopely_hotkeys={} allow_key_fallthrough={} frame_owner=FrameTickHooks initialize_actions_hook={}",
               Config::Get().installHotkeyHooks,
               Config::Get().hotkeys_enabled,
               Config::Get().use_scopely_hotkeys,
               AllowKeyFallthrough(),
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

  if (kEnableSectionManagerBackButtonHook) {
    auto section_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
    if (!section_manager_helper.isValidHelper()) {
      hooks.record_missing_helper(kSectionManagerBackButtonPressedHook);
    } else {
      auto back_button_pressed = section_manager_helper.GetMethod("BackButtonPressed");
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

  hooks.log_summary();
}
