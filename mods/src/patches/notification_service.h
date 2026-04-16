/**
 * @file notification_service.h
 * @brief OS-native notification delivery for in-game toast events.
 *
 * Bridges the game's Toast system to platform notification APIs (Windows toast
 * notifications via WinRT). Resolves IL2CPP LanguageManager methods at init
 * time to localize toast text, then formats and delivers notifications for
 * user-configured toast types.
 */
#pragma once

struct Toast;

/**
 * @brief One-time initialization: resolve IL2CPP methods and init the platform.
 *
 * Must be called once during InstallToastBannerHooks(). Resolves
 * LanguageManager::Localize(out string, LocaleTextContext) for text
 * localization and initializes WinRT on Windows.
 */
void notification_init();

/**
 * @brief Process a game toast for potential OS notification delivery.
 *
 * Checks the toast state against the user's configured notify_banner_types,
 * builds a human-readable body (battle data or localized text), and delivers
 * an OS-native notification if the toast type matches.
 *
 * @param toast The game Toast object from the hooked banner display method.
 */
void notification_handle_toast(Toast* toast);
