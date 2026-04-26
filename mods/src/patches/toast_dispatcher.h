/**
 * @file toast_dispatcher.h
 * @brief Shared routing decision for ToastObserver hooks.
 */
#pragma once

struct Toast;

enum class ToastDispatchDecision {
  PassThrough = 0,
  Consumed = 1,
  Filtered = 2,
};

ToastDispatchDecision toast_dispatcher_dispatch(Toast* toast);
bool toast_dispatch_decision_allows_original(ToastDispatchDecision decision);
