/**
 * @file action_queue_repair.cc
 * @brief Narrow Kir'Shara queue completion repair.
 *
 * Repairs the observed off-screen queue stall by replaying the target-id ProcessQueue completion seam only when a
 * recently planned course target is still present in the same fleet's live queue. The repair intentionally uses only
 * the course-response and deployed-fleet queue seams.
 */
#include "config.h"
#include "dev/diagnostics.h"
#include "errormsg.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/FleetPlayerData.h>
#include <prime/FleetsManager.h>
#include <prime/IList.h>
#include <spdlog/spdlog.h>
#include <string>
#include <type_traits>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

namespace
{
using RepairClock = std::chrono::steady_clock;

constexpr std::size_t kMaxPlayerFleets    = 8;
constexpr auto        kCourseTargetWindow = std::chrono::minutes(5);
const auto            kDiagnosticsSource  = dev::diagnostics::RegisterSource("kirshara-queue");

struct SetCourseResponseEventArgs {
  std::int64_t  fleet_id  = 0;
  bool          success   = false;
  bool          is_recall = false;
  std::byte     padding[6]{};
  Il2CppObject* target_id = nullptr;
};

static_assert(offsetof(SetCourseResponseEventArgs, target_id) == 0x10);
static_assert(sizeof(SetCourseResponseEventArgs) == 0x18);
static_assert(std::is_standard_layout_v<SetCourseResponseEventArgs>);
static_assert(std::is_trivially_copyable_v<SetCourseResponseEventArgs>);

struct CourseTargetCompletionCandidate {
  std::int64_t            fleet_id  = 0;
  std::int64_t            target_id = 0;
  RepairClock::time_point updated_at{};
};

struct FleetDeployedDataSnapshot {
  std::int64_t id                  = 0;
  bool         player_combat_start = false;
};

struct CourseTargetQueueGuard {
  bool relevant     = false;
  int  queue_count  = -1;
  int  target_index = -1;
};

struct CourseTargetCompletionSynthesis {
  bool         should_synthesize = false;
  std::int64_t deployed_id       = 0;
  std::int64_t target_id         = 0;
  std::int64_t course_age_ms     = 0;
  int          queue_count       = -1;
  int          target_index      = -1;
};

using ProcessQueueTargetMethod = void(ActionQueueManager*, std::int64_t, bool);

std::atomic_bool s_repair_hooks_ready = false;
std::atomic<ProcessQueueTargetMethod*> s_process_queue_target = nullptr;

bool RepairEnabled()
{
  const auto& config = Config::Get();
  return s_repair_hooks_ready.load(std::memory_order_acquire) && config.queue_enabled && config.kirshara_queue_repair;
}

std::mutex& CourseTargetCompletionMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::array<CourseTargetCompletionCandidate, kMaxPlayerFleets>& CourseTargetCompletionTargets()
{
  static std::array<CourseTargetCompletionCandidate, kMaxPlayerFleets> targets;
  return targets;
}

bool FleetIdsMatch(std::int64_t lhs, std::int64_t rhs)
{
  return lhs == rhs
         || static_cast<std::uint32_t>(static_cast<std::uint64_t>(lhs))
                == static_cast<std::uint32_t>(static_cast<std::uint64_t>(rhs));
}

bool HasCourseTargetCompletionTarget(std::int64_t fleet_id)
{
  std::lock_guard lk(CourseTargetCompletionMutex());
  const auto      now = RepairClock::now();
  for (auto& target : CourseTargetCompletionTargets()) {
    if (target.target_id != 0 && now - target.updated_at > kCourseTargetWindow) {
      target = {};
    } else if (target.target_id != 0 && FleetIdsMatch(target.fleet_id, fleet_id)) {
      return true;
    }
  }
  return false;
}

FleetDeployedDataSnapshot SnapshotFleetDeployedData(FleetDeployedData* deployed_data, std::int64_t deployed_id)
{
  FleetDeployedDataSnapshot snapshot;
  if (!deployed_data) {
    return snapshot;
  }

  const auto current_state     = deployed_data->CurrentState;
  const auto previous_state    = deployed_data->PreviousState;
  snapshot.id                  = deployed_id;
  snapshot.player_combat_start = deployed_data->FleetType == DeployedFleetType::Player && current_state == 6
                                 && (previous_state == 0 || previous_state == 1) && deployed_data->CurrentlyBattling
                                 && !deployed_data->IsDestroyed;
  return snapshot;
}

std::int64_t ReadCourseTargetId(Il2CppObject* boxed_target_id)
{
  if (!boxed_target_id) {
    return 0;
  }

  auto* klass = il2cpp_object_get_class(boxed_target_id);
  auto* name  = klass ? il2cpp_class_get_name(klass) : nullptr;
  auto* value = il2cpp_object_unbox(boxed_target_id);
  if (!name || !value) {
    return 0;
  }
  if (std::string_view{name} == "Int64") {
    return *reinterpret_cast<const std::int64_t*>(value);
  }
  if (std::string_view{name} == "UInt64") {
    return static_cast<std::int64_t>(*reinterpret_cast<const std::uint64_t*>(value));
  }
  return 0;
}

FleetPlayerData* FindPlayerFleetDataById(std::int64_t fleet_id)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager || fleet_id == 0) {
    return nullptr;
  }

  const auto low_fleet_id = static_cast<std::uint32_t>(static_cast<std::uint64_t>(fleet_id));
  auto*      low_match    = static_cast<FleetPlayerData*>(nullptr);
  for (std::size_t index = 0; index < kMaxPlayerFleets; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (!fleet) {
      continue;
    }

    if (static_cast<std::int64_t>(fleet->Id) == fleet_id) {
      return fleet;
    }

    if (static_cast<std::uint32_t>(fleet->Id) == low_fleet_id) {
      low_match = fleet;
    }
  }

  return low_match;
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

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("<PlayerFleetId>k__BackingField");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return 0;
  }

  return *reinterpret_cast<std::uint64_t*>(reinterpret_cast<char*>(action_queue_instance) + field_offset);
}

