/**
 * @file incoming_attack_notifications.cc
 * @brief Minimal incoming-attack toast routing.
 *
 * The queue hook owns targeted incoming-attack notifications. This helper keeps
 * notification_service.cc from knowing incoming-attack policy and consumes the
 * matching toast so the user does not get a second generic alert.
 */
#include "patches/incoming_attack_notifications.h"

#include "config.h"

#include <prime/Toast.h>
#include <testable_functions.h>

#include <spdlog/spdlog.h>

IncomingAttackToastAction incoming_attack_notifications_handle_toast(const ToastEnqueuedSignal& signal)
{
  if (!incoming_attack_policy_consumes_toast_state(signal.state)) {
    return IncomingAttackToastAction::NotIncomingAttack;
  }

  const auto& notifications = Config::Get().notifications;
  spdlog::debug("[IncomingAttack] consumed toast state={} title='{}' notificationsEnabled={}",
                signal.state,
                signal.title ? signal.title : "",
                notifications.enabled);
  return IncomingAttackToastAction::Consumed;
}
