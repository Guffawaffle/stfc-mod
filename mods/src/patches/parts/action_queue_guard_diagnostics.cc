/**
 * @file action_queue_guard_diagnostics.cc
 * @brief Read-only diagnostics for the native action-queue stall watchdog.
 *
 * This module intentionally does not reuse the dormant Kir'shara repair hooks.
 * It observes the current native watchdog and completion callbacks so a later
 * target-specific repair can be designed from exact before/after evidence.
 */
#include "config.h"
#include "errormsg.h"
#include "patches/hook_registry.h"
#include "probe/probe.h"

#include <algorithm>
#include <cstdint>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/IList.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
namespace
{
constexpr HookDescriptor kActionQueueHandleStallHook = {
    "ActionQueueManager.HandleStall",
    "observe native action-queue watchdog recovery without mutating queue state",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "HandleStall"},
    "action-queue stall diagnostics lose the native watchdog postcondition",
    HookSupportTier::Science,
};

constexpr HookDescriptor kActionQueueOnFleetsDisposedHook = {
    "ActionQueueManager.OnFleetsDisposedEventHandler",
    "correlate disposed hostile identities with native queue transitions",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "OnFleetsDisposedEventHandler"},
    "action-queue diagnostics cannot distinguish disposed-target transitions",
    HookSupportTier::Science,
};

constexpr HookDescriptor kActionQueueOnStrikeCompleteHook = {
    "ActionQueueManager.OnStrikeCompleteEventHandler",
    "correlate local strike completion with native queue transitions",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "OnStrikeCompleteEventHandler"},
    "action-queue diagnostics cannot distinguish local strike completion",
    HookSupportTier::Science,
};

constexpr HookDescriptor kActionQueueProcessTargetHook = {
    "ActionQueueManager.ProcessQueue(Int64,bool)",
    "observe exact-target native removals and whether native code requests immediate advancement",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "ProcessQueue(Int64,bool)"},
    "action-queue diagnostics lose the authoritative exact-target removal seam",
    HookSupportTier::Science,
};

struct ActionQueueInstanceSnapshot {
  bool         present                  = false;
  std::int64_t player_fleet_id          = 0;
  int          count                    = -1;
  std::int64_t head_target_id           = 0;
  bool         is_engaging              = false;
  float        last_engage_attempt_time = 0.0f;
  std::int64_t last_engaged_target_id   = 0;
  std::int64_t pending_engage_target_id = 0;
};

IL2CppClassHelper& ActionQueueInstanceClass()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  return helper;
}

IL2CppClassHelper& QueueableActionClass()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  return helper;
}

std::int64_t QueueableActionTargetId(Il2CppObject* action)
{
  if (!action || !QueueableActionClass().isValidHelper()) {
    return 0;
  }

  static const auto offset = QueueableActionClass().GetField("<FleetId>k__BackingField").offset();
  return *reinterpret_cast<std::int64_t*>(reinterpret_cast<char*>(action) + offset);
}

ActionQueueInstanceSnapshot SnapshotActionQueueInstance(void* instance)
{
  ActionQueueInstanceSnapshot snapshot;
  if (!instance || !ActionQueueInstanceClass().isValidHelper()) {
    return snapshot;
  }

  static const auto is_engaging_offset    = ActionQueueInstanceClass().GetField("IsEngaging").offset();
  static const auto last_attempt_offset   = ActionQueueInstanceClass().GetField("LastEngageAttemptTime").offset();
  static const auto last_target_offset    = ActionQueueInstanceClass().GetField("LastEngagedTargetId").offset();
  static const auto pending_target_offset = ActionQueueInstanceClass().GetField("PendingEngageTargetId").offset();
  static const auto queue_offset          = ActionQueueInstanceClass().GetField("_actionQueue").offset();
  static const auto fleet_offset = ActionQueueInstanceClass().GetField("<PlayerFleetId>k__BackingField").offset();

  auto* bytes = reinterpret_cast<char*>(instance);
  auto* queue = *reinterpret_cast<IList**>(bytes + queue_offset);

  snapshot.present                  = true;
  snapshot.player_fleet_id          = *reinterpret_cast<std::int64_t*>(bytes + fleet_offset);
  snapshot.count                    = queue ? queue->Count : -1;
  snapshot.head_target_id           = queue && queue->Count > 0 ? QueueableActionTargetId(queue->Get(0)) : 0;
  snapshot.is_engaging              = *reinterpret_cast<bool*>(bytes + is_engaging_offset);
  snapshot.last_engage_attempt_time = *reinterpret_cast<float*>(bytes + last_attempt_offset);
  snapshot.last_engaged_target_id   = *reinterpret_cast<std::int64_t*>(bytes + last_target_offset);
  snapshot.pending_engage_target_id = *reinterpret_cast<std::int64_t*>(bytes + pending_target_offset);
  return snapshot;
}

