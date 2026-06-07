/**
 * @file action_queue_repair.cc
 * @brief Focused Kir'shara action queue advancement repair hooks.
 *
 * Contains the narrow queue-skip repair hooks used to keep queued combat advancing when native queue state drops
 * target/completion bookkeeping.
 */
#include "config.h"
#include "diagnostics_file_policy.h"
#include "errormsg.h"
#include "file.h"
#include "patches/action_queue_repair_config.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/FleetsManager.h>
#include <prime/Hub.h>
#include <prime/IList.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <unordered_map>

#include <il2cpp/il2cpp_helper.h>

#include <nlohmann/json.hpp>

#include <spud/detour.h>

#include "probe/probe.h"

bool ActionQueueProbeEnabled()
{
  const auto level                  = RuntimeTraceLevelSetting();
  const auto detailed_runtime_trace = level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
  return BuildKirsharaQueueRepairInstallPlan(KirsharaQueueRepairSettings(), detailed_runtime_trace).emit_probe_logs;
}

bool ActionQueueProbeDetoursEnabled()
{ return false; }

bool ActionQueueRepairEnabled()
{
  const auto level                  = RuntimeTraceLevelSetting();
  const auto detailed_runtime_trace = level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
  return BuildKirsharaQueueRepairInstallPlan(KirsharaQueueRepairSettings(), detailed_runtime_trace)
      .install_repair_hooks;
}

constexpr char kActionQueueProbeJsonlFile[] = "community_patch_action_queue_probe.jsonl";

nlohmann::json ActionQueueInstanceJson(void* action_queue_instance);
nlohmann::json ActionQueueSlotsJson(ActionQueueManager* manager);
std::uint64_t  ActionQueueInstanceFleetId(void* action_queue_instance);
std::int64_t   ActionQueueInstanceLastTargetId(void* action_queue_instance);
std::int64_t   ActionQueueInstanceHeadTargetId(void* action_queue_instance);
bool           ActionQueueInstanceIsEngaging(void* action_queue_instance);
int            ActionQueueInstanceCount(void* action_queue_instance);
int            FindActionQueueItemIndex(IList* list, std::int64_t target_id);
IList*         ProbeActionQueueInstanceList(void* action_queue_instance);
bool TryGetNativeActionQueueInstance(ActionQueueManager* manager, FleetPlayerData* fleet, void** action_queue_instance);
void* FindActionQueueInstanceForFleet(ActionQueueManager* manager, FleetPlayerData* fleet);

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
{ return std::max(1, AdvancedDiagnosticsFileSettings().action_queue_probe_files); }

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

std::uintptr_t PtrValue(const void* ptr)
{ return reinterpret_cast<std::uintptr_t>(ptr); }

std::uint64_t NextKirsharaQueueMarkerSeq()
{
  static std::atomic<std::uint64_t> seq{0};
  return seq.fetch_add(1, std::memory_order_relaxed) + 1;
}

bool KirsharaQueueMarkerEnabled(bool KirsharaQueueDiagnosticsConfig::* flag)
{
  const auto& diagnostics = KirsharaQueueRepairSettings().diagnostics;
  return diagnostics.enabled && diagnostics.*flag;
}

void DumpInterestingActionQueueManagerMethodsOnce()
{
  static bool dumped = false;
  if (dumped) {
    return;
  }
  dumped = true;

  auto  class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  auto* klass        = class_helper.get_cls();
  if (!klass) {
    spdlog::warn("[KirsharaQueueMarker] unable to dump ActionQueueManager methods: class not found");
    return;
  }

  constexpr const char* kInterestingNameParts[] = {"Queue",  "Clear",  "Battle", "Combat", "Complete",
                                                   "Strike", "Target", "Fleet",  "Course", "Stall"};

  spdlog::info("[KirsharaQueueMarker] interesting ActionQueueManager methods:");
  void* iter = nullptr;
  while (const MethodInfo* method = il2cpp_class_get_methods(klass, &iter)) {
    if (!method || !method->name) {
      continue;
    }

    auto interesting = false;
    for (const auto* part : kInterestingNameParts) {
      if (std::strstr(method->name, part) != nullptr) {
        interesting = true;
        break;
      }
    }
    if (!interesting) {
      continue;
    }

    spdlog::info("[KirsharaQueueMarker]   {}", probe::detail::method_signature(method));
  }
}

std::string KirsharaQueueMarkerViewSummary()
{
  auto* section_manager = Hub::get_SectionManager();
  if (!section_manager) {
    return "section=unavailable is_system=0 is_galaxy=0";
  }

  const auto section = section_manager->CurrentSection;
  return std::string("section=") + std::to_string(static_cast<int64_t>(section))
         + " is_system=" + std::to_string(section == SectionID::Navigation_System ? 1 : 0)
         + " is_galaxy=" + std::to_string(section == SectionID::Navigation_Galaxy ? 1 : 0);
}

