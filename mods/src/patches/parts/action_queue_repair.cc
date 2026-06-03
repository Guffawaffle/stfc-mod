/**
 * @file action_queue_repair.cc
 * @brief Action queue repair hooks and investigation diagnostics.
 *
 * Contains the narrow queue-skip repair hooks, sticky engaged-target experiment,
 * and trace-gated queue diagnostics used to investigate occasional skipped
 * hostile actions without importing the broader NetNiv runtime cleanup work.
 */
#include "errormsg.h"
#include "file.h"
#include "config.h"
#include "diagnostics_file_policy.h"
#include "patches/fleet_runtime_diagnostics.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/IList.h>
#include <prime/Vector3.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

#include <il2cpp/il2cpp_helper.h>

#include <nlohmann/json.hpp>

#include <spud/detour.h>

#include "probe/probe.h"

bool ActionQueueProbeEnabled()
{
  const auto level = RuntimeTraceLevelSetting();
  return level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
}

bool ActionQueueProbeDetoursEnabled()
{ return ActionQueueProbeEnabled(); }

bool ActionQueueRepairEnabled()
{ return true; }

constexpr char kActionQueueProbeJsonlFile[] = "community_patch_action_queue_probe.jsonl";

nlohmann::json ActionQueueInstanceJson(void* action_queue_instance);
nlohmann::json ActionQueueSlotsJson(ActionQueueManager* manager);
long           ActionQueueInstanceFleetId(void* action_queue_instance);
long           ActionQueueInstanceLastTargetId(void* action_queue_instance);
long           ActionQueueInstanceHeadTargetId(void* action_queue_instance);
bool           ActionQueueInstanceIsEngaging(void* action_queue_instance);
void           SetActionQueueInstanceIsEngaging(void* action_queue_instance, bool is_engaging);
int            FindActionQueueItemIndex(IList* list, long target_id);
IList*         ProbeActionQueueInstanceList(void* action_queue_instance);

std::mutex& ActionQueueProbeJsonlMutex()
{
  static std::mutex mutex;
  return mutex;
}

const std::filesystem::path& ActionQueueProbeJsonlPath()
{
  static const auto path = []() {
    const auto& settings = AdvancedDiagnosticsFileSettings();
    const auto  target   = ResolveDiagnosticsFileTarget(
        kActionQueueProbeJsonlFile, std::filesystem::path(File::MakePathString(kActionQueueProbeJsonlFile, true)),
        settings.root);
    if (target.warning.has_value()) {
      spdlog::warn("[ActionQueueProbe] {}", *target.warning);
    }
    return target.path;
  }();

  return path;
}

std::uintmax_t ActionQueueProbeJsonlMaxBytes()
{
  const auto& settings = AdvancedDiagnosticsFileSettings();
  return static_cast<std::uintmax_t>(std::max(1, settings.action_queue_probe_max_kb)) * 1024u;
}

int ActionQueueProbeJsonlTotalFiles()
{
  return std::max(1, AdvancedDiagnosticsFileSettings().action_queue_probe_files);
}

void WarnActionQueueProbePolicyOnce(const std::optional<std::string>& warning)
{
  if (!warning.has_value()) {
    return;
  }

  static bool warned = false;
  if (warned) {
    return;
  }

  warned = true;
  spdlog::warn("[ActionQueueProbe] {}", *warning);
}

template <typename EventFactory> void AppendActionQueueProbeJsonlIfEnabled(EventFactory build_event);

std::int64_t ActionQueueProbeTimestampMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

struct StickyActionQueueTargetState {
  long         target_id = 0;
  std::int64_t updated_at_ms = 0;
  std::string  source;
};

std::mutex& StickyActionQueueTargetMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::uintptr_t, StickyActionQueueTargetState>& StickyActionQueueTargets()
{
  static std::unordered_map<std::uintptr_t, StickyActionQueueTargetState> targets;
  return targets;
}

std::uintptr_t StickyActionQueueTargetKey(void* action_queue_instance)
{ return reinterpret_cast<std::uintptr_t>(action_queue_instance); }

long StickyActionQueueTargetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  std::lock_guard lk(StickyActionQueueTargetMutex());
  auto&           targets = StickyActionQueueTargets();
  const auto      it      = targets.find(StickyActionQueueTargetKey(action_queue_instance));
  return it == targets.end() ? 0 : it->second.target_id;
}

bool LatchStickyActionQueueTarget(ActionQueueManager* manager, void* action_queue_instance, long target_id, const char* hook,
                                  const char* reason)
{
  if (!action_queue_instance || target_id == 0) {
    return false;
  }

  StickyActionQueueTargetState previous;
  {
    std::lock_guard lk(StickyActionQueueTargetMutex());
    auto&           targets = StickyActionQueueTargets();
    auto&           state   = targets[StickyActionQueueTargetKey(action_queue_instance)];
    previous                = state;
    state.target_id         = target_id;
    state.updated_at_ms     = ActionQueueProbeTimestampMs();
    state.source            = reason ? reason : "";
  }

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    return nlohmann::json{{"phase", "repair"},
                          {"hook", hook},
                          {"repair", "latch-sticky-engaged-target"},
                          {"reason", reason},
                          {"target_id", target_id},
                          {"previous_target_id", previous.target_id},
                          {"instance", ActionQueueInstanceJson(action_queue_instance)},
                          {"slots", ActionQueueSlotsJson(manager)}};
  });

  if (ActionQueueProbeEnabled() && previous.target_id != target_id) {
    spdlog::info("[ActionQueueProbe] repair hook={} latched sticky target={} previous={} reason={}", hook, target_id,
                 previous.target_id, reason ? reason : "");
  }

  return previous.target_id != target_id;
}

bool ClearStickyActionQueueTarget(ActionQueueManager* manager, void* action_queue_instance, const char* hook,
                                  const char* reason)
{
  if (!action_queue_instance) {
    return false;
  }

  StickyActionQueueTargetState previous;
  {
    std::lock_guard lk(StickyActionQueueTargetMutex());
    auto&           targets = StickyActionQueueTargets();
    const auto      it      = targets.find(StickyActionQueueTargetKey(action_queue_instance));
    if (it == targets.end()) {
      return false;
    }
    previous = it->second;
    targets.erase(it);
  }

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    return nlohmann::json{{"phase", "repair"},
                          {"hook", hook},
                          {"repair", "clear-sticky-engaged-target"},
                          {"reason", reason},
                          {"target_id", previous.target_id},
                          {"instance", ActionQueueInstanceJson(action_queue_instance)},
                          {"slots", ActionQueueSlotsJson(manager)}};
  });

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] repair hook={} cleared sticky target={} reason={}", hook, previous.target_id,
                 reason ? reason : "");
  }

  return true;
}

