#include "config.h"
#include "patches/client_ship_state_probe_policy.h"
#include "patches/hook_registry.h"
#include "patches/live_debug_event_dispatcher.h"
#include "prime/ActionData.h"
#include "prime/FleetPlayerData.h"
#include "prime/GenericButtonContext.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#if _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
constexpr HookDescriptor kRepairActionStatusHook{
    "FleetPlayerData.GetActionStatus(ActionType)",
    "Observe repair action-status transitions and optionally project a coherent in-progress status.",
    {"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData", "GetActionStatus"},
    "Repair action-status transitions and the explicit Ready-while-Repairing guard will be unavailable.",
    HookSupportTier::Science};

constexpr HookDescriptor kRepairInstantButtonContextHook{
    "ActionElementWidget.GetInstantButtonContext()",
    "Observe the final Repair Instant-button context without changing the returned context or widget behavior.",
    {"Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget", "GetInstantButtonContext"},
    "Final Repair Instant-button context transitions will be unavailable.",
    HookSupportTier::Science};

constexpr HookDescriptor kRepairActionButtonClickHook{
    "ActionElementWidget.OnActionButtonClickCallback()",
    "Record which Repair action button the human clicked before game handling begins.",
    {"Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget", "OnActionButtonClickCallback"},
    "Human clicks on the Repair action button will not be correlated with repair state.",
    HookSupportTier::Science};

constexpr HookDescriptor kRepairInstantButtonClickHook{
    "ActionElementWidget.OnInstantButtonClickCallback()",
    "Record Repair instant clicks and suppress only the proven stale post-completion race.",
    {"Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget", "OnInstantButtonClickCallback"},
    "Human clicks on the Repair instant button will not be correlated with repair state.",
    HookSupportTier::Science};

constexpr HookDescriptor kRepairHelpRequestHook{
    "JobService.RequestHelpJob(IJob, CallbackContainer<string>)",
    "Record downstream help-request dispatches so Repair clicks can be correlated with actual requests.",
    {"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService", "RequestHelpJob"},
    "Repair Ask-for-Help clicks cannot be correlated with downstream help requests.",
    HookSupportTier::Science};

struct ActionElementWidget;

ship_state_probe::RepairStatusTransitionCache         g_observedRepairStatuses;
std::mutex                                            g_observedRepairStatusesMutex;
ship_state_probe::RepairInstantContextTransitionCache g_observedRepairInstantContexts;
std::mutex                                            g_observedRepairInstantContextsMutex;
ship_state_probe::RepairStatusSnapshotCache           g_repairStatusSnapshots;
std::mutex                                            g_repairStatusSnapshotsMutex;
ship_state_probe::RepairCoherentStatusHoldCache       g_repairCoherentStatusHold;
std::mutex                                            g_repairCoherentStatusHoldMutex;
std::atomic<uint64_t>                                 g_sequence{0};
std::atomic<int>                                      g_stackBudget{0};
std::atomic<bool>                                     g_guardEnabled{false};
std::atomic<bool>                                     g_holdEnabled{false};

ship_state_probe::RepairStatusTransition ObserveRepairStatus(uint64_t fleet_id, int32_t status)
{
  const std::scoped_lock lock(g_observedRepairStatusesMutex);
  return g_observedRepairStatuses.observe(fleet_id, status);
}

bool ObserveRepairInstantContext(uint64_t fleet_id, const ship_state_probe::RepairInstantContextSnapshot& snapshot)
{
  const std::scoped_lock lock(g_observedRepairInstantContextsMutex);
  return g_observedRepairInstantContexts.record(fleet_id, snapshot);
}

bool GetRepairInstantContext(uint64_t fleet_id, ship_state_probe::RepairInstantContextSnapshot& snapshot)
{
  const std::scoped_lock lock(g_observedRepairInstantContextsMutex);
  return g_observedRepairInstantContexts.get(fleet_id, snapshot);
}