std::string FormatActionQueueInstance(const ActionQueueInstanceSnapshot& snapshot)
{
  std::ostringstream out;
  out << "present=" << snapshot.present << " fleet=" << snapshot.player_fleet_id << " count=" << snapshot.count
      << " head=" << snapshot.head_target_id << " engaging=" << snapshot.is_engaging
      << " last=" << snapshot.last_engaged_target_id << " pending=" << snapshot.pending_engage_target_id
      << " last_attempt=" << snapshot.last_engage_attempt_time;
  return out.str();
}

std::string SnapshotAllActionQueues(ActionQueueManager* manager)
{
  if (!manager) {
    return "manager-unavailable";
  }

  static auto manager_class = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!manager_class.isValidHelper()) {
    return "manager-helper-unavailable";
  }

  static const auto battle_queue_offset = manager_class.GetField("_battleQueue").offset();
  auto* battle_queue = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(manager) + battle_queue_offset);
  if (!battle_queue) {
    return "battle-queue-unavailable";
  }

  auto*              array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  const auto         count = std::min<size_t>(array->max_length, 8);
  std::ostringstream out;
  out << "slots=" << array->max_length;
  for (size_t index = 0; index < count; ++index) {
    auto* instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    out << " slot" << index << "={" << FormatActionQueueInstance(SnapshotActionQueueInstance(instance)) << '}';
  }
  return out.str();
}

std::string SnapshotDisposedFleets(void* fleets)
{
  if (!fleets) {
    return "list-unavailable";
  }

  static auto fleet_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
  static const auto removal_reason_offset =
      fleet_class.isValidHelper() ? fleet_class.GetField("<RemovalReason>k__BackingField").offset() : 0;

  auto*              list  = static_cast<IList*>(fleets);
  const auto         count = std::min(std::max(list->Count, 0), 8);
  std::ostringstream out;
  out << "count=" << list->Count;
  for (int index = 0; index < count; ++index) {
    auto*      fleet          = reinterpret_cast<FleetDeployedData*>(list->Get(index));
    const auto removal_reason = fleet && removal_reason_offset
                                    ? *reinterpret_cast<int*>(reinterpret_cast<char*>(fleet) + removal_reason_offset)
                                    : -1;
    out << " item" << index << "={id=" << (fleet ? fleet->ID : 0) << " state=" << (fleet ? fleet->CurrentState : -1)
        << " prev=" << (fleet ? fleet->PreviousState : -1) << " destroyed=" << (fleet ? fleet->IsDestroyed : false)
        << " battling=" << (fleet ? fleet->CurrentlyBattling : false) << " removal_reason=" << removal_reason << '}';
  }
  return out.str();
}

struct StrikeSnapshot {
  std::int64_t attacker_fleet_id = 0;
  std::int64_t target_fleet_id   = 0;
  bool         target_destroyed  = false;
};