void ClearStickyActionQueueTargetsForFleet(ActionQueueManager* manager, FleetPlayerData* fleet, const char* hook,
                                           const char* reason)
{
  if (!manager || !fleet) {
    return;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return;
  }

  static auto battle_queue_field = class_helper.GetField("_battleQueue").offset();
  auto        battle_queue       = *(Il2CppArray**)((char*)manager + battle_queue_field);
  if (!battle_queue) {
    return;
  }

  auto sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    if (ActionQueueInstanceFleetId(action_queue_instance) == fleet->Id) {
      ClearStickyActionQueueTarget(manager, action_queue_instance, hook, reason);
    }
  }
}

bool SyncStickyActionQueueTargetFromNative(ActionQueueManager* manager, void* action_queue_instance, const char* hook,
                                           const char* reason)
{
  const auto native_target_id = ActionQueueInstanceLastTargetId(action_queue_instance);
  if (native_target_id == 0) {
    return false;
  }

  return LatchStickyActionQueueTarget(manager, action_queue_instance, native_target_id, hook, reason);
}

std::int64_t FleetRuntimeTriggerAgeMs(const FleetRuntimeDiagnosticsSnapshot& snapshot)
{
  const auto now = ActionQueueProbeTimestampMs();
  return now >= snapshot.latestTriggerAtMs ? now - snapshot.latestTriggerAtMs : snapshot.latestTriggerAtMs - now;
}

bool IsRecentFleetRuntimeTrigger(const FleetRuntimeDiagnosticsSnapshot& snapshot, const char* source,
                                 std::int64_t max_age_ms = 2000)
{ return snapshot.latestTriggerSource == source && FleetRuntimeTriggerAgeMs(snapshot) <= max_age_ms; }

nlohmann::json FleetRuntimeTriggerJson(const FleetRuntimeDiagnosticsSnapshot& snapshot)
{
  return {{"source", snapshot.latestTriggerSource},
          {"trigger_ms", snapshot.latestTriggerAtMs},
          {"age_ms", FleetRuntimeTriggerAgeMs(snapshot)}};
}

bool ShouldClearEngagingAfterManualRemove(int count_after_repair, const FleetRuntimeDiagnosticsSnapshot& latest_trigger)
{
  if (count_after_repair <= 0) {
    return true;
  }

  return !IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-arrived-at-destination");
}

bool ShouldClearEngagingForRemovedMissingTarget(const FleetRuntimeDiagnosticsSnapshot& latest_trigger)
{
  return IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-combat-ended")
         || IsRecentFleetRuntimeTrigger(latest_trigger, "deployment-battle-end-event");
}

bool ShouldDeferArrivalRemoveRepair(const FleetRuntimeDiagnosticsSnapshot& latest_trigger)
{ return IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-arrived-at-destination"); }

bool ShouldPreserveOrphanedEngagingInStall(const FleetRuntimeDiagnosticsSnapshot& latest_trigger)
{
  return IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-arrived-at-destination", 20000)
         || IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-impulse-started", 20000)
         || IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-combat-started", 20000)
         || IsRecentFleetRuntimeTrigger(latest_trigger, "fleet-slot-combat-ended", 5000)
         || IsRecentFleetRuntimeTrigger(latest_trigger, "deployment-battle-end-event", 5000);
}

std::uintptr_t PtrValue(const void* ptr)
{ return reinterpret_cast<std::uintptr_t>(ptr); }

void AppendActionQueueProbeJsonl(nlohmann::json event)
{
  if (!ActionQueueProbeEnabled()) {
    return;
  }

  event["ts_ms"] = ActionQueueProbeTimestampMs();
  const auto payload = event.dump();

  std::lock_guard lk(ActionQueueProbeJsonlMutex());
  const auto&     path = ActionQueueProbeJsonlPath();
  const auto      prepare = PrepareDiagnosticsFileForAppend(
      path, ActionQueueProbeJsonlMaxBytes(), ActionQueueProbeJsonlTotalFiles(), payload.size() + 1);
  WarnActionQueueProbePolicyOnce(prepare.warning);
  if (!prepare.append_allowed) {
    return;
  }
  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::app);
  if (!file.is_open()) {
    static auto warned = false;
    if (!warned) {
      warned = true;
      spdlog::error("Failed to open action queue probe JSONL for append: {}", path.string());
    }
    return;
  }

  file << payload << '\n';
}

template <typename EventFactory> void AppendActionQueueProbeJsonlIfEnabled(EventFactory build_event)
{
  if (!ActionQueueProbeEnabled()) {
    return;
  }

  AppendActionQueueProbeJsonl(build_event());
}

void ResetActionQueueProbeJsonl()
{
  if (!ActionQueueProbeEnabled()) {
    return;
  }

  std::lock_guard lk(ActionQueueProbeJsonlMutex());
  const auto&     path = ActionQueueProbeJsonlPath();
  std::ofstream   file(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    spdlog::error("Failed to open action queue probe JSONL for reset: {}", path.string());
    return;
  }

  nlohmann::json event;
  event["ts_ms"]   = ActionQueueProbeTimestampMs();
  event["event"]   = "session-start";
  event["level"]   = RuntimeTraceLevelName(RuntimeTraceLevelSetting());
  event["detours"] = ActionQueueProbeDetoursEnabled();
  file << event.dump() << '\n';
}

struct ActionQueueSnapshot {
  uint64_t fleet_id    = 0;
  int      fleet_state = -1;
  int      count       = -1;
  int      max         = -1;
  int      state       = -1;
  int      reason      = -1;
  bool     full        = false;
  bool     in_queue    = false;
  bool     any_queue   = false;
  bool     unlocked    = false;
};

ActionQueueSnapshot ReadActionQueueSnapshot(ActionQueueManager* manager, FleetPlayerData* fleet)
{
  ActionQueueSnapshot snapshot;
  if (!manager) {
    return snapshot;
  }

  snapshot.any_queue = manager->AnyPlayerFleetInQueue();
  snapshot.unlocked  = manager->IsQueueUnlocked();
  snapshot.max       = manager->GetMaxQueueable();

  if (fleet) {
    snapshot.fleet_id    = fleet->Id;
    snapshot.fleet_state = static_cast<int>(fleet->CurrentState);
    snapshot.count       = manager->GetActionQueueCount(fleet);
    snapshot.full        = manager->IsQueueFull(fleet);
    snapshot.in_queue    = manager->IsFleetInQueue(fleet);
    int reason           = -1;
    snapshot.state       = manager->GetActionQueueState(fleet, &reason);
    snapshot.reason      = reason;
  }

  return snapshot;
}

