/**
 * @file cargo_display.h
 * @brief Automatic cargo/rewards panel display based on target type and config.
 *
 * Decides whether to auto-show the cargo rewards panel when a target is
 * selected, based on the fleet type (player, hostile, armada, station) and
 * the user's per-type toggle settings.
 */
#pragma once

class RewardsButtonWidget;
struct PreScanTargetWidget;

/**
 * @brief Evaluate whether the cargo panel should auto-show for this widget's target.
 * @param widget The RewardsButtonWidget whose context to check.
 * @return true if config allows auto-display for this target type.
 */
bool CheckShowCargo(RewardsButtonWidget* widget);

/**
 * @brief Hook handler for RewardsButtonWidget::BindContext — auto-shows cargo if enabled.
 * @param _this The hooked RewardsButtonWidget instance.
 */
void HandleCargoBindContext(RewardsButtonWidget* _this);

/**
 * @brief Hook handler for PreScanTargetWidget::ShowFleet — auto-shows cargo if enabled.
 * @param _this The hooked PreScanTargetWidget instance.
 */
void HandleCargoShowFleet(PreScanTargetWidget* _this);

/**
 * @brief Re-apply cargo display filters to currently visible target widgets.
 *
 * Used after cargo filter hotkeys toggle runtime config so the currently
 * selected target reflects the new state immediately.
 */
void RefreshVisibleCargoDisplays();