void RecordRepairStatusSnapshot(uint64_t fleet_id, int32_t original_status, int32_t returned_status)
{
  const std::scoped_lock lock(g_repairStatusSnapshotsMutex);
  g_repairStatusSnapshots.record(fleet_id, {original_status, returned_status});
}

bool GetRepairStatusSnapshot(uint64_t fleet_id, ship_state_probe::RepairStatusSnapshot& snapshot)
{
  const std::scoped_lock lock(g_repairStatusSnapshotsMutex);
  return g_repairStatusSnapshots.get(fleet_id, snapshot);
}

int32_t ProjectCoherentRepairStatus(uint64_t fleet_id, int32_t original_status, int32_t current_fleet_state,
                                    int32_t previous_fleet_state)
{
  const std::scoped_lock lock(g_repairCoherentStatusHoldMutex);
  return g_repairCoherentStatusHold.project(fleet_id, original_status, current_fleet_state, previous_fleet_state);
}

IL2CppClassHelper& ActionElementWidgetHelper()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget");
  return helper;
}

ActionType ReadActionType(ActionElementWidget* widget)
{
  static auto property = ActionElementWidgetHelper().GetProperty("ActionType");
  const auto* value    = property.Get<ActionType>(widget);
  return value == nullptr ? ActionType::Invalid : *value;
}

FleetPlayerData* ReadFleetContext(ActionElementWidget* widget)
{
  static auto property = ActionElementWidgetHelper().GetProperty("Context");
  auto*       context  = property.GetRaw<void>(widget);
  if (context == nullptr) {
    return nullptr;
  }

  auto* klass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(context));
  if (klass == nullptr) {
    return nullptr;
  }

  const auto* class_name      = il2cpp_class_get_name(klass);
  const auto* class_namespace = il2cpp_class_get_namespace(klass);
  if (class_name == nullptr || class_namespace == nullptr || std::strcmp(class_name, "FleetPlayerData") != 0
      || std::strcmp(class_namespace, "Digit.PrimeServer.Models") != 0) {
    return nullptr;
  }
  return reinterpret_cast<FleetPlayerData*>(context);
}

bool ReadButtonBehaviours(ActionElementWidget* widget, std::string_view control, int32_t& behaviours)
{
  const auto* field_name = control == "action" ? "_actionBehaviours" : "_instantBehaviours";
  auto        field      = ActionElementWidgetHelper().GetField(field_name);
  if (!field.isValidHelper()) {
    return false;
  }

  behaviours = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(widget) + field.offset());
  return true;
}

std::string_view RepairStatusName(int32_t status)
{
  switch (status) {
    case 0:
      return "Disabled";
    case 100:
      return "Ready";
    case 200:
      return "InProgress";
    case 201:
      return "InProgress_Free";
    case 202:
      return "InProgress_AskForHelp";
    case 300:
      return "Complete";
    default:
      return "Unknown";
  }
}

bool ReadInstantAmount(GenericButtonContext* context, int64_t& amount)
{
  static auto button_helper     = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericButtonContext");
  static auto resource_property = button_helper.GetProperty("ResourceData");
  auto*       resource          = resource_property.GetRaw<void>(context);
  if (resource == nullptr) {
    return false;
  }

  static auto resource_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceData");
  static auto amount_property = resource_helper.GetProperty("Amount");
  const auto* value           = amount_property.Get<int64_t>(resource);
  if (value == nullptr) {
    return false;
  }
  amount = *value;
  return true;
}

