/**
 * @file hotkeys.cc
 * @brief Thin MinHook hook layer for keyboard shortcut processing.
 *
 * This file installs game-method detours that delegate all actual input
 * handling to the hotkey_router (hotkey_router.h / hotkey_router.cc).
 * Each hook is intentionally minimal — it captures the call, forwards
 * context to the router, and invokes the original method.
 */

#include "config.h"

#include <hook/hook.h>

#include "patches/hotkey_router.h"

#include "prime/ChatMessageListLocalViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScreenManager.h"

// ─── MinHook Hook Delegates ──────────────────────────────────────────────────

typedef void (*ScreenManager_Update_fn)(ScreenManager*);
static ScreenManager_Update_fn ScreenManager_Update_original = nullptr;

void ScreenManager_Update_Hook(ScreenManager* _this)
{
  if (hotkey_router_screen_update(_this)) {
    return ScreenManager_Update_original(_this);
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
typedef void (*InitializeActions_fn)(void*);
static InitializeActions_fn InitializeActions_original = nullptr;

void InitializeActions_Hook(void* _this)
{
  if (hotkey_router_init_actions()) {
    return InitializeActions_original(_this);
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
typedef void (*OnDidBindContext_fn)(RewardsButtonWidget*);
static OnDidBindContext_fn OnDidBindContext_original = nullptr;

void OnDidBindContext_Hook(RewardsButtonWidget* _this)
{
  OnDidBindContext_original(_this);
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
typedef void (*ShowWithFleet_fn)(PreScanTargetWidget*, void*);
static ShowWithFleet_fn ShowWithFleet_original = nullptr;

void ShowWithFleet_Hook(PreScanTargetWidget* _this, void* a1)
{
  ShowWithFleet_original(_this, a1);
  hotkey_router_show_fleet(_this);
}

// ─── Manual (Non-MinHook) Hook ──────────────────────────────────────────────

/**
 * @brief Hook: ChatMessageListLocalViewController::AboutToShow
 *
 * Intercepts chat panel display to auto-focus the input field.
 * This is a manual trampoline hook (not MinHook) because the class
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
      MH_INSTALL(ptr_can_user_shortcuts, InitializeActions_Hook, InitializeActions_original);
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
      MH_INSTALL(ptr_update, ScreenManager_Update_Hook, ScreenManager_Update_original);
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
      MH_INSTALL(on_did_bind_context_ptr, OnDidBindContext_Hook, OnDidBindContext_original);
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
      MH_INSTALL(show_with_fleet_ptr, ShowWithFleet_Hook, ShowWithFleet_original);
    }
  }
}