void LogActionQueueSnapshot(const char* phase, const char* hook, ActionQueueManager* manager, FleetPlayerData* fleet)
{
  if (!ActionQueueProbeDetoursEnabled()) {
    return;
  }

  const auto snapshot = ReadActionQueueSnapshot(manager, fleet);
  spdlog::info(
      "[ActionQueueProbe] phase={} hook={} manager={} fleet_ptr={} fleet={} fleet_state={} count={} max={} state={} "
      "reason={} full={} in_queue={} any_queue={} unlocked={}",
      phase, hook, static_cast<void*>(manager), static_cast<void*>(fleet), snapshot.fleet_id, snapshot.fleet_state,
      snapshot.count, snapshot.max, snapshot.state, snapshot.reason, snapshot.full, snapshot.in_queue,
      snapshot.any_queue, snapshot.unlocked);
}

int ActionQueueInstanceCount(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return -1;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return -1;
  }

  static auto field = class_helper.GetField("_actionQueue").offset();
  auto        list  = *(IList**)((char*)action_queue_instance + field);
  return list ? list->Count : -1;
}

long ActionQueueInstanceFleetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("<PlayerFleetId>k__BackingField").offset();
  return *(long*)((char*)action_queue_instance + field);
}

long ActionQueueInstanceLastTargetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("LastEngagedTargetId").offset();
  return *(long*)((char*)action_queue_instance + field);
}

void SetActionQueueInstanceLastTargetId(void* action_queue_instance, long target_id)
{
  if (!action_queue_instance) {
    return;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return;
  }

  static auto field                              = class_helper.GetField("LastEngagedTargetId").offset();
  *(long*)((char*)action_queue_instance + field) = target_id;
}

bool RepairActionQueueInstanceLastTargetFromHead(ActionQueueManager* manager, void* action_queue_instance, const char* hook,
                                                 const char* repair)
{
  if (!action_queue_instance || !ActionQueueInstanceIsEngaging(action_queue_instance)) {
    return false;
  }

  const auto count_before          = ActionQueueInstanceCount(action_queue_instance);
  const auto last_target_before    = ActionQueueInstanceLastTargetId(action_queue_instance);
  const auto head_target_id        = ActionQueueInstanceHeadTargetId(action_queue_instance);
  const auto sticky_target_id      = StickyActionQueueTargetId(action_queue_instance);
  const auto sticky_target_present =
      sticky_target_id != 0
      && FindActionQueueItemIndex(ProbeActionQueueInstanceList(action_queue_instance), sticky_target_id) >= 0;
  const auto restore_target_id = sticky_target_present ? sticky_target_id : head_target_id;
  if (count_before <= 0 || last_target_before != 0 || restore_target_id == 0) {
    return false;
  }

  SetActionQueueInstanceLastTargetId(action_queue_instance, restore_target_id);
  LatchStickyActionQueueTarget(manager, action_queue_instance, restore_target_id, hook,
                               sticky_target_present ? "restore-from-sticky-target" : "restore-from-live-head");

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    return nlohmann::json{{"phase", "repair"},
                          {"hook", hook},
                          {"repair", repair},
                          {"count_before", count_before},
                          {"last_target_before", last_target_before},
                          {"sticky_target_id", sticky_target_id},
                          {"sticky_target_present", sticky_target_present},
                          {"head_target_id", head_target_id},
                          {"restore_target_id", restore_target_id},
                          {"instance", ActionQueueInstanceJson(action_queue_instance)},
                          {"slots", ActionQueueSlotsJson(manager)}};
  });

  if (ActionQueueProbeEnabled()) {
    spdlog::warn("[ActionQueueProbe] repair hook={} restored LastEngagedTargetId={} count_before={} sticky={} head={}",
                 hook, restore_target_id, count_before, sticky_target_id, head_target_id);
  }

  return true;
}

bool ActionQueueInstanceIsEngaging(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return false;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return false;
  }

  static auto field = class_helper.GetField("IsEngaging").offset();
  return *(bool*)((char*)action_queue_instance + field);
}

void SetActionQueueInstanceIsEngaging(void* action_queue_instance, bool is_engaging)
{
  if (!action_queue_instance) {
    return;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return;
  }

  static auto field                              = class_helper.GetField("IsEngaging").offset();
  *(bool*)((char*)action_queue_instance + field) = is_engaging;
}

bool ClearActionQueueEngagingState(ActionQueueManager* manager, void* action_queue_instance, const char* hook,
                                   const char* repair, bool require_orphaned_last_target)
{
  if (!action_queue_instance || !ActionQueueInstanceIsEngaging(action_queue_instance)) {
    return false;
  }

  const auto count       = ActionQueueInstanceCount(action_queue_instance);
  const auto last_target = ActionQueueInstanceLastTargetId(action_queue_instance);
  if (require_orphaned_last_target && last_target != 0) {
    return false;
  }

  SetActionQueueInstanceLastTargetId(action_queue_instance, 0);
  SetActionQueueInstanceIsEngaging(action_queue_instance, false);

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    return nlohmann::json{{"phase", "repair"},
                          {"hook", hook},
                          {"repair", repair},
                          {"count_before", count},
                          {"last_target_before", last_target},
                          {"instance", ActionQueueInstanceJson(action_queue_instance)},
                          {"slots", ActionQueueSlotsJson(manager)}};
  });

  if (ActionQueueProbeEnabled()) {
    spdlog::warn(
        "[ActionQueueProbe] repair hook={} cleared orphaned engaging state count_before={} last_target_before={}", hook,
        count, last_target);
  }

  return true;
}

IList* ProbeActionQueueInstanceList(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return nullptr;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto field = class_helper.GetField("_actionQueue").offset();
  return *(IList**)((char*)action_queue_instance + field);
}

long ProbeQueueableActionFleetId(Il2CppObject* queueable_action)
{
  if (!queueable_action) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("<FleetId>k__BackingField").offset();
  return *(long*)((char*)queueable_action + field);
}