int64_t MonotonicMicros()
{
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

std::string ModuleBasename(std::string_view path)
{
  if (path.empty()) {
    return "unknown";
  }
  const auto separator = path.find_last_of("\\/");
  return std::string(separator == std::string_view::npos ? path : path.substr(separator + 1));
}

nlohmann::json CaptureModuleRelativeStack()
{
  constexpr size_t              kMaxFrames = 32;
  std::array<void*, kMaxFrames> addresses{};
  nlohmann::json                frames = nlohmann::json::array();
#if _WIN32
  const auto count =
      static_cast<size_t>(CaptureStackBackTrace(0, static_cast<DWORD>(kMaxFrames), addresses.data(), nullptr));
  for (size_t index = 0; index < count; ++index) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addresses[index]), &module)
        || module == nullptr) {
      continue;
    }

    std::array<char, MAX_PATH> module_path{};
    const auto path_length = GetModuleFileNameA(module, module_path.data(), static_cast<DWORD>(module_path.size()));
    frames.push_back({{"index", index},
                      {"module", ModuleBasename({module_path.data(), path_length})},
                      {"offset", reinterpret_cast<uintptr_t>(addresses[index]) - reinterpret_cast<uintptr_t>(module)}});
  }
#else
  const auto captured = backtrace(addresses.data(), static_cast<int>(kMaxFrames));
  const auto count    = captured > 0 ? static_cast<size_t>(captured) : 0;
  for (size_t index = 0; index < count; ++index) {
    Dl_info info{};
    if (dladdr(addresses[index], &info) == 0 || info.dli_fbase == nullptr) {
      continue;
    }
    frames.push_back(
        {{"index", index},
         {"module", ModuleBasename(info.dli_fname == nullptr ? std::string_view{} : info.dli_fname)},
         {"offset", reinterpret_cast<uintptr_t>(addresses[index]) - reinterpret_cast<uintptr_t>(info.dli_fbase)}});
  }
#endif
  return frames;
}

bool ConsumeStackBudget()
{
  auto remaining = g_stackBudget.load(std::memory_order_relaxed);
  while (remaining > 0) {
    if (g_stackBudget.compare_exchange_weak(remaining, remaining - 1, std::memory_order_relaxed)) {
      return true;
    }
  }
  return false;
}