void LogFleetQueueStateSummary(const char* hook, const char* phase, ActionQueueManager* manager)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!manager || !fleets_manager) {
    spdlog::info(
        "[KirsharaQueueMarker] phase={} hook={} view={} fleet_summary=unavailable manager={} fleets_manager={}", phase,
        hook, KirsharaQueueMarkerViewSummary(), PtrValue(manager), PtrValue(fleets_manager));
    return;
  }

  std::ostringstream summary;
  auto               wrote_any = false;
  for (int index = 0; index < 8; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (!fleet) {
      continue;
    }

    int  reason                = -1;
    auto count                 = manager->GetActionQueueCount(fleet);
    auto state                 = manager->GetActionQueueState(fleet, &reason);
    auto in_queue              = manager->IsFleetInQueue(fleet);
    auto action_queue_instance = (count > 0 || in_queue) ? FindActionQueueInstanceForFleet(manager, fleet) : nullptr;
    const auto instance_count  = action_queue_instance ? ActionQueueInstanceCount(action_queue_instance) : -1;
    const auto head_target_id  = action_queue_instance ? ActionQueueInstanceHeadTargetId(action_queue_instance) : 0;
    const auto last_target_id  = action_queue_instance ? ActionQueueInstanceLastTargetId(action_queue_instance) : 0;
    const auto is_engaging     = action_queue_instance ? ActionQueueInstanceIsEngaging(action_queue_instance) : false;

    if (wrote_any) {
      summary << "; ";
    }
    wrote_any = true;
    summary << "slot=" << index << " id=" << fleet->Id << " state=" << static_cast<int>(fleet->CurrentState)
            << " prev=" << static_cast<int>(fleet->PreviousState) << " count=" << count << " qstate=" << state
            << " reason=" << reason << " inq=" << in_queue;
    if (count > 0 || in_queue || action_queue_instance) {
      summary << " inst_count=" << instance_count << " head=" << head_target_id << " last=" << last_target_id
              << " engaging=" << is_engaging;
    }
  }

  spdlog::info("[KirsharaQueueMarker] phase={} hook={} view={} fleet_summary={}", phase, hook,
               KirsharaQueueMarkerViewSummary(), wrote_any ? summary.str() : "none");
}

void AppendActionQueueProbeJsonl(nlohmann::json event)
{
  if (!ActionQueueProbeEnabled()) {
    return;
  }

  event["ts_ms"]     = ActionQueueProbeTimestampMs();
  const auto payload = event.dump();

  std::lock_guard lk(ActionQueueProbeJsonlMutex());
  const auto&     path    = ActionQueueProbeJsonlPath();
  const auto      prepare = PrepareDiagnosticsFileForAppend(path, ActionQueueProbeJsonlMaxBytes(),
                                                            ActionQueueProbeJsonlTotalFiles(), payload.size() + 1);
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

std::uint64_t ActionQueueInstanceFleetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("<PlayerFleetId>k__BackingField").offset();
  return *(std::uint64_t*)((char*)action_queue_instance + field);
}

std::int64_t ActionQueueInstanceLastTargetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("LastEngagedTargetId").offset();
  return *(std::int64_t*)((char*)action_queue_instance + field);
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

