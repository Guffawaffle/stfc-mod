#include "patches/queue_science.h"
#include "config.h"
#include "version.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <string>
#if _WIN32
#include <Windows.h>
#else
#include <execinfo.h>
#endif

namespace queue_science
{
namespace
{
  using Object = Il2CppObject;
  using Json   = nlohmann::json;
  using Clock  = std::chrono::steady_clock;
  std::shared_ptr<spdlog::logger>               logger;
  std::shared_ptr<spdlog::details::thread_pool> pool;
  std::set<std::string>                         owners;
  std::atomic_uint64_t                          sequence{};
  thread_local uint64_t                         parentSpan{};
  thread_local const char*                      parentSource = "native";
  thread_local Scope*                           addressScope{};
  thread_local Object*                          addressPlayer{};
  const auto                                    started = Clock::now();
  constexpr uint64_t                            limit   = 50000;
  Il2CppClass *                                 managerClass{}, *queueClass{}, *actionClass{};
  FieldInfo *queuesField{}, *fleetField{}, *engagingField{}, *attemptField{}, *lastField{}, *pendingField{},
      *actionsField{}, *targetField{}, *retriesField{};

  FieldInfo* Field(Il2CppClass* cls, const char* name, Il2CppTypeEnum type)
  {
    auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
    return field && field->offset >= 0x10 && field->type && field->type->type == type ? field : nullptr;
  }
  template <typename T> T Read(Object* object, FieldInfo* field)
  {
    T value{};
    if (object && field)
      il2cpp_field_get_value(object, field, &value);
    return value;
  }

  Json Queue(Object* queue)
  {
    if (!queue || il2cpp_object_get_class(queue) != queueClass)
      return {{"valid", false}};
    Json  result{{"valid", false},
                 {"fleet", Read<int64_t>(queue, fleetField)},
                 {"engaging", Read<bool>(queue, engagingField)},
                 {"attempt_time", Read<float>(queue, attemptField)},
                 {"last_engaged", Read<int64_t>(queue, lastField)},
                 {"pending", Read<int64_t>(queue, pendingField)}};
    auto* list = Read<Object*>(queue, actionsField);
    if (!list)
      return result;
    auto* cls        = il2cpp_object_get_class(list);
    auto* size       = Field(cls, "_size", IL2CPP_TYPE_I4);
    auto* itemsField = Field(cls, "_items", IL2CPP_TYPE_SZARRAY);
    if (!size || !itemsField)
      return result;
    const int count = Read<int>(list, size);
    auto*     array = Read<Il2CppArray*>(list, itemsField);
    result["count"] = count;
    if (count < 0 || count > 128 || !array || il2cpp_array_length(array) < static_cast<unsigned>(count)
        || il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Object*>(array))) != actionClass)
      return result;
    auto* items   = reinterpret_cast<Il2CppArraySize*>(array);
    auto  targets = Json::array();
    for (int i = 0; i < count; ++i) {
      auto* action = static_cast<Object*>(items->vector[i]);
      if (!action || il2cpp_object_get_class(action) != actionClass)
        return result;
      targets.push_back({{"id", Read<int64_t>(action, targetField)}, {"retries", Read<int>(action, retriesField)}});
    }
    result["targets"] = std::move(targets);
    result["valid"]   = true;
    return result;
  }
  Json Snapshot(Object* object)
  {
    if (!object)
      return {{"valid", false}};
    if (il2cpp_object_get_class(object) == queueClass)
      return Json::array({Queue(object)});
    if (il2cpp_object_get_class(object) != managerClass)
      return {{"valid", false}};
    auto* array = Read<Il2CppArray*>(object, queuesField);
    if (!array)
      return {{"valid", false}, {"reason", "no_queue_array"}};
    if (il2cpp_array_length(array) > 64
        || il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Object*>(array))) != queueClass)
      return {{"valid", false}, {"reason", "unknown_queue_array"}};
    Json  queues = Json::array();
    auto* items  = reinterpret_cast<Il2CppArraySize*>(array);
    for (unsigned i = 0; i < items->max_length; ++i) {
      auto* queue = static_cast<Object*>(items->vector[i]);
      if (queue) {
        auto entry    = Queue(queue);
        entry["slot"] = i;
        queues.push_back(std::move(entry));
      }
    }
    return queues;
  }
  Json Stack()
  {
    void* addresses[16]{};
#if _WIN32
    const auto count = CaptureStackBackTrace(2, 16, addresses, nullptr);
    const auto game  = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
    const auto unity = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"UnityPlayer.dll"));
    Json       result{{"game_base", game}, {"unity_base", unity}};
