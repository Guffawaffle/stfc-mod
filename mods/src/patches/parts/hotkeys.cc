/**
 * @file hotkeys.cc
 * @brief Thin SPUD hook layer for keyboard shortcut processing.
 *
 * This file installs game-method detours that delegate all actual input
 * handling to the hotkey_router (hotkey_router.h / hotkey_router.cc).
 * Each hook is intentionally minimal — it captures the call, forwards
 * context to the router, and invokes the original method.
 */

#include "config.h"

#include <spud/detour.h>

#include "patches/hotkey_router.h"
#include "patches/live_debug.h"

#include "prime/ChatMessageListLocalViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"

// ─── SPUD Hook Delegates ─────────────────────────────────────────────────────

/**
 * @brief Hook: ScreenManager::Update
 *
 * Intercepts the per-frame UI update to process keyboard shortcuts.
 * Original method: drives UI state machine each frame.
 * Our modification: calls hotkey_router_screen_update() before the
 * original; the router may consume the frame (return false → skip original).
 */
void ScreenManager_Update_Hook(auto original, ScreenManager* _this)
{
  live_debug_tick(_this);

  if (hotkey_router_screen_update(_this)) {
    return original(_this);
  }
}

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
  if (hotkey_router_init_actions()) {
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
  auto shortcuts_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  if (!shortcuts_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("GameInput", "ShortcutsManager");
  } else {
    auto ptr_can_user_shortcuts = shortcuts_manager_helper.GetMethod("InitializeActions");
    if (ptr_can_user_shortcuts == nullptr) {
      ErrorMsg::MissingMethod("ShortcutsManager", "InitializeActions");
    } else {
      SPUD_STATIC_DETOUR(ptr_can_user_shortcuts, InitializeActions_Hook);
    }
  }

  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  if (!screen_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UI", "ScreenManager");
  } else {
    auto ptr_update = screen_manager_helper.GetMethod("Update");
    if (ptr_update == nullptr) {
      ErrorMsg::MissingMethod("ScreenManager", "Update");
    } else {
      SPUD_STATIC_DETOUR(ptr_update, ScreenManager_Update_Hook);
    }
  }

  static auto rewards_button_widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget");
  if (!rewards_button_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Combat", "RewardsButtonWidget");
  } else {
    auto on_did_bind_context_ptr = rewards_button_widget.GetMethod("OnDidBindContext");
    if (on_did_bind_context_ptr == nullptr) {
      ErrorMsg::MissingMethod("RewardsButtonWidget", "OnDidBindContext");
    } else {
      SPUD_STATIC_DETOUR(on_did_bind_context_ptr, OnDidBindContext_Hook);
    }
  }

  static auto pre_scan_target_widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget");
  if (!pre_scan_target_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Combat", "PreScanTargetWidget");
  } else {
    auto show_with_fleet_ptr = pre_scan_target_widget.GetMethod("ShowWithFleet");
    if (show_with_fleet_ptr == nullptr) {
      ErrorMsg::MissingMethod("PreScanTargetWidget", "ShowWithFleet");
    } else {
      SPUD_STATIC_DETOUR(show_with_fleet_ptr, ShowWithFleet_Hook);
    }
  }
}