int32_t FleetPlayerData_GetActionStatus_Hook(auto original, FleetPlayerData* fleet, ActionType action_type)
{
  const auto status = original(fleet, action_type);
  if (action_type != ActionType::Repair || fleet == nullptr) {
    return status;
  }

  const auto fleet_id       = fleet->Id;
  const auto current_state  = fleet->CurrentState;
  const auto previous_state = fleet->PreviousState;
  const auto transition     = ObserveRepairStatus(fleet_id, status);
  const auto returned_status =
      g_holdEnabled.load(std::memory_order_relaxed)
          ? ProjectCoherentRepairStatus(fleet_id, status, static_cast<int32_t>(current_state),
                                        static_cast<int32_t>(previous_state))
          : ship_state_probe::project_repair_action_status(status, static_cast<int32_t>(current_state),
                                                           g_guardEnabled.load(std::memory_order_relaxed));
  RecordRepairStatusSnapshot(fleet_id, status, returned_status);

  if (transition.changed) {
    const auto transition_sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    live_debug_events::RecordEvent("ship-state-probe.repair-action-status-transition",
                                   {{"probeVersion", 1},
                                    {"buildVersion", VER_RUNTIME_VERSION_STR},
                                    {"sequence", transition_sequence},
                                    {"monotonicMicros", MonotonicMicros()},
                                    {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
                                    {"seam", "FleetPlayerData.GetActionStatus(ActionType)"},
                                    {"phase", "transition"},
                                    {"fleetId", fleet_id},
                                    {"currentState", static_cast<int32_t>(current_state)},
                                    {"previousState", static_cast<int32_t>(previous_state)},
                                    {"actionType", static_cast<int32_t>(action_type)},
                                    {"actionStatus", status},
                                    {"originalReturn", status},
                                    {"returnedStatus", returned_status},
                                    {"guardApplied", returned_status != status}});
    spdlog::debug(
        "[ShipStateProbe] seam=repair-action-status sequence={} fleet_id={} current_state={} previous_state={} "
        "original_status={} returned_status={} projection_applied={}",
        transition_sequence, fleet_id, static_cast<int32_t>(current_state), static_cast<int32_t>(previous_state),
        status, returned_status, returned_status != status);

    if (ship_state_probe::should_capture_ready_while_repairing_caller(transition, status,
                                                                      static_cast<int32_t>(current_state))
        && ConsumeStackBudget()) {
      auto frames = CaptureModuleRelativeStack();
      live_debug_events::RecordEvent("ship-state-probe.repair-action-status-caller-sample",
                                     {{"probeVersion", 1},
                                      {"buildVersion", VER_RUNTIME_VERSION_STR},
                                      {"triggerSequence", transition_sequence},
                                      {"monotonicMicros", MonotonicMicros()},
                                      {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
                                      {"seam", "FleetPlayerData.GetActionStatus(ActionType)"},
                                      {"trigger", "ReadyWhileRepairing"},
                                      {"fleetId", fleet_id},
                                      {"frameCount", frames.size()},
                                      {"frames", std::move(frames)}});
    }
  }

  return returned_status;
}

GenericButtonContext* ActionElementWidget_GetInstantButtonContext_Hook(auto original, ActionElementWidget* widget)
{
  auto* context = original(widget);
  if (widget == nullptr || ReadActionType(widget) != ActionType::Repair) {
    return context;
  }

  auto* fleet = ReadFleetContext(widget);
  if (fleet == nullptr) {
    return context;
  }

  ship_state_probe::RepairInstantContextSnapshot snapshot{};
  snapshot.current_fleet_state  = static_cast<int32_t>(fleet->CurrentState);
  snapshot.previous_fleet_state = static_cast<int32_t>(fleet->PreviousState);
  snapshot.context_present      = context != nullptr;
  if (context != nullptr) {
    snapshot.interactable = context->Interactable;
    snapshot.has_amount   = ReadInstantAmount(context, snapshot.amount);
  }

  const auto fleet_id = fleet->Id;
  if (!ObserveRepairInstantContext(fleet_id, snapshot)) {
    return context;
  }

  const auto transition_sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  live_debug_events::RecordEvent("ship-state-probe.repair-instant-button-context",
                                 {{"probeVersion", 1},
                                  {"buildVersion", VER_RUNTIME_VERSION_STR},
                                  {"sequence", transition_sequence},
                                  {"monotonicMicros", MonotonicMicros()},
                                  {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
                                  {"seam", "ActionElementWidget.GetInstantButtonContext()"},
                                  {"phase", "post"},
                                  {"fleetId", fleet_id},
                                  {"currentState", snapshot.current_fleet_state},
                                  {"previousState", snapshot.previous_fleet_state},
                                  {"actionType", static_cast<int32_t>(ActionType::Repair)},
                                  {"contextPresent", snapshot.context_present},
                                  {"interactable", snapshot.interactable},
                                  {"hasAmount", snapshot.has_amount},
                                  {"amount", snapshot.amount}});
  spdlog::debug("[ShipStateProbe] seam=repair-instant-button-context sequence={} fleet_id={} current_state={} "
                "previous_state={} context_present={} interactable={} has_amount={} amount={}",
                transition_sequence, fleet_id, snapshot.current_fleet_state, snapshot.previous_fleet_state,
                snapshot.context_present, snapshot.interactable, snapshot.has_amount, snapshot.amount);

  return context;
}

bool RecordRepairHumanClick(ActionElementWidget* widget, std::string_view control)
{
  if (widget == nullptr || ReadActionType(widget) != ActionType::Repair) {
    return false;
  }

  auto*      fleet          = ReadFleetContext(widget);
  const auto has_fleet      = fleet != nullptr;
  const auto fleet_id       = has_fleet ? fleet->Id : 0;
  const auto current_state  = has_fleet ? static_cast<int32_t>(fleet->CurrentState) : 0;
  const auto previous_state = has_fleet ? static_cast<int32_t>(fleet->PreviousState) : 0;

  ship_state_probe::RepairStatusSnapshot status_snapshot{};
  const auto has_status_snapshot = has_fleet && GetRepairStatusSnapshot(fleet_id, status_snapshot);

  ship_state_probe::RepairInstantContextSnapshot instant_snapshot{};
  const auto has_instant_snapshot = has_fleet && GetRepairInstantContext(fleet_id, instant_snapshot);

  int32_t    behaviours     = 0;
  const auto has_behaviours = ReadButtonBehaviours(widget, control, behaviours);
  const auto is_ask_help    = has_behaviours && (behaviours & static_cast<int32_t>(ActionBehaviour::AskHelp)) != 0;
  const auto is_speedup     = has_behaviours && (behaviours & static_cast<int32_t>(ActionBehaviour::Speedup)) != 0;
  const auto is_instant     = has_behaviours && (behaviours & static_cast<int32_t>(ActionBehaviour::Instant)) != 0;
  const auto suppress_click = control == "instant" && has_status_snapshot && has_instant_snapshot
                              && ship_state_probe::should_suppress_stale_instant_click_after_repair(
                                  current_state, previous_state, status_snapshot.original_status, instant_snapshot);

  const auto click_sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  live_debug_events::RecordEvent(
      "ship-state-probe.repair-human-click",
      {{"probeVersion", 1},
       {"buildVersion", VER_RUNTIME_VERSION_STR},
       {"sequence", click_sequence},
       {"monotonicMicros", MonotonicMicros()},
       {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
       {"seam", control == "action" ? "ActionElementWidget.OnActionButtonClickCallback()"
                                    : "ActionElementWidget.OnInstantButtonClickCallback()"},
       {"phase", "pre"},
       {"clickedControl", std::string(control)},
       {"hasFleetContext", has_fleet},
       {"fleetId", fleet_id},
       {"currentState", current_state},
       {"previousState", previous_state},
       {"actionType", static_cast<int32_t>(ActionType::Repair)},
       {"hasButtonBehaviours", has_behaviours},
       {"buttonBehaviours", behaviours},
       {"askHelpBehaviour", is_ask_help},
       {"speedupBehaviour", is_speedup},
       {"instantBehaviour", is_instant},
       {"suppressed", suppress_click},
       {"hasStatusSnapshot", has_status_snapshot},
       {"lastOriginalStatus", status_snapshot.original_status},
       {"lastReturnedStatus", status_snapshot.returned_status},
       {"lastReturnedStatusName", std::string(RepairStatusName(status_snapshot.returned_status))},
       {"hasInstantContextSnapshot", has_instant_snapshot},
       {"instantContextPresent", instant_snapshot.context_present},
       {"instantInteractable", instant_snapshot.interactable},
       {"instantHasAmount", instant_snapshot.has_amount},
       {"instantAmount", instant_snapshot.amount}});
  spdlog::info("[ShipStateProbe] seam=repair-human-click sequence={} control={} fleet_id={} current_state={} "
               "previous_state={} behaviours={} ask_help={} speedup={} instant={} original_status={} "
               "returned_status={} status_name={} instant_context_present={} instant_interactable={} "
               "instant_has_amount={} instant_amount={} suppressed={}",
               click_sequence, control, fleet_id, current_state, previous_state, behaviours, is_ask_help, is_speedup,
               is_instant, status_snapshot.original_status, status_snapshot.returned_status,
               RepairStatusName(status_snapshot.returned_status), instant_snapshot.context_present,
               instant_snapshot.interactable, instant_snapshot.has_amount, instant_snapshot.amount, suppress_click);
  return suppress_click;
}

void ActionElementWidget_OnActionButtonClickCallback_Hook(auto original, ActionElementWidget* widget)
{
  RecordRepairHumanClick(widget, "action");
  original(widget);
}

void ActionElementWidget_OnInstantButtonClickCallback_Hook(auto original, ActionElementWidget* widget)
{
  if (!RecordRepairHumanClick(widget, "instant")) {
    original(widget);
  }
}

struct RepairHelpRequestSnapshot {
  int32_t job_id              = 0;
  int64_t target_id           = 0;
  bool    has_job_id          = false;
  bool    has_target_id       = false;
  bool    has_repair_job_flag = false;
  bool    is_repair_fleet_job = false;
  bool    has_help_requested  = false;
  bool    is_help_requested   = false;
};

RepairHelpRequestSnapshot ReadRepairHelpRequestSnapshot(void* job)
{
  RepairHelpRequestSnapshot snapshot{};
  if (job == nullptr) {
    return snapshot;
  }

  static auto helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IJob");
  if (!helper.isValidHelper()) {
    return snapshot;
  }

  static auto id_property             = helper.GetProperty("Id");
  static auto target_id_property      = helper.GetProperty("TargetId");
  static auto repair_job_property     = helper.GetProperty("IsRepairFleetJob");
  static auto help_requested_property = helper.GetProperty("IsHelpRequested");
  if (const auto* value = id_property.Get<int32_t>(job); value != nullptr) {
    snapshot.job_id     = *value;
    snapshot.has_job_id = true;
  }
  if (const auto* value = target_id_property.Get<int64_t>(job); value != nullptr) {
    snapshot.target_id     = *value;
    snapshot.has_target_id = true;
  }
  if (const auto* value = repair_job_property.Get<bool>(job); value != nullptr) {
    snapshot.is_repair_fleet_job = *value;
    snapshot.has_repair_job_flag = true;
  }
  if (const auto* value = help_requested_property.Get<bool>(job); value != nullptr) {
    snapshot.is_help_requested  = *value;
    snapshot.has_help_requested = true;
  }
  return snapshot;
}

void JobService_RequestHelpJob_Hook(auto original, void* service, void* job, void* callbacks)
{
  const auto snapshot         = ReadRepairHelpRequestSnapshot(job);
  const auto request_sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  live_debug_events::RecordEvent("ship-state-probe.job-help-request",
                                 {{"probeVersion", 1},
                                  {"buildVersion", VER_RUNTIME_VERSION_STR},
                                  {"sequence", request_sequence},
                                  {"monotonicMicros", MonotonicMicros()},
                                  {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
                                  {"seam", "JobService.RequestHelpJob(IJob, CallbackContainer<string>)"},
                                  {"phase", "pre"},
                                  {"jobPresent", job != nullptr},
                                  {"hasJobId", snapshot.has_job_id},
                                  {"jobId", snapshot.job_id},
                                  {"hasTargetId", snapshot.has_target_id},
                                  {"targetId", snapshot.target_id},
                                  {"hasRepairFleetJobFlag", snapshot.has_repair_job_flag},
                                  {"isRepairFleetJob", snapshot.is_repair_fleet_job},
                                  {"hasHelpRequested", snapshot.has_help_requested},
                                  {"isHelpRequestedBefore", snapshot.is_help_requested}});
  spdlog::info("[ShipStateProbe] seam=job-help-request sequence={} job_present={} has_job_id={} job_id={} "
               "has_target_id={} target_id={} has_repair_flag={} is_repair_job={} has_help_requested={} "
               "help_requested_before={}",
               request_sequence, job != nullptr, snapshot.has_job_id, snapshot.job_id, snapshot.has_target_id,
               snapshot.target_id, snapshot.has_repair_job_flag, snapshot.is_repair_fleet_job,
               snapshot.has_help_requested, snapshot.is_help_requested);
  original(service, job, callbacks);
}
} // namespace

void InstallClientShipStateProbeHooks()
{
  HookModuleHealth hooks("ClientShipStateProbe");
  if (!ClientShipStateProbeEnabled()) {
    hooks.record_skipped(kRepairActionStatusHook, "a ship-state science mode and live_query are not both enabled");
    hooks.record_skipped(kRepairInstantButtonContextHook,
                         "a ship-state science mode and live_query are not both enabled");
    hooks.record_skipped(kRepairActionButtonClickHook,
                         "the coherent repair hold trace and live_query are not both enabled");
    hooks.record_skipped(kRepairInstantButtonClickHook,
                         "the coherent repair hold trace and live_query are not both enabled");
    hooks.record_skipped(kRepairHelpRequestHook, "the coherent repair hold trace and live_query are not both enabled");
    hooks.log_summary();
    return;
  }

  g_stackBudget.store(RepairActionStatusProbeStackBudget(), std::memory_order_relaxed);
  g_guardEnabled.store(RepairReadyWhileRepairingGuardEnabled(), std::memory_order_relaxed);
  g_holdEnabled.store(RepairCoherentActionStatusHoldEnabled(), std::memory_order_relaxed);

  if (RepairActionStatusProbeEnabled()) {
    static auto helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
    if (!helper.isValidHelper()) {
      hooks.record_missing_helper(kRepairActionStatusHook);
    } else if (auto method = helper.GetMethod("GetActionStatus", 1); method == nullptr) {
      hooks.record_missing_method(kRepairActionStatusHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairActionStatusHook, method, FleetPlayerData_GetActionStatus_Hook);
    }
  }

  if (RepairInstantButtonContextProbeEnabled()) {
    auto& helper = ActionElementWidgetHelper();
    if (!helper.isValidHelper()) {
      hooks.record_missing_helper(kRepairInstantButtonContextHook);
    } else if (auto method = helper.GetMethod("GetInstantButtonContext", 0); method == nullptr) {
      hooks.record_missing_method(kRepairInstantButtonContextHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairInstantButtonContextHook, method,
                                       ActionElementWidget_GetInstantButtonContext_Hook);
    }
  }

  if (RepairCoherentActionStatusHoldEnabled()) {
    auto& helper = ActionElementWidgetHelper();
    if (!helper.isValidHelper()) {
      hooks.record_missing_helper(kRepairActionButtonClickHook);
      hooks.record_missing_helper(kRepairInstantButtonClickHook);
    } else {
      if (auto method = helper.GetMethod("OnActionButtonClickCallback", 0); method == nullptr) {
        hooks.record_missing_method(kRepairActionButtonClickHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairActionButtonClickHook, method,
                                         ActionElementWidget_OnActionButtonClickCallback_Hook);
      }

      if (auto method = helper.GetMethod("OnInstantButtonClickCallback", 0); method == nullptr) {
        hooks.record_missing_method(kRepairInstantButtonClickHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairInstantButtonClickHook, method,
                                         ActionElementWidget_OnInstantButtonClickCallback_Hook);
      }
    }

    static auto job_service_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
    if (!job_service_helper.isValidHelper()) {
      hooks.record_missing_helper(kRepairHelpRequestHook);
    } else {
      const auto* method = job_service_helper.GetMethodInfoSpecial(
          "RequestHelpJob", [](int parameter_count, const Il2CppType** parameters) {
            if (parameter_count != 2 || parameters == nullptr || parameters[0] == nullptr) {
              return false;
            }
            auto* parameter_class = il2cpp_class_from_type(parameters[0]);
            if (parameter_class == nullptr) {
              return false;
            }
            const auto* class_name      = il2cpp_class_get_name(parameter_class);
            const auto* class_namespace = il2cpp_class_get_namespace(parameter_class);
            return class_name != nullptr && class_namespace != nullptr && std::strcmp(class_name, "IJob") == 0
                   && std::strcmp(class_namespace, "Digit.PrimeServer.Models") == 0;
          });
      if (method == nullptr || method->methodPointer == nullptr) {
        hooks.record_missing_method(kRepairHelpRequestHook);
      } else {
        HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairHelpRequestHook, method->methodPointer,
                                         JobService_RequestHelpJob_Hook);
      }
    }
  } else {
    hooks.record_skipped(kRepairActionButtonClickHook, "the coherent repair hold trace is not selected");
    hooks.record_skipped(kRepairInstantButtonClickHook, "the coherent repair hold trace is not selected");
    hooks.record_skipped(kRepairHelpRequestHook, "the coherent repair hold trace is not selected");
  }
  hooks.log_summary();
}
