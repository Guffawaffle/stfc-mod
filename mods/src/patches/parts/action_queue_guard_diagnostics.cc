/**
 * @file action_queue_guard_diagnostics.cc
 * @brief Thin action-queue protection plus diagnostics for the native watchdog.
 *
 * This module intentionally does not reuse the dormant Kir'shara repair hooks.
 * Repairs are feature-gated, target-specific, and implemented through native
 * queue methods. Detailed diagnostics remain independently trace-gated.
 */
#include "config.h"
#include "errormsg.h"
#include "patches/action_queue_guard_policy.h"
#include "patches/hook_registry.h"
#include "probe/probe.h"
#include "str_utils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/IList.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

namespace
{
constexpr HookDescriptor kActionQueueHandleStallHook = {
    "ActionQueueManager.HandleStall",
    "resume an exact surviving queue suffix when native pruning leaves its new head idle",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "HandleStall"},
    "multi-target native pruning can leave the surviving queue head idle",
    HookSupportTier::Production,
};

constexpr HookDescriptor kActionQueueOnFleetsDisposedHook = {
    "ActionQueueManager.OnFleetsDisposedEventHandler",
    "process an authoritative destroyed target that native disposal leaves as the exact queue head",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "OnFleetsDisposedEventHandler"},
    "externally destroyed hostile can remain at the queue head until watchdog recovery",
    HookSupportTier::Production,
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

constexpr HookDescriptor kActionQueueDoPlanAndEngageHook = {
    "ActionQueueManager.DoPlanPathAndEngageTarget",
    "resume an exact surviving queue suffix immediately when native planning prunes its old head",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "DoPlanPathAndEngageTarget"},
    "action-queue diagnostics cannot identify the native plan/engage boundary",
    HookSupportTier::Production,
};

constexpr HookDescriptor kActionQueueOnFleetStateChangeHook = {
    "ActionQueueManager.OnFleetStateChangeEventHandler",
    "correlate deployed-fleet state transitions with active action queues",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "OnFleetStateChangeEventHandler"},
    "action-queue diagnostics cannot locate the deployed-state handoff preceding queue advancement",
    HookSupportTier::Science,
};

constexpr HookDescriptor kActionQueueOnPlayerFleetStateChangedHook = {
    "ActionQueueManager.OnPlayerFleetStateChangedEventHandler",
    "correlate player-fleet state transitions with active action queues",
    {"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager", "OnPlayerFleetStateChangedEventHandler"},
    "action-queue diagnostics cannot locate the player-state handoff preceding queue advancement",
    HookSupportTier::Science,
};

struct ActionQueueInstanceSnapshot {
  bool                        present         = false;
  std::int64_t                player_fleet_id = 0;
  int                         count           = -1;
  std::int64_t                head_target_id  = 0;
  std::array<std::int64_t, 8> target_ids{};
  int                         captured_target_count    = 0;
  bool                        is_engaging              = false;
  float                       last_engage_attempt_time = 0.0f;
  std::int64_t                last_engaged_target_id   = 0;
  std::int64_t                pending_engage_target_id = 0;
};

struct PlayerFleetIdentitySnapshot {
  std::int64_t fleet_id = 0;
  long         hull_id  = 0;
  std::string  hull_name;
  int          hull_type      = -1;
  int          drydock_id     = -1;
  int          fleet_index    = -1;
  int          ui_order_index = -1;
};

using ProcessQueueTargetMethod         = void(ActionQueueManager*, std::int64_t, bool);
using TryPlanPathAndEngageTargetMethod = int(ActionQueueManager*, FleetPlayerData*, void*);

bool DiagnosticsEnabled()
{ return AdvancedDiagnosticsSettings().action_queue_guard_logging; }

bool ProtectionEnabled()
{ return AdvancedQueueSettings().thin_queue_protection; }

IL2CppClassHelper& ActionQueueInstanceClass()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  return helper;
}

