/**
 * @file incoming_attack_notifications.h
 * @brief Incoming-attack toast routing.
 *
 * Notification architecture rule: notification_service.cc should stay a thin
 * platform router. It owns generic toast delivery, queueing, and OS APIs. This
 * module owns incoming-attack-specific routing decisions.
 */
#pragma once

struct Toast;

enum class IncomingAttackToastAction {
  NotIncomingAttack = 0,
  Consumed = 1,
};

/**
 * @brief Handle an incoming-attack toast if the state matches that feature.
 *
 * Incoming-attack notifications are emitted from ToastFleetObserver.QueueNotifications;
 * matching toast events are consumed here to avoid duplicate generic toasts.
 */
IncomingAttackToastAction incoming_attack_notifications_handle_toast(Toast* toast, int state, const char* title);