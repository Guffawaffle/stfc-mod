/**
 * @file server_transfer_diagnostics_hooks.cc
 * @brief Science-tier hooks for temporary server-transfer incident capture.
 */
#include "patches/server_transfer_diagnostics.h"

#include "patches/hook_registry.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>
#include <string>

namespace server_transfer_diagnostics
{
namespace
{
  constexpr HookDescriptor kTransferOutHook{
      "TournamentDetailServerClashViewController.TransferOut()",
      "Correlate the player's Incursion transfer-to-rival intent with the native request lifecycle.",
      {"Assembly-CSharp", "Digit.Prime.Tournaments", "TournamentDetailServerClashViewController", "TransferOut"},
      "An Incursion transfer can fail or target an unexpected server after relocation.",
      HookSupportTier::Science};

  constexpr HookDescriptor kTransferHomeHook{
      "TournamentDetailServerClashViewController.TransferHome()",
      "Correlate the player's Incursion recall-home intent with the native request lifecycle.",
      {"Assembly-CSharp", "Digit.Prime.Tournaments", "TournamentDetailServerClashViewController", "TransferHome"},
      "A recall-home request can fail while the account is reported on the home server.",
      HookSupportTier::Science};

  constexpr HookDescriptor kDispatchRivalHook{
      "ServerTransferManager.SendStartServerClashEventTransferRequest(...) ",
      "Capture the numeric target instance for an Incursion temporary transfer.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager",
       "SendStartServerClashEventTransferRequest"},
      "The temporary transfer request can target or leave the account on an unexpected instance.",
      HookSupportTier::Science};

  constexpr HookDescriptor kDispatchHomeHook{
      "ServerTransferManager.SendStartRecallToHomeServerTransfer(...) ",
      "Capture the numeric target instance for an Incursion recall-home request.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager", "SendStartRecallToHomeServerTransfer"},
      "The recall request can disagree with the account's current server state.",
      HookSupportTier::Science};

  constexpr HookDescriptor kPollingStartedHook{
      "ServerTransferManager.StartPollForProgress()",
      "Capture transfer polling admission and its configured timing state.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager", "StartPollForProgress"},
      "Transfer polling can stall, retry, or reset without enough evidence for support.",
      HookSupportTier::Science};

  constexpr HookDescriptor kPollingCompletedHook{
      "ServerTransferManager.OnTransferPollingComplete()",
      "Capture completion of the manager-level transfer polling lifecycle.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager", "OnTransferPollingComplete"},
      "The client can present a transfer failure after an ambiguous polling lifecycle.",
      HookSupportTier::Science};

  constexpr HookDescriptor kManagerPollingFailedHook{
      "ServerTransferManager.OnTransferPollingFailed(GSError)",
      "Capture manager state and bounded server error evidence when polling fails.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager", "OnTransferPollingFailed"},
      "Polling failures can leave the account and client on disagreeing server instances.",
      HookSupportTier::Science};

  constexpr HookDescriptor kTransferFailedPopupHook{
      "ServerTransferManager.ShowServerTransferFailedPopup()",
      "Record when the general server-transfer failure is presented to the player.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager", "ShowServerTransferFailedPopup"},
      "A visible failure can be disconnected from the request error that caused it.",
      HookSupportTier::Science};

  constexpr HookDescriptor kTemporaryTransferFailedPopupHook{
      "ServerTransferManager.ShowTemporaryServerTransferFailedPopup(Action)",
      "Record when the Incursion-specific transfer failure is presented to the player.",
      {"Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager",
       "ShowTemporaryServerTransferFailedPopup"},
      "The Incursion failure popup can report state that contradicts the preceding request.",
      HookSupportTier::Science};

  constexpr HookDescriptor kTransferStateHook{
      "ServerTransferRequestHandler.SetServerTransferProgress(...) ",
      "Capture the login-layer transfer state machine and progress transitions.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler",
       "SetServerTransferProgress"},
      "The transfer state machine can diverge from the account's actual server instance.",
      HookSupportTier::Science};

  constexpr HookDescriptor kTemporaryRequestHook{
      "ServerTransferRequestHandler.RequestTransfer(uint, string, out operation)",
      "Capture the actual temporary-transfer target and native request return value without its reference string.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler", "RequestTransfer"},
      "The login layer can reinterpret a rival transfer as a recall-home request.",
      HookSupportTier::Science};

  constexpr HookDescriptor kRecallRequestHook{
      "ServerTransferRequestHandler.RequestRecallHome(out operation)",
      "Capture the actual recall-home request return value and output operation.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler", "RequestRecallHome"},
      "The login layer can reject recall because account and client server state disagree.",
      HookSupportTier::Science};

  constexpr HookDescriptor kPollRequestHook{
      "ServerTransferRequestHandler.PollProgress(out operation)",
      "Capture each native progress-poll admission result and output operation.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler", "PollProgress"},
      "A transfer can remain unresolved when polling admission or operation state diverges.",
      HookSupportTier::Science};

  constexpr HookDescriptor kRequestSuccessHook{
      "ServerTransferRequestHandler.OnTransferRequestSuccess(int)",
      "Capture successful admission of a transfer request before polling.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler",
       "OnTransferRequestSuccess"},
      "A request can be accepted but later resolve to contradictory server state.",
      HookSupportTier::Science};

  constexpr HookDescriptor kRequestErrorHook{"ServerTransferRequestHandler.OnTransferRequestError(GSError)",
                                             "Capture bounded request error evidence and transaction correlation.",
                                             {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime",
                                              "ServerTransferRequestHandler", "OnTransferRequestError"},
                                             "The transfer request can fail without actionable error evidence.",
                                             HookSupportTier::Science};

  constexpr HookDescriptor kPollSuccessHook{
      "ServerTransferRequestHandler.OnPollSuccess(int)",
      "Capture successful progress polls in the login-layer transfer state machine.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler", "OnPollSuccess"},
      "Progress can advance before the client returns to an unexpected server.",
      HookSupportTier::Science};

  constexpr HookDescriptor kPollErrorHook{
      "ServerTransferRequestHandler.OnPollError(GSError)",
      "Capture bounded polling error evidence and transaction correlation.",
      {"Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler", "OnPollError"},
      "Repeated polling errors can leave transfer state unresolved.",
      HookSupportTier::Science};

  constexpr HookDescriptor kRelocateStarbaseHook{
      "StarbaseManager.RelocateStarbase(long, CallbackContainer<bool>)",
      "Order ordinary starbase relocation against an active Incursion transfer lifecycle.",
      {"Assembly-CSharp", "Digit.Prime.Starbase", "StarbaseManager", "RelocateStarbase"},
      "Relocating on the rival server can precede an unexpected return to the home server.",
      HookSupportTier::Science};

  constexpr HookDescriptor kMoveStarbaseInstanceHook{
      "StarbaseManager.MoveStarbaseToServerInstance(int, long, CallbackContainer<int>)",
      "Capture cross-instance starbase movement with numeric destination and node identifiers.",
      {"Assembly-CSharp", "Digit.Prime.Starbase", "StarbaseManager", "MoveStarbaseToServerInstance"},
      "A relocation can move the starbase or client to an unexpected server instance.",
      HookSupportTier::Science};

  IL2CppClassHelper& server_clash_controller_helper()
  {
    static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Tournaments",
                                                 "TournamentDetailServerClashViewController");
    return helper;
  }

  IL2CppClassHelper& transfer_manager_helper()
  {
    static auto helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ServerTransfer", "ServerTransferManager");
    return helper;
  }

  IL2CppClassHelper& request_handler_helper()
  {
    static auto helper =
        il2cpp_get_class_helper("Digit.Engine.Login.Prime", "Digit.Engine.Login.Prime", "ServerTransferRequestHandler");
    return helper;
  }

  IL2CppClassHelper& starbase_manager_helper()
  {
    static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Starbase", "StarbaseManager");
    return helper;
  }

  template <typename T> T read_field(void* object, IL2CppClassHelper& helper, const char* name, T fallback = {})
  {
    if (object == nullptr || !helper.isValidHelper()) {
      return fallback;
    }
    auto field = helper.GetField(name);
    if (!field.isValidHelper()) {
      return fallback;
    }
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + field.offset());
  }

  RequestHandlerSnapshot snapshot_request_handler(void* handler)
  {
    auto& helper = request_handler_helper();
    return {
        .start_request_in_progress   = read_field<bool>(handler, helper, "_isStartRequestInProgress"),
        .polling_request_in_progress = read_field<bool>(handler, helper, "_isPollingRequestInProgress"),
        .active_operation_present    = read_field<void*>(handler, helper, "_activeTransferOperation") != nullptr,
    };
  }

  ManagerSnapshot snapshot_manager(void* manager)
  {
    auto& helper = transfer_manager_helper();
    return {
        .transfer_timeout_seconds   = read_field<int32_t>(manager, helper, "_transferTimeout"),
        .max_poll_errors            = read_field<uint32_t>(manager, helper, "_maxNumberOfPollErrorsBeforeReset"),
        .polling_frequency_seconds  = read_field<float>(manager, helper, "_pollingFreqSecs"),
        .time_until_next_poll       = read_field<float>(manager, helper, "_timeTillNextPoll"),
        .transfer_failure_count     = read_field<int32_t>(manager, helper, "_serverTransferFailedCount"),
        .polling_for_progress       = read_field<bool>(manager, helper, "_pollingForProgress"),
        .poll_requested             = read_field<bool>(manager, helper, "_pollRequested"),
        .active_operation_present   = read_field<void*>(manager, helper, "_activeTransferOperation") != nullptr,
        .transfer_operation_present = read_field<void*>(manager, helper, "_transferOperation") != nullptr,
    };
  }

  std::string managed_string(void* value)
  {
    if (value == nullptr) {
      return {};
    }
    try {
      return to_string(static_cast<Il2CppString*>(value));
    } catch (...) {
      return "<conversion-failed>";
    }
  }

  ErrorSnapshot snapshot_error(void* error)
  {
    ErrorSnapshot result{.present = error != nullptr};
    if (error == nullptr) {
      return result;
    }

    static auto helper = il2cpp_get_class_helper("Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "GSError");
    result.type        = read_field<int32_t>(error, helper, "<Type>k__BackingField");
    result.code        = read_field<int32_t>(error, helper, "<Code>k__BackingField");
    result.http_response_code = read_field<int32_t>(error, helper, "<HttpResponseCode>k__BackingField");
    result.category =
        CopyBoundedText<kCategoryBytes>(managed_string(read_field<void*>(error, helper, "<Category>k__BackingField")));
    result.message =
        CopyBoundedText<kMessageBytes>(managed_string(read_field<void*>(error, helper, "<Message>k__BackingField")));
    result.transaction_id = CopyBoundedText<kTransactionIdBytes>(
        managed_string(read_field<void*>(error, helper, "<TransactionId>k__BackingField")));
    result.request_url =
        SanitizeRequestUrl(managed_string(read_field<void*>(error, helper, "<RequestUrl>k__BackingField")));
    return result;
  }

  void TransferOut_Hook(auto original, void* controller)
  {
    auto&      helper   = server_clash_controller_helper();
    const auto rival_id = read_field<int32_t>(controller, helper, "_rivalInstanceId");
    RecordIntent(Direction::Rival, rival_id > 0 ? static_cast<uint32_t>(rival_id) : 0);
    original(controller);
  }

  void TransferHome_Hook(auto original, void* controller)
  {
    RecordIntent(Direction::Home);
    original(controller);
  }

  void DispatchRival_Hook(auto original, void* manager, uint32_t target_instance_id, Il2CppString* event_id,
                          void* callbacks)
  {
    RecordDispatch(Direction::Rival, target_instance_id, snapshot_manager(manager));
    original(manager, target_instance_id, event_id, callbacks);
  }

  void DispatchHome_Hook(auto original, void* manager, uint32_t target_instance_id, void* callbacks)
  {
    RecordDispatch(Direction::Home, target_instance_id, snapshot_manager(manager));
    original(manager, target_instance_id, callbacks);
  }

  void PollingStarted_Hook(auto original, void* manager)
  {
    RecordTransfer(TransferStage::PollingStarted, -1, -1.0F, {}, snapshot_manager(manager));
    original(manager);
  }

  void PollingCompleted_Hook(auto original, void* manager)
  {
    original(manager);
    RecordTransfer(TransferStage::PollingCompleted, -1, -1.0F, {}, snapshot_manager(manager));
  }

  void ManagerPollingFailed_Hook(auto original, void* manager, void* error)
  {
    RecordError(ErrorStage::ManagerPoll, snapshot_error(error), {}, snapshot_manager(manager));
    original(manager, error);
  }

  void TransferFailedPopup_Hook(auto original, void* manager)
  {
    RecordTransfer(TransferStage::TransferFailedPopup, -1, -1.0F, {}, snapshot_manager(manager));
    original(manager);
  }

  void TemporaryTransferFailedPopup_Hook(auto original, void* manager, void* callback)
  {
    RecordTransfer(TransferStage::TemporaryTransferFailedPopup, -1, -1.0F, {}, snapshot_manager(manager));
    original(manager, callback);
  }

  void TransferState_Hook(auto original, void* handler, int32_t state, float progress)
  {
    original(handler, state, progress);
    RecordTransfer(TransferStage::StateChanged, state, progress, snapshot_request_handler(handler));
  }

  bool TemporaryRequest_Hook(auto original, void* handler, uint32_t target_instance_id,
                             Il2CppString* temporary_reference, void** operation)
  {
    const auto result = original(handler, target_instance_id, temporary_reference, operation);
    RecordRequestResult(TransferStage::TemporaryRequestReturned, Direction::Rival, target_instance_id, result,
                        operation != nullptr && *operation != nullptr, snapshot_request_handler(handler));
    return result;
  }

  bool RecallRequest_Hook(auto original, void* handler, void** operation)
  {
    const auto result = original(handler, operation);
    RecordRequestResult(TransferStage::RecallRequestReturned, Direction::Home, 0, result,
                        operation != nullptr && *operation != nullptr, snapshot_request_handler(handler));
    return result;
  }

  bool PollRequest_Hook(auto original, void* handler, void** operation)
  {
    const auto result = original(handler, operation);
    RecordRequestResult(TransferStage::PollRequestReturned, CurrentDirection(), 0, result,
                        operation != nullptr && *operation != nullptr, snapshot_request_handler(handler));
    return result;
  }

  void RequestSuccess_Hook(auto original, void* handler, int32_t progress)
  {
    original(handler, progress);
    RecordTransfer(TransferStage::RequestSucceeded, -1, static_cast<float>(progress),
                   snapshot_request_handler(handler));
  }

  void RequestError_Hook(auto original, void* handler, void* error)
  {
    RecordError(ErrorStage::Request, snapshot_error(error), snapshot_request_handler(handler));
    original(handler, error);
  }

  void PollSuccess_Hook(auto original, void* handler, int32_t progress)
  {
    original(handler, progress);
    RecordTransfer(TransferStage::PollSucceeded, -1, static_cast<float>(progress), snapshot_request_handler(handler));
  }

  void PollError_Hook(auto original, void* handler, void* error)
  {
    RecordError(ErrorStage::Poll, snapshot_error(error), snapshot_request_handler(handler));
    original(handler, error);
  }

  void RelocateStarbase_Hook(auto original, void* manager, int64_t node_id, void* callbacks)
  {
    RecordRelocation(RelocationStage::RelocateStarbase, node_id);
    original(manager, node_id, callbacks);
  }

  void MoveStarbaseInstance_Hook(auto original, void* manager, int32_t instance_id, int64_t node_id, void* callbacks)
  {
    RecordRelocation(RelocationStage::MoveStarbaseToServerInstance, node_id, instance_id);
    original(manager, instance_id, node_id, callbacks);
  }

#define INSTALL_TRANSFER_DIAGNOSTIC_HOOK(registry, helper, descriptor, parameter_count, hook_fn)                       \
  do {                                                                                                                 \
    if (!(helper).isValidHelper()) {                                                                                   \
      (registry).record_missing_helper((descriptor));                                                                  \
    } else if (auto method = (helper).GetMethod((descriptor).target.method_name.data(), (parameter_count));            \
               method == nullptr) {                                                                                    \
      (registry).record_missing_method((descriptor));                                                                  \
    } else {                                                                                                           \
      HOOK_REGISTRY_SPUD_STATIC_DETOUR((registry), (descriptor), method, hook_fn);                                     \
    }                                                                                                                  \
  } while (false)
} // namespace