IL2CppClassHelper& ActionQueueManagerClass()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
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

  snapshot.present         = true;
  snapshot.player_fleet_id = *reinterpret_cast<std::int64_t*>(bytes + fleet_offset);
  snapshot.count           = queue ? queue->Count : -1;
  snapshot.captured_target_count =
      queue ? std::min(std::max(queue->Count, 0), static_cast<int>(snapshot.target_ids.size())) : 0;
  for (int index = 0; index < snapshot.captured_target_count; ++index) {
    snapshot.target_ids[index] = QueueableActionTargetId(queue->Get(index));
  }
  snapshot.head_target_id           = snapshot.captured_target_count > 0 ? snapshot.target_ids[0] : 0;
  snapshot.is_engaging              = *reinterpret_cast<bool*>(bytes + is_engaging_offset);
  snapshot.last_engage_attempt_time = *reinterpret_cast<float*>(bytes + last_attempt_offset);
  snapshot.last_engaged_target_id   = *reinterpret_cast<std::int64_t*>(bytes + last_target_offset);
  snapshot.pending_engage_target_id = *reinterpret_cast<std::int64_t*>(bytes + pending_target_offset);
  return snapshot;
}

action_queue_guard::QueueState ToPolicyState(const ActionQueueInstanceSnapshot& snapshot)
{
  return {
      .present                = snapshot.present,
      .player_fleet_id        = snapshot.player_fleet_id,
      .count                  = snapshot.count,
      .head_target_id         = snapshot.head_target_id,
      .target_ids             = snapshot.target_ids,
      .captured_target_count  = snapshot.captured_target_count,
      .targets_truncated      = snapshot.count > snapshot.captured_target_count,
      .is_engaging            = snapshot.is_engaging,
      .last_engaged_target_id = snapshot.last_engaged_target_id,
      .pending_target_id      = snapshot.pending_engage_target_id,
  };
}

std::string FormatActionQueueInstance(const ActionQueueInstanceSnapshot& snapshot)
{
  std::ostringstream out;
  out << "present=" << snapshot.present << " fleet=" << snapshot.player_fleet_id << " count=" << snapshot.count
      << " head=" << snapshot.head_target_id << " targets=[";
  for (int index = 0; index < snapshot.captured_target_count; ++index) {
    if (index > 0) {
      out << ',';
    }
    out << snapshot.target_ids[index];
  }
  out << "] targets_truncated=" << (snapshot.count > snapshot.captured_target_count)
      << " engaging=" << snapshot.is_engaging << " last=" << snapshot.last_engaged_target_id
      << " pending=" << snapshot.pending_engage_target_id << " last_attempt=" << snapshot.last_engage_attempt_time;
  return out.str();
}

PlayerFleetIdentitySnapshot SnapshotPlayerFleetIdentity(FleetPlayerData* fleet)
{
  PlayerFleetIdentitySnapshot snapshot;
  if (!fleet) {
    return snapshot;
  }

  snapshot.fleet_id = fleet->Id;
  if (auto* hull = fleet->Hull; hull) {
    snapshot.hull_id   = hull->Id;
    snapshot.hull_type = static_cast<int>(hull->Type);
    if (auto* name = hull->Name; name) {
      snapshot.hull_name = to_string(name);
    }
  }

  static auto fleet_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
  if (fleet_class.isValidHelper()) {
    static const auto drydock_offset  = fleet_class.GetField("<DrydockID>k__BackingField").offset();
    static const auto index_offset    = fleet_class.GetField("<Index>k__BackingField").offset();
    static const auto ui_order_offset = fleet_class.GetField("<UiOrderIndex>k__BackingField").offset();
    auto*             bytes           = reinterpret_cast<char*>(fleet);
    snapshot.drydock_id               = *reinterpret_cast<int*>(bytes + drydock_offset);
    snapshot.fleet_index              = *reinterpret_cast<int*>(bytes + index_offset);
    snapshot.ui_order_index           = *reinterpret_cast<int*>(bytes + ui_order_offset);
  }
  return snapshot;
}

std::string FormatPlayerFleetIdentity(const PlayerFleetIdentitySnapshot& snapshot)
{
  std::ostringstream out;
  out << "fleet=" << snapshot.fleet_id << " hull_id=" << snapshot.hull_id << " hull_name='" << snapshot.hull_name
      << "' hull_type=" << snapshot.hull_type << " drydock=" << snapshot.drydock_id << " index=" << snapshot.fleet_index
      << " ui_order=" << snapshot.ui_order_index;
  return out.str();
}