int ProbeQueueableActionRetryCount(Il2CppObject* queueable_action)
{
  if (!queueable_action) {
    return -1;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  if (!class_helper.isValidHelper()) {
    return -1;
  }

  static auto field = class_helper.GetField("SetCourseFailRetryCount").offset();
  return *(int*)((char*)queueable_action + field);
}

long ActionQueueInstanceHeadTargetId(void* action_queue_instance)
{
  auto list = ProbeActionQueueInstanceList(action_queue_instance);
  if (!list || list->Count <= 0) {
    return 0;
  }

  return ProbeQueueableActionFleetId(list->Get(0));
}

int FindActionQueueItemIndex(IList* list, long target_id)
{
  if (!list) {
    return -1;
  }

  const auto item_count = std::min(list->Count, 32);
  for (int index = 0; index < item_count; ++index) {
    if (ProbeQueueableActionFleetId(list->Get(index)) == target_id) {
      return index;
    }
  }

  return -1;
}

void* FindActionQueueInstanceContainingTarget(ActionQueueManager* manager, long target_id)
{
  if (!manager || target_id == 0) {
    return nullptr;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto battle_queue_field = class_helper.GetField("_battleQueue").offset();
  auto        battle_queue       = *(Il2CppArray**)((char*)manager + battle_queue_field);
  if (!battle_queue) {
    return nullptr;
  }

  auto sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    auto list                  = ProbeActionQueueInstanceList(action_queue_instance);
    if (FindActionQueueItemIndex(list, target_id) >= 0) {
      return action_queue_instance;
    }
  }

  return nullptr;
}

void* FindActionQueueInstanceWithLastTarget(ActionQueueManager* manager, long target_id)
{
  if (!manager || target_id == 0) {
    return nullptr;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto battle_queue_field = class_helper.GetField("_battleQueue").offset();
  auto        battle_queue       = *(Il2CppArray**)((char*)manager + battle_queue_field);
  if (!battle_queue) {
    return nullptr;
  }

  auto sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    if (ActionQueueInstanceLastTargetId(action_queue_instance) == target_id) {
      return action_queue_instance;
    }
  }

  return nullptr;
}

bool RemoveActionQueueListAt(IList* list, int index)
{
  if (!list || index < 0 || index >= list->Count) {
    return false;
  }

  auto list_object  = reinterpret_cast<Il2CppObject*>(list);
  auto class_helper = IL2CppClassHelper{list_object->klass};
  auto remove_at    = class_helper.GetMethod<void(IList*, int32_t)>("RemoveAt", 1);
  if (!remove_at) {
    remove_at = class_helper.GetMethodSpecial2<void(IList*, int32_t)>(list_object, "RemoveAt");
  }
  if (!remove_at) {
    static auto warn = true;
    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("ActionQueueInstance._actionQueue", "RemoveAt");
    }
    return false;
  }

  remove_at(list, index);
  return true;
}

nlohmann::json ActionQueueItemsJson(IList* list)
{
  auto items = nlohmann::json::array();
  if (!list) {
    return items;
  }

  const auto item_count = std::min(list->Count, 8);
  for (int index = 0; index < item_count; ++index) {
    auto item = list->Get(index);
    items.push_back({{"index", index},
                     {"ptr", PtrValue(item)},
                     {"fleet_id", ProbeQueueableActionFleetId(item)},
                     {"retry_count", ProbeQueueableActionRetryCount(item)}});
  }

  return items;
}

nlohmann::json ActionQueueInstanceJson(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return nullptr;
  }

  auto list = ProbeActionQueueInstanceList(action_queue_instance);
  return {{"ptr", PtrValue(action_queue_instance)},
          {"player_fleet", ActionQueueInstanceFleetId(action_queue_instance)},
          {"count", list ? list->Count : -1},
          {"is_engaging", ActionQueueInstanceIsEngaging(action_queue_instance)},
          {"last_target", ActionQueueInstanceLastTargetId(action_queue_instance)},
          {"sticky_target", StickyActionQueueTargetId(action_queue_instance)},
          {"items", ActionQueueItemsJson(list)}};
}

nlohmann::json ActionQueueSlotsJson(ActionQueueManager* manager)
{
  auto slots = nlohmann::json::array();
  if (!manager) {
    return slots;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return slots;
  }

  static auto battle_queue_field = class_helper.GetField("_battleQueue").offset();
  auto        battle_queue       = *(Il2CppArray**)((char*)manager + battle_queue_field);
  if (!battle_queue) {
    return slots;
  }

  auto sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    slots.push_back({{"slot", index}, {"instance", ActionQueueInstanceJson(action_queue_instance)}});
  }

  return slots;
}

nlohmann::json ActionQueueSnapshotJson(ActionQueueManager* manager, FleetPlayerData* fleet)
{
  const auto snapshot = ReadActionQueueSnapshot(manager, fleet);
  return {{"manager", PtrValue(manager)},    {"fleet_ptr", PtrValue(fleet)},
          {"fleet_id", snapshot.fleet_id},   {"fleet_state", snapshot.fleet_state},
          {"count", snapshot.count},         {"max", snapshot.max},
          {"state", snapshot.state},         {"reason", snapshot.reason},
          {"full", snapshot.full},           {"in_queue", snapshot.in_queue},
          {"any_queue", snapshot.any_queue}, {"unlocked", snapshot.unlocked}};
}

nlohmann::json BuildActionQueueProbeEvent(const char* phase, const char* hook, ActionQueueManager* manager,
                                          FleetPlayerData* fleet, void* action_queue_instance)
{
  return {{"phase", phase},
          {"hook", hook},
          {"manager", PtrValue(manager)},
          {"fleet_ptr", PtrValue(fleet)},
          {"snapshot", ActionQueueSnapshotJson(manager, fleet)},
          {"instance", ActionQueueInstanceJson(action_queue_instance)},
          {"slots", ActionQueueSlotsJson(manager)}};
}

void LogActionQueueInstance(const char* phase, const char* hook, void* action_queue_instance)
{
  if (!ActionQueueProbeEnabled()) {
    return;
  }

  spdlog::info("[ActionQueueProbe] phase={} hook={} instance={} player_fleet={} count={} is_engaging={} last_target={}",
               phase, hook, action_queue_instance, ActionQueueInstanceFleetId(action_queue_instance),
               ActionQueueInstanceCount(action_queue_instance), ActionQueueInstanceIsEngaging(action_queue_instance),
               ActionQueueInstanceLastTargetId(action_queue_instance));
}

long FleetDeployedId(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->ID : 0; }

void ActionQueueManager_AddActionToQueue(auto original, ActionQueueManager* _this, long target_id)
{
  auto before         = BuildActionQueueProbeEvent("before", "AddActionToQueue", _this, nullptr, nullptr);
  before["target_id"] = target_id;
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=AddActionToQueue manager={} target={} any_queue={}",
                 static_cast<void*>(_this), target_id, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
  original(_this, target_id);

  auto after         = BuildActionQueueProbeEvent("after", "AddActionToQueue", _this, nullptr, nullptr);
  after["target_id"] = target_id;
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=AddActionToQueue manager={} target={} any_queue={}",
                 static_cast<void*>(_this), target_id, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
}