std::int64_t ProbeQueueableActionFleetId(Il2CppObject* queueable_action)
{
  if (!queueable_action) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field = class_helper.GetField("<FleetId>k__BackingField").offset();
  return *(std::int64_t*)((char*)queueable_action + field);
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

std::int64_t ActionQueueInstanceHeadTargetId(void* action_queue_instance)
{
  auto list = ProbeActionQueueInstanceList(action_queue_instance);
  if (!list || list->Count <= 0) {
    return 0;
  }

  return ProbeQueueableActionFleetId(list->Get(0));
}

int FindActionQueueItemIndex(IList* list, std::int64_t target_id)
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

void* FindActionQueueInstanceForFleet(ActionQueueManager* manager, FleetPlayerData* fleet)
{
  if (!manager || !fleet) {
    return nullptr;
  }

  void* native_action_queue_instance = nullptr;
  if (TryGetNativeActionQueueInstance(manager, fleet, &native_action_queue_instance) && native_action_queue_instance) {
    return native_action_queue_instance;
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
    if (ActionQueueInstanceFleetId(action_queue_instance) == fleet->Id) {
      return action_queue_instance;
    }
  }

  return nullptr;
}

bool TryGetNativeActionQueueInstance(ActionQueueManager* manager, FleetPlayerData* fleet, void** action_queue_instance)
{
  if (action_queue_instance) {
    *action_queue_instance = nullptr;
  }
  if (!manager || !fleet) {
    return false;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return false;
  }

  static const MethodInfo* try_get_action_queue_instance =
      class_helper.GetMethodInfoSpecial("TryGetActionQueueInstance", [](int param_count, const Il2CppType** params) {
        return param_count == 2 && params && params[0]
               && probe::detail::type_name(params[0]).find("FleetPlayerData") != std::string::npos;
      });
  if (!try_get_action_queue_instance) {
    return false;
  }

  void*            instance  = nullptr;
  void*            args[2]   = {fleet, &instance};
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(try_get_action_queue_instance, manager, args, &exception);
  if (exception || !result) {
    return false;
  }

  const auto success = *reinterpret_cast<bool*>(il2cpp_object_unbox(result));
  if (success && action_queue_instance) {
    *action_queue_instance = instance;
  }
  return success;
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

std::int64_t FleetDeployedId(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->ID : 0; }

int FleetDeployedState(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->CurrentState : -1; }

int FleetDeployedPreviousState(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->PreviousState : -1; }

int FleetDeployedType(FleetDeployedData* deployed_data)
{ return deployed_data ? static_cast<int>(deployed_data->FleetType) : -1; }

bool FleetDeployedIsDestroyed(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->IsDestroyed : false; }

bool FleetDeployedCurrentlyBattling(FleetDeployedData* deployed_data)
{ return deployed_data ? deployed_data->CurrentlyBattling : false; }

struct CourseTargetDeployedFleetId {
  bool         present = false;
  std::int64_t value   = 0;
};

struct CourseTargetCompletionCandidate {
  std::int64_t target_id     = 0;
  std::int64_t updated_at_ms = 0;
  bool         consumed      = false;
};

struct PlayerOnlyBattleStartCandidate {
  std::int64_t  deployed_id   = 0;
  std::int64_t  updated_at_ms = 0;
  std::uint64_t seq           = 0;
};

struct FleetDeployedDataSnapshot {
  bool         present             = false;
  std::int64_t id                  = 0;
  int          state               = -1;
  int          previous_state      = -1;
  int          type                = -1;
  bool         destroyed           = false;
  bool         currently_battling  = false;
  bool         player_combat_start = false;
};

struct CourseTargetCompletionSynthesis {
  bool          should_synthesize = false;
  std::int64_t  fleet_key         = 0;
  std::int64_t  deployed_id       = 0;
  std::int64_t  target_id         = 0;
  std::int64_t  course_age_ms     = 0;
  std::int64_t  battle_age_ms     = 0;
  std::uint64_t battle_seq        = 0;
  int           queue_count       = -1;
  int           target_index      = -1;
};

CourseTargetDeployedFleetId ReadCourseDataTargetDeployedFleetId(void* course_data)
{
  if (!course_data) {
    return {};
  }

  static auto class_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseData");
  if (!class_helper.isValidHelper()) {
    return {};
  }

  static auto field_offset = class_helper.GetField("<TargetDeployedFleetId>k__BackingField").offset();
  auto*       nullable     = reinterpret_cast<const char*>(course_data) + field_offset;
  const auto  present      = *reinterpret_cast<const bool*>(nullable);
  const auto  value        = present ? *reinterpret_cast<const std::int64_t*>(nullable + 8) : 0;
  return {present, value};
}

std::mutex& CourseTargetCompletionMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::int64_t, CourseTargetCompletionCandidate>& CourseTargetCompletionTargets()
{
  static std::unordered_map<std::int64_t, CourseTargetCompletionCandidate> targets;
  return targets;
}

std::unordered_map<std::int64_t, PlayerOnlyBattleStartCandidate>& CourseTargetCompletionBattleStarts()
{
  static std::unordered_map<std::int64_t, PlayerOnlyBattleStartCandidate> starts;
  return starts;
}

bool CourseTargetCompletionEnabled()
{
  const auto level                  = RuntimeTraceLevelSetting();
  const auto detailed_runtime_trace = level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
  return BuildKirsharaQueueRepairInstallPlan(KirsharaQueueRepairSettings(), detailed_runtime_trace)
      .install_course_target_completion;
}

std::int64_t AgeMs(std::int64_t now, std::int64_t previous)
{ return now >= previous ? now - previous : previous - now; }

bool IsPlayerCombatStart(FleetDeployedData* deployed_data)
{
  if (!deployed_data) {
    return false;
  }

  const auto previous_state = FleetDeployedPreviousState(deployed_data);
  return FleetDeployedType(deployed_data) == static_cast<int>(DeployedFleetType::Player)
         && FleetDeployedState(deployed_data) == 6 && (previous_state == 0 || previous_state == 1)
         && FleetDeployedCurrentlyBattling(deployed_data) && !FleetDeployedIsDestroyed(deployed_data);
}

FleetDeployedDataSnapshot SnapshotFleetDeployedData(FleetDeployedData* deployed_data)
{
  FleetDeployedDataSnapshot snapshot;
  if (!deployed_data) {
    return snapshot;
  }

  snapshot.present            = true;
  snapshot.id                 = FleetDeployedId(deployed_data);
  snapshot.state              = FleetDeployedState(deployed_data);
  snapshot.previous_state     = FleetDeployedPreviousState(deployed_data);
  snapshot.type               = FleetDeployedType(deployed_data);
  snapshot.destroyed          = FleetDeployedIsDestroyed(deployed_data);
  snapshot.currently_battling = FleetDeployedCurrentlyBattling(deployed_data);
  snapshot.player_combat_start =
      snapshot.type == static_cast<int>(DeployedFleetType::Player) && snapshot.state == 6
      && (snapshot.previous_state == 0 || snapshot.previous_state == 1) && snapshot.currently_battling
      && !snapshot.destroyed;
  return snapshot;
}

FleetPlayerData* FindPlayerFleetDataById(std::int64_t fleet_id)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager || fleet_id == 0) {
    return nullptr;
  }

  for (int index = 0; index < 8; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (fleet && fleet->Id == fleet_id) {
      return fleet;
    }
  }

  return nullptr;
}

FleetPlayerData* FindPlayerFleetDataByCourseFleetId(std::int64_t fleet_id)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager || fleet_id == 0) {
    return nullptr;
  }

  const auto low_fleet_id = static_cast<std::uint32_t>(static_cast<std::uint64_t>(fleet_id));
  auto*      low_match    = static_cast<FleetPlayerData*>(nullptr);
  for (int index = 0; index < 8; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (!fleet) {
      continue;
    }

    if (fleet->Id == fleet_id) {
      return fleet;
    }

    if (static_cast<std::uint32_t>(fleet->Id) == low_fleet_id) {
      low_match = fleet;
    }
  }

  return low_match;
}

