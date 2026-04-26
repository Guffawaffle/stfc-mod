/**
 * @file notification_service.h
 * @brief OS-native notification delivery for in-game toast events.
 *
 * Bridges the game's Toast system to platform notification APIs (Windows toast
 * notifications via WinRT). Resolves IL2CPP LanguageManager methods at init
 * time to localize toast text, then formats and delivers notifications for
 * user-configured notification types.
 *
 * Canonical config model:
 * - `[notifications]` selects which events should produce OS notifications.
 * - `[ui].disabled_banner_types` only suppresses in-game banner display.
 * - Legacy `[ui]` notification allowlists are migration-only compatibility.
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
 * @brief Return the notification title for a toast state.
 */
const char* notification_toast_title(int state);

/**
 * @brief Deduplicate repeated ToastObserver passes for the same Toast pointer.
 */
bool notification_should_process_toast(Toast* toast);

/**
 * @brief Process a non-feature-specific game toast for potential OS notification delivery.
 *
 * Checks the toast state against the unified `[notifications]` config,
 * builds a human-readable body (battle data or localized text), and delivers
 * an OS-native notification if the toast type matches.
 *
 * @param toast The game Toast object from the hooked banner display method.
 */
void notification_handle_generic_toast(Toast* toast, int state, const char* title);

/**
 * @brief Send an arbitrary OS-native notification with the given title and body.
 *
 * Requires notification_init() to have been called first. No-op on non-Windows.
 *
 * @param title Notification title text.
 * @param body  Notification body text.
 */
void notification_show(const char* title, const char* body);