bool ActionQueueManager_RemoveActionFromQueue(auto original, ActionQueueManager* _this, long target_id,
                                              FleetPlayerData* fleet, int* index)
{
  auto       target_instance_before = FindActionQueueInstanceContainingTarget(_this, target_id);
  auto       list_before            = ProbeActionQueueInstanceList(target_instance_before);
  const auto target_index_before    = FindActionQueueItemIndex(list_before, target_id);
  const auto count_before           = list_before ? list_before->Count : -1;
  SyncStickyActionQueueTargetFromNative(_this, target_instance_before, "RemoveActionFromQueue", "native-before-remove");

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    auto before                   = BuildActionQueueProbeEvent("before", "RemoveActionFromQueue", _this, fleet, nullptr);
    before["target_id"]           = target_id;
    before["index_ptr"]           = PtrValue(index);
    before["index"]               = index ? *index : -1;
    before["target_index_before"] = target_index_before;
    before["target_instance"]     = ActionQueueInstanceJson(target_instance_before);
    return before;
  });

  LogActionQueueSnapshot("before", "RemoveActionFromQueue", _this, fleet);
  const auto result = original(_this, target_id, fleet, index);

  auto       target_instance_after = FindActionQueueInstanceContainingTarget(_this, target_id);
  auto       list_after            = ProbeActionQueueInstanceList(target_instance_after);
  const auto target_index_after    = FindActionQueueItemIndex(list_after, target_id);
  const auto count_after_original  = list_after ? list_after->Count : -1;
  const auto latest_trigger        = fleet_runtime_diagnostics_snapshot();
  const auto defer_arrival_remove_repair =
      result && target_index_after >= 0 && count_before >= 0 && count_after_original >= count_before
      && ShouldDeferArrivalRemoveRepair(latest_trigger);
  const auto apply_remove_repair =
      result && target_index_after >= 0 && count_before >= 0 && count_after_original >= count_before
      && !defer_arrival_remove_repair;
  const auto applied_remove_repair  = apply_remove_repair && RemoveActionQueueListAt(list_after, target_index_after);
  const auto count_after_repair     = list_after ? list_after->Count : -1;
  const auto sticky_target_before   = StickyActionQueueTargetId(target_instance_before);
  auto       cleared_engaging_state = false;
  if (applied_remove_repair) {
    if (index) {
      *index = target_index_after;
    }

    if (sticky_target_before == target_id) {
      ClearStickyActionQueueTarget(_this, target_instance_before, "RemoveActionFromQueue", "confirmed-remove-target");
    }

    const auto should_clear_engaging_state = ShouldClearEngagingAfterManualRemove(count_after_repair, latest_trigger);
    if (should_clear_engaging_state) {
      cleared_engaging_state = ClearActionQueueEngagingState(_this, target_instance_after, "RemoveActionFromQueue",
                                                             "clear-engaging-after-manual-remove", false);
    } else {
      AppendActionQueueProbeJsonlIfEnabled([&]() {
        return nlohmann::json{{"phase", "repair"},
                              {"hook", "RemoveActionFromQueue"},
                              {"repair", "preserve-engaging-after-arrival-remove"},
                              {"target_id", target_id},
                              {"count_after_repair", count_after_repair},
                              {"latest_trigger", FleetRuntimeTriggerJson(latest_trigger)},
                              {"instance", ActionQueueInstanceJson(target_instance_after)},
                              {"slots", ActionQueueSlotsJson(_this)}};
      });
    }

    AppendActionQueueProbeJsonlIfEnabled([&]() {
      return nlohmann::json{{"phase", "repair"},
                            {"hook", "RemoveActionFromQueue"},
                            {"repair", "remove-stuck-queue-item"},
                            {"target_id", target_id},
                            {"target_index_before", target_index_before},
                            {"target_index_after", target_index_after},
                            {"count_before", count_before},
                            {"count_after_original", count_after_original},
                            {"count_after_repair", count_after_repair},
                            {"cleared_engaging_state", cleared_engaging_state},
                            {"latest_trigger", FleetRuntimeTriggerJson(latest_trigger)},
                            {"instance", ActionQueueInstanceJson(target_instance_after)},
                            {"slots", ActionQueueSlotsJson(_this)}};
    });

    if (ActionQueueProbeEnabled()) {
      spdlog::warn("[ActionQueueProbe] repair hook=RemoveActionFromQueue removed target={} index={} count {} -> {}",
                   target_id, target_index_after, count_after_original, count_after_repair);
    }
  }
  if (defer_arrival_remove_repair) {
    AppendActionQueueProbeJsonlIfEnabled([&]() {
      return nlohmann::json{{"phase", "repair"},
                            {"hook", "RemoveActionFromQueue"},
                            {"repair", "defer-arrival-remove-repair"},
                            {"target_id", target_id},
                            {"target_index_before", target_index_before},
                            {"target_index_after", target_index_after},
                            {"count_before", count_before},
                            {"count_after_original", count_after_original},
                            {"latest_trigger", FleetRuntimeTriggerJson(latest_trigger)},
                            {"instance", ActionQueueInstanceJson(target_instance_after)},
                            {"slots", ActionQueueSlotsJson(_this)}};
    });

    if (ActionQueueProbeEnabled()) {
      spdlog::warn("[ActionQueueProbe] repair hook=RemoveActionFromQueue deferred arrival remove target={} index={} "
                   "count_after_original={}",
                   target_id, target_index_after, count_after_original);
    }
  }
  if (!applied_remove_repair && result && target_index_after < 0
      && ShouldClearEngagingForRemovedMissingTarget(latest_trigger)) {
    if (sticky_target_before == target_id) {
      ClearStickyActionQueueTarget(_this, target_instance_before, "RemoveActionFromQueue",
                                   "confirmed-missing-target-remove");
    }
    auto active_instance   = FindActionQueueInstanceWithLastTarget(_this, target_id);
    cleared_engaging_state = ClearActionQueueEngagingState(_this, active_instance, "RemoveActionFromQueue",
                                                           "clear-engaging-after-missing-combat-end-remove", false);
  }

  if (result && count_after_repair <= 0) {
    ClearStickyActionQueueTarget(_this, target_instance_before, "RemoveActionFromQueue", "queue-empty-after-remove");
    ClearStickyActionQueueTarget(_this, target_instance_after, "RemoveActionFromQueue", "queue-empty-after-remove");
  } else {
    SyncStickyActionQueueTargetFromNative(_this, target_instance_after, "RemoveActionFromQueue", "native-after-remove");
  }

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    auto after                      = BuildActionQueueProbeEvent("after", "RemoveActionFromQueue", _this, fleet, nullptr);
    after["target_id"]              = target_id;
    after["result"]                 = result;
    after["index_ptr"]              = PtrValue(index);
    after["index"]                  = index ? *index : -1;
    after["target_index_before"]    = target_index_before;
    after["target_index_after"]     = target_index_after;
    after["count_before"]           = count_before;
    after["count_after_original"]   = count_after_original;
    after["count_after_repair"]     = count_after_repair;
    after["applied_remove_repair"]  = applied_remove_repair;
    after["deferred_arrival_remove_repair"] = defer_arrival_remove_repair;
    after["cleared_engaging_state"] = cleared_engaging_state;
    after["latest_trigger"]         = FleetRuntimeTriggerJson(latest_trigger);
    return after;
  });

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=result hook=RemoveActionFromQueue target={} result={} index={}", target_id,
                 result, index ? *index : -1);
  }
  LogActionQueueSnapshot("after", "RemoveActionFromQueue", _this, fleet);
  return result;
}