struct CourseTargetQueueGuard {
  bool  relevant     = false;
  int   queue_count  = -1;
  int   target_index = -1;
  void* instance     = nullptr;
};

CourseTargetQueueGuard CheckCourseTargetStillQueued(ActionQueueManager* manager, std::int64_t deployed_id,
                                                    std::int64_t target_id)
{
  CourseTargetQueueGuard guard;
  if (!manager || target_id == 0) {
    return guard;
  }

  auto* fleet = FindPlayerFleetDataById(deployed_id);
  if (!fleet || !manager->IsFleetInQueue(fleet)) {
    return guard;
  }

  guard.queue_count = manager->GetActionQueueCount(fleet);
  if (guard.queue_count <= 0) {
    return guard;
  }

  guard.instance     = FindActionQueueInstanceForFleet(manager, fleet);
  auto* list         = ProbeActionQueueInstanceList(guard.instance);
  guard.target_index = FindActionQueueItemIndex(list, target_id);
  guard.relevant     = guard.target_index >= 0;
  return guard;
}

void LatchCourseTargetCompletionTarget(std::int64_t fleet_id, std::int64_t target_id, const char* source)
{
  if (!CourseTargetCompletionEnabled() || target_id == 0) {
    return;
  }

  auto*      fleet              = FindPlayerFleetDataByCourseFleetId(fleet_id);
  const auto normalized_fleet_id = fleet ? static_cast<std::int64_t>(fleet->Id) : fleet_id;
  const auto key                 = normalized_fleet_id;
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    auto&           state = CourseTargetCompletionTargets()[key];
    state.target_id       = target_id;
    state.updated_at_ms   = ActionQueueProbeTimestampMs();
    state.consumed        = false;
  }

  spdlog::info("[KirsharaQueueRepair] repair=course-target-completion phase=latch-course-target fleet_key={} "
               "fleet={} normalized_fleet={} target={} source={}",
               key, fleet_id, normalized_fleet_id, target_id, source ? source : "");
}

void LatchPlayerOnlyBattleStart(void* fleets, std::uint64_t seq)
{
  if (!CourseTargetCompletionEnabled() || !fleets) {
    return;
  }

  auto* list = static_cast<IList*>(fleets);
  if (list->Count != 1) {
    return;
  }

  auto* deployed_data = reinterpret_cast<FleetDeployedData*>(list->Get(0));
  if (!IsPlayerCombatStart(deployed_data)) {
    return;
  }

  const auto deployed_id = FleetDeployedId(deployed_data);
  const auto key         = deployed_id;
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    CourseTargetCompletionBattleStarts()[key] = {
        .deployed_id   = deployed_id,
        .updated_at_ms = ActionQueueProbeTimestampMs(),
        .seq           = seq,
    };
  }

  spdlog::info("[KirsharaQueueRepair] repair=course-target-completion phase=latch-player-only-battle-start "
               "fleet_key={} deployed_id={} seq={}",
               key, deployed_id, seq);
}

bool MarkCourseTargetCompletionConsumed(std::int64_t fleet_key, const CourseTargetCompletionCandidate& expected)
{
  std::lock_guard lk(CourseTargetCompletionMutex());
  auto&           targets = CourseTargetCompletionTargets();
  const auto      target  = targets.find(fleet_key);
  if (target == targets.end() || target->second.target_id != expected.target_id
      || target->second.updated_at_ms != expected.updated_at_ms) {
    return false;
  }

  target->second.consumed = true;
  return true;
}

CourseTargetCompletionSynthesis TakeCourseTargetCompletionSynthesis(ActionQueueManager*                 manager,
                                                                    const FleetDeployedDataSnapshot& deployed)
{
  if (!CourseTargetCompletionEnabled() || !deployed.player_combat_start) {
    return {};
  }

  constexpr std::int64_t kBattleStartWindowMs  = 5000;
  constexpr std::int64_t kCourseTargetWindowMs = 300000;

  const auto now         = ActionQueueProbeTimestampMs();
  const auto deployed_id = deployed.id;
  const auto key         = deployed_id;

  CourseTargetCompletionCandidate  target_state;
  PlayerOnlyBattleStartCandidate   start_state;
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    auto&           targets = CourseTargetCompletionTargets();
    auto&           starts  = CourseTargetCompletionBattleStarts();
    const auto      target  = targets.find(key);
    const auto      start   = starts.find(key);
    if (target == targets.end() || start == starts.end() || target->second.consumed || target->second.target_id == 0) {
      return {};
    }
    target_state = target->second;
    start_state  = start->second;
  }

  const auto battle_age = AgeMs(now, start_state.updated_at_ms);
  const auto course_age = AgeMs(now, target_state.updated_at_ms);
  if (battle_age > kBattleStartWindowMs || course_age > kCourseTargetWindowMs) {
    return {};
  }

  const auto guard = CheckCourseTargetStillQueued(manager, deployed_id, target_state.target_id);
  if (!guard.relevant) {
    MarkCourseTargetCompletionConsumed(key, target_state);
    spdlog::info("[KirsharaQueueRepair] repair=course-target-completion phase=skip-synthesize-process-target "
                 "reason=target-not-queued fleet={} target={} queue_count={} target_index={} instance={}",
                 deployed_id, target_state.target_id, guard.queue_count, guard.target_index, PtrValue(guard.instance));
    return {};
  }

  MarkCourseTargetCompletionConsumed(key, target_state);
  return {
      .should_synthesize = true,
      .fleet_key         = key,
      .deployed_id       = deployed_id,
      .target_id         = target_state.target_id,
      .course_age_ms     = course_age,
      .battle_age_ms     = battle_age,
      .battle_seq        = start_state.seq,
      .queue_count       = guard.queue_count,
      .target_index      = guard.target_index,
  };
}