#else
    const auto count  = backtrace(addresses, 16);
    Json       result = Json::object();
#endif
    auto frames = Json::array();
    for (int i = 0; i < count; ++i)
      frames.push_back(reinterpret_cast<uintptr_t>(addresses[i]));
    result["addresses"] = std::move(frames);
    return result;
  }
  void Write(Json event)
  {
    event["schema"]     = "kirshara-queue-science/v1";
    event["elapsed_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
    event["wall_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    event["async_overruns"] = pool->overrun_counter();
    logger->info("{}", event.dump());
  }
  bool requested()
  {
    // This dedicated science branch captures by default; explicit 0 gives an observer-off control.
    const char* value = std::getenv("STFC_MOD_KIRSHARA_TRACE");
    return !value || std::strcmp(value, "0") != 0;
  }

  void Add(auto original, Object* manager, int64_t target)
  {
    Scope scope(manager, "native.add", target, true);
    original(manager, target);
  }
  void Clear(auto original, Object* manager, Object* fleet)
  {
    Scope scope(manager, "native.clear", 0, true);
    scope.Object("fleet", fleet);
    original(manager, fleet);
  }
  void CheckClear(auto original, Object* manager, Object* fleet)
  {
    Scope scope(manager, "native.check-clear");
    scope.Object("fleet", fleet);
    original(manager, fleet);
  }
  void InstanceClear(auto original, Object* queue)
  {
    Scope scope(queue, "native.instance-clear", 0, true);
    original(queue);
  }
  void Process(auto original, Object* manager, int64_t target, bool select)
  {
    Scope scope(manager, "native.process", target, true);
    scope.Note("select_new_target", select);
    original(manager, target, select);
  }
  void Cleanup(auto original, Object* manager)
  {
    Scope scope(manager, "native.cleanup");
    original(manager);
  }
  void StateChange(auto original, Object* manager, Object* fleets)
  {
    Scope scope(manager, "native.deployed-state-change");
    scope.Object("fleets", fleets, true);
    original(manager, fleets);
  }
  void PlayerStateChange(auto original, Object* manager, Object* fleets)
  {
    Scope scope(manager, "native.player-state-change");
    scope.Object("fleets", fleets, true);
    original(manager, fleets);
  }
  void Enable(auto original, Object* manager, bool enabled)
  {
    Scope scope(manager, "native.set-enabled", 0, true);
    scope.Note("enabled", enabled);
    original(manager, enabled);
  }
  bool Plan(auto original, Object* manager, Object* fleet)
  {
    Scope      scope(manager, "native.planner");
    const bool result = original(manager, fleet);
    scope.Note("result", result);
    return result;
  }
  void Stall(auto original, Object* manager, Object* queue, Object* player, Object* deployed)
  {
    Scope scope(manager, "native.watchdog");
    original(manager, queue, player, deployed);
  }
  void Disposed(auto original, Object* manager, Object* fleets)
  {
    Scope scope(manager, "native.disposed");
    original(manager, fleets);
  }
  int Engage(auto original, Object* manager, Object* player, Object* queue)
  {
    Scope     scope(manager, "native.engage");
    const int result = original(manager, player, queue);
    scope.Note("result", result);
    return result;
  }
  void ClearAll(auto original, Object* manager)
  {
    Scope scope(manager, "native.clear-all", 0, true);
    original(manager);
  }

  bool AddressMismatch(auto original, Object* manager, Object* player)
  {
    Scope scope(manager, "native.address-mismatch-check", 0, true);
    scope.Object("player", player);
    struct Context {
      Scope*  previousScope{addressScope};
      Object* previousPlayer{addressPlayer};
      Context(Scope& scope, Object* player)
      {
        addressScope  = &scope;
        addressPlayer = player;
      }
      ~Context()
      {
        addressScope  = previousScope;
        addressPlayer = previousPlayer;
      }
    } context(scope, player);
    const bool result = original(manager, player);
    scope.Note("native_mismatch", result);
    return result;
  }
  Object* PlayerAddress(auto original, Object* player)
  {
    auto* result = original(player);
    if (addressScope && player == addressPlayer) {
      addressScope->Note("player_address_found", result != nullptr);
      addressScope->Object("player_address", result);
    }
    return result;
  }
  Object* LookupTarget(auto original, Object* service, int64_t target)
  {
    auto* result = original(service, target);
    if (addressScope) {
      try {
        const std::string key = "target_" + std::to_string(target);
        addressScope->Note((key + "_found").c_str(), result != nullptr);
        addressScope->Object(key.c_str(), result);
        auto* activeSystem = Field(il2cpp_object_get_class(service), "_activeSystem", IL2CPP_TYPE_I8);
        if (activeSystem)
          addressScope->Note("deployment_active_system", Read<int64_t>(service, activeSystem));
      } catch (...) {
      }
    }
    return result;
  }
} // namespace

struct Scope::State {
  Il2CppObject* object{}; // Borrowed only across a synchronous game call.
  const char*   source{};
  const char*   previousSource{};
  uint64_t      span{}, parent{};
  int64_t       target{};
  bool          force{};
  Json          before, stack, notes = Json::object();
};
Scope::Scope(void* manager, const char* source, int64_t target, bool force) noexcept
{
  if (!logger)
    return;
  try {
    const auto span = ++sequence;
    if (span > limit) {
      if (span == limit + 1)
        Write({{"event", "capture_limit"}, {"limit", limit}});
      return;
    }
    auto s            = std::make_unique<State>();
    s->object         = static_cast<Il2CppObject*>(manager);
    s->source         = source;
    s->previousSource = parentSource;
    s->span           = span;
    s->parent         = parentSpan;
    s->target         = target;
    s->force          = force;
    s->before         = Snapshot(s->object);
    s->stack          = Stack();
    state_            = std::move(s);
    parentSpan        = span;
    parentSource      = source;
  } catch (...) { /* Observer failures must never change gameplay. */
  }
}
Scope::~Scope() noexcept
{
  if (!state_)
    return;
  auto& s      = *state_;
  parentSpan   = s.parent;
  parentSource = s.previousSource;
  try {
    auto after = Snapshot(s.object);
    if (s.force || s.before != after || !s.notes.empty()) {
      Write({{"event", "span"},
             {"span", s.span},
             {"parent", s.parent},
             {"parent_source", s.previousSource},
             {"source", s.source},
             {"target", s.target},
             {"before", s.before},
             {"after", after},
             {"changed", s.before != after},
             {"notes", s.notes},
             {"stack", s.stack},
             {"queue_enabled", Config::Get().queue_enabled},
             {"recovery_enabled", Config::Get().faster_queue_recovery},
             {"protection_enabled", Config::Get().thin_queue_protection}});
    }
  } catch (...) {
  }
}
void Scope::Note(const char* key, int64_t value) noexcept
{
  try {
    if (state_)
      state_->notes[key] = value;
  } catch (...) {
  }
}
void Scope::Object(const char* key, void* value, bool list) noexcept
{
  if (!state_ || !value)
    return;
  try {
    auto describe = [](Il2CppObject* object) {
      if (!object)
        return Json{{"valid", false}};
      auto* cls = il2cpp_object_get_class(object);
      Json  result{{"class", cls->name}};
      for (const char* name :
           {"fleetId_", "galaxy_", "system_", "planet_", "instance_", "<ID>k__BackingField", "<Index>k__BackingField",
            "_currentlyBattling", "<RemovalReason>k__BackingField", "<IsPlanningRecallCourse>k__BackingField"}) {
        auto* field = il2cpp_class_get_field_from_name(cls, name);
        if (!field || !field->type || field->offset < 0x10)
          continue;
        if (field->type->type == IL2CPP_TYPE_I8)
          result[name] = Read<int64_t>(object, field);
        else if (field->type->type == IL2CPP_TYPE_I4)
          result[name] = Read<int>(object, field);
        else if (field->type->type == IL2CPP_TYPE_BOOLEAN)
          result[name] = Read<bool>(object, field);
        else if (auto* enumClass = il2cpp_class_from_type(field->type); enumClass && il2cpp_class_is_enum(enumClass)) {
          auto* base = il2cpp_class_enum_basetype(enumClass);
          if (base && base->type == IL2CPP_TYPE_I4)
            result[name] = Read<int>(object, field);
        }
      }

      auto* address = Read<Il2CppObject*>(object, Field(cls, "_address", IL2CPP_TYPE_CLASS));
      if (address) {
        auto* addressClass = il2cpp_object_get_class(address);
        Json  values       = Json::object();
        for (const char* name : {"galaxy_", "system_", "planet_"}) {
          auto* field = Field(addressClass, name, IL2CPP_TYPE_I8);
          if (field)
            values[name] = Read<int64_t>(address, field);
        }
        auto* instance = Field(addressClass, "instance_", IL2CPP_TYPE_I4);
        if (instance)
          values["instance_"] = Read<int>(address, instance);
        result["address"] = std::move(values);
      }
      auto* deploymentField = il2cpp_class_get_field_from_name(cls, "_deploymentFleet");
      auto* deploymentType =
          deploymentField && deploymentField->type ? il2cpp_class_from_type(deploymentField->type) : nullptr;
      if (deploymentType && deploymentField->offset >= 0x10 && !il2cpp_class_is_valuetype(deploymentType)) {
        auto* model = Read<Il2CppObject*>(object, deploymentField);
        auto* id    = model ? Field(il2cpp_object_get_class(model), "fleetId_", IL2CPP_TYPE_I8) : nullptr;
        if (id)
          result["deployed_id"] = Read<int64_t>(model, id);
      }
      for (const char* name : {"_fleetStateContainer", "_stateContainer"}) {
        auto* field     = il2cpp_class_get_field_from_name(cls, name);
        auto* container = field && field->type ? il2cpp_class_from_type(field->type) : nullptr;
        if (!container || !il2cpp_class_is_valuetype(container) || field->offset < 0x10)
          continue;
        uint32_t  alignment{};
        const int size = il2cpp_class_value_size(container, &alignment);
        for (const char* member : {"_currentState", "_previousState"}) {
          auto* inner     = il2cpp_class_get_field_from_name(container, member);
          auto* enumClass = inner && inner->type ? il2cpp_class_from_type(inner->type) : nullptr;
          auto* base = enumClass && il2cpp_class_is_enum(enumClass) ? il2cpp_class_enum_basetype(enumClass) : nullptr;
          const int offset = inner ? inner->offset - static_cast<int>(sizeof(Il2CppObject)) : -1;
          if (!base || base->type != IL2CPP_TYPE_I4 || offset < 0 || offset + 4 > size)
            continue;
          int state{};
          std::memcpy(&state, reinterpret_cast<const char*>(object) + field->offset + offset, sizeof(state));
          result[member] = state;
        }
      }
      return result;
    };
    auto* object = static_cast<Il2CppObject*>(value);
    if (!list) {
      state_->notes[key] = describe(object);
      return;
    }
    auto*     cls     = il2cpp_object_get_class(object);
    auto*     size    = Field(cls, "_size", IL2CPP_TYPE_I4);
    auto*     storage = Field(cls, "_items", IL2CPP_TYPE_SZARRAY);
    const int count   = size ? Read<int>(object, size) : -1;
    auto*     array   = storage ? Read<Il2CppArray*>(object, storage) : nullptr;
    if (count < 0 || count > 64 || !array || il2cpp_array_length(array) < static_cast<unsigned>(count)) {
      state_->notes[key] = {{"valid", false}, {"count", count}};
      return;
    }
    auto  objects = Json::array();
    auto* items   = reinterpret_cast<Il2CppArraySize*>(array);
    for (int i = 0; i < count; ++i)
      objects.push_back(describe(static_cast<Il2CppObject*>(items->vector[i])));
    state_->notes[key] = std::move(objects);
  } catch (...) {
  }
}
void Own(const char* method, bool installed)
{
  if (installed)
    owners.insert(method);
}

void Install()
{
  if (!requested()) {
    spdlog::info("[QueueScience] observer disabled by environment");
    return;
  }
  managerClass  = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager").get_cls();
  queueClass    = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance").get_cls();
  actionClass   = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction").get_cls();
  queuesField   = Field(managerClass, "_battleQueue", IL2CPP_TYPE_SZARRAY);
  fleetField    = Field(queueClass, "<PlayerFleetId>k__BackingField", IL2CPP_TYPE_I8);
  engagingField = Field(queueClass, "IsEngaging", IL2CPP_TYPE_BOOLEAN);
  attemptField  = Field(queueClass, "LastEngageAttemptTime", IL2CPP_TYPE_R4);
  lastField     = Field(queueClass, "LastEngagedTargetId", IL2CPP_TYPE_I8);
  pendingField  = Field(queueClass, "PendingEngageTargetId", IL2CPP_TYPE_I8);
  actionsField  = Field(queueClass, "_actionQueue", IL2CPP_TYPE_GENERICINST);
  targetField   = Field(actionClass, "<FleetId>k__BackingField", IL2CPP_TYPE_I8);
  retriesField  = Field(actionClass, "SetCourseFailRetryCount", IL2CPP_TYPE_I4);
  if (!queuesField || !fleetField || !engagingField || !attemptField || !lastField || !pendingField || !actionsField
      || !targetField || !retriesField) {
    spdlog::warn("[QueueScience] queue field contract unavailable; capture not installed");
    return;
  }
  const auto        stamp    = std::chrono::system_clock::now().time_since_epoch().count();
  const std::string filename = "community_kirshara_queue_" + std::to_string(stamp) + ".jsonl";
  try {
    pool      = std::make_shared<spdlog::details::thread_pool>(4096, 1);
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, 3 * 1024 * 1024, 2);
    logger    = std::make_shared<spdlog::async_logger>("kirshara-science", sink, pool,
                                                       spdlog::async_overflow_policy::overrun_oldest);
    logger->set_pattern("%v");
    logger->flush_on(spdlog::level::info);
  } catch (...) {
    logger.reset();
    spdlog::warn("[QueueScience] capture file unavailable");
    return;
  }
  Json hooks = Json::array();
  auto hook  = [&](Il2CppClass* cls, const char* method, const char* result,
                   std::initializer_list<const char*> parameters, auto installer) {
    const auto* info      = method_contract::Resolve(cls, method, false, result, parameters);
    auto*       pointer   = method_contract::Pointer(info);
    const bool  shared    = cls == managerClass && owners.contains(method);
    const bool  installed = shared || (pointer && !info->has_full_generic_sharing_signature && installer(pointer));
    hooks.push_back(
        {{"class", cls ? cls->name : "missing"}, {"method", method}, {"installed", installed}, {"shared", shared}});
    if (!installed)
      spdlog::warn("[QueueScience] unavailable hook {}", method);
  };
#define QUEUE_HOOK(cls, method, result, callback, ...)                                                                 \
  hook(cls, method, result, {__VA_ARGS__},                                                                             \
       [](void* pointer) { return SPUD_STATIC_DETOUR(pointer, callback) != nullptr; })
  QUEUE_HOOK(managerClass, "AddActionToQueue", "System.Void", Add, "System.Int64");
  QUEUE_HOOK(managerClass, "ClearQueue", "System.Void", Clear, "Digit.PrimeServer.Models.FleetPlayerData");
  QUEUE_HOOK(managerClass, "CheckToClearActionQueue", "System.Void", CheckClear,
             "Digit.PrimeServer.Models.FleetPlayerData");
  QUEUE_HOOK(queueClass, "ClearQueue", "System.Void", InstanceClear);
  QUEUE_HOOK(managerClass, "ProcessQueue", "System.Void", Process, "System.Int64", "System.Boolean");
  QUEUE_HOOK(managerClass, "Cleanup", "System.Void", Cleanup);
  QUEUE_HOOK(managerClass, "OnFleetStateChangeEventHandler", "System.Void", StateChange,
             "System.Collections.Generic.List<Digit.PrimeServer.Models.FleetDeployedData>");
  QUEUE_HOOK(managerClass, "OnPlayerFleetStateChangedEventHandler", "System.Void", PlayerStateChange,
             "System.Collections.Generic.List<Digit.PrimeServer.Models.FleetPlayerData>");
  QUEUE_HOOK(managerClass, "set_IsQueueEnabled", "System.Void", Enable, "System.Boolean");
  QUEUE_HOOK(managerClass, "DoPlanPathAndEngageTarget", "System.Boolean", Plan,
             "Digit.PrimeServer.Models.FleetPlayerData");
  QUEUE_HOOK(managerClass, "HandleStall", "System.Void", Stall, "Prime.ActionQueue.ActionQueueInstance",
             "Digit.PrimeServer.Models.FleetPlayerData", "Digit.PrimeServer.Models.FleetDeployedData");
  QUEUE_HOOK(managerClass, "OnFleetsDisposedEventHandler", "System.Void", Disposed,
             "System.Collections.Generic.List<Digit.PrimeServer.Models.FleetDeployedData>");
  QUEUE_HOOK(managerClass, "TryPlanPathAndEngageTarget", "Digit.Prime.Combat.EngageResult", Engage,
             "Digit.PrimeServer.Models.FleetPlayerData", "Prime.ActionQueue.ActionQueueInstance");
  QUEUE_HOOK(managerClass, "StopWatchdogAndClearAllQueues", "System.Void", ClearAll);
  QUEUE_HOOK(managerClass, "DoesFleetQueueContainMismatchedAddressTargets", "System.Boolean", AddressMismatch,
             "Digit.PrimeServer.Models.FleetPlayerData");
  auto* playerClass =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData").get_cls();
  auto* serviceClass =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "DeploymentService")
          .get_cls();
  QUEUE_HOOK(playerClass, "get_Address", "Digit.PrimeServer.Models.NodeAddress", PlayerAddress);
  QUEUE_HOOK(serviceClass, "GetDeployedFleet", "Digit.PrimeServer.Models.FleetDeployedData", LookupTarget,
             "System.Int64");
#undef QUEUE_HOOK
  Write({{"event", "session"},
         {"identity", STFC_IDENTITY_COMMENT_STR},
         {"hooks", hooks},
         {"max_spans", limit},
         {"max_targets", 128},
         {"max_queues", 64},
         {"limits",
          "read-only field snapshots; invalid is unknown, not empty; rotated history and async overruns possible"}});
  spdlog::warn("[QueueScience] read-only Kirshara capture active: {}", filename);
}
} // namespace queue_science
