/**
 * @file server_transfer_diagnostics.h
 * @brief Temporary #258 server-transfer incident diagnostic concern.
 */
#pragma once

#include "targeted_diagnostics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace server_transfer_diagnostics
{
inline constexpr targeted_diagnostics::ConcernSpec kConcernSpec{
    .id                    = "server-transfer",
    .owner                 = "runtime-observability",
    .tracking_issue        = "#258",
    .status                = targeted_diagnostics::ConcernStatus::Temporary,
    .introduced_in         = {2, 1, 0},
    .sunset_at             = {2, 2, 0},
    .remove_or_revise_when = "A Scopely-ready Incursion transfer report is assembled or the failing "
                             "relocation/transfer sequence is understood.",
    .promotion_criteria =
        "A recurring support workflow and reviewed low-overhead schema justify permanent transfer diagnostics.",
};

inline constexpr size_t kCategoryBytes      = 40;
inline constexpr size_t kMessageBytes       = 128;
inline constexpr size_t kTransactionIdBytes = 72;
inline constexpr size_t kRequestUrlBytes    = 120;

enum class Direction : uint8_t {
  Unknown,
  Rival,
  Home,
};

enum class TransferStage : uint8_t {
  UiIntent,
  ManagerDispatch,
  TemporaryRequestReturned,
  RecallRequestReturned,
  PollRequestReturned,
  PollingStarted,
  StateChanged,
  RequestSucceeded,
  PollSucceeded,
  PollingCompleted,
  TransferFailedPopup,
  TemporaryTransferFailedPopup,
};

enum class ErrorStage : uint8_t {
  Request,
  Poll,
  ManagerPoll,
};

enum class RelocationStage : uint8_t {
  RelocateStarbase,
  MoveStarbaseToServerInstance,
};

struct RequestHandlerSnapshot {
  bool start_request_in_progress   = false;
  bool polling_request_in_progress = false;
  bool active_operation_present    = false;
};

struct ManagerSnapshot {
  int32_t  transfer_timeout_seconds   = 0;
  uint32_t max_poll_errors            = 0;
  float    polling_frequency_seconds  = 0.0F;
  float    time_until_next_poll       = 0.0F;
  int32_t  transfer_failure_count     = 0;
  bool     polling_for_progress       = false;
  bool     poll_requested             = false;
  bool     active_operation_present   = false;
  bool     transfer_operation_present = false;
};

struct ErrorSnapshot {
  std::array<char, kCategoryBytes>      category{};
  std::array<char, kMessageBytes>       message{};
  std::array<char, kTransactionIdBytes> transaction_id{};
  std::array<char, kRequestUrlBytes>    request_url{};
  int32_t                               type               = 0;
  int32_t                               code               = 0;
  int32_t                               http_response_code = 0;
  bool                                  present            = false;
};

struct TransferEvent {
  uint64_t               attempt_id               = 0;
  TransferStage          stage                    = TransferStage::UiIntent;
  Direction              direction                = Direction::Unknown;
  uint32_t               target_instance_id       = 0;
  int32_t                transfer_state           = -1;
  float                  progress                 = -1.0F;
  bool                   result_available         = false;
  bool                   result                   = false;
  bool                   output_operation_present = false;
  RequestHandlerSnapshot request_handler;
  ManagerSnapshot        manager;
};

struct TransferErrorEvent {
  uint64_t               attempt_id = 0;
  ErrorStage             stage      = ErrorStage::Request;
  Direction              direction  = Direction::Unknown;
  ErrorSnapshot          error;
  RequestHandlerSnapshot request_handler;
  ManagerSnapshot        manager;
};

struct RelocationEvent {
  uint64_t        relocation_id              = 0;
  uint64_t        active_transfer_attempt_id = 0;
  RelocationStage stage                      = RelocationStage::RelocateStarbase;
  Direction       active_transfer_direction  = Direction::Unknown;
  int64_t         node_id                    = 0;
  int32_t         target_instance_id         = 0;
};

targeted_diagnostics::Concern& Concern();
[[nodiscard]] bool             Enabled();
void                           Reset();

uint64_t RecordIntent(Direction direction, uint32_t target_instance_id = 0);
uint64_t RecordDispatch(Direction direction, uint32_t target_instance_id, const ManagerSnapshot& manager);
uint64_t RecordRequestResult(TransferStage stage, Direction direction, uint32_t target_instance_id, bool result,
                             bool output_operation_present, const RequestHandlerSnapshot& request_handler);
void     RecordTransfer(TransferStage stage, int32_t transfer_state, float progress,
                        const RequestHandlerSnapshot& request_handler = {}, const ManagerSnapshot& manager = {});
void     RecordError(ErrorStage stage, const ErrorSnapshot& error, const RequestHandlerSnapshot& request_handler = {},
                     const ManagerSnapshot& manager = {});
uint64_t RecordRelocation(RelocationStage stage, int64_t node_id, int32_t target_instance_id = 0);

[[nodiscard]] uint64_t  CurrentAttemptId();
[[nodiscard]] Direction CurrentDirection();

template <size_t Size> std::array<char, Size> CopyBoundedText(const std::string_view value)
{
  std::array<char, Size> copy{};
  if constexpr (Size > 0) {
    const auto length = value.size() < Size - 1 ? value.size() : Size - 1;
    for (size_t index = 0; index < length; ++index) {
      copy[index] = value[index];
    }
  }
  return copy;
}

[[nodiscard]] std::array<char, kRequestUrlBytes> SanitizeRequestUrl(std::string_view value);
[[nodiscard]] std::string_view                   DirectionName(Direction direction);
[[nodiscard]] std::string_view                   TransferStageName(TransferStage stage);
[[nodiscard]] std::string_view                   ErrorStageName(ErrorStage stage);
[[nodiscard]] std::string_view                   RelocationStageName(RelocationStage stage);

void InstallServerTransferDiagnosticHooks();
} // namespace server_transfer_diagnostics

namespace targeted_diagnostics
{
template <> struct EventTraits<server_transfer_diagnostics::TransferEvent> {
  static constexpr std::string_view event_type       = "transfer-event";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const server_transfer_diagnostics::TransferEvent&, nlohmann::ordered_json&);
};

template <> struct EventTraits<server_transfer_diagnostics::TransferErrorEvent> {
  static constexpr std::string_view event_type       = "transfer-error";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const server_transfer_diagnostics::TransferErrorEvent&, nlohmann::ordered_json&);
};

template <> struct EventTraits<server_transfer_diagnostics::RelocationEvent> {
  static constexpr std::string_view event_type       = "relocation-event";
  static constexpr uint32_t         schema_version   = 1;
  static constexpr bool             owns_queued_data = true;
  static void SerializeFields(const server_transfer_diagnostics::RelocationEvent&, nlohmann::ordered_json&);
};
} // namespace targeted_diagnostics