Il2CppArray* BattleQueueArray(ActionQueueManager* manager)
{
  if (!manager || !ActionQueueManagerClass().isValidHelper()) {
    return nullptr;
  }

  static const auto battle_queue_offset = ActionQueueManagerClass().GetField("_battleQueue").offset();
  return *reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(manager) + battle_queue_offset);
}

ActionQueueInstanceSnapshot FindQueueWithExactHead(ActionQueueManager* manager, std::int64_t target_id)
{
  auto* battle_queue = BattleQueueArray(manager);
  if (!battle_queue || target_id == 0) {
    return {};
  }

  auto*      array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  const auto count = std::min<size_t>(array->max_length, 8);
  for (size_t index = 0; index < count; ++index) {
    auto* instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    auto  snapshot = SnapshotActionQueueInstance(instance);
    if (snapshot.head_target_id == target_id) {
      return snapshot;
    }
  }
  return {};
}

void* FindQueueInstanceForPlayerFleet(ActionQueueManager* manager, std::int64_t player_fleet_id)
{
  auto* battle_queue = BattleQueueArray(manager);
  if (!battle_queue || player_fleet_id == 0) {
    return nullptr;
  }

  auto*      array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  const auto count = std::min<size_t>(array->max_length, 8);
  for (size_t index = 0; index < count; ++index) {
    auto* instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    if (SnapshotActionQueueInstance(instance).player_fleet_id == player_fleet_id) {
      return instance;
    }
  }
  return nullptr;
}

ProcessQueueTargetMethod* ResolveProcessQueueTarget()
{
  if (!ActionQueueManagerClass().isValidHelper()) {
    return nullptr;
  }

  static auto method = ActionQueueManagerClass().GetMethodSpecial<ProcessQueueTargetMethod>(
      "ProcessQueue", [](int param_count, const Il2CppType** params) {
        return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
      });
  return method;
}

TryPlanPathAndEngageTargetMethod* ResolveTryPlanPathAndEngageTarget()
{
  if (!ActionQueueManagerClass().isValidHelper()) {
    return nullptr;
  }

  static auto method =
      ActionQueueManagerClass().GetMethod<TryPlanPathAndEngageTargetMethod>("TryPlanPathAndEngageTarget");
  return method;
}

