/**
 * @file server_transfer_diagnostics.cc
 * @brief Correlated records for the temporary #258 server-transfer concern.
 */
#include "patches/server_transfer_diagnostics.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <mutex>

namespace server_transfer_diagnostics
{
namespace
{
  targeted_diagnostics::Concern s_concern{kConcernSpec};

  struct CorrelationState {
    uint64_t  next_attempt_id    = 0;
    uint64_t  current_attempt_id = 0;
    uint64_t  next_relocation_id = 0;
    uint32_t  target_instance_id = 0;
    Direction direction          = Direction::Unknown;
    bool      dispatched         = false;
    bool      active             = false;
  };

  CorrelationState s_state;
  std::mutex       s_state_mutex;

  struct AttemptSnapshot {
    uint64_t  attempt_id         = 0;
    uint32_t  target_instance_id = 0;
    Direction direction          = Direction::Unknown;
  };

  AttemptSnapshot current_attempt_locked()
  {
    if (!s_state.active) {
      return {};
    }
    return {
        .attempt_id         = s_state.current_attempt_id,
        .target_instance_id = s_state.target_instance_id,
        .direction          = s_state.direction,
    };
  }
} // namespace

targeted_diagnostics::Concern& Concern()
{ return s_concern; }

bool Enabled()
{ return TARGET_DIAGNOSTIC_ENABLED(s_concern); }

void Reset()
{
  const std::scoped_lock lock(s_state_mutex);
  s_state = {};
}

uint64_t RecordIntent(const Direction direction, const uint32_t target_instance_id)
{
  if (!Enabled()) {
    return 0;
  }

  uint64_t attempt_id = 0;
  {
    const std::scoped_lock lock(s_state_mutex);
    attempt_id                 = ++s_state.next_attempt_id;
    s_state.current_attempt_id = attempt_id;
    s_state.target_instance_id = target_instance_id;
    s_state.direction          = direction;
    s_state.dispatched         = false;
    s_state.active             = true;
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, TransferEvent{.attempt_id         = attempt_id,
                                                         .stage              = TransferStage::UiIntent,
                                                         .direction          = direction,
                                                         .target_instance_id = target_instance_id});
  return attempt_id;
}

uint64_t RecordDispatch(const Direction direction, const uint32_t target_instance_id, const ManagerSnapshot& manager)
{
  if (!Enabled()) {
    return 0;
  }

  uint64_t attempt_id = 0;
  {
    const std::scoped_lock lock(s_state_mutex);
    const bool             reuse_intent = s_state.active && !s_state.dispatched && s_state.direction == direction;
    if (!reuse_intent) {
      s_state.current_attempt_id = ++s_state.next_attempt_id;
    }
    attempt_id                 = s_state.current_attempt_id;
    s_state.target_instance_id = target_instance_id;
    s_state.direction          = direction;
    s_state.dispatched         = true;
    s_state.active             = true;
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, TransferEvent{.attempt_id         = attempt_id,
                                                         .stage              = TransferStage::ManagerDispatch,
                                                         .direction          = direction,
                                                         .target_instance_id = target_instance_id,
                                                         .manager            = manager});
  return attempt_id;
}

uint64_t RecordRequestResult(const TransferStage stage, const Direction direction, const uint32_t target_instance_id,
                             const bool result, const bool output_operation_present,
                             const RequestHandlerSnapshot& request_handler)
{
  if (!Enabled()) {
    return 0;
  }

  AttemptSnapshot attempt;
  {
    const std::scoped_lock lock(s_state_mutex);
    const bool             direction_changed = direction != Direction::Unknown && s_state.direction != direction;
    if (!s_state.active || direction_changed) {
      s_state.current_attempt_id = ++s_state.next_attempt_id;
      s_state.dispatched         = true;
    }
    if (direction != Direction::Unknown) {
      s_state.direction = direction;
    }
    if (target_instance_id != 0) {
      s_state.target_instance_id = target_instance_id;
    }
    s_state.active = true;
    attempt        = current_attempt_locked();
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, TransferEvent{.attempt_id               = attempt.attempt_id,
                                                         .stage                    = stage,
                                                         .direction                = attempt.direction,
                                                         .target_instance_id       = attempt.target_instance_id,
                                                         .result_available         = true,
                                                         .result                   = result,
                                                         .output_operation_present = output_operation_present,
                                                         .request_handler          = request_handler});
  return attempt.attempt_id;
}

void RecordTransfer(const TransferStage stage, const int32_t transfer_state, const float progress,
                    const RequestHandlerSnapshot& request_handler, const ManagerSnapshot& manager)
{
  if (!Enabled()) {
    return;
  }

  AttemptSnapshot attempt;
  {
    const std::scoped_lock lock(s_state_mutex);
    attempt = current_attempt_locked();
    if (stage == TransferStage::PollingCompleted) {
      s_state.active = false;
    }
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, TransferEvent{.attempt_id         = attempt.attempt_id,
                                                         .stage              = stage,
                                                         .direction          = attempt.direction,
                                                         .target_instance_id = attempt.target_instance_id,
                                                         .transfer_state     = transfer_state,
                                                         .progress           = progress,
                                                         .request_handler    = request_handler,
                                                         .manager            = manager});
}

void RecordError(const ErrorStage stage, const ErrorSnapshot& error, const RequestHandlerSnapshot& request_handler,
                 const ManagerSnapshot& manager)
{
  if (!Enabled()) {
    return;
  }

  AttemptSnapshot attempt;
  {
    const std::scoped_lock lock(s_state_mutex);
    attempt = current_attempt_locked();
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, TransferErrorEvent{.attempt_id      = attempt.attempt_id,
                                                              .stage           = stage,
                                                              .direction       = attempt.direction,
                                                              .error           = error,
                                                              .request_handler = request_handler,
                                                              .manager         = manager});
}

uint64_t RecordRelocation(const RelocationStage stage, const int64_t node_id, const int32_t target_instance_id)
{
  if (!Enabled()) {
    return 0;
  }

  uint64_t        relocation_id = 0;
  AttemptSnapshot attempt;
  {
    const std::scoped_lock lock(s_state_mutex);
    relocation_id = ++s_state.next_relocation_id;
    attempt       = current_attempt_locked();
  }

  (void)TARGET_DIAGNOSTIC_WRITE(s_concern, RelocationEvent{.relocation_id              = relocation_id,
                                                           .active_transfer_attempt_id = attempt.attempt_id,
                                                           .stage                      = stage,
                                                           .active_transfer_direction  = attempt.direction,
                                                           .node_id                    = node_id,
                                                           .target_instance_id         = target_instance_id});
  return relocation_id;
}

uint64_t CurrentAttemptId()
{
  const std::scoped_lock lock(s_state_mutex);
  return current_attempt_locked().attempt_id;
}

Direction CurrentDirection()
{
  const std::scoped_lock lock(s_state_mutex);
  return current_attempt_locked().direction;
}

std::array<char, kRequestUrlBytes> SanitizeRequestUrl(const std::string_view value)
{
  const auto query    = value.find('?');
  const auto fragment = value.find('#');
  const auto end      = std::min(query == std::string_view::npos ? value.size() : query,
                                 fragment == std::string_view::npos ? value.size() : fragment);
  return CopyBoundedText<kRequestUrlBytes>(value.substr(0, end));
}

std::string_view DirectionName(const Direction direction)
{
  switch (direction) {
    case Direction::Rival:
      return "rival";
    case Direction::Home:
      return "home";
    case Direction::Unknown:
      return "unknown";
  }
  return "unknown";
}

std::string_view TransferStageName(const TransferStage stage)
{
  switch (stage) {
    case TransferStage::UiIntent:
      return "ui-intent";
    case TransferStage::ManagerDispatch:
      return "manager-dispatch";
    case TransferStage::TemporaryRequestReturned:
      return "temporary-request-returned";
    case TransferStage::RecallRequestReturned:
      return "recall-request-returned";
    case TransferStage::PollRequestReturned:
      return "poll-request-returned";
    case TransferStage::PollingStarted:
      return "polling-started";
    case TransferStage::StateChanged:
      return "state-changed";
    case TransferStage::RequestSucceeded:
      return "request-succeeded";
    case TransferStage::PollSucceeded:
      return "poll-succeeded";
    case TransferStage::PollingCompleted:
      return "polling-completed";
    case TransferStage::TransferFailedPopup:
      return "transfer-failed-popup";
    case TransferStage::TemporaryTransferFailedPopup:
      return "temporary-transfer-failed-popup";
  }
  return "unknown";
}

std::string_view ErrorStageName(const ErrorStage stage)
{
  switch (stage) {
    case ErrorStage::Request:
      return "request";
    case ErrorStage::Poll:
      return "poll";
    case ErrorStage::ManagerPoll:
      return "manager-poll";
  }
  return "unknown";
}

std::string_view RelocationStageName(const RelocationStage stage)
{
  switch (stage) {
    case RelocationStage::RelocateStarbase:
      return "relocate-starbase";
    case RelocationStage::MoveStarbaseToServerInstance:
      return "move-starbase-to-server-instance";
  }
  return "unknown";
}
} // namespace server_transfer_diagnostics