void ActionQueueManager_ClearQueue(auto original, ActionQueueManager* _this, FleetPlayerData* fleet)
{
  AppendActionQueueProbeJsonl(BuildActionQueueProbeEvent("before", "ClearQueue", _this, fleet, nullptr));
  LogActionQueueSnapshot("before", "ClearQueue", _this, fleet);
  original(_this, fleet);
  ClearStickyActionQueueTargetsForFleet(_this, fleet, "ClearQueue", "native-clear-queue");
  LogActionQueueSnapshot("after", "ClearQueue", _this, fleet);
  AppendActionQueueProbeJsonl(BuildActionQueueProbeEvent("after", "ClearQueue", _this, fleet, nullptr));
}

void ActionQueueManager_CheckToClearActionQueue(auto original, ActionQueueManager* _this, FleetPlayerData* fleet)
{
  AppendActionQueueProbeJsonl(BuildActionQueueProbeEvent("before", "CheckToClearActionQueue", _this, fleet, nullptr));
  LogActionQueueSnapshot("before", "CheckToClearActionQueue", _this, fleet);
  original(_this, fleet);
  LogActionQueueSnapshot("after", "CheckToClearActionQueue", _this, fleet);
  AppendActionQueueProbeJsonl(BuildActionQueueProbeEvent("after", "CheckToClearActionQueue", _this, fleet, nullptr));
}

void ActionQueueManager_ClearQueueAndMove(auto original, ActionQueueManager* _this, FleetPlayerData* fleet,
                                          Vector3 position)
{
  auto before        = BuildActionQueueProbeEvent("before", "ClearQueueAndMove", _this, fleet, nullptr);
  before["position"] = {{"x", position.x}, {"y", position.y}, {"z", position.z}};
  AppendActionQueueProbeJsonl(std::move(before));

  LogActionQueueSnapshot("before", "ClearQueueAndMove", _this, fleet);
  original(_this, fleet, position);
  ClearStickyActionQueueTargetsForFleet(_this, fleet, "ClearQueueAndMove", "native-clear-queue-and-move");
  LogActionQueueSnapshot("after", "ClearQueueAndMove", _this, fleet);

  auto after        = BuildActionQueueProbeEvent("after", "ClearQueueAndMove", _this, fleet, nullptr);
  after["position"] = {{"x", position.x}, {"y", position.y}, {"z", position.z}};
  AppendActionQueueProbeJsonl(std::move(after));
}

int ActionQueueManager_TryPlanPathAndEngageTarget(auto original, ActionQueueManager* _this, FleetPlayerData* fleet,
                                                  void* action_queue_instance)
{
  constexpr int kEngageResultSuccess = 0;

  const auto head_target_before = ActionQueueInstanceHeadTargetId(action_queue_instance);
  const auto count_before       = ActionQueueProbeEnabled() ? ActionQueueInstanceCount(action_queue_instance) : -1;
  AppendActionQueueProbeJsonlIfEnabled([&]() {
    return BuildActionQueueProbeEvent("before", "TryPlanPathAndEngageTarget", _this, fleet, action_queue_instance);
  });
  LogActionQueueSnapshot("before", "TryPlanPathAndEngageTarget", _this, fleet);
  LogActionQueueInstance("before", "TryPlanPathAndEngageTarget", action_queue_instance);
  const auto result = original(_this, fleet, action_queue_instance);

  const auto last_target_after_original = ActionQueueInstanceLastTargetId(action_queue_instance);
  const auto applied_last_target_repair = result == kEngageResultSuccess && head_target_before != 0
                                          && ActionQueueInstanceIsEngaging(action_queue_instance)
                                          && last_target_after_original == 0;
  if (applied_last_target_repair) {
    SetActionQueueInstanceLastTargetId(action_queue_instance, head_target_before);

    AppendActionQueueProbeJsonlIfEnabled([&]() {
      return nlohmann::json{{"phase", "repair"},
                            {"hook", "TryPlanPathAndEngageTarget"},
                            {"repair", "set-last-engaged-target"},
                            {"target_id", head_target_before},
                            {"count_before", count_before},
                            {"last_target_before", last_target_after_original},
                            {"instance", ActionQueueInstanceJson(action_queue_instance)},
                            {"slots", ActionQueueSlotsJson(_this)}};
    });

    if (ActionQueueProbeEnabled()) {
      spdlog::warn(
          "[ActionQueueProbe] repair hook=TryPlanPathAndEngageTarget set LastEngagedTargetId={} count_before={}",
          head_target_before, count_before);
    }
  }

  const auto sticky_target_id = ActionQueueInstanceLastTargetId(action_queue_instance);
  if (result == kEngageResultSuccess && ActionQueueInstanceIsEngaging(action_queue_instance) && sticky_target_id != 0) {
    LatchStickyActionQueueTarget(_this, action_queue_instance, sticky_target_id, "TryPlanPathAndEngageTarget",
                                 applied_last_target_repair ? "repair-set-last-target" : "native-engage-target");
  }

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    auto after = BuildActionQueueProbeEvent("after", "TryPlanPathAndEngageTarget", _this, fleet, action_queue_instance);
    after["result"]                     = result;
    after["head_target_before"]         = head_target_before;
    after["last_target_after_original"] = last_target_after_original;
    after["applied_last_target_repair"] = applied_last_target_repair;
    return after;
  });

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=result hook=TryPlanPathAndEngageTarget result={}", result);
  }
  LogActionQueueSnapshot("after", "TryPlanPathAndEngageTarget", _this, fleet);
  LogActionQueueInstance("after", "TryPlanPathAndEngageTarget", action_queue_instance);
  return result;
}