std::string SnapshotAllActionQueues(ActionQueueManager* manager)
{
  auto* battle_queue = BattleQueueArray(manager);
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

bool HasActiveQueueForFleet(ActionQueueManager* manager, std::int64_t player_fleet_id)
{
  auto* battle_queue = BattleQueueArray(manager);
  if (!battle_queue || player_fleet_id == 0) {
    return false;
  }

  auto*      array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  const auto count = std::min<size_t>(array->max_length, 8);
  for (size_t index = 0; index < count; ++index) {
    auto* instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    auto  snapshot = SnapshotActionQueueInstance(instance);
    if (snapshot.present && snapshot.player_fleet_id == player_fleet_id && snapshot.count > 0) {
      return true;
    }
  }
  return false;
}

bool DeployedFleetListTouchesActiveQueue(ActionQueueManager* manager, void* fleets)
{
  if (!fleets) {
    return false;
  }

  auto*      list  = static_cast<IList*>(fleets);
  const auto count = std::max(list->Count, 0);
  for (int index = 0; index < count; ++index) {
    auto* fleet = reinterpret_cast<FleetDeployedData*>(list->Get(index));
    if (fleet && HasActiveQueueForFleet(manager, fleet->ID)) {
      return true;
    }
  }
  return false;
}

bool PlayerFleetListTouchesActiveQueue(ActionQueueManager* manager, void* fleets)
{
  if (!fleets) {
    return false;
  }

  auto*      list  = static_cast<IList*>(fleets);
  const auto count = std::max(list->Count, 0);
  for (int index = 0; index < count; ++index) {
    auto* fleet = reinterpret_cast<FleetPlayerData*>(list->Get(index));
    if (fleet && HasActiveQueueForFleet(manager, static_cast<std::int64_t>(fleet->Id))) {
      return true;
    }
  }
  return false;
}

struct DestroyedTargetIds {
  std::array<std::int64_t, 8> values{};
  int                         count = 0;
};

DestroyedTargetIds SnapshotDestroyedTargetIds(void* fleets)
{
  DestroyedTargetIds destroyed;
  if (!fleets) {
    return destroyed;
  }

  static auto fleet_class =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
  static const auto removal_reason_offset =
      fleet_class.isValidHelper() ? fleet_class.GetField("<RemovalReason>k__BackingField").offset() : 0;
  if (!removal_reason_offset) {
    return destroyed;
  }

  auto*      list        = static_cast<IList*>(fleets);
  const auto count       = std::min(std::max(list->Count, 0), static_cast<int>(destroyed.values.size()));
  int        write_index = 0;
  for (int index = 0; index < count; ++index) {
    auto*      fleet = reinterpret_cast<FleetDeployedData*>(list->Get(index));
    const auto removal_reason =
        fleet ? *reinterpret_cast<int*>(reinterpret_cast<char*>(fleet) + removal_reason_offset) : -1;
    if (!fleet || !fleet->IsDestroyed || removal_reason != 1 || fleet->ID == 0) {
      continue;
    }

    const auto existing_end = destroyed.values.begin() + write_index;
    if (std::find(destroyed.values.begin(), existing_end, fleet->ID) == existing_end) {
      destroyed.values[write_index++] = fleet->ID;
    }
  }
  destroyed.count = write_index;
  return destroyed;
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

std::string SnapshotPlayerFleets(void* fleets)
{
  if (!fleets) {
    return "list-unavailable";
  }

  auto*              list  = static_cast<IList*>(fleets);
  const auto         count = std::min(std::max(list->Count, 0), 8);
  std::ostringstream out;
  out << "count=" << list->Count;
  for (int index = 0; index < count; ++index) {
    auto* fleet    = reinterpret_cast<FleetPlayerData*>(list->Get(index));
    auto  identity = SnapshotPlayerFleetIdentity(fleet);
    out << " item" << index << "={" << FormatPlayerFleetIdentity(identity)
        << " state=" << (fleet ? static_cast<int>(fleet->CurrentState) : -1)
        << " prev=" << (fleet ? static_cast<int>(fleet->PreviousState) : -1) << '}';
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

struct ResumeAttempt {
  bool candidate = false;
  bool attempted = false;
  int  result    = -1;

  bool succeeded() const
  { return attempted && result == action_queue_guard::kEngageResultSuccess; }
};

ResumeAttempt TryResumeExactSurvivingSuffix(ActionQueueManager* manager, void* instance, FleetPlayerData* player_fleet,
                                            bool player_idle, const ActionQueueInstanceSnapshot& before_native,
                                            const ActionQueueInstanceSnapshot& after_native, const char* source)
{
  ResumeAttempt attempt;
  const auto    protection_enabled = ProtectionEnabled();
  attempt.candidate =
      action_queue_guard::IsNativePruneResumeCandidate(protection_enabled || DiagnosticsEnabled(), player_idle,
                                                       ToPolicyState(before_native), ToPolicyState(after_native));

  if (attempt.candidate) {
    spdlog::info("[ActionQueueGuard] candidate=resume-after-native-prune source={} action={} fleet={} old_head={} "
                 "new_head={} count_before={} count_after_native={} last={} pending={}",
                 source, protection_enabled ? "verify-and-resume" : "observe-only", after_native.player_fleet_id,
                 before_native.head_target_id, after_native.head_target_id, before_native.count, after_native.count,
                 after_native.last_engaged_target_id, after_native.pending_engage_target_id);
  }

  if (!attempt.candidate || !protection_enabled) {
    return attempt;
  }

  const auto confirmed = SnapshotActionQueueInstance(instance);
  if (!action_queue_guard::IsStableResumePostcondition(ToPolicyState(after_native), ToPolicyState(confirmed))) {
    spdlog::info("[ActionQueueGuard] action=resume-after-native-prune source={} suppressed=postcondition-changed "
                 "fleet={} expected_head={} confirmed_head={} expected_count={} confirmed_count={} expected_last={} "
                 "confirmed_last={} expected_pending={} confirmed_pending={}",
                 source, after_native.player_fleet_id, after_native.head_target_id, confirmed.head_target_id,
                 after_native.count, confirmed.count, after_native.last_engaged_target_id,
                 confirmed.last_engaged_target_id, after_native.pending_engage_target_id,
                 confirmed.pending_engage_target_id);
    return attempt;
  }

  if (auto* try_engage = ResolveTryPlanPathAndEngageTarget(); try_engage && player_fleet) {
    attempt.attempted = true;
    attempt.result    = try_engage(manager, player_fleet, instance);
    spdlog::info("[ActionQueueGuard] action=resume-after-native-prune source={} fleet={} head={} count={} "
                 "engage_result={} engage_result_name={}",
                 source, confirmed.player_fleet_id, confirmed.head_target_id, confirmed.count, attempt.result,
                 action_queue_guard::EngageResultName(attempt.result));
  } else {
    spdlog::warn("[ActionQueueGuard] action=resume-after-native-prune source={} "
                 "skipped=missing-method-or-fleet fleet={} head={}",
                 source, confirmed.player_fleet_id, confirmed.head_target_id);
  }
  return attempt;
}

bool ActionQueueManager_DoPlanPathAndEngageTarget_Guard(auto original, ActionQueueManager* manager,
                                                        FleetPlayerData* player_fleet)
{
  const auto trace           = DiagnosticsEnabled();
  const auto player_identity = trace ? SnapshotPlayerFleetIdentity(player_fleet) : PlayerFleetIdentitySnapshot{};
  const auto player_fleet_id = player_fleet ? static_cast<std::int64_t>(player_fleet->Id) : 0;
  auto*      instance        = FindQueueInstanceForPlayerFleet(manager, player_fleet_id);
  const auto before          = SnapshotActionQueueInstance(instance);
  if (trace) {
    spdlog::info("[ActionQueueGuard] phase=before hook=DoPlanPathAndEngageTarget player={} state={} prev={} queue={}",
                 FormatPlayerFleetIdentity(player_identity),
                 player_fleet ? static_cast<int>(player_fleet->CurrentState) : -1,
                 player_fleet ? static_cast<int>(player_fleet->PreviousState) : -1, FormatActionQueueInstance(before));
  }

  const auto native_result = original(manager, player_fleet);
  const auto after_native  = SnapshotActionQueueInstance(instance);
  const auto player_idle   = !native_result && player_fleet && before.player_fleet_id == player_fleet_id
                             && after_native.player_fleet_id == player_fleet_id
                             && player_fleet->CurrentState == FleetState::IdleInSpace;
  const auto resume =
      TryResumeExactSurvivingSuffix(manager, instance, player_fleet, player_idle, before, after_native, "DoPlan");

  if (trace) {
    spdlog::info("[ActionQueueGuard] phase=after hook=DoPlanPathAndEngageTarget player={} native_result={} "
                 "resume_candidate={} replay_result={} replay_result_name={} state={} prev={} queue_after_native={} "
                 "queue_after_guard={}",
                 FormatPlayerFleetIdentity(player_identity), native_result, resume.candidate,
                 resume.attempted ? resume.result : -1,
                 action_queue_guard::EngageResultName(resume.attempted ? resume.result : -1),
                 player_fleet ? static_cast<int>(player_fleet->CurrentState) : -1,
                 player_fleet ? static_cast<int>(player_fleet->PreviousState) : -1,
                 FormatActionQueueInstance(after_native),
                 FormatActionQueueInstance(SnapshotActionQueueInstance(instance)));
  }
  return native_result || resume.succeeded();
}

void ActionQueueManager_OnFleetStateChange_Diagnostics(auto original, ActionQueueManager* manager, void* fleets)
{
  const auto trace         = DiagnosticsEnabled();
  const auto active_before = trace && DeployedFleetListTouchesActiveQueue(manager, fleets);
  const auto fleets_before = trace ? SnapshotDisposedFleets(fleets) : std::string{};
  const auto queues_before = trace ? SnapshotAllActionQueues(manager) : std::string{};
  if (active_before) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnFleetStateChange fleets={} queues={}", fleets_before,
                 queues_before);
  }

  original(manager, fleets);

  if (!trace) {
    return;
  }
  const auto active_after = DeployedFleetListTouchesActiveQueue(manager, fleets);
  if (!active_before && active_after) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnFleetStateChange fleets={} queues={}", fleets_before,
                 queues_before);
  }
  if (active_before || active_after) {
    spdlog::info("[ActionQueueGuard] phase=after hook=OnFleetStateChange fleets={} queues={}",
                 SnapshotDisposedFleets(fleets), SnapshotAllActionQueues(manager));
  }
}