void InstallServerTransferDiagnosticHooks()
{
  HookModuleHealth hooks("ServerTransferDiagnostics");
  auto&            controller = server_clash_controller_helper();
  auto&            manager    = transfer_manager_helper();
  auto&            handler    = request_handler_helper();
  auto&            starbase   = starbase_manager_helper();

  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, controller, kTransferOutHook, 0, TransferOut_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, controller, kTransferHomeHook, 0, TransferHome_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kDispatchRivalHook, 3, DispatchRival_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kDispatchHomeHook, 2, DispatchHome_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kPollingStartedHook, 0, PollingStarted_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kPollingCompletedHook, 0, PollingCompleted_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kManagerPollingFailedHook, 1, ManagerPollingFailed_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kTransferFailedPopupHook, 0, TransferFailedPopup_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, manager, kTemporaryTransferFailedPopupHook, 1,
                                   TemporaryTransferFailedPopup_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kTemporaryRequestHook, 3, TemporaryRequest_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kRecallRequestHook, 1, RecallRequest_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kPollRequestHook, 1, PollRequest_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kTransferStateHook, 2, TransferState_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kRequestSuccessHook, 1, RequestSuccess_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kRequestErrorHook, 1, RequestError_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kPollSuccessHook, 1, PollSuccess_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, handler, kPollErrorHook, 1, PollError_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, starbase, kRelocateStarbaseHook, 2, RelocateStarbase_Hook);
  INSTALL_TRANSFER_DIAGNOSTIC_HOOK(hooks, starbase, kMoveStarbaseInstanceHook, 3, MoveStarbaseInstance_Hook);
  hooks.log_summary();
}
} // namespace server_transfer_diagnostics