using ProcessQueueTargetMethod = void(ActionQueueManager*, std::int64_t, bool);

ProcessQueueTargetMethod* ResolveProcessQueueTargetForCompletion()
{
  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    return nullptr;
  }

  static auto method = actionqueue_manager.GetMethodSpecial<ProcessQueueTargetMethod>(
      "ProcessQueue", [](int param_count, const Il2CppType** params) {
        return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
      });
  return method;
}

std::string FleetDeployedListSummary(void* fleets)
{
  if (!fleets) {
    return "ptr=0 count=-1";
  }

  auto*              list = static_cast<IList*>(fleets);
  std::ostringstream out;
  const auto         count = list->Count;
  const auto         limit = std::min(std::max(count, 0), 8);

  out << "ptr=" << PtrValue(fleets) << " count=" << count;
  for (auto index = 0; index < limit; ++index) {
    auto* deployed_data = reinterpret_cast<FleetDeployedData*>(list->Get(index));
    out << " item" << index << "_ptr=" << PtrValue(deployed_data) << " item" << index
        << "_id=" << FleetDeployedId(deployed_data) << " item" << index
        << "_state=" << FleetDeployedState(deployed_data) << " item" << index
        << "_prev=" << FleetDeployedPreviousState(deployed_data) << " item" << index
        << "_type=" << FleetDeployedType(deployed_data) << " item" << index
        << "_destroyed=" << FleetDeployedIsDestroyed(deployed_data) << " item" << index
        << "_battling=" << FleetDeployedCurrentlyBattling(deployed_data);
  }
  if (count > limit) {
    out << " truncated=true";
  }
  return out.str();
}

void ActionQueueManager_RemoveTargetAndAttackNext_Marker(auto original, ActionQueueManager* _this,
                                                         void* action_queue_instance, Il2CppObject* target,
                                                         FleetPlayerData* fleet)
{
  const auto seq = NextKirsharaQueueMarkerSeq();

  spdlog::info(
      "[KirsharaQueueMarker] phase=before hook=RemoveTargetAndAttackNext seq={} manager={} instance={} target={} "
      "fleet={}",
      seq, PtrValue(_this), PtrValue(action_queue_instance), PtrValue(target), PtrValue(fleet));

  original(_this, action_queue_instance, target, fleet);

  spdlog::info(
      "[KirsharaQueueMarker] phase=after hook=RemoveTargetAndAttackNext seq={} manager={} instance={} target={} "
      "fleet={}",
      seq, PtrValue(_this), PtrValue(action_queue_instance), PtrValue(target), PtrValue(fleet));
}

void ActionQueueManager_CheckToClearActionQueue_Marker(auto original, ActionQueueManager* _this, FleetPlayerData* fleet)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=CheckToClearActionQueue seq={} manager={} fleet={}", seq,
               PtrValue(_this), PtrValue(fleet));

  original(_this, fleet);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=CheckToClearActionQueue seq={} manager={} fleet={}", seq,
               PtrValue(_this), PtrValue(fleet));
}

void ActionQueueManager_OnFleetStateChangeEventHandler_Marker(auto original, ActionQueueManager* _this, void* fleets)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  if (KirsharaQueueMarkerEnabled(&KirsharaQueueDiagnosticsConfig::on_fleet_state_change)) {
    const auto before_fleets = FleetDeployedListSummary(fleets);
    spdlog::info("[KirsharaQueueMarker] phase=before hook=OnFleetStateChange seq={} manager={} fleet_list={}", seq,
                 PtrValue(_this), before_fleets);
  }
  LatchPlayerOnlyBattleStart(fleets, seq);

  original(_this, fleets);

  if (KirsharaQueueMarkerEnabled(&KirsharaQueueDiagnosticsConfig::on_fleet_state_change)) {
    const auto after_fleets = FleetDeployedListSummary(fleets);
    spdlog::info("[KirsharaQueueMarker] phase=after hook=OnFleetStateChange seq={} manager={} fleet_list={}", seq,
                 PtrValue(_this), after_fleets);
  }
}

void ActionQueueManager_OnFleetsDisposedEventHandler_Marker(auto original, ActionQueueManager* _this, void* fleets)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=OnFleetsDisposed seq={} manager={} fleets={}", seq,
               PtrValue(_this), PtrValue(fleets));

  // Disposal paths can tear down fleet/queue objects underneath us. Keep this hook marker-only.

  original(_this, fleets);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=OnFleetsDisposed seq={} manager={} fleets={}", seq,
               PtrValue(_this), PtrValue(fleets));
}

void ActionQueueManager_OnStrikeCompleteEventHandler_Marker(auto original, ActionQueueManager* _this, void* strike_data)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=OnStrikeComplete seq={} manager={} strike={}", seq,
               PtrValue(_this), PtrValue(strike_data));

  original(_this, strike_data);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=OnStrikeComplete seq={} manager={} strike={}", seq,
               PtrValue(_this), PtrValue(strike_data));
}

