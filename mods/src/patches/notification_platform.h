/**
 * @file notification_platform.h
 * @brief Platform adapter for OS-native notification delivery.
 */
#pragma once

using NotificationPlatformDeliveryOverride = void (*)(const char* title, const char* body);

void notification_platform_init();
void notification_platform_show(const char* title, const char* body);
void notification_platform_set_delivery_override_for_testing(NotificationPlatformDeliveryOverride override_callback);
void notification_platform_reset_delivery_override_for_testing();