void ActionQueueManager_HandleStall(auto original, ActionQueueManager* _this, void* action_queue_instance,
                                    FleetPlayerData* fleet, FleetDeployedData* deployed_data)
{
  AppendActionQueueProbeJsonlIfEnabled([&]() {
    auto before            = BuildActionQueueProbeEvent("before", "HandleStall", _this, fleet, action_queue_instance);
    before["deployed_id"]  = FleetDeployedId(deployed_data);
    before["deployed_ptr"] = PtrValue(deployed_data);
    return before;
  });

  LogActionQueueSnapshot("before", "HandleStall", _this, fleet);
  LogActionQueueInstance("before", "HandleStall", action_queue_instance);
  const auto latest_trigger = fleet_runtime_diagnostics_snapshot();
  SyncStickyActionQueueTargetFromNative(_this, action_queue_instance, "HandleStall", "native-before-stall");
  const auto queue_count             = ActionQueueInstanceCount(action_queue_instance);
  const auto head_target_id          = ActionQueueInstanceHeadTargetId(action_queue_instance);
  const auto repaired_last_target_id =
      RepairActionQueueInstanceLastTargetFromHead(_this, action_queue_instance, "HandleStall",
                                                  "set-last-engaged-target-from-live-head");
  const auto has_live_queue_head_target = queue_count > 0 && head_target_id != 0;
  const auto should_preserve_orphaned_engaging =
      !repaired_last_target_id && action_queue_instance && queue_count > 0
      && ActionQueueInstanceIsEngaging(action_queue_instance)
      && ActionQueueInstanceLastTargetId(action_queue_instance) == 0
      && (has_live_queue_head_target || ShouldPreserveOrphanedEngagingInStall(latest_trigger));
  auto cleared_orphaned_engaging_state = false;
  if (should_preserve_orphaned_engaging) {
    AppendActionQueueProbeJsonlIfEnabled([&]() {
      return nlohmann::json{{"phase", "repair"},
                            {"hook", "HandleStall"},
                            {"repair", has_live_queue_head_target ? "preserve-orphaned-engaging-with-live-head"
                                                                  : "preserve-orphaned-engaging-during-grace-window"},
                            {"latest_trigger", FleetRuntimeTriggerJson(latest_trigger)},
                            {"instance", ActionQueueInstanceJson(action_queue_instance)},
                            {"slots", ActionQueueSlotsJson(_this)}};
    });
  } else {
    cleared_orphaned_engaging_state =
        ClearActionQueueEngagingState(_this, action_queue_instance, "HandleStall", "clear-orphaned-engaging", true);
  }
  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=HandleStall deployed={} cleared_orphaned_engaging={} "
                 "preserved_orphaned_engaging={} latest_trigger={} age_ms={}",
                 FleetDeployedId(deployed_data), cleared_orphaned_engaging_state, should_preserve_orphaned_engaging,
                 latest_trigger.latestTriggerSource, FleetRuntimeTriggerAgeMs(latest_trigger));
  }
  original(_this, action_queue_instance, fleet, deployed_data);
  SyncStickyActionQueueTargetFromNative(_this, action_queue_instance, "HandleStall", "native-after-stall");
  LogActionQueueSnapshot("after", "HandleStall", _this, fleet);
  LogActionQueueInstance("after", "HandleStall", action_queue_instance);

  AppendActionQueueProbeJsonlIfEnabled([&]() {
    auto after            = BuildActionQueueProbeEvent("after", "HandleStall", _this, fleet, action_queue_instance);
    after["deployed_id"]  = FleetDeployedId(deployed_data);
    after["deployed_ptr"] = PtrValue(deployed_data);
    return after;
  });
}

void ActionQueueManager_ProcessQueueDeployed(auto original, ActionQueueManager* _this, FleetDeployedData* deployed_data,
                                             bool can_select_new_target)
{
  auto before                     = BuildActionQueueProbeEvent("before", "ProcessQueue.deployed", _this, nullptr, nullptr);
  before["deployed_id"]           = FleetDeployedId(deployed_data);
  before["deployed_ptr"]          = PtrValue(deployed_data);
  before["can_select_new_target"] = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=ProcessQueue.deployed manager={} deployed={} can_select_new={}",
                 static_cast<void*>(_this), FleetDeployedId(deployed_data), can_select_new_target);
  }
  original(_this, deployed_data, can_select_new_target);

  auto after                      = BuildActionQueueProbeEvent("after", "ProcessQueue.deployed", _this, nullptr, nullptr);
  after["deployed_id"]            = FleetDeployedId(deployed_data);
  after["deployed_ptr"]           = PtrValue(deployed_data);
  after["can_select_new_target"]  = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=ProcessQueue.deployed manager={} deployed={} can_select_new={}",
                 static_cast<void*>(_this), FleetDeployedId(deployed_data), can_select_new_target);
  }
}

void ActionQueueManager_ProcessQueueTarget(auto original, ActionQueueManager* _this, long target_id,
                                           bool can_select_new_target)
{
  auto before                     = BuildActionQueueProbeEvent("before", "ProcessQueue.target", _this, nullptr, nullptr);
  before["target_id"]             = target_id;
  before["can_select_new_target"] = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=ProcessQueue.target manager={} target={} can_select_new={}",
                 static_cast<void*>(_this), target_id, can_select_new_target);
  }
  original(_this, target_id, can_select_new_target);

  auto after                      = BuildActionQueueProbeEvent("after", "ProcessQueue.target", _this, nullptr, nullptr);
  after["target_id"]              = target_id;
  after["can_select_new_target"]  = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=ProcessQueue.target manager={} target={} can_select_new={}",
                 static_cast<void*>(_this), target_id, can_select_new_target);
  }
}

void ActionQueueManager_OnStrikeCompleteEventHandler(auto original, ActionQueueManager* _this, void* strike_data)
{
  auto before      = BuildActionQueueProbeEvent("before", "OnStrikeComplete", _this, nullptr, nullptr);
  before["strike"] = PtrValue(strike_data);
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=OnStrikeComplete manager={} strike={} any_queue={}",
                 static_cast<void*>(_this), strike_data, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
  original(_this, strike_data);

  auto after      = BuildActionQueueProbeEvent("after", "OnStrikeComplete", _this, nullptr, nullptr);
  after["strike"] = PtrValue(strike_data);
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=OnStrikeComplete manager={} strike={} any_queue={}",
                 static_cast<void*>(_this), strike_data, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
}