IList* ActionQueueInstanceList(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return nullptr;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("_actionQueue");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return nullptr;
  }

  return *reinterpret_cast<IList**>(reinterpret_cast<char*>(action_queue_instance) + field_offset);
}

std::int64_t QueueableActionFleetId(Il2CppObject* queueable_action)
{
  if (!queueable_action) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("<FleetId>k__BackingField");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return 0;
  }

  return *reinterpret_cast<std::int64_t*>(reinterpret_cast<char*>(queueable_action) + field_offset);
}

int FindActionQueueItemIndex(IList* list, std::int64_t target_id)
{
  if (!list) {
    return -1;
  }

  const auto item_count = std::min(list->Count, 32);
  for (int index = 0; index < item_count; ++index) {
    if (QueueableActionFleetId(list->Get(index)) == target_id) {
      return index;
    }
  }

  return -1;
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
        return param_count == 2 && params && params[0] && params[1] && params[1]->byref;
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

  static auto battle_queue_field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("_battleQueue");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (battle_queue_field_offset < 0) {
    return nullptr;
  }

  auto* battle_queue = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(manager) + battle_queue_field_offset);
  if (!battle_queue) {
    return nullptr;
  }

  auto* sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto* action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    if (ActionQueueInstanceFleetId(action_queue_instance) == fleet->Id) {
      return action_queue_instance;
    }
  }

  return nullptr;
}

CourseTargetQueueGuard CheckCourseTargetStillQueued(ActionQueueManager* manager, std::int64_t deployed_id,
                                                    std::int64_t target_id)
{
  CourseTargetQueueGuard guard;
  if (!manager || target_id == 0) {
    return guard;
  }

  auto* fleet = FindPlayerFleetDataById(deployed_id);
  if (!fleet) {
    return guard;
  }

  auto* queue_instance = FindActionQueueInstanceForFleet(manager, fleet);
  auto* list           = ActionQueueInstanceList(queue_instance);
  guard.queue_count    = list ? list->Count : -1;
  if (guard.queue_count <= 0) {
    return guard;
  }

  guard.target_index = FindActionQueueItemIndex(list, target_id);
  guard.relevant     = guard.target_index >= 0;
  return guard;
}

void UpdateCourseTargetCompletionTarget(std::int64_t fleet_id, std::int64_t target_id)
{
  if (!RepairEnabled()) {
    return;
  }

  auto* fleet = FindPlayerFleetDataById(fleet_id);
  if (!fleet) {
    return;
  }

  const auto      key = static_cast<std::int64_t>(fleet->Id);
  std::lock_guard lk(CourseTargetCompletionMutex());
  auto&           targets = CourseTargetCompletionTargets();
  auto            slot    = std::ranges::find(targets, key, &CourseTargetCompletionCandidate::fleet_id);
  if (target_id == 0) {
    if (slot != targets.end()) {
      *slot = {};
    }
    return;
  }

  if (slot == targets.end()) {
    slot = std::ranges::min_element(targets, {}, &CourseTargetCompletionCandidate::updated_at);
  }
  *slot = {.fleet_id = key, .target_id = target_id, .updated_at = RepairClock::now()};
}