void ActionQueueManager_ProcessQueueDeployed_Marker(auto original, ActionQueueManager* _this,
                                                    FleetDeployedData* deployed_data, bool can_select_new_target)
{
  const auto seq        = NextKirsharaQueueMarkerSeq();
  const auto log_marker = KirsharaQueueMarkerEnabled(&KirsharaQueueDiagnosticsConfig::process_queue_deployed);
  const auto deployed   = SnapshotFleetDeployedData(deployed_data);
  if (log_marker) {
    spdlog::info("[KirsharaQueueMarker] phase=before hook=ProcessQueue.deployed seq={} manager={} deployed={} "
                 "deployed_id={} state={} prev={} type={} destroyed={} battling={} can_select_new_target={}",
                 seq, PtrValue(_this), PtrValue(deployed_data), deployed.id, deployed.state, deployed.previous_state,
                 deployed.type, deployed.destroyed, deployed.currently_battling, can_select_new_target);
  }

  original(_this, deployed_data, can_select_new_target);

  const auto synthesis = TakeCourseTargetCompletionSynthesis(_this, deployed);
  if (synthesis.should_synthesize) {
    auto* process_target = ResolveProcessQueueTargetForCompletion();
    if (process_target) {
      spdlog::info("[KirsharaQueueRepair] repair=course-target-completion phase=synthesize-process-target seq={} "
                   "manager={} fleet_key={} deployed_id={} target={} course_age_ms={} battle_age_ms={} "
                   "battle_seq={} queue_count={} target_index={} can_select_new_target={}",
                   seq, PtrValue(_this), synthesis.fleet_key, synthesis.deployed_id, synthesis.target_id,
                   synthesis.course_age_ms, synthesis.battle_age_ms, synthesis.battle_seq, synthesis.queue_count,
                   synthesis.target_index, can_select_new_target);
      process_target(_this, synthesis.target_id, can_select_new_target);
    } else {
      spdlog::warn("[KirsharaQueueRepair] repair=course-target-completion phase=synthesize-process-target "
                   "result=missing-method seq={} manager={} fleet_key={} deployed_id={} target={}",
                   seq, PtrValue(_this), synthesis.fleet_key, synthesis.deployed_id, synthesis.target_id);
    }
  }

  if (log_marker) {
    spdlog::info("[KirsharaQueueMarker] phase=after hook=ProcessQueue.deployed seq={} manager={} deployed={} "
                 "deployed_id={} state={} prev={} type={} destroyed={} battling={} can_select_new_target={} "
                 "snapshot=before-original",
                 seq, PtrValue(_this), PtrValue(deployed_data), deployed.id, deployed.state, deployed.previous_state,
                 deployed.type, deployed.destroyed, deployed.currently_battling, can_select_new_target);
  }
}

bool ActionQueueManager_IsTargetValid_Marker(auto original, ActionQueueManager* _this, FleetDeployedData* marauder)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=IsTargetValid seq={} manager={} deployed={} deployed_id={} "
               "state={} prev={} type={} destroyed={} battling={}",
               seq, PtrValue(_this), PtrValue(marauder), FleetDeployedId(marauder), FleetDeployedState(marauder),
               FleetDeployedPreviousState(marauder), FleetDeployedType(marauder), FleetDeployedIsDestroyed(marauder),
               FleetDeployedCurrentlyBattling(marauder));

  const auto result = original(_this, marauder);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=IsTargetValid seq={} manager={} deployed={} deployed_id={} "
               "state={} prev={} type={} destroyed={} battling={} result={}",
               seq, PtrValue(_this), PtrValue(marauder), FleetDeployedId(marauder), FleetDeployedState(marauder),
               FleetDeployedPreviousState(marauder), FleetDeployedType(marauder), FleetDeployedIsDestroyed(marauder),
               FleetDeployedCurrentlyBattling(marauder), result);
  return result;
}

void ActionQueueManager_ProcessQueueTarget_Marker(auto original, ActionQueueManager* _this, std::int64_t target_id,
                                                  bool can_select_new_target)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=ProcessQueue.target seq={} manager={} target={} "
               "can_select_new_target={}",
               seq, PtrValue(_this), target_id, can_select_new_target);

  original(_this, target_id, can_select_new_target);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=ProcessQueue.target seq={} manager={} target={} "
               "can_select_new_target={}",
               seq, PtrValue(_this), target_id, can_select_new_target);
}

void ActionQueueManager_OnSetCourseResponseEventHandler_Marker(auto original, ActionQueueManager* _this,
                                                               std::int64_t fleet_id, bool request_successful,
                                                               bool is_recall, void* planned_course_data)
{
  const auto seq           = NextKirsharaQueueMarkerSeq();
  const auto course_target = ReadCourseDataTargetDeployedFleetId(planned_course_data);
  if (KirsharaQueueMarkerEnabled(&KirsharaQueueDiagnosticsConfig::on_set_course_response)) {
    spdlog::info("[KirsharaQueueMarker] phase=before hook=OnSetCourseResponse seq={} manager={} fleet={} ok={} "
                 "recall={} course={} course_target_present={} course_target={}",
                 seq, PtrValue(_this), fleet_id, request_successful, is_recall, PtrValue(planned_course_data),
                 course_target.present, course_target.value);
  }
  if (request_successful && !is_recall && course_target.present) {
    LatchCourseTargetCompletionTarget(fleet_id, course_target.value, "OnSetCourseResponse.ok");
  }

  original(_this, fleet_id, request_successful, is_recall, planned_course_data);

  if (KirsharaQueueMarkerEnabled(&KirsharaQueueDiagnosticsConfig::on_set_course_response)) {
    spdlog::info("[KirsharaQueueMarker] phase=after hook=OnSetCourseResponse seq={} manager={} fleet={} ok={} "
                 "recall={} course={} course_target_present={} course_target={}",
                 seq, PtrValue(_this), fleet_id, request_successful, is_recall, PtrValue(planned_course_data),
                 course_target.present, course_target.value);
  }
}