namespace targeted_diagnostics
{
namespace
{
  nlohmann::ordered_json request_handler_json(const server_transfer_diagnostics::RequestHandlerSnapshot& snapshot)
  {
    return {{"start_request_in_progress", snapshot.start_request_in_progress},
            {"polling_request_in_progress", snapshot.polling_request_in_progress},
            {"active_operation_present", snapshot.active_operation_present}};
  }

  nlohmann::ordered_json manager_json(const server_transfer_diagnostics::ManagerSnapshot& snapshot)
  {
    return {{"transfer_timeout_seconds", snapshot.transfer_timeout_seconds},
            {"max_poll_errors", snapshot.max_poll_errors},
            {"polling_frequency_seconds", snapshot.polling_frequency_seconds},
            {"time_until_next_poll", snapshot.time_until_next_poll},
            {"transfer_failure_count", snapshot.transfer_failure_count},
            {"polling_for_progress", snapshot.polling_for_progress},
            {"poll_requested", snapshot.poll_requested},
            {"active_operation_present", snapshot.active_operation_present},
            {"transfer_operation_present", snapshot.transfer_operation_present}};
  }
} // namespace

void EventTraits<server_transfer_diagnostics::TransferEvent>::SerializeFields(
    const server_transfer_diagnostics::TransferEvent& event, nlohmann::ordered_json& fields)
{
  fields = {{"attempt_id", event.attempt_id},
            {"stage", server_transfer_diagnostics::TransferStageName(event.stage)},
            {"direction", server_transfer_diagnostics::DirectionName(event.direction)},
            {"target_instance_id", event.target_instance_id},
            {"transfer_state", event.transfer_state},
            {"progress", event.progress},
            {"result_available", event.result_available},
            {"result", event.result},
            {"output_operation_present", event.output_operation_present},
            {"request_handler", request_handler_json(event.request_handler)},
            {"manager", manager_json(event.manager)}};
}

void EventTraits<server_transfer_diagnostics::TransferErrorEvent>::SerializeFields(
    const server_transfer_diagnostics::TransferErrorEvent& event, nlohmann::ordered_json& fields)
{
  fields = {{"attempt_id", event.attempt_id},
            {"stage", server_transfer_diagnostics::ErrorStageName(event.stage)},
            {"direction", server_transfer_diagnostics::DirectionName(event.direction)},
            {"error",
             {{"present", event.error.present},
              {"type", event.error.type},
              {"code", event.error.code},
              {"http_response_code", event.error.http_response_code},
              {"category", event.error.category.data()},
              {"message", event.error.message.data()},
              {"transaction_id", event.error.transaction_id.data()},
              {"request_url", event.error.request_url.data()}}},
            {"request_handler", request_handler_json(event.request_handler)},
            {"manager", manager_json(event.manager)}};
}

void EventTraits<server_transfer_diagnostics::RelocationEvent>::SerializeFields(
    const server_transfer_diagnostics::RelocationEvent& event, nlohmann::ordered_json& fields)
{
  fields = {{"relocation_id", event.relocation_id},
            {"active_transfer_attempt_id", event.active_transfer_attempt_id},
            {"stage", server_transfer_diagnostics::RelocationStageName(event.stage)},
            {"active_transfer_direction", server_transfer_diagnostics::DirectionName(event.active_transfer_direction)},
            {"node_id", event.node_id},
            {"target_instance_id", event.target_instance_id}};
}
} // namespace targeted_diagnostics