CourseTargetCompletionCandidate TakeCourseTargetCompletionTarget(std::int64_t fleet_id)
{
  std::lock_guard lk(CourseTargetCompletionMutex());
  auto&           targets = CourseTargetCompletionTargets();
  const auto      slot    = std::ranges::find_if(targets, [fleet_id](const auto& target) {
    return target.target_id != 0 && FleetIdsMatch(target.fleet_id, fleet_id);
  });
  if (slot == targets.end()) {
    return {};
  }

  const auto target = *slot;
  *slot             = {};
  return target;
}

CourseTargetCompletionSynthesis TakeCourseTargetCompletionSynthesis(ActionQueueManager*              manager,
                                                                    const FleetDeployedDataSnapshot& deployed)
{
  if (!RepairEnabled() || !deployed.player_combat_start) {
    return {};
  }

  const auto target_state = TakeCourseTargetCompletionTarget(deployed.id);
  if (target_state.target_id == 0) {
    return {};
  }

  const auto course_age = RepairClock::now() - target_state.updated_at;
  if (course_age > kCourseTargetWindow) {
    return {};
  }

  const auto guard = CheckCourseTargetStillQueued(manager, deployed.id, target_state.target_id);
  if (!guard.relevant) {
    return {};
  }

  return {
      .should_synthesize = true,
      .deployed_id       = deployed.id,
      .target_id         = target_state.target_id,
      .course_age_ms     = std::chrono::duration_cast<std::chrono::milliseconds>(course_age).count(),
      .queue_count       = guard.queue_count,
      .target_index      = guard.target_index,
  };
}

bool IsProcessQueueTargetSignature(int param_count, const Il2CppType** params)
{
  return param_count == 2 && params && params[0] && params[1] && params[0]->type == IL2CPP_TYPE_I8
         && params[1]->type == IL2CPP_TYPE_BOOLEAN;
}

std::string TypeName(const Il2CppType* type)
{
  if (!type) {
    return {};
  }

  auto* raw_name = il2cpp_type_get_name(type);
  if (!raw_name) {
    return {};
  }

  std::string name = raw_name;
  il2cpp_free(raw_name);
  return name;
}

bool IsProcessQueueDeployedSignature(int param_count, const Il2CppType** params)
{
  return param_count == 2 && params && params[0] && params[1] && params[1]->type == IL2CPP_TYPE_BOOLEAN
         && TypeName(params[0]).find("FleetDeployedData") != std::string::npos;
}

bool IsOnSetCourseResponseSignature(int param_count, const Il2CppType** params)
{
  if (param_count != 1 || !params || !params[0] || params[0]->byref || params[0]->type != IL2CPP_TYPE_VALUETYPE
      || TypeName(params[0]) != "Digit.PrimeServer.Events.SetCourseResponseEventArgs") {
    return false;
  }

  auto* klass = il2cpp_class_from_type(params[0]);
  if (!klass) {
    return false;
  }

  uint32_t   alignment  = 0;
  const auto value_size = il2cpp_class_value_size(klass, &alignment);
  if (value_size != sizeof(SetCourseResponseEventArgs)) {
    spdlog::warn("[KirsharaQueueRepair] rejected SetCourseResponseEventArgs layout: size={} alignment={}", value_size,
                 alignment);
    return false;
  }

  struct ExpectedField {
    const char*    name;
    size_t         offset;
    Il2CppTypeEnum type;
  };
  constexpr std::array expected_fields{
      ExpectedField{"<FleetId>k__BackingField", offsetof(SetCourseResponseEventArgs, fleet_id), IL2CPP_TYPE_I8},
      ExpectedField{"<Success>k__BackingField", offsetof(SetCourseResponseEventArgs, success), IL2CPP_TYPE_BOOLEAN},
      ExpectedField{"<IsRecall>k__BackingField", offsetof(SetCourseResponseEventArgs, is_recall), IL2CPP_TYPE_BOOLEAN},
      ExpectedField{"<TargetId>k__BackingField", offsetof(SetCourseResponseEventArgs, target_id), IL2CPP_TYPE_OBJECT},
  };
  const auto fields_match = std::ranges::all_of(expected_fields, [klass](const auto& expected) {
    auto* field = il2cpp_class_get_field_from_name(klass, expected.name);
    // Runtime FieldInfo offsets address boxed values and therefore include the Il2CppObject header.
    const auto boxed_offset = sizeof(Il2CppObject) + expected.offset;
    return field && field->offset == boxed_offset && field->type && !field->type->byref
           && field->type->type == expected.type;
  });
  if (!fields_match) {
    spdlog::warn("[KirsharaQueueRepair] rejected SetCourseResponseEventArgs field layout");
  }
  return fields_match;
}