StrikeSnapshot SnapshotStrike(void* strike)
{
  StrikeSnapshot snapshot;
  if (!strike) {
    return snapshot;
  }

  static auto strike_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StrikeData");
  if (!strike_class.isValidHelper()) {
    return snapshot;
  }

  static const auto attacker_offset  = strike_class.GetField("<AttackerFleetId>k__BackingField").offset();
  static const auto target_offset    = strike_class.GetField("<TargetFleetId>k__BackingField").offset();
  static const auto destroyed_offset = strike_class.GetField("<TargetDestroyed>k__BackingField").offset();
  auto*             bytes            = reinterpret_cast<char*>(strike);
  snapshot.attacker_fleet_id         = *reinterpret_cast<std::int64_t*>(bytes + attacker_offset);
  snapshot.target_fleet_id           = *reinterpret_cast<std::int64_t*>(bytes + target_offset);
  snapshot.target_destroyed          = *reinterpret_cast<bool*>(bytes + destroyed_offset);
  return snapshot;
}

std::string SnapshotStrikeTargets(void* strike)
{
  if (!strike) {
    return "strike-unavailable";
  }

  static auto strike_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StrikeData");
  static auto target_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StrikeTarget");
  if (!strike_class.isValidHelper() || !target_class.isValidHelper()) {
    return "strike-helper-unavailable";
  }

  static const auto targets_offset   = strike_class.GetField("<Targets>k__BackingField").offset();
  static const auto fleet_offset     = target_class.GetField("<TargetFleetId>k__BackingField").offset();
  static const auto destroyed_offset = target_class.GetField("<TargetDestroyed>k__BackingField").offset();
  auto*             targets          = *reinterpret_cast<IList**>(reinterpret_cast<char*>(strike) + targets_offset);
  if (!targets) {
    return "list-unavailable";
  }

  const auto         count = std::min(std::max(targets->Count, 0), 8);
  std::ostringstream out;
  out << "count=" << targets->Count;
  for (int index = 0; index < count; ++index) {
    auto*      target = targets->Get(index);
    const auto target_id =
        target ? *reinterpret_cast<std::int64_t*>(reinterpret_cast<char*>(target) + fleet_offset) : 0;
    const auto destroyed =
        target ? *reinterpret_cast<bool*>(reinterpret_cast<char*>(target) + destroyed_offset) : false;
    out << " item" << index << "={id=" << target_id << " destroyed=" << destroyed << '}';
  }
  return out.str();
}

void ActionQueueManager_HandleStall_Diagnostics(auto original, ActionQueueManager* manager, void* instance,
                                                FleetPlayerData* player_fleet, FleetDeployedData* deployed_fleet)
{
  const auto before = SnapshotActionQueueInstance(instance);
  spdlog::info("[ActionQueueGuard] phase=before hook=HandleStall instance={} player_fleet={} deployed={} "
               "deployed_state={} deployed_prev={} deployed_destroyed={} deployed_battling={} queue={}",
               static_cast<void*>(instance), player_fleet ? player_fleet->Id : 0,
               deployed_fleet ? deployed_fleet->ID : 0, deployed_fleet ? deployed_fleet->CurrentState : -1,
               deployed_fleet ? deployed_fleet->PreviousState : -1,
               deployed_fleet ? deployed_fleet->IsDestroyed : false,
               deployed_fleet ? deployed_fleet->CurrentlyBattling : false, FormatActionQueueInstance(before));

  original(manager, instance, player_fleet, deployed_fleet);

  const auto after = SnapshotActionQueueInstance(instance);
  spdlog::info("[ActionQueueGuard] phase=after hook=HandleStall instance={} same_head={} count_delta={} queue={} "
               "all_queues={}",
               static_cast<void*>(instance),
               before.head_target_id != 0 && before.head_target_id == after.head_target_id,
               before.count >= 0 && after.count >= 0 ? after.count - before.count : 0, FormatActionQueueInstance(after),
               SnapshotAllActionQueues(manager));
}