void ActionQueueManager_OnPlayerFleetStateChangedEventHandler_Marker(auto original, ActionQueueManager* _this,
                                                                     void* fleets)
{
  const auto seq = NextKirsharaQueueMarkerSeq();
  spdlog::info("[KirsharaQueueMarker] phase=before hook=OnPlayerFleetStateChanged seq={} manager={} fleets={}", seq,
               PtrValue(_this), PtrValue(fleets));

  original(_this, fleets);

  spdlog::info("[KirsharaQueueMarker] phase=after hook=OnPlayerFleetStateChanged seq={} manager={} fleets={}", seq,
               PtrValue(_this), PtrValue(fleets));
}

void ActionQueueManager_ProcessQueueDeployed(auto original, ActionQueueManager* _this, FleetDeployedData* deployed_data,
                                             bool can_select_new_target)
{
  auto before            = BuildActionQueueProbeEvent("before", "ProcessQueue.deployed", _this, nullptr, nullptr);
  before["deployed_id"]  = FleetDeployedId(deployed_data);
  before["deployed_ptr"] = PtrValue(deployed_data);
  before["can_select_new_target"] = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=ProcessQueue.deployed manager={} deployed={} can_select_new={}",
                 static_cast<void*>(_this), FleetDeployedId(deployed_data), can_select_new_target);
  }
  original(_this, deployed_data, can_select_new_target);

  auto after            = BuildActionQueueProbeEvent("after", "ProcessQueue.deployed", _this, nullptr, nullptr);
  after["deployed_id"]  = FleetDeployedId(deployed_data);
  after["deployed_ptr"] = PtrValue(deployed_data);
  after["can_select_new_target"] = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=after hook=ProcessQueue.deployed manager={} deployed={} can_select_new={}",
                 static_cast<void*>(_this), FleetDeployedId(deployed_data), can_select_new_target);
  }
}

void ActionQueueManager_ProcessQueueTarget(auto original, ActionQueueManager* _this, std::int64_t target_id,
                                           bool can_select_new_target)
{
  auto before         = BuildActionQueueProbeEvent("before", "ProcessQueue.target", _this, nullptr, nullptr);
  before["target_id"] = target_id;
  before["can_select_new_target"] = can_select_new_target;
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=ProcessQueue.target manager={} target={} can_select_new={}",
                 static_cast<void*>(_this), target_id, can_select_new_target);
  }
  original(_this, target_id, can_select_new_target);

  auto after                     = BuildActionQueueProbeEvent("after", "ProcessQueue.target", _this, nullptr, nullptr);
  after["target_id"]             = target_id;
  after["can_select_new_target"] = can_select_new_target;
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