void ActionQueueManager_OnPlayerFleetStateChanged_Diagnostics(auto original, ActionQueueManager* manager, void* fleets)
{
  const auto trace         = DiagnosticsEnabled();
  const auto active_before = trace && PlayerFleetListTouchesActiveQueue(manager, fleets);
  const auto fleets_before = trace ? SnapshotPlayerFleets(fleets) : std::string{};
  const auto queues_before = trace ? SnapshotAllActionQueues(manager) : std::string{};
  if (active_before) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnPlayerFleetStateChanged fleets={} queues={}", fleets_before,
                 queues_before);
  }

  original(manager, fleets);

  if (!trace) {
    return;
  }
  const auto active_after = PlayerFleetListTouchesActiveQueue(manager, fleets);
  if (!active_before && active_after) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnPlayerFleetStateChanged fleets={} queues={}", fleets_before,
                 queues_before);
  }
  if (active_before || active_after) {
    spdlog::info("[ActionQueueGuard] phase=after hook=OnPlayerFleetStateChanged fleets={} queues={}",
                 SnapshotPlayerFleets(fleets), SnapshotAllActionQueues(manager));
  }
}

void ActionQueueManager_HandleStall_Guard(auto original, ActionQueueManager* manager, void* instance,
                                          FleetPlayerData* player_fleet, FleetDeployedData* deployed_fleet)
{
  const auto before = SnapshotActionQueueInstance(instance);
  if (DiagnosticsEnabled()) {
    const auto player_identity = SnapshotPlayerFleetIdentity(player_fleet);
    spdlog::info("[ActionQueueGuard] phase=before hook=HandleStall instance={} player={} deployed={} "
                 "deployed_state={} deployed_prev={} deployed_destroyed={} deployed_battling={} queue={}",
                 static_cast<void*>(instance), FormatPlayerFleetIdentity(player_identity),
                 deployed_fleet ? deployed_fleet->ID : 0, deployed_fleet ? deployed_fleet->CurrentState : -1,
                 deployed_fleet ? deployed_fleet->PreviousState : -1,
                 deployed_fleet ? deployed_fleet->IsDestroyed : false,
                 deployed_fleet ? deployed_fleet->CurrentlyBattling : false, FormatActionQueueInstance(before));
  }

  original(manager, instance, player_fleet, deployed_fleet);

  const auto after_native = SnapshotActionQueueInstance(instance);
  const auto player_idle =
      player_fleet && deployed_fleet && before.player_fleet_id == static_cast<std::int64_t>(player_fleet->Id)
      && after_native.player_fleet_id == before.player_fleet_id && deployed_fleet->ID == before.player_fleet_id
      && player_fleet->CurrentState == FleetState::IdleInSpace && deployed_fleet->CurrentState == 0
      && !deployed_fleet->IsDestroyed && !deployed_fleet->CurrentlyBattling;
  const auto resume =
      TryResumeExactSurvivingSuffix(manager, instance, player_fleet, player_idle, before, after_native, "HandleStall");

  if (DiagnosticsEnabled()) {
    const auto after_guard = SnapshotActionQueueInstance(instance);
    spdlog::info(
        "[ActionQueueGuard] phase=after hook=HandleStall instance={} same_head={} count_delta_native={} "
        "resume_candidate={} replay_result={} replay_result_name={} queue_after_native={} queue_after_guard={}",
        static_cast<void*>(instance),
        before.head_target_id != 0 && before.head_target_id == after_native.head_target_id,
        before.count >= 0 && after_native.count >= 0 ? after_native.count - before.count : 0, resume.candidate,
        resume.attempted ? resume.result : -1,
        action_queue_guard::EngageResultName(resume.attempted ? resume.result : -1),
        FormatActionQueueInstance(after_native), FormatActionQueueInstance(after_guard));
  }
}