void ActionQueueManager_OnFleetsDisposed_Diagnostics(auto original, ActionQueueManager* manager, void* fleets)
{
  const auto disposed = SnapshotDisposedFleets(fleets);
  spdlog::info("[ActionQueueGuard] phase=before hook=OnFleetsDisposed disposed={} queues={}", disposed,
               SnapshotAllActionQueues(manager));

  original(manager, fleets);

  spdlog::info("[ActionQueueGuard] phase=after hook=OnFleetsDisposed disposed={} queues={}", disposed,
               SnapshotAllActionQueues(manager));
}

void ActionQueueManager_OnStrikeComplete_Diagnostics(auto original, ActionQueueManager* manager, void* strike)
{
  const auto snapshot = SnapshotStrike(strike);
  spdlog::info("[ActionQueueGuard] phase=before hook=OnStrikeComplete attacker={} target={} target_destroyed={} "
               "targets={} queues={}",
               snapshot.attacker_fleet_id, snapshot.target_fleet_id, snapshot.target_destroyed,
               SnapshotStrikeTargets(strike), SnapshotAllActionQueues(manager));

  original(manager, strike);

  spdlog::info("[ActionQueueGuard] phase=after hook=OnStrikeComplete attacker={} target={} target_destroyed={} "
               "targets={} queues={}",
               snapshot.attacker_fleet_id, snapshot.target_fleet_id, snapshot.target_destroyed,
               SnapshotStrikeTargets(strike), SnapshotAllActionQueues(manager));
}

void ActionQueueManager_ProcessTarget_Diagnostics(auto original, ActionQueueManager* manager, std::int64_t target_id,
                                                  bool can_select_new_target)
{
  spdlog::info("[ActionQueueGuard] phase=before hook=ProcessQueue.target target={} can_select_new_target={} queues={}",
               target_id, can_select_new_target, SnapshotAllActionQueues(manager));

  original(manager, target_id, can_select_new_target);

  spdlog::info("[ActionQueueGuard] phase=after hook=ProcessQueue.target target={} can_select_new_target={} queues={}",
               target_id, can_select_new_target, SnapshotAllActionQueues(manager));
}
} // namespace

void InstallActionQueueGuardDiagnosticsHooks()
{
  HookModuleHealth hooks("ActionQueueGuardDiagnostics");
  static auto manager_class = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!manager_class.isValidHelper()) {
    hooks.record_missing_helper(kActionQueueHandleStallHook);
    hooks.record_missing_helper(kActionQueueOnFleetsDisposedHook);
    hooks.record_missing_helper(kActionQueueOnStrikeCompleteHook);
    hooks.record_missing_helper(kActionQueueProcessTargetHook);
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
    hooks.log_summary();
    return;
  }

  if (auto method = manager_class.GetMethod("HandleStall"); method) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueHandleStallHook, method,
                                     ActionQueueManager_HandleStall_Diagnostics);
  } else {
    hooks.record_missing_method(kActionQueueHandleStallHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "HandleStall");
  }

  if (auto method = manager_class.GetMethod("OnFleetsDisposedEventHandler"); method) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnFleetsDisposedHook, method,
                                     ActionQueueManager_OnFleetsDisposed_Diagnostics);
  } else {
    hooks.record_missing_method(kActionQueueOnFleetsDisposedHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetsDisposedEventHandler");
  }

  if (auto method = manager_class.GetMethod("OnStrikeCompleteEventHandler"); method) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnStrikeCompleteHook, method,
                                     ActionQueueManager_OnStrikeComplete_Diagnostics);
  } else {
    hooks.record_missing_method(kActionQueueOnStrikeCompleteHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "OnStrikeCompleteEventHandler");
  }

  auto process_target = manager_class.GetMethodSpecial("ProcessQueue", [](int param_count, const Il2CppType** params) {
    return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
  });
  if (process_target) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueProcessTargetHook, process_target,
                                     ActionQueueManager_ProcessTarget_Diagnostics);
  } else {
    hooks.record_missing_method(kActionQueueProcessTargetHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(Int64, bool)");
  }

  hooks.log_summary();
}
#endif