void ActionQueueManager_OnSetCourseResponseEventHandler(auto original, ActionQueueManager* _this, std::int64_t fleet_id,
                                                        bool request_successful, bool is_recall,
                                                        void* planned_course_data)
{
  auto before                   = BuildActionQueueProbeEvent("before", "OnSetCourseResponse", _this, nullptr, nullptr);
  before["fleet_id"]            = fleet_id;
  before["request_successful"]  = request_successful;
  before["is_recall"]           = is_recall;
  before["planned_course_data"] = PtrValue(planned_course_data);
  AppendActionQueueProbeJsonl(std::move(before));

  if (ActionQueueProbeEnabled()) {
    spdlog::info("[ActionQueueProbe] phase=before hook=OnSetCourseResponse manager={} fleet={} ok={} recall={} "
                 "course={} any_queue={}",
                 static_cast<void*>(_this), fleet_id, request_successful, is_recall, planned_course_data,
                 _this ? _this->AnyPlayerFleetInQueue() : false);
  }
  original(_this, fleet_id, request_successful, is_recall, planned_course_data);

  auto after                   = BuildActionQueueProbeEvent("after", "OnSetCourseResponse", _this, nullptr, nullptr);
  after["fleet_id"]            = fleet_id;
  after["request_successful"]  = request_successful;
  after["is_recall"]           = is_recall;
  after["planned_course_data"] = PtrValue(planned_course_data);
  AppendActionQueueProbeJsonl(std::move(after));

  if (ActionQueueProbeEnabled()) {
    spdlog::info(
        "[ActionQueueProbe] phase=after hook=OnSetCourseResponse manager={} fleet={} ok={} recall={} course={} "
        "any_queue={}",
        static_cast<void*>(_this), fleet_id, request_successful, is_recall, planned_course_data,
        _this ? _this->AnyPlayerFleetInQueue() : false);
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
  const auto  level                  = RuntimeTraceLevelSetting();
  const auto  detailed_runtime_trace = level == RuntimeTraceLevel::Detailed || level == RuntimeTraceLevel::Verbose;
  const auto& repair_settings        = KirsharaQueueRepairSettings();
  const auto  install_plan           = BuildKirsharaQueueRepairInstallPlan(repair_settings, detailed_runtime_trace);
  const auto  install_action_queue_repairs     = install_plan.install_repair_hooks;
  const auto  install_course_target_completion = install_plan.install_course_target_completion;
  const auto  install_any_action_queue_marker =
      install_plan.install_dump_interesting_methods || install_plan.install_on_strike_complete_marker
      || install_plan.install_remove_target_and_attack_next_marker
      || install_plan.install_check_to_clear_action_queue_marker || install_plan.install_is_target_valid_marker
      || install_plan.install_process_queue_deployed_marker || install_plan.install_process_queue_target_marker
      || install_plan.install_on_set_course_response_marker || install_plan.install_on_player_fleet_state_changed_marker
      || install_plan.install_on_fleet_state_change_marker || install_plan.install_on_fleets_disposed_marker;
  const auto install_completion_pipeline_hooks = install_any_action_queue_marker || install_course_target_completion;

  if (!install_action_queue_repairs && !install_completion_pipeline_hooks) {
    if (KirsharaQueueRepairEnabled()) {
      spdlog::info("[ActionQueueRepair] enabled but no safe staged hooks selected; skipping repair install");
    }
    return;
  }

  if (ActionQueueProbeEnabled()) {
    ResetActionQueueProbeJsonl();
    spdlog::info("[ActionQueueProbe] install level={} repair_detours={}",
                 RuntimeTraceLevelName(RuntimeTraceLevelSetting()), install_action_queue_repairs);
    AppendActionQueueProbeJsonl({{"phase", "install"},
                                 {"hook", "InstallActionQueueRepairHooks"},
                                 {"level", RuntimeTraceLevelName(RuntimeTraceLevelSetting())},
                                 {"repair_detours", install_action_queue_repairs}});
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

    if (install_completion_pipeline_hooks) {
      if (install_any_action_queue_marker) {
        spdlog::info("[KirsharaQueueMarker] installing selected completion markers");
      }
      if (install_course_target_completion) {
        spdlog::info("[KirsharaQueueRepair] installing course-target completion repair hooks");
      }
      if (install_plan.install_dump_interesting_methods) {
        DumpInterestingActionQueueManagerMethodsOnce();
      }

      if (install_plan.install_on_strike_complete_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("OnStrikeCompleteEventHandler"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnStrikeCompleteEventHandler_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "OnStrikeCompleteEventHandler");
        }
      }

      if (install_plan.install_remove_target_and_attack_next_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("RemoveTargetAndAttackNext"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_RemoveTargetAndAttackNext_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "RemoveTargetAndAttackNext");
        }
      }

      if (install_plan.install_check_to_clear_action_queue_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("CheckToClearActionQueue"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_CheckToClearActionQueue_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "CheckToClearActionQueue");
        }
      }

      if (install_plan.install_is_target_valid_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("IsTargetValid"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_IsTargetValid_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "IsTargetValid");
        }
      }

      auto ptr_process_deployed = actionqueue_manager.GetMethodSpecial("ProcessQueue", [](int param_count,
                                                                                          const Il2CppType** params) {
        return param_count == 2 && probe::detail::type_name(params[0]).find("FleetDeployedData") != std::string::npos;
      });
      if (ptr_process_deployed
          && (install_course_target_completion || install_plan.install_process_queue_deployed_marker)) {
        SPUD_STATIC_DETOUR(ptr_process_deployed, ActionQueueManager_ProcessQueueDeployed_Marker);
      } else if (install_course_target_completion || install_plan.install_process_queue_deployed_marker) {
        ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(FleetDeployedData, bool)");
      }

      auto ptr_process_target =
          actionqueue_manager.GetMethodSpecial("ProcessQueue", [](int param_count, const Il2CppType** params) {
            return param_count == 2 && probe::detail::type_name(params[0]).find("Int64") != std::string::npos;
          });
      if (ptr_process_target && install_plan.install_process_queue_target_marker) {
        SPUD_STATIC_DETOUR(ptr_process_target, ActionQueueManager_ProcessQueueTarget_Marker);
      } else if (install_plan.install_process_queue_target_marker) {
        ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(Int64, bool)");
      }

      if (install_course_target_completion || install_plan.install_on_set_course_response_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("OnSetCourseResponseEventHandler"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnSetCourseResponseEventHandler_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "OnSetCourseResponseEventHandler");
        }
      }

      if (install_plan.install_on_player_fleet_state_changed_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("OnPlayerFleetStateChangedEventHandler"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnPlayerFleetStateChangedEventHandler_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "OnPlayerFleetStateChangedEventHandler");
        }
      }

      if (install_course_target_completion || install_plan.install_on_fleet_state_change_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("OnFleetStateChangeEventHandler"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnFleetStateChangeEventHandler_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetStateChangeEventHandler");
        }
      }

      if (install_plan.install_on_fleets_disposed_marker) {
        if (auto ptr = actionqueue_manager.GetMethod("OnFleetsDisposedEventHandler"); ptr) {
          SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnFleetsDisposedEventHandler_Marker);
        } else {
          ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetsDisposedEventHandler");
        }
      }
    }
  }
}