void ActionQueueManager_OnFleetsDisposed_Guard(auto original, ActionQueueManager* manager, void* fleets)
{
  const auto destroyed = SnapshotDestroyedTargetIds(fleets);
  const auto disposed  = DiagnosticsEnabled() ? SnapshotDisposedFleets(fleets) : std::string{};
  if (DiagnosticsEnabled()) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnFleetsDisposed disposed={} queues={}", disposed,
                 SnapshotAllActionQueues(manager));
  }

  original(manager, fleets);

  for (int index = 0; index < destroyed.count; ++index) {
    const auto target_id      = destroyed.values[index];
    const auto exact_queue    = FindQueueWithExactHead(manager, target_id);
    const auto should_process = action_queue_guard::ShouldProcessDestroyedHead(ProtectionEnabled(), true, target_id,
                                                                               ToPolicyState(exact_queue));
    if (!should_process) {
      continue;
    }

    const auto confirmed_queue = FindQueueWithExactHead(manager, target_id);
    if (!action_queue_guard::ShouldProcessDestroyedHead(ProtectionEnabled(), true, target_id,
                                                        ToPolicyState(confirmed_queue))
        || confirmed_queue.player_fleet_id != exact_queue.player_fleet_id
        || confirmed_queue.count != exact_queue.count) {
      spdlog::info("[ActionQueueGuard] action=process-destroyed-head suppressed=postcondition-changed target={} "
                   "fleet={}",
                   target_id, exact_queue.player_fleet_id);
      continue;
    }

    if (auto* process_target = ResolveProcessQueueTarget(); process_target) {
      process_target(manager, target_id, false);
      spdlog::info("[ActionQueueGuard] action=process-destroyed-head target={} fleet={} count_before={} "
                   "can_select_new_target=false",
                   target_id, confirmed_queue.player_fleet_id, confirmed_queue.count);
    } else {
      spdlog::warn("[ActionQueueGuard] action=process-destroyed-head skipped=missing-method target={} fleet={}",
                   target_id, exact_queue.player_fleet_id);
    }
  }

  if (DiagnosticsEnabled()) {
    spdlog::info("[ActionQueueGuard] phase=after hook=OnFleetsDisposed disposed={} queues={}", disposed,
                 SnapshotAllActionQueues(manager));
  }
}

