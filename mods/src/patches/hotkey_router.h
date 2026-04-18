/**
 * @file hotkey_router.h
 * @brief Layer-2 hotkey router — bridges IL2CPP hook entry points to concern handlers.
 *
 * The router sits between the raw hooks (which intercept IL2CPP methods)
 * and the individual feature modules (navigation, fleet actions, cargo, viewers).
 * Each function here is called from exactly one hook and fans out to the
 * appropriate subsystem based on key state, UI context, and config flags.
 */
#pragma once

struct ScreenManager;
class RewardsButtonWidget;
struct PreScanTargetWidget;

/**
 * @brief Main per-frame hotkey processing, called from the ScreenManager::Update hook.
 * @param _this The hooked ScreenManager instance.
 * @return true if the original ScreenManager::Update should still execute.
 */
bool  hotkey_router_screen_update(ScreenManager* _this);

/**
 * @brief Called from the InitializeActions hook.
 * @return true to let the original InitializeActions run (Scopely hotkeys or fallthrough).
 */
bool  hotkey_router_init_actions();

/**
 * @brief Called from RewardsButtonWidget::BindContext hook to auto-show cargo panels.
 * @param _this The hooked RewardsButtonWidget instance.
 */
void  hotkey_router_bind_context(RewardsButtonWidget* _this);

/**
 * @brief Called from PreScanTargetWidget::ShowFleet hook to auto-show cargo panels.
 * @param _this The hooked PreScanTargetWidget instance.
 */
void  hotkey_router_show_fleet(PreScanTargetWidget* _this);
