/**
 * @file viewer_mgmt.h
 * @brief Object viewer visibility management and rewards panel toggling.
 *
 * Provides utilities for hiding all visible object viewer overlays (used by
 * Escape key handling), dismissing the animated rewards screen, and toggling
 * the cargo/rewards info panel with multi-frame show support.
 */
#pragma once

/** @brief Check if any object viewer overlay is currently visible. */
bool CanHideViewers();

/**
 * @brief Hide all visible object viewer overlays.
 * @return true if at least one viewer was hidden.
 */
bool DidHideViewers();

/**
 * @brief Dismiss the golden animated rewards screen if it is active.
 * @return true if the rewards screen was dismissed.
 */
bool TryDismissRewardsScreen();

/**
 * @brief Toggle the cargo/rewards info panel on visible pre-scan targets.
 *
 * If the panel is hidden, queues it to show over the next several frames;
 * if already visible, hides it immediately.
 */
void HandleActionView();

/**
 * @brief Decrement the info-pending counter and show cargo panels while active.
 *
 * Called every frame from the router. When the counter is positive, attempts
 * to make the rewards panel visible on all pre-scan target widgets.
 */
void TickInfoPending();

/**
 * @brief Set the info-pending frame counter for multi-frame panel display.
 * @param frames Number of frames to keep attempting to show the panel.
 */
void SetInfoPending(int frames);