void ActionQueueManager_OnStrikeComplete_Diagnostics(auto original, ActionQueueManager* manager, void* strike)
{
  const auto snapshot = SnapshotStrike(strike);
  if (DiagnosticsEnabled()) {
    spdlog::info("[ActionQueueGuard] phase=before hook=OnStrikeComplete attacker={} target={} target_destroyed={} "
                 "targets={} queues={}",
                 snapshot.attacker_fleet_id, snapshot.target_fleet_id, snapshot.target_destroyed,
                 SnapshotStrikeTargets(strike), SnapshotAllActionQueues(manager));
  }

  original(manager, strike);

  if (DiagnosticsEnabled()) {
    spdlog::info("[ActionQueueGuard] phase=after hook=OnStrikeComplete attacker={} target={} target_destroyed={} "
                 "targets={} queues={}",
                 snapshot.attacker_fleet_id, snapshot.target_fleet_id, snapshot.target_destroyed,
                 SnapshotStrikeTargets(strike), SnapshotAllActionQueues(manager));
  }
}

void ActionQueueManager_ProcessTarget_Diagnostics(auto original, ActionQueueManager* manager, std::int64_t target_id,
                                                  bool can_select_new_target)
{
  if (DiagnosticsEnabled()) {
    spdlog::info(
        "[ActionQueueGuard] phase=before hook=ProcessQueue.target target={} can_select_new_target={} queues={}",
        target_id, can_select_new_target, SnapshotAllActionQueues(manager));
  }

  original(manager, target_id, can_select_new_target);

  if (DiagnosticsEnabled()) {
    spdlog::info("[ActionQueueGuard] phase=after hook=ProcessQueue.target target={} can_select_new_target={} queues={}",
                 target_id, can_select_new_target, SnapshotAllActionQueues(manager));
  }
}
} // namespace

