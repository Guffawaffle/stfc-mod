#pragma once

// The production adapter against the existing controlled runtime. Native method bodies and Config are fixtures;
// the queue observations, checked bindings, hooks and request owner are the shipped implementation.
#include "../src/mod/features/action_queue.cc"

#include <functional>

namespace
{
struct QueueNativeList {
  Il2CppObject               header{};
  std::vector<Il2CppObject*> items;
  int                        reads = 0, fail_count_at = 0, fail_item_at = -1;
  std::function<void(int)>   on_count;
};
struct QueueNativeAction {
  Il2CppObject header{};
  int64_t      target = 0;
};
struct QueueNativeInstance {
  Il2CppObject     header{};
  bool             engaging = false;
  float            attempt  = 2.5f;
  int64_t          last = 0, pending = 0;
  QueueNativeList* actions = nullptr;
  int64_t          fleet   = 999;
};
struct QueueNativeVector {
  Il2CppObject                        header{};
  Il2CppArrayBounds*                  bounds = nullptr;
  il2cpp_array_size_t                 size   = 2;
  std::array<QueueNativeInstance*, 2> items{};
};
struct QueueNativeManager {
  Il2CppObject       header{};
  QueueNativeVector* queues = nullptr;
};
struct QueueNativeDeployed {
  Il2CppObject header{};
  int64_t      id        = 100;
  bool         destroyed = true;
};
int          queue_native_processes = 0, queue_native_engages = 0;
bool         queue_native_engage_throws = false;
EngageResult queue_native_engage_result = EngageResult::Success;

int32_t QueueNativeCount(QueueNativeList* list, const MethodInfo*)
{
  const auto read = ++list->reads;
  if (list->on_count)
    list->on_count(read);
  if (list->fail_count_at == read)
    throw Il2CppExceptionWrapper(&thrown);
  return static_cast<int32_t>(list->items.size());
}
Il2CppObject* QueueNativeItem(QueueNativeList* list, int32_t index, const MethodInfo*)
{
  if (list->fail_item_at == index || index < 0 || static_cast<size_t>(index) >= list->items.size())
    throw Il2CppExceptionWrapper(&thrown);
  return list->items[index];
}
EngageResult QueueNativeEngage(ActionQueueManager*, FleetPlayerData*, ActionQueueInstance*, const MethodInfo*)
{
  ++queue_native_engages;
  if (queue_native_engage_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return queue_native_engage_result;
}
void QueueNativeProcess(QueueNativeManager* manager, int64_t target, bool select, const MethodInfo*)
{
  Require(!select, "destroyed-head processing preserves native false selection argument");
  ++queue_native_processes;
  for (auto* queue : manager->queues->items)
    if (queue && queue->actions)
      std::erase_if(queue->actions->items, [target](Il2CppObject* item) {
        return reinterpret_cast<QueueNativeAction*>(item)->target == target;
      });
}
int64_t QueueNativeDeployedId(QueueNativeDeployed* fleet, const MethodInfo*)
{ return fleet->id; }
bool QueueNativeDestroyed(QueueNativeDeployed* fleet, const MethodInfo*)
{ return fleet->destroyed; }
DeployedFleetState QueueNativeDeployedState(QueueNativeDeployed*, const MethodInfo*)
{ return DeployedFleetState::Idle; }
bool QueueNativeBattling(QueueNativeDeployed*, const MethodInfo*)
{ return false; }
FleetRemovalReason QueueNativeRemoval(QueueNativeDeployed*, const MethodInfo*)
{ return FleetRemovalReason::Destroyed; }
FleetState QueueNativeState(FleetPlayerData*, const MethodInfo*)
{ return FleetState::IdleInSpace; }
void QueueNativeResponse(ActionQueueManager*, SetCourseResponse, const MethodInfo*) {}
bool QueueNativeRetry(ActionQueueManager*, int64_t, ActionQueueInstance*, const MethodInfo*)
{ return false; }
void QueueNativeClear(ActionQueueManager*, const MethodInfo*) {}

void QueueNativeCases(std::string_view mode)
{
  auto& action_type        = Define("Prime.ActionQueue", "QueueableAction", IL2CPP_TYPE_CLASS, &object_class);
  auto& queue_type         = Define("Prime.ActionQueue", "ActionQueueInstance", IL2CPP_TYPE_CLASS, &object_class);
  auto& manager_type       = Define("Prime.ActionQueue", "ActionQueueManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& list_type          = Define("Fixture", "QueueList", IL2CPP_TYPE_CLASS, &object_class);
  auto& disposed_list_type = Define("Fixture", "DisposedList", IL2CPP_TYPE_CLASS, &object_class);
  auto& vector_type        = Define("Fixture", "QueueVector", IL2CPP_TYPE_SZARRAY, &array_class);
  auto& deployed_type      = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& removal_type   = Define("Digit.PrimeServer.Models", "FleetRemovalReason", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& fleet_state    = Define("Digit.PrimeServer.Models", "FleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& deployed_state = Define("Digit.PrimeServer.Models", "DeployedFleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& engage_type    = Define("Digit.Prime.Combat", "EngageResult", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& response_type = Define("Digit.PrimeServer.Events", "SetCourseResponseEventArgs", IL2CPP_TYPE_VALUETYPE, nullptr,
                               sizeof(SetCourseResponse));
  removal_type.is_enum = fleet_state.is_enum = engage_type.is_enum = true;
  deployed_state.is_enum                                           = true;
  vector_type.klass.byval_arg.data.type                            = T(queue_type);
  AddField(action_type, "<FleetId>k__BackingField", long_class, offsetof(QueueNativeAction, target));
  AddField(queue_type, "IsEngaging", bool_class, offsetof(QueueNativeInstance, engaging));
  AddField(queue_type, "LastEngageAttemptTime", float_class, offsetof(QueueNativeInstance, attempt));
  AddField(queue_type, "LastEngagedTargetId", long_class, offsetof(QueueNativeInstance, last));
  AddField(queue_type, "PendingEngageTargetId", long_class, offsetof(QueueNativeInstance, pending));
  AddField(queue_type, "_actionQueue", mode == "queue-list-pointer" ? intptr_class : list_type,
           offsetof(QueueNativeInstance, actions));
  AddField(queue_type, "<PlayerFleetId>k__BackingField", long_class, offsetof(QueueNativeInstance, fleet));
  AddField(manager_type, "_battleQueue", mode == "queue-field-pointer" ? intptr_class : vector_type,
           offsetof(QueueNativeManager, queues));
  // A vector's metadata union holds its element type; preserve the registered descriptor's identity in this
  // fixture API rather than copying it into the scalar-field descriptor pool.
  manager_type.fields.back().type = T(mode == "queue-field-pointer" ? intptr_class : vector_type);
  AddMethod(list_type, "get_Count", reinterpret_cast<void*>(&QueueNativeCount), T(int_class), {});
  AddMethod(list_type, "get_Item", reinterpret_cast<void*>(&QueueNativeItem), T(action_type), {T(int_class)});
  AddMethod(disposed_list_type, "get_Count", reinterpret_cast<void*>(&QueueNativeCount), T(int_class), {});
  AddMethod(disposed_list_type, "get_Item", reinterpret_cast<void*>(&QueueNativeItem), T(deployed_type),
            {T(int_class)});
  AddMethod(manager_type, "TryPlanPathAndEngageTarget", reinterpret_cast<void*>(&QueueNativeEngage), T(engage_type),
            {T(fleet_data_class), T(queue_type)});
  AddMethod(manager_type, "ShouldRetryFailedSetCourse", reinterpret_cast<void*>(&QueueNativeRetry), T(bool_class),
            {T(long_class), T(queue_type)});
  AddMethod(manager_type, "StopWatchdogAndClearAllQueues",
            mode == "queue-alias" ? reinterpret_cast<void*>(&QueueNativeRetry)
                                  : reinterpret_cast<void*>(&QueueNativeClear),
            T(void_class), {});
  AddMethod(manager_type, "ProcessQueue", reinterpret_cast<void*>(&QueueNativeProcess), T(void_class),
            {T(long_class), T(bool_class)});
  const auto& id =
      AddMethod(deployed_type, "get_ID", reinterpret_cast<void*>(&QueueNativeDeployedId), T(long_class), {});
  AddProperty(deployed_type, "ID", &id);
  const auto& destroyed =
      AddMethod(deployed_type, "get_IsDestroyed", reinterpret_cast<void*>(&QueueNativeDestroyed), T(bool_class), {});
  AddProperty(deployed_type, "IsDestroyed", &destroyed);
  const auto& removal =
      AddMethod(deployed_type, "get_RemovalReason", reinterpret_cast<void*>(&QueueNativeRemoval), T(removal_type), {});
  AddProperty(deployed_type, "RemovalReason", &removal);
  AddMethod(deployed_type, "get_CurrentState", reinterpret_cast<void*>(&QueueNativeDeployedState), T(deployed_state),
            {});
  AddMethod(deployed_type, "get_CurrentlyBattling", reinterpret_cast<void*>(&QueueNativeBattling), T(bool_class), {});
  const auto& state =
      AddMethod(fleet_data_class, "get_CurrentState", reinterpret_cast<void*>(&QueueNativeState), T(fleet_state), {});
  AddProperty(fleet_data_class, "CurrentState", &state);
  AddField(response_type, "<FleetId>k__BackingField", long_class,
           sizeof(Il2CppObject) + offsetof(SetCourseResponse, fleet_id));
  AddField(response_type, "<Success>k__BackingField", bool_class,
           sizeof(Il2CppObject) + offsetof(SetCourseResponse, success));
  AddField(response_type, "<IsRecall>k__BackingField", bool_class,
           sizeof(Il2CppObject) + offsetof(SetCourseResponse, is_recall));
  AddField(response_type, "<TargetId>k__BackingField", object_class,
           sizeof(Il2CppObject) + offsetof(SetCourseResponse, target_id));
  AddMethod(manager_type, "OnSetCourseResponseEventHandler", reinterpret_cast<void*>(&QueueNativeResponse),
            T(void_class), {T(response_type)});
  const auto response = ActionQueueManager::Class::Method<void(SetCourseResponse)>("OnSetCourseResponseEventHandler");
  Require(queue_binding::ResponseLayout(response), "actual callback layout passes shared ABI qualification");
  const auto engage = ActionQueueManager::Class::Method<EngageResult(FleetPlayerData*, ActionQueueInstance*)>(
      "TryPlanPathAndEngageTarget");
  Require(queue_binding::EngageResultLayout(engage), "named native Int32 EngageResult is qualified");
  if (mode == "queue-alias") {
    Require(!InstallActionQueueRecovery() && !il2cpp::HookStateIndeterminate(),
            "production installer refuses incompatible aliased callbacks before staging hooks");
    return;
  }
  if (mode == "queue-response-layout") {
    ++response_type.fields.back().offset;
    Require(!InstallActionQueueRecovery() && !il2cpp::HookStateIndeterminate(),
            "production installer refuses a same-size malformed response before staging hooks");
    return;
  }
  if (mode == "queue-result-layout") {
    const_cast<MethodInfo*>(engage.Info())->return_type = T(unsigned_int_class);
    Require(!InstallActionQueueRecovery() && !InstallThinQueueProtection() && !il2cpp::HookStateIndeterminate(),
            "same-width unqualified engage result cannot be treated as success zero");
    return;
  }
  for (auto& field : response_type.fields) {
    const auto offset = field.offset;
    ++field.offset;
    Require(!queue_binding::ResponseLayout(response), "same-size response with shifted field is refused");
    field.offset         = offset;
    auto type            = *field.type;
    type.byref           = 1;
    const auto* original = field.type;
    field.type           = &type;
    Require(!queue_binding::ResponseLayout(response), "byref response field is refused");
    type.byref = 0;
    type.attrs |= FIELD_ATTRIBUTE_STATIC;
    Require(!queue_binding::ResponseLayout(response), "static response field is refused");
    field.type = original;
  }

  QueueNativeAction   old{{&action_type.klass}, 100}, next{{&action_type.klass}, 200};
  QueueNativeList     actions{{&list_type.klass}, {&old.header, &next.header}};
  QueueNativeList     secondary{{&list_type.klass}, {}};
  QueueNativeInstance queue{{&queue_type.klass}, false, 2.5f, 0, 0, &actions, 999};
  QueueNativeInstance other{{&queue_type.klass}, false, 2.5f, 0, 0, &secondary, 1000};
  QueueNativeVector   vector{{&vector_type.klass}, nullptr, 2, {&queue, &other}};
  QueueNativeManager  manager{{&manager_type.klass}, &vector};
  auto*               owner  = reinterpret_cast<ActionQueueManager*>(&manager);
  auto*               source = reinterpret_cast<ActionQueueInstance*>(&queue);
  if (mode == "queue-list-pointer") {
    queue.actions = reinterpret_cast<QueueNativeList*>(uintptr_t{1});
    Require(!ActionQueueInstance::SafetyFieldsAvailable() && !SnapshotOf(source).present
                && !RetryStateOf(source, 100).valid,
            "native-pointer list field is refused before dereferencing its non-object value");
    return;
  }
  if (mode == "queue-field-pointer") {
    Require(owner->battleQueue() == nullptr, "IntPtr field cannot authorize array storage access");
    Require(!AbsentFromAllQueues(owner, 100), "unavailable queue collection is not observed absence");
    return;
  }
  Require(owner->battleQueue() && ActionQueueInstance::SafetyFieldsAvailable(),
          "actual checked queue storage is available");
  vector.header.klass = &object_class.klass;
  Require(!owner->battleQueue(), "ordinary reference is not a queue vector");
  vector.header.klass              = &vector_type.klass;
  vector_type.klass.byval_arg.type = IL2CPP_TYPE_ARRAY;
  Require(!owner->battleQueue(), "multidimensional array is refused");
  vector_type.klass.byval_arg.type      = IL2CPP_TYPE_SZARRAY;
  vector_type.klass.byval_arg.data.type = T(int_class);
  Require(!owner->battleQueue(), "primitive array is refused");
  vector_type.klass.byval_arg.data.type = T(other_class);
  Require(!owner->battleQueue(), "wrong reference element type is refused");
  vector_type.klass.byval_arg.data.type = T(queue_type);

  actions.reads         = 0;
  actions.fail_count_at = 2;
  const auto observed   = RetryStateOf(source, 200);
  Require(observed.valid && observed.contains_target && actions.reads == 1,
          "one qualified Count governs the full scan; no unchecked second read can invent absence");
  actions.fail_count_at = 0;
  actions.fail_item_at  = 1;
  Require(!RetryStateOf(source, 200).valid && !SnapshotOf(source).present,
          "unreadable item invalidates both recovery observations");
  actions.fail_item_at = -1;

  Config::Get().control.queue_enabled = Config::Get().control.kirshara_fixes = true;
  const auto roots_before                                                    = warp_roots.size();
  const auto arm                                                             = [&] {
    ClearRequests();
    actions.items  = {&old.header, &next.header};
    queue.engaging = true;
    queue.last = queue.pending = 0;
    ActionQueueManager_TryPlanPathAndEngageTarget_Hook([](auto*, auto*, auto*) { return EngageResult::Success; }, owner,
                                                       nullptr, source);
    actions.items  = {&next.header};
    queue.engaging = false;
  };
  CourseContext  course{999, 100, source, true};
  QueueCallScope scope(current_course, &course);
  int            native_retries = 0;
  const auto     retry          = [&] {
    return ActionQueueManager_ShouldRetryFailedSetCourse_Hook(
        [&](auto*, auto, auto*) {
          ++native_retries;
          return false;
        },
        owner, 100, source);
  };
  arm();
  Require(retry() && !retry() && native_retries == 2,
          "native retry executes once per call; removed-target handoff is one-shot");
  Il2CppObject boxed_target{&long_class.klass};
  int          courses = 0;
  for (const auto response_flags : {0, 1, 2, 3}) {
    arm();
    queue.engaging   = true;
    bool  handed_off = false;
    auto* target_box = response_flags == 3 ? &string_object : &boxed_target;
    ActionQueueManager_OnSetCourseResponseEventHandler_Hook(
        [&](auto*, SetCourseResponse) {
          ++courses;
          queue.engaging = false;
          handed_off     = retry();
        },
        owner, {999, response_flags == 1, response_flags == 2, reinterpret_cast<il2cpp::Object*>(target_box)});
    Require(handed_off == (response_flags == 0) && current_course == &course,
            "actual response scope hands off only a qualified failed nonrecall response and restores its caller");
  }
  Require(courses == 4, "every native course response is delivered exactly once");
  arm();
  secondary.fail_count_at = secondary.reads + 1;
  Require(!retry(), "unreadable secondary queue cannot suppress native cross-fleet cleanup");
  secondary.fail_count_at = 0;
  Require(retry(), "refused absence proof did not consume the pending correlation");
  arm();
  Require(!ActionQueueManager_ShouldRetryFailedSetCourse_Hook(
              [&](auto*, auto, auto*) {
                Config::Get().control.queue_enabled = false;
                return false;
              },
              owner, 100, source),
          "native callback disabling queues prevents a later override");
  Config::Get().control.queue_enabled = true;
  arm();
  ActionQueueManager_StopWatchdogAndClearAllQueues_Hook([](auto*) {}, owner);
  Require(!retry() && warp_roots.size() == roots_before, "native teardown releases request custody");

  // These hooks invoke production snapshot/decision/confirmation and checked native engage.
  auto* fleet          = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
  actions.items        = {&old.header, &next.header};
  queue_native_engages = 0;
  Require(ActionQueueManager_DoPlanPathAndEngageTarget_Hook(
              [&](auto*, auto*) {
                actions.items = {&next.header};
                return false;
              },
              owner, fleet)
              && queue_native_engages == 1,
          "native prefix pruning resumes the exact surviving queue");
  actions.items              = {&old.header, &next.header};
  queue_native_engage_throws = true;
  Require(!ActionQueueManager_DoPlanPathAndEngageTarget_Hook(
              [&](auto*, auto*) {
                actions.items = {&next.header};
                return false;
              },
              owner, fleet),
          "contained engage failure does not become success zero");
  queue_native_engage_throws = false;
  actions.items              = {&old.header, &next.header};
  actions.reads              = 0;
  actions.on_count           = [&](int read) {
    if (read == 3)
      Config::Get().control.queue_enabled = false;
  };
  const auto before_disable = queue_native_engages;
  Require(!ActionQueueManager_DoPlanPathAndEngageTarget_Hook(
              [&](auto*, auto*) {
                actions.items = {&next.header};
                return false;
              },
              owner, fleet)
              && queue_native_engages == before_disable,
          "policy disabled during confirmation prevents extra engage");
  actions.on_count                    = {};
  Config::Get().control.queue_enabled = true;

  QueueNativeDeployed idle_fleet{{&deployed_type.klass}, 999, false};
  auto*               deployed = reinterpret_cast<FleetDeployedData*>(&idle_fleet);
  actions.items                = {&old.header, &next.header};
  queue.engaging               = false;
  const auto before_watchdog   = queue_native_engages;
  ActionQueueManager_HandleStall_Hook([&](auto*, auto*, auto*, auto*) { actions.items = {&next.header}; }, owner,
                                      source, fleet, deployed);
  Require(queue_native_engages == before_watchdog + 1, "watchdog native prefix prune resumes a matching idle fleet");
  actions.items = {&old.header, &next.header};
  idle_fleet.id = 1000;
  ActionQueueManager_HandleStall_Hook([&](auto*, auto*, auto*, auto*) { actions.items = {&next.header}; }, owner,
                                      source, fleet, deployed);
  Require(queue_native_engages == before_watchdog + 1, "mismatched deployed participant cannot authorize resume");

  QueueNativeDeployed destroyed_fleet{{&deployed_type.klass}, 100, true};
  QueueNativeList     disposed{{&disposed_list_type.klass}, {&destroyed_fleet.header}};
  auto*               disposing = reinterpret_cast<il2cpp::List<FleetDeployedData*>*>(&disposed);
  const auto          dispose   = [&](auto original) {
    actions.items          = {&old.header};
    actions.reads          = 0;
    queue.engaging         = false;
    vector.items[0]        = &queue;
    queue_native_processes = 0;
    ActionQueueManager_OnFleetsDisposedEventHandler_Hook(original, owner, disposing);
  };
  dispose([](auto*, auto*) {});
  Require(queue_native_processes == 1 && actions.items.empty(), "confirmed destroyed head delegates to native removal");
  dispose([](auto*, auto*) { Config::Get().control.queue_enabled = false; });
  Require(queue_native_processes == 0, "disable during disposal prevents processing");
  Config::Get().control.queue_enabled = true;
  actions.on_count                    = [&](int read) {
    if (read == 2)
      vector.items[0] = &other;
  };
  dispose([](auto*, auto*) {});
  Require(queue_native_processes == 0, "queue replacement during inspection prevents stale processing");
  actions.on_count = [&](int read) {
    if (read == 4)
      queue.engaging = true;
  };
  dispose([](auto*, auto*) {});
  Require(queue_native_processes == 0, "engagement beginning at confirmation prevents processing");
  actions.on_count = [&](int read) {
    if (read == 4)
      actions.items.push_back(&next.header);
  };
  dispose([](auto*, auto*) {});
  Require(queue_native_processes == 0, "changed confirmed count prevents processing");
  actions.on_count = {};
  ClearRequests();
  Require(warp_roots.size() == roots_before, "all queue fixture paths release correlation handles");
  std::cout << "Queue production adapter: ABI, arrays, checked scans, original calls, cross-fleet absence, disable, "
               "teardown, prune, contained engage failure and disposal confirmation passed\n";
}
} // namespace

API(Il2CppGCHandle, il2cpp_gchandle_new_weakref, (Il2CppObject * object, bool))
{ return il2cpp_gchandle_new(object, false); }
END_API

API(const Il2CppType*, il2cpp_class_enum_basetype, (Il2CppClass * type))
{ return Fake(type).is_enum ? T(int_class) : nullptr; }
END_API