ProcessQueueTargetMethod* ResolveProcessQueueTargetForCompletion()
{
  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    return nullptr;
  }

  static auto method =
      actionqueue_manager.GetMethodSpecial<ProcessQueueTargetMethod>("ProcessQueue", IsProcessQueueTargetSignature);
  return method;
}

void ActionQueueManager_ProcessQueueDeployed(auto original, ActionQueueManager* _this, FleetDeployedData* deployed_data,
                                             bool can_select_new_target)
{
  if (!deployed_data) {
    original(_this, deployed_data, can_select_new_target);
    return;
  }

  const auto deployed_id = deployed_data->ID;
  if (!HasCourseTargetCompletionTarget(deployed_id)) {
    original(_this, deployed_data, can_select_new_target);
    return;
  }

  const auto deployed = SnapshotFleetDeployedData(deployed_data, deployed_id);

  original(_this, deployed_data, can_select_new_target);

  const auto synthesis = TakeCourseTargetCompletionSynthesis(_this, deployed);
  if (!synthesis.should_synthesize) {
    return;
  }

  auto* process_target = s_process_queue_target.load(std::memory_order_acquire);
  if (!process_target) {
    return;
  }

  spdlog::info("[KirsharaQueueRepair] completing queued target fleet={} target={} count={} index={} course_age_ms={}",
               synthesis.deployed_id, synthesis.target_id, synthesis.queue_count, synthesis.target_index,
               synthesis.course_age_ms);
  dev::diagnostics::PublishLazy(kDiagnosticsSource, dev::diagnostics::Severity::Info, [&synthesis] {
    return "repair fleet=" + std::to_string(synthesis.deployed_id) + " target=" + std::to_string(synthesis.target_id)
           + " queue=" + std::to_string(synthesis.queue_count) + " index=" + std::to_string(synthesis.target_index)
           + " age_ms=" + std::to_string(synthesis.course_age_ms);
  });
  process_target(_this, synthesis.target_id, can_select_new_target);
}

void ActionQueueManager_OnSetCourseResponseEventHandler(auto original, ActionQueueManager* _this,
                                                        SetCourseResponseEventArgs args)
{
  const auto target_id = args.success && !args.is_recall ? ReadCourseTargetId(args.target_id) : 0;
  UpdateCourseTargetCompletionTarget(args.fleet_id, target_id);

  original(_this, args);
}

} // namespace

void InstallActionQueueRepairHooks()
{
  if (!Config::Get().kirshara_queue_repair) {
    return;
  }

  s_repair_hooks_ready.store(false, std::memory_order_release);
  s_process_queue_target.store(nullptr, std::memory_order_release);

  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
    return;
  }

  auto* ptr_process_deployed = actionqueue_manager.GetMethodSpecial("ProcessQueue", IsProcessQueueDeployedSignature);
  auto* ptr_course_response =
      actionqueue_manager.GetMethodSpecial("OnSetCourseResponseEventHandler", IsOnSetCourseResponseSignature);
  auto* ptr_process_target = ResolveProcessQueueTargetForCompletion();
  if (!ptr_process_deployed) {
    ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(FleetDeployedData, bool)");
  }
  if (!ptr_course_response) {
    ErrorMsg::MissingMethod("ActionQueueManager", "OnSetCourseResponseEventHandler");
  }
  if (!ptr_process_target) {
    ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(Int64, bool)");
  }
  if (!ptr_process_deployed || !ptr_course_response || !ptr_process_target) {
    spdlog::warn("[KirsharaQueueRepair] disabled because all repair dependencies were not resolved");
    return;
  }

  const bool process_deployed_installed =
      SPUD_STATIC_DETOUR(ptr_process_deployed, ActionQueueManager_ProcessQueueDeployed) != nullptr;
  const bool course_response_installed =
      SPUD_STATIC_DETOUR(ptr_course_response, ActionQueueManager_OnSetCourseResponseEventHandler) != nullptr;
  if (!process_deployed_installed || !course_response_installed) {
    spdlog::warn("[KirsharaQueueRepair] disabled because both repair detours were not installed");
    return;
  }

  s_process_queue_target.store(ptr_process_target, std::memory_order_release);
  s_repair_hooks_ready.store(true, std::memory_order_release);
  spdlog::info("[KirsharaQueueRepair] installed two-seam off-screen queue completion repair");
}