void InstallActionQueueGuardHooks()
{
  HookModuleHealth hooks("ActionQueueGuard");
  auto&            manager_class = ActionQueueManagerClass();
  const auto       guard_enabled = DiagnosticsEnabled() || ProtectionEnabled();
  if (!manager_class.isValidHelper()) {
    hooks.record_missing_helper(kActionQueueOnFleetsDisposedHook);
    if (guard_enabled) {
      hooks.record_missing_helper(kActionQueueHandleStallHook);
      hooks.record_missing_helper(kActionQueueDoPlanAndEngageHook);
    }
    if (DiagnosticsEnabled()) {
      hooks.record_missing_helper(kActionQueueOnStrikeCompleteHook);
      hooks.record_missing_helper(kActionQueueProcessTargetHook);
      hooks.record_missing_helper(kActionQueueOnFleetStateChangeHook);
      hooks.record_missing_helper(kActionQueueOnPlayerFleetStateChangedHook);
    }
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
    hooks.log_summary();
    return;
  }

  if (!guard_enabled) {
    hooks.record_skipped(kActionQueueHandleStallHook, "thin protection and action queue logging disabled");
  } else {
    if (auto method = manager_class.GetMethod("HandleStall"); method) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueHandleStallHook, method,
                                       ActionQueueManager_HandleStall_Guard);
    } else {
      hooks.record_missing_method(kActionQueueHandleStallHook);
      ErrorMsg::MissingMethod("ActionQueueManager", "HandleStall");
    }
  }

  if (!guard_enabled) {
    hooks.record_skipped(kActionQueueDoPlanAndEngageHook, "thin protection and action queue logging disabled");
  } else if (auto method = manager_class.GetMethod("DoPlanPathAndEngageTarget"); method) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueDoPlanAndEngageHook, method,
                                     ActionQueueManager_DoPlanPathAndEngageTarget_Guard);
  } else {
    hooks.record_missing_method(kActionQueueDoPlanAndEngageHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "DoPlanPathAndEngageTarget");
  }

  if (auto method = manager_class.GetMethod("OnFleetsDisposedEventHandler"); method) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnFleetsDisposedHook, method,
                                     ActionQueueManager_OnFleetsDisposed_Guard);
  } else {
    hooks.record_missing_method(kActionQueueOnFleetsDisposedHook);
    ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetsDisposedEventHandler");
  }

  if (!DiagnosticsEnabled()) {
    hooks.record_skipped(kActionQueueOnStrikeCompleteHook, "action queue logging disabled");
    hooks.record_skipped(kActionQueueProcessTargetHook, "action queue logging disabled");
    hooks.record_skipped(kActionQueueOnFleetStateChangeHook, "action queue logging disabled");
    hooks.record_skipped(kActionQueueOnPlayerFleetStateChangedHook, "action queue logging disabled");
  } else {
    if (auto method = manager_class.GetMethod("OnStrikeCompleteEventHandler"); method) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnStrikeCompleteHook, method,
                                       ActionQueueManager_OnStrikeComplete_Diagnostics);
    } else {
      hooks.record_missing_method(kActionQueueOnStrikeCompleteHook);
      ErrorMsg::MissingMethod("ActionQueueManager", "OnStrikeCompleteEventHandler");
    }

    auto process_target =
        manager_class.GetMethodSpecial("ProcessQueue", [](int param_count, const Il2CppType** params) {
          return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
        });
    if (process_target) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueProcessTargetHook, process_target,
                                       ActionQueueManager_ProcessTarget_Diagnostics);
    } else {
      hooks.record_missing_method(kActionQueueProcessTargetHook);
      ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(Int64, bool)");
    }

    if (auto method = manager_class.GetMethod("OnFleetStateChangeEventHandler"); method) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnFleetStateChangeHook, method,
                                       ActionQueueManager_OnFleetStateChange_Diagnostics);
    } else {
      hooks.record_missing_method(kActionQueueOnFleetStateChangeHook);
      ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetStateChangeEventHandler");
    }

    if (auto method = manager_class.GetMethod("OnPlayerFleetStateChangedEventHandler"); method) {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kActionQueueOnPlayerFleetStateChangedHook, method,
                                       ActionQueueManager_OnPlayerFleetStateChanged_Diagnostics);
    } else {
      hooks.record_missing_method(kActionQueueOnPlayerFleetStateChangedHook);
      ErrorMsg::MissingMethod("ActionQueueManager", "OnPlayerFleetStateChangedEventHandler");
    }
  }

  hooks.log_summary();
}