void ActionQueueManager_OnSetCourseResponseEventHandler(auto original, ActionQueueManager* _this, void* args)
{
  auto before    = BuildActionQueueProbeEvent("before", "OnSetCourseResponse", _this, nullptr, nullptr);
  before["args"] = PtrValue(args);
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=OnSetCourseResponse manager={} args={} any_queue={}",
                 static_cast<void*>(_this), args, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
  original(_this, args);

  auto after    = BuildActionQueueProbeEvent("after", "OnSetCourseResponse", _this, nullptr, nullptr);
  after["args"] = PtrValue(args);
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=OnSetCourseResponse manager={} args={} any_queue={}",
                 static_cast<void*>(_this), args, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
}

void ActionQueueManager_OnPlayerFleetStateChangedEventHandler(auto original, ActionQueueManager* _this, void* fleets)
{
  auto before      = BuildActionQueueProbeEvent("before", "OnPlayerFleetStateChanged", _this, nullptr, nullptr);
  before["fleets"] = PtrValue(fleets);
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=OnPlayerFleetStateChanged manager={} fleets={} any_queue={}",
                 static_cast<void*>(_this), fleets, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
  original(_this, fleets);

  auto after      = BuildActionQueueProbeEvent("after", "OnPlayerFleetStateChanged", _this, nullptr, nullptr);
  after["fleets"] = PtrValue(fleets);
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=OnPlayerFleetStateChanged manager={} fleets={} any_queue={}",
                 static_cast<void*>(_this), fleets, _this ? _this->AnyPlayerFleetInQueue() : false);
  }
}

void ActionQueueEvents_TriggerActionAddedToQueueEvent(auto original)
{
  AppendActionQueueProbeJsonl({{"phase", "before"}, {"hook", "TriggerActionAddedToQueueEvent"}});
  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=TriggerActionAddedToQueueEvent");
  }
  original();
  AppendActionQueueProbeJsonl({{"phase", "after"}, {"hook", "TriggerActionAddedToQueueEvent"}});
  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=TriggerActionAddedToQueueEvent");
  }
}

void ActionQueueEvents_TriggerActionRemovedFromQueueEvent(auto original)
{
  AppendActionQueueProbeJsonl({{"phase", "before"}, {"hook", "TriggerActionRemovedFromQueueEvent"}});
  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=TriggerActionRemovedFromQueueEvent");
  }
  original();
  AppendActionQueueProbeJsonl({{"phase", "after"}, {"hook", "TriggerActionRemovedFromQueueEvent"}});
  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=TriggerActionRemovedFromQueueEvent");
  }
}

void InstallActionQueueRepairHooks()
{
  const auto install_action_queue_repairs     = ActionQueueRepairEnabled();
  const auto install_action_queue_diagnostics = ActionQueueProbeDetoursEnabled();

  if (ActionQueueProbeEnabled()) {
    ResetActionQueueProbeJsonl();
    spdlog::info("[ActionQueueProbe] install level={} repair_detours={} diagnostic_detours={}",
                 RuntimeTraceLevelName(RuntimeTraceLevelSetting()), install_action_queue_repairs,
                 install_action_queue_diagnostics);
    AppendActionQueueProbeJsonl({{"phase", "install"},
                                 {"hook", "InstallActionQueueRepairHooks"},
                                 {"level", RuntimeTraceLevelName(RuntimeTraceLevelSetting())},
                                 {"repair_detours", install_action_queue_repairs},
                                 {"diagnostic_detours", install_action_queue_diagnostics}});
  }

  if (!install_action_queue_repairs && !install_action_queue_diagnostics) {
    return;
  }

  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueMaanger");
  } else {
    AppendActionQueueProbeJsonlIfEnabled([]() {
      return nlohmann::json{{"phase", "install-skip"},
                            {"hook", "ActionQueueEvents"},
                            {"reason", "keep enqueue path untouched while probing drain callbacks"}};
    });

    AppendActionQueueProbeJsonlIfEnabled([]() {
      return nlohmann::json{{"phase", "install-skip"},
                            {"hook", "AddActionToQueue"},
                            {"reason", "detouring this method suppresses native enqueue on current build"}};
    });

    if (install_action_queue_repairs) {
      if (auto ptr = actionqueue_manager.GetMethod("RemoveActionFromQueue"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_RemoveActionFromQueue);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "RemoveActionFromQueue");
      }

      if (auto ptr = actionqueue_manager.GetMethod("TryPlanPathAndEngageTarget"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_TryPlanPathAndEngageTarget);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "TryPlanPathAndEngageTarget");
      }

      if (auto ptr = actionqueue_manager.GetMethod("HandleStall"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_HandleStall);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "HandleStall");
      }
    }

    if (install_action_queue_diagnostics) {
      if (auto ptr = actionqueue_manager.GetMethod("ClearQueue"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_ClearQueue);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "ClearQueue");
      }

      if (auto ptr = actionqueue_manager.GetMethod("CheckToClearActionQueue"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_CheckToClearActionQueue);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "CheckToClearActionQueue");
      }

      if (auto ptr = actionqueue_manager.GetMethod("ClearQueueAndMove"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_ClearQueueAndMove);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "ClearQueueAndMove");
      }

      auto ptr_process_deployed = actionqueue_manager.GetMethodSpecial("ProcessQueue", [](int param_count,
                                                                                          const Il2CppType** params) {
        return param_count == 2 && probe::detail::type_name(params[0]).find("FleetDeployedData") != std::string::npos;
      });
      if (ptr_process_deployed) {
        SPUD_STATIC_DETOUR(ptr_process_deployed, ActionQueueManager_ProcessQueueDeployed);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(FleetDeployedData, bool)");
      }

      auto ptr_process_target =
          actionqueue_manager.GetMethodSpecial("ProcessQueue", [](int param_count, const Il2CppType** params) {
            return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
          });
      if (ptr_process_target) {
        SPUD_STATIC_DETOUR(ptr_process_target, ActionQueueManager_ProcessQueueTarget);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(long, bool)");
      }

      if (auto ptr = actionqueue_manager.GetMethod("OnStrikeCompleteEventHandler"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnStrikeCompleteEventHandler);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "OnStrikeCompleteEventHandler");
      }

      if (auto ptr = actionqueue_manager.GetMethod("OnSetCourseResponseEventHandler"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnSetCourseResponseEventHandler);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "OnSetCourseResponseEventHandler");
      }

      if (auto ptr = actionqueue_manager.GetMethod("OnPlayerFleetStateChangedEventHandler"); ptr) {
        SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnPlayerFleetStateChangedEventHandler);
      } else {
        ErrorMsg::MissingMethod("ActionQueueManager", "OnPlayerFleetStateChangedEventHandler");
      }
    }
  }
}
