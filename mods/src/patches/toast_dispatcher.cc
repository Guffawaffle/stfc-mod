/**
 * @file toast_dispatcher.cc
 * @brief Central ToastObserver routing for filtering and notifications.
 */
#include "patches/toast_dispatcher.h"

#include "config.h"
#include "patches/incoming_attack_notifications.h"
#include "patches/live_debug.h"
#include "patches/notification_service.h"
#include "patches/signals.h"

#include <prime/Toast.h>

#include <algorithm>
#include <spdlog/spdlog.h>

ToastDispatchDecision toast_dispatcher_dispatch(Toast* toast)
{
  if (!toast) {
    return ToastDispatchDecision::PassThrough;
  }

  const auto state = toast->get_State();
  const auto* title = notification_toast_title(state);
  const auto should_process_notifications = notification_should_process_toast(toast);
  const ToastEnqueuedSignal signal{toast, state, title, "ToastObserver"};

  IncomingAttackToastAction incoming_attack_action = IncomingAttackToastAction::NotIncomingAttack;
  if (should_process_notifications) {
    live_debug_record_toast_notification(signal.source, signal.raw_toast, signal.state, signal.title ? signal.title : "");
    incoming_attack_action = incoming_attack_notifications_handle_toast(signal);
  }

  if (std::ranges::find(Config::Get().disabled_banner_types, state) != Config::Get().disabled_banner_types.end()) {
    spdlog::debug("[ToastDispatcher] state={} decision=Filtered title='{}'", state, title ? title : "");
    return ToastDispatchDecision::Filtered;
  }

  if (!should_process_notifications) {
    return ToastDispatchDecision::PassThrough;
  }

  if (incoming_attack_action == IncomingAttackToastAction::Consumed) {
    spdlog::debug("[ToastDispatcher] state={} decision=Consumed title='{}'", state, title ? title : "");
    return ToastDispatchDecision::Consumed;
  }

  notification_handle_generic_toast(toast, state, title);
  return ToastDispatchDecision::PassThrough;
}

bool toast_dispatch_decision_allows_original(ToastDispatchDecision decision)
{
  return decision != ToastDispatchDecision::Filtered;
}
