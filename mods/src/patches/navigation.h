/**
 * @file navigation.h
 * @brief Section navigation helpers for hotkey-driven screen transitions.
 *
 * Wraps the game's SectionManager and NavigationSectionManager APIs to provide
 * simple functions for navigating to any game section from a hotkey handler.
 */
#pragma once

enum class SectionID : int;

/**
 * @brief Navigate directly to a game section by triggering a section change.
 * @param sectionID Target section to navigate to.
 * @param screen_data Optional opaque data passed to the section (default: nullptr).
 */
void GotoSection(SectionID sectionID, void* screen_data = nullptr);

/**
 * @brief Navigate to a navigation-layer section (Galaxy/System views).
 *
 * Attempts to restore persisted section state first; falls back to
 * NavigationSectionManager::ChangeNavigationSection if no saved state exists.
 *
 * @param sectionID Target navigation section.
 */
void ChangeNavigationSection(SectionID sectionID);

/**
 * @brief Attempt to scroll the officer showcase canvas left or right.
 * @param goLeft true to move left, false to move right.
 * @return true if the canvas was moved (currently always returns false — stub).
 */
bool MoveOfficerCanvas(bool goLeft);
