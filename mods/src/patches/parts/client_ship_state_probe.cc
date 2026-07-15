#include "config.h"
#include "patches/client_ship_state_probe_policy.h"
#include "patches/hook_registry.h"
#include "patches/live_debug_event_dispatcher.h"
#include "prime/ActionData.h"
#include "prime/FleetPlayerData.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>

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
    "Observe distinct repair action-status results without changing game behavior.",
    {"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData", "GetActionStatus"},
    "Repair action-status transitions will not be available to the bounded live-query event ring.",
    HookSupportTier::Science};

ship_state_probe::RepairStatusTransitionCache g_observedRepairStatuses;
std::mutex                                    g_observedRepairStatusesMutex;
std::atomic<uint64_t>                         g_sequence{0};
std::atomic<int>                              g_stackBudget{0};

ship_state_probe::RepairStatusTransition ObserveRepairStatus(uint64_t fleet_id, int32_t status)
{
  const std::scoped_lock lock(g_observedRepairStatusesMutex);
  return g_observedRepairStatuses.observe(fleet_id, status);
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
  if (!transition.changed) {
    return status;
  }

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
                                  {"originalReturn", status}});

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

  return status;
}
} // namespace

void InstallClientShipStateProbeHooks()
{
  HookModuleHealth hooks("ClientShipStateProbe");
  if (!RepairActionStatusProbeEnabled()) {
    hooks.record_skipped(kRepairActionStatusHook, "repair_action_status mode and live_query are not both enabled");
    hooks.log_summary();
    return;
  }

  g_stackBudget.store(RepairActionStatusProbeStackBudget(), std::memory_order_relaxed);

  static auto helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
  if (!helper.isValidHelper()) {
    hooks.record_missing_helper(kRepairActionStatusHook);
  } else if (auto method = helper.GetMethod("GetActionStatus", 1); method == nullptr) {
    hooks.record_missing_method(kRepairActionStatusHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairActionStatusHook, method, FleetPlayerData_GetActionStatus_Hook);
  }
  hooks.log_summary();
}
