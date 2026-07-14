#include "config.h"
#include "patches/client_ship_state_probe_policy.h"
#include "patches/hook_registry.h"
#include "patches/live_debug_event_dispatcher.h"
#include "prime/ActionData.h"
#include "prime/FleetPlayerData.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

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

bool RepairStatusChanged(uint64_t fleet_id, int32_t status)
{
  const std::scoped_lock lock(g_observedRepairStatusesMutex);
  return g_observedRepairStatuses.record(fleet_id, status);
}

int64_t MonotonicMicros()
{
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

int32_t FleetPlayerData_GetActionStatus_Hook(auto original, FleetPlayerData* fleet, ActionType action_type)
{
  const auto status = original(fleet, action_type);
  if (action_type != ActionType::Repair || fleet == nullptr) {
    return status;
  }

  const auto fleet_id = fleet->Id;
  if (!RepairStatusChanged(fleet_id, status)) {
    return status;
  }

  live_debug_events::RecordEvent("ship-state-probe.repair-action-status-transition",
                                 {{"probeVersion", 1},
                                  {"buildVersion", VER_RUNTIME_VERSION_STR},
                                  {"sequence", g_sequence.fetch_add(1, std::memory_order_relaxed) + 1},
                                  {"monotonicMicros", MonotonicMicros()},
                                  {"threadId", std::hash<std::thread::id>{}(std::this_thread::get_id())},
                                  {"seam", "FleetPlayerData.GetActionStatus(ActionType)"},
                                  {"phase", "transition"},
                                  {"fleetId", fleet_id},
                                  {"currentState", static_cast<int32_t>(fleet->CurrentState)},
                                  {"previousState", static_cast<int32_t>(fleet->PreviousState)},
                                  {"actionType", static_cast<int32_t>(action_type)},
                                  {"actionStatus", status},
                                  {"originalReturn", status}});

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
