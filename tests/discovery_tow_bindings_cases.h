#pragma once

#include <functional>

// Extends the already-included production fleet_actions.cc consumer. Only managed
// metadata, generated method bodies and Unity's scheduler are controlled here.
// The actual Plan/Start/Stop/Callbacks/Submit adapters and Begin/Frame run intact.
namespace
{
struct TowNativeAddress {
  Il2CppObject header;
  int64_t      location = 17;
};
struct TowNativeFleet {
  Il2CppObject      header;
  int64_t           id = 0, hull = 0;
  int32_t           index   = 0;
  TowNativeAddress* address = nullptr;
};
struct TowNativeIterator {
  Il2CppObject  header;
  int32_t       state = 0;
  Il2CppObject *fleet = nullptr, *destination = nullptr;
};
struct TowNativeCallbacks {
  Il2CppObject  header;
  Il2CppObject *success = nullptr, *error = nullptr, *retry = nullptr;
  int32_t       mode = -1;
};
struct TowNativeArray {
  Il2CppObject        header;
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 0;
  Il2CppObject*       values[5]{};
};
struct TowNativeGeneric {
  std::array<const Il2CppType*, 1> arguments{};
  Il2CppGenericInst                instance{};
  Il2CppGenericClass               generic{};
  void                             Bind(FakeClass& closed, FakeClass& definition)
  {
    arguments[0]                              = T(int_class);
    instance.type_argc                        = 1;
    instance.type_argv                        = arguments.data();
    generic.type                              = T(definition);
    generic.context.class_inst                = &instance;
    closed.klass.byval_arg.data.generic_class = &generic;
  }
};

FakeClass *tow_native_address_class = nullptr, *tow_native_success_class = nullptr, *tow_native_error_class = nullptr,
          *tow_native_callbacks_class = nullptr;
TowNativeGeneric tow_native_success_generic, tow_native_callbacks_generic;
Il2CppObject     tow_native_singleton{}, tow_native_coroutine{}, tow_native_fleets{}, tow_native_sections{},
    tow_native_context{}, tow_native_bar{}, tow_native_course{}, tow_native_old_course{}, tow_native_path{},
    tow_native_node{}, tow_native_replacement_owner{};
TowNativeAddress  tow_native_address{}, tow_native_other_address{};
TowNativeFleet    tow_native_selected{}, tow_native_discovery{}, tow_native_later_discovery{}, tow_native_replacement{};
TowNativeArray    tow_native_array{};
TowNativeIterator tow_native_iterator{};
std::deque<TowNativeAddress>   tow_native_clones;
std::deque<Il2CppDelegate>     tow_native_delegates;
std::deque<TowNativeCallbacks> tow_native_containers;
FieldInfo*                     tow_native_singleton_field = nullptr;
const MethodInfo *             tow_native_plan_method = nullptr, *tow_native_submit_method = nullptr;
Il2CppObject*                  tow_native_current_owner = nullptr;
Il2CppObject*                  tow_native_planned       = nullptr;
SectionID                      tow_native_section       = SectionID::Navigation_Galaxy;
int  tow_native_plans = 0, tow_native_starts = 0, tow_native_stops = 0, tow_native_iterator_stops = 0,
     tow_native_submits = 0, tow_native_success_calls = 0, tow_native_error_calls = 0;
bool tow_native_owner_available = true, tow_native_callback_throw = false, tow_native_bad_readback = false,
     tow_native_allocation_failure = false, tow_native_start_throw = false, tow_native_start_null = false,
     tow_native_stop_throw = false, tow_native_can_tow = true, tow_native_can_be_towed = true,
     tow_native_can_tow_throw = false, tow_native_course_valid = true, tow_native_course_in_use = false;
std::function<void()> tow_native_on_plan, tow_native_on_start, tow_native_on_stop, tow_native_on_baseline,
    tow_native_on_valid;

void TowNativeReenter(std::function<void()>& callback)
{
  if (auto entered = std::exchange(callback, {}))
    entered();
}
bool TowNativeRooted(const Il2CppObject* object)
{
  return object && std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; });
}
void TowNativeReplaceMethod(FakeClass& owner, std::string_view name, void* body)
{
  const auto found = std::ranges::find_if(
      owner.methods, [name](const MethodInfo* method) { return std::string_view(method->name) == name; });
  Require(found != owner.methods.end(), "tow host only replaces an existing controlled native method");
  auto* method          = const_cast<MethodInfo*>(*found);
  method->methodPointer = method->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(body);
}
MethodInfo* TowNativeMetadata(std::string_view owner, std::string_view name)
{
  const auto klass = std::ranges::find_if(
      classes, [owner](const FakeClass& candidate) { return std::string_view(candidate.klass.name) == owner; });
  Require(klass != classes.end(), "tow fixture mutation names an existing controlled class");
  const auto method = std::ranges::find_if(
      klass->methods, [name](const MethodInfo* candidate) { return std::string_view(candidate->name) == name; });
  Require(method != klass->methods.end(), "tow fixture mutation names an existing controlled method");
  return const_cast<MethodInfo*>(*method);
}
Il2CppObject* TowNativeAllocate(const Il2CppClass* klass, bool& handled)
{
  handled = klass == &tow_native_success_class->klass || klass == &tow_native_error_class->klass
            || klass == &tow_native_callbacks_class->klass;
  if (!handled || tow_native_allocation_failure)
    return nullptr;
  if (klass == &tow_native_callbacks_class->klass) {
    auto& value        = tow_native_containers.emplace_back();
    value.header.klass = const_cast<Il2CppClass*>(klass);
    return &value.header;
  }
  auto& value        = tow_native_delegates.emplace_back();
  value.object.klass = const_cast<Il2CppClass*>(klass);
  return &value.object;
}
bool TowNativeStaticField(FieldInfo* field, void* value)
{
  if (field != tow_native_singleton_field)
    return false;
  Il2CppObject* singleton = tow_native_owner_available ? &tow_native_singleton : nullptr;
  std::memcpy(value, &singleton, sizeof(singleton));
  return true;
}
void TowNativeDelegateCtor(Il2CppMethodPointer, const MethodInfo*, void* self, void** args, void*)
{
  Require(TowNativeRooted(static_cast<Il2CppObject*>(self)) && TowNativeRooted(&tow_native_singleton),
          "actual closed delegate construction retains receiver and allocation");
  auto* delegate             = static_cast<Il2CppDelegate*>(self);
  delegate->method           = *static_cast<const MethodInfo**>(args[1]);
  delegate->target           = static_cast<Il2CppObject*>(args[0]);
  delegate->invoke_impl_this = delegate->target;
}
void TowNativeSuccess(Il2CppObject* self, int32_t value, const MethodInfo*)
{
  Require(self == &tow_native_singleton && value == 19, "closed success dispatch preserves receiver and Int32");
  ++tow_native_success_calls;
}
void TowNativeError(Il2CppObject* self, discovery_tow_binding::Error* error, const MethodInfo*)
{
  Require(self == &tow_native_singleton && error == nullptr, "closed error dispatch preserves receiver and null");
  ++tow_native_error_calls;
}
void TowNativeCallbackCtor(TowNativeCallbacks* self, Il2CppObject* success, Il2CppObject* error, int32_t mode,
                           Il2CppObject* retry, const MethodInfo*)
{
  Require(TowNativeRooted(&self->header) && TowNativeRooted(success) && TowNativeRooted(error),
          "callback object and both real delegates remain rooted through the constructor");
  Require(success != error && success != &self->header && error != &self->header && mode == 2 && retry == nullptr,
          "actual callback construction uses distinct delegates, HandleError and no retry");
  if (tow_native_callback_throw)
    throw Il2CppExceptionWrapper(&thrown);
  self->success = success;
  self->error   = error;
  self->retry   = retry;
  self->mode    = mode;
}
Il2CppObject* TowNativeSuccessRead(TowNativeCallbacks* self, const MethodInfo*)
{ return tow_native_bad_readback ? self->error : self->success; }
Il2CppObject* TowNativeErrorRead(TowNativeCallbacks* self, const MethodInfo*)
{ return self->error; }
Il2CppObject* TowNativeOwner(const MethodInfo*)
{ return tow_native_current_owner; }
Il2CppObject* TowNativeFleets(const MethodInfo*)
{ return &tow_native_fleets; }
Il2CppObject* TowNativeSections(const MethodInfo*)
{ return &tow_native_sections; }
SectionID TowNativeSection(Il2CppObject*, const MethodInfo*)
{ return tow_native_section; }
Il2CppObject* TowNativeContext(Il2CppObject*, const MethodInfo*)
{ return &tow_native_context; }
Il2CppObject* TowNativeFleetArray(Il2CppObject*, const MethodInfo*)
{ return &tow_native_array.header; }
Il2CppObject* TowNativeFleetAt(Il2CppObject*, int32_t index, const MethodInfo*)
{
  if (index == tow_native_selected.index)
    return &tow_native_selected.header;
  if (index == tow_native_discovery.index)
    return &tow_native_discovery.header;
  return nullptr;
}
int64_t TowNativeFleetId(TowNativeFleet* self, const MethodInfo*)
{ return self->id; }
int64_t TowNativeHullId(TowNativeFleet* self, const MethodInfo*)
{ return self->hull; }
int32_t TowNativeFleetIndex(TowNativeFleet* self, const MethodInfo*)
{ return self->index; }
Il2CppObject* TowNativeAddressRead(TowNativeFleet* self, const MethodInfo*)
{ return self->address ? &self->address->header : nullptr; }
Il2CppObject* TowNativeClone(TowNativeAddress* self, const MethodInfo*)
{
  Require(TowNativeRooted(&self->header), "native address stays rooted before cloning");
  auto& clone = tow_native_clones.emplace_back(*self);
  return &clone.header;
}
bool TowNativeAddressEquals(TowNativeAddress* self, TowNativeAddress* other, const MethodInfo*)
{ return other && self->location == other->location; }
bool TowNativeCanTow(Il2CppObject*, Il2CppObject* discovery, const MethodInfo*)
{
  Require(discovery == &tow_native_discovery.header, "tow eligibility uses acquired Discovery fleet");
  if (tow_native_can_tow_throw)
    throw Il2CppExceptionWrapper(&thrown);
  return tow_native_can_tow;
}
bool TowNativeCanBeTowed(Il2CppObject*, Il2CppObject* fleet, Il2CppObject* discovery, const MethodInfo*)
{
  Require(fleet == &tow_native_selected.header && discovery == &tow_native_discovery.header,
          "tow eligibility uses original selected and Discovery objects");
  return tow_native_can_be_towed;
}
Il2CppObject* TowNativeCourse(Il2CppObject*, int64_t fleet, const MethodInfo*)
{
  Require(fleet == tow_native_selected.id, "planned course lookup preserves the exact signed fleet identity");
  TowNativeReenter(tow_native_on_baseline);
  return tow_native_planned;
}
int64_t TowNativeCourseFleet(Il2CppObject*, const MethodInfo*)
{ return tow_native_selected.id; }
bool TowNativeCourseInUse(Il2CppObject*, const MethodInfo*)
{ return tow_native_course_in_use; }
Il2CppObject* TowNativeCoursePath(Il2CppObject*, const MethodInfo*)
{ return &tow_native_path; }
Il2CppObject* TowNativeCourseNode(Il2CppObject*, const MethodInfo*)
{ return &tow_native_node; }
Il2CppObject* TowNativeNodeAddress(Il2CppObject*, const MethodInfo*)
{ return tow_native_clones.empty() ? &tow_native_address.header : &tow_native_clones.back().header; }
bool TowNativePathValid(Il2CppObject*, const MethodInfo*)
{
  TowNativeReenter(tow_native_on_valid);
  return tow_native_course_valid;
}
Il2CppObject* TowNativePlan(Il2CppObject* self, Il2CppObject* fleet, Il2CppObject* destination, unity::Vector3 position,
                            Il2CppObject* deployed, Il2CppObject* starbase, Il2CppObject* alliance,
                            Il2CppObject* outpost, const MethodInfo*)
{
  ++tow_native_plans;
  Require(self == tow_native_current_owner && fleet == &tow_native_selected.header && destination && position.x == 0
              && position.y == 0 && position.z == 0 && !deployed && !starbase && !alliance && !outpost,
          "actual seven-argument planner preserves the exact request and empty optional native targets");
  Require(TowNativeRooted(fleet) && TowNativeRooted(destination), "planner input references are under owned custody");
  tow_native_iterator.state       = 0;
  tow_native_iterator.fleet       = fleet;
  tow_native_iterator.destination = destination;
  TowNativeReenter(tow_native_on_plan);
  return &tow_native_iterator.header;
}
Il2CppObject* TowNativeStart(Il2CppObject*, Il2CppObject* iterator, const MethodInfo*)
{
  ++tow_native_starts;
  Require(iterator == &tow_native_iterator.header && TowNativeRooted(iterator),
          "native scheduler receives the exact rooted iterator");
  tow_native_iterator.state = 1; // Unity, not the mod, advances the coroutine.
  TowNativeReenter(tow_native_on_start);
  if (tow_native_start_throw)
    throw Il2CppExceptionWrapper(&thrown);
  return tow_native_start_null ? nullptr : &tow_native_coroutine;
}
void TowNativeStop(Il2CppObject*, Il2CppObject* coroutine, const MethodInfo*)
{
  ++tow_native_stops;
  Require(coroutine == &tow_native_coroutine, "normal cancellation stops the exact returned native coroutine");
  TowNativeReenter(tow_native_on_stop);
  if (tow_native_stop_throw)
    throw Il2CppExceptionWrapper(&thrown);
}
void TowNativeStopIterator(Il2CppObject*, Il2CppObject* iterator, const MethodInfo*)
{
  ++tow_native_iterator_stops;
  Require(iterator == &tow_native_iterator.header, "start failure falls back to the original iterator handle");
  TowNativeReenter(tow_native_on_stop);
  if (tow_native_stop_throw)
    throw Il2CppExceptionWrapper(&thrown);
}
void TowNativeSubmit(Il2CppObject* self, int64_t selected_id, int64_t discovery_id, Il2CppObject* callbacks,
                     const MethodInfo*)
{
  ++tow_native_submits;
  Require(self == tow_native_current_owner && selected_id == tow_native_selected.id
              && discovery_id == tow_native_discovery.id && callbacks
              && callbacks->klass == &tow_native_callbacks_class->klass && TowNativeRooted(callbacks),
          "actual native submission preserves both signed identities and the owned real callback container");
  Require(!DiscoveryTowing().run, "completed request is retired before entering native submission");
}

Il2CppObject* TowNativeSharedInvoke(const MethodInfo* method, void* receiver, void** args, Il2CppException** exception,
                                    bool& handled)
{
  const auto name = std::string_view(method->name);
  handled         = method == tow_native_plan_method || method == tow_native_submit_method
                    || (name == "StartCoroutine" && std::string_view(method->klass->name) == "MonoBehaviour")
                    || (name == ".ctor" && method->klass == &tow_native_callbacks_class->klass);
  if (!handled)
    return nullptr;
  try {
    auto* self = static_cast<Il2CppObject*>(receiver);
    if (method == tow_native_plan_method)
      return TowNativePlan(self, static_cast<Il2CppObject*>(args[0]), static_cast<Il2CppObject*>(args[1]),
                           *static_cast<unity::Vector3*>(args[2]), static_cast<Il2CppObject*>(args[3]),
                           static_cast<Il2CppObject*>(args[4]), static_cast<Il2CppObject*>(args[5]),
                           static_cast<Il2CppObject*>(args[6]), method);
    if (method == tow_native_submit_method)
      TowNativeSubmit(self, *static_cast<int64_t*>(args[0]), *static_cast<int64_t*>(args[1]),
                      static_cast<Il2CppObject*>(args[2]), method);
    else if (name == "StartCoroutine")
      return TowNativeStart(self, static_cast<Il2CppObject*>(args[0]), method);
    else
      TowNativeCallbackCtor(static_cast<TowNativeCallbacks*>(receiver), static_cast<Il2CppObject*>(args[0]),
                            static_cast<Il2CppObject*>(args[1]), *static_cast<int32_t*>(args[2]),
                            static_cast<Il2CppObject*>(args[3]), method);
  } catch (const Il2CppExceptionWrapper& error) {
    *exception = error.ex;
  }
  return nullptr;
}

void BuildTowNativeRuntime()
{
  auto& mono      = Define("UnityEngine", "MonoBehaviour", IL2CPP_TYPE_CLASS, &component_class);
  auto& routine   = Define("System.Collections", "IEnumerator", IL2CPP_TYPE_CLASS, &object_class);
  auto& coroutine = Define("UnityEngine", "Coroutine", IL2CPP_TYPE_CLASS, &object_class);
  auto& service   = Define("Digit.PrimeServer.Services", "DeploymentService", IL2CPP_TYPE_CLASS, &object_class);
  auto& iterator  = Define("", "<PlanCourse>d__136", IL2CPP_TYPE_CLASS, &routine);
  iterator.klass.declaringType = &service.klass;
  service.nested.push_back(&iterator.klass);
  auto& shortcuts      = Define("Digit.Prime.GameInput", "ShortcutsManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& callback_owner = Define("", "<>c", IL2CPP_TYPE_CLASS, &object_class);
  callback_owner.klass.declaringType = &shortcuts.klass;
  shortcuts.nested.push_back(&callback_owner.klass);
  auto& callbacks_definition = Define("Digit.Networking.Core", "CallbackContainer`1", IL2CPP_TYPE_CLASS, &object_class);
  auto& success_definition   = Define("Digit.Networking.Core", "OnSuccess`1", IL2CPP_TYPE_CLASS, &delegate_base);
  auto& callbacks = Define("Digit.Networking.Core", "CallbackContainer<Int32>", IL2CPP_TYPE_GENERICINST, &object_class);
  auto& success   = Define("Digit.Networking.Core", "OnSuccess<Int32>", IL2CPP_TYPE_GENERICINST, &delegate_base);
  auto& error     = Define("Digit.Networking.Core", "OnError", IL2CPP_TYPE_CLASS, &delegate_base);
  auto& retry     = Define("Digit.Networking.Core", "OnRetry", IL2CPP_TYPE_CLASS, &delegate_base);
  auto& gs_error  = Define("Digit.Networking.Core", "GSError", IL2CPP_TYPE_CLASS, &object_class);
  auto& mode      = Define("Digit.Networking.Core", "CallbackErrorHandling", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  mode.is_enum    = true;
  AddField(mode, "value__", int_class, sizeof(Il2CppObject));
  tow_native_callbacks_generic.Bind(callbacks, callbacks_definition);
  tow_native_success_generic.Bind(success, success_definition);
  tow_native_success_class   = &success;
  tow_native_error_class     = &error;
  tow_native_callbacks_class = &callbacks;
  AddField(callback_owner, "<>9", callback_owner, 0, true);
  tow_native_singleton_field = &callback_owner.fields[0];
  tow_native_singleton.klass = &callback_owner.klass;
  for (auto* delegate : {&success, &error}) {
    auto& ctor          = AddMethod(*delegate, ".ctor", reinterpret_cast<void*>(&Never), T(void_class),
                                    {T(object_class), T(intptr_class)});
    ctor.invoker_method = &TowNativeDelegateCtor;
  }
  AddMethod(success, "Invoke", reinterpret_cast<void*>(&Never), T(void_class), {T(int_class)});
  AddMethod(error, "Invoke", reinterpret_cast<void*>(&Never), T(void_class), {T(gs_error)});
  auto& success_target          = AddMethod(callback_owner, "<OnShipRecallAction>b__91_0",
                                            reinterpret_cast<void*>(&TowNativeSuccess), T(void_class), {T(int_class)});
  auto& error_target            = AddMethod(callback_owner, "<OnShipRecallAction>b__91_1",
                                            reinterpret_cast<void*>(&TowNativeError), T(void_class), {T(gs_error)});
  success_target.invoker_method = error_target.invoker_method = &WarpInvokerSentinel;
  AddMethod(callbacks, ".ctor", reinterpret_cast<void*>(&TowNativeCallbackCtor), T(void_class),
            {T(success), T(error), T(mode), T(retry)});
  AddMethod(callbacks, "get_OnSuccess", reinterpret_cast<void*>(&TowNativeSuccessRead), T(success), {});
  AddMethod(callbacks, "get_OnError", reinterpret_cast<void*>(&TowNativeErrorRead), T(error), {});
  auto& address  = Define("Digit.PrimeServer.Models", "NodeAddress", IL2CPP_TYPE_CLASS, &object_class);
  auto& deployed = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& starbase = Define("Digit.PrimeServer.Models", "StarbaseData", IL2CPP_TYPE_CLASS, &object_class);
  auto& alliance = Define("Digit.PrimeServer.Models", "AllianceStarbaseData", IL2CPP_TYPE_CLASS, &object_class);
  auto& outpost  = Define("Digit.PrimeServer.Models", "Outpost", IL2CPP_TYPE_CLASS, &object_class);
  auto& path     = Define("Digit.PrimeServer.Models", "PathFinderResultData", IL2CPP_TYPE_CLASS, &object_class);
  auto& fleets   = Define("Digit.Prime.FleetManagement", "FleetsManager", IL2CPP_TYPE_CLASS, &mono);
  auto& sections = Define("Digit.Prime.Navigation", "NavigationSectionManager", IL2CPP_TYPE_CLASS, &mono);
  auto& bar      = Define("Digit.Prime.HUD", "FleetBarViewController", IL2CPP_TYPE_CLASS, &mono);
  auto& context  = Define("Digit.Prime.HUD", "FleetBarContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& vector   = Define("System", "FleetPlayerData[]", IL2CPP_TYPE_SZARRAY, &array_class);
  vector.klass.byval_arg.data.type   = T(fleet_data_class);
  warp_deployment_class.klass.parent = &mono.klass;
  tow_native_address_class           = &address;
  tow_native_address.header.klass = tow_native_other_address.header.klass = &address.klass;
  tow_native_other_address.location                                       = 99;
  tow_native_iterator.header.klass                                        = &iterator.klass;
  tow_native_coroutine.klass                                              = &coroutine.klass;
  tow_native_fleets.klass                                                 = &fleets.klass;
  tow_native_sections.klass                                               = &sections.klass;
  tow_native_context.klass                                                = &context.klass;
  tow_native_bar.klass                                                    = &bar.klass;
  tow_native_course.klass = tow_native_old_course.klass = &warp_course_class.klass;
  tow_native_path.klass                                 = &path.klass;
  tow_native_node.klass                                 = &warp_node_class.klass;
  tow_native_current_owner                              = &warp_deployment;
  tow_native_replacement_owner.klass                    = &warp_deployment_class.klass;
  tow_native_selected           = {{&fleet_data_class.klass}, 9007199254740993LL, 123, 0, &tow_native_address};
  tow_native_discovery          = {{&fleet_data_class.klass}, 9007199254740995LL, 1307832955, 12, &tow_native_address};
  tow_native_later_discovery    = {{&fleet_data_class.klass}, 9007199254740997LL, 1307832955, 18, &tow_native_address};
  tow_native_replacement        = {{&fleet_data_class.klass}, 9007199254740999LL, 123, 0, &tow_native_address};
  tow_native_array.header.klass = &vector.klass;
  tow_native_array.length       = 5;
  tow_native_array.values[0]    = &tow_native_later_discovery.header;
  tow_native_array.values[1]    = nullptr;
  tow_native_array.values[2]    = &tow_native_selected.header;
  tow_native_array.values[3]    = &tow_native_discovery.header;
  tow_native_array.values[4]    = nullptr;
  TowNativeReplaceMethod(warp_deployment_class, "get_Instance", reinterpret_cast<void*>(&TowNativeOwner));
  TowNativeReplaceMethod(warp_deployment_class, "GetPlannedCourse", reinterpret_cast<void*>(&TowNativeCourse));
  TowNativeReplaceMethod(fleet_data_class, "get_Id", reinterpret_cast<void*>(&TowNativeFleetId));
  TowNativeReplaceMethod(warp_course_class, "get_FleetID", reinterpret_cast<void*>(&TowNativeCourseFleet));
  TowNativeReplaceMethod(warp_course_class, "get_TargetNode", reinterpret_cast<void*>(&TowNativeCourseNode));
  AddMethod(fleet_data_class, "get_HullId", reinterpret_cast<void*>(&TowNativeHullId), T(long_class), {});
  AddMethod(fleet_data_class, "get_Index", reinterpret_cast<void*>(&TowNativeFleetIndex), T(int_class), {});
  AddMethod(fleet_data_class, "get_Address", reinterpret_cast<void*>(&TowNativeAddressRead), T(address), {});
  AddMethod(address, "Clone", reinterpret_cast<void*>(&TowNativeClone), T(address), {});
  AddMethod(address, "Equals", reinterpret_cast<void*>(&TowNativeAddressEquals), T(bool_class), {T(address)});
  AddMethod(fleets, "get_Instance", reinterpret_cast<void*>(&TowNativeFleets), T(fleets), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(fleets, "GetFleetPlayerData", reinterpret_cast<void*>(&TowNativeFleetAt), T(fleet_data_class),
            {T(int_class)});
  AddMethod(sections, "get_Instance", reinterpret_cast<void*>(&TowNativeSections), T(sections), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&TowNativeSection), T(int_class), {});
  AddMethod(bar, "get_CanvasContext", reinterpret_cast<void*>(&TowNativeContext), T(context), {});
  AddMethod(context, "get_FleetData", reinterpret_cast<void*>(&TowNativeFleetArray), T(vector), {});
  AddMethod(warp_deployment_class, "CanTow", reinterpret_cast<void*>(&TowNativeCanTow), T(bool_class),
            {T(fleet_data_class)});
  AddMethod(warp_deployment_class, "CanBeTowed", reinterpret_cast<void*>(&TowNativeCanBeTowed), T(bool_class),
            {T(fleet_data_class), T(fleet_data_class)});
  tow_native_plan_method =
      &AddMethod(warp_deployment_class, "PlanCourse", reinterpret_cast<void*>(&TowNativePlan), T(routine),
                 {T(fleet_data_class), T(address), T(vec3_class), T(deployed), T(starbase), T(alliance), T(outpost)});
  tow_native_submit_method =
      &AddMethod(warp_deployment_class, "SetTowRequest", reinterpret_cast<void*>(&TowNativeSubmit), T(void_class),
                 {T(long_class), T(long_class), T(callbacks)});
  AddMethod(mono, "StartCoroutine", reinterpret_cast<void*>(&TowNativeStart), T(coroutine), {T(routine)});
  AddMethod(mono, "StopCoroutine", reinterpret_cast<void*>(&TowNativeStop), T(void_class), {T(coroutine)});
  AddMethod(mono, "StopCoroutine", reinterpret_cast<void*>(&TowNativeStopIterator), T(void_class), {T(routine)});
  AddField(iterator, "<>1__state", int_class, offsetof(TowNativeIterator, state));
  AddField(iterator, "fleet", fleet_data_class, offsetof(TowNativeIterator, fleet));
  AddField(iterator, "targetAddress", address, offsetof(TowNativeIterator, destination));
  AddMethod(warp_course_class, "get_IsInUse", reinterpret_cast<void*>(&TowNativeCourseInUse), T(bool_class), {});
  AddMethod(warp_course_class, "get_PathFinderResult", reinterpret_cast<void*>(&TowNativeCoursePath), T(path), {});
  AddMethod(warp_node_class, "get_Address", reinterpret_cast<void*>(&TowNativeNodeAddress), T(address), {});
  AddMethod(path, "get_Valid", reinterpret_cast<void*>(&TowNativePathValid), T(bool_class), {});
  tow_object_new_override                   = &TowNativeAllocate;
  tow_static_field_read_override            = &TowNativeStaticField;
  Config::Get().control.hotkeys_enabled     = true;
  Config::Get().control.use_scopely_hotkeys = false;
  fleet_native_capture                      = false;
  visual_alive                              = true;
}

void TowNativePoll(int32_t state)
{
  tow_native_iterator.state = state;
  if (DiscoveryTowing().run)
    DiscoveryTowing().run->next_poll = {};
  DiscoveryTowFrame();
}

void DiscoveryTowNativeCases(std::string_view mode)
{
  BuildTowNativeRuntime();
  if (mode == "discovery-tow-virtual-equals") {
    // Retained NodeAddress exposes Equals(object) before Equals(NodeAddress).
    // The shared resolver can select that compatible virtual override.
    auto* equals = TowNativeMetadata("NodeAddress", "Equals");
    equals->flags |= METHOD_ATTRIBUTE_VIRTUAL;
    const_cast<const Il2CppType**>(equals->parameters)[0] = T(object_class);
  } else if (mode == "discovery-tow-shared") {
    for (auto* method :
         {const_cast<MethodInfo*>(tow_native_plan_method), const_cast<MethodInfo*>(tow_native_submit_method),
          TowNativeMetadata("MonoBehaviour", "StartCoroutine"),
          TowNativeMetadata("CallbackContainer<Int32>", ".ctor")}) {
      method->has_full_generic_sharing_signature = 1;
      method->invoker_method                     = &WarpInvokerSentinel;
    }
    tow_runtime_invoke_override = &TowNativeSharedInvoke;
  } else if (mode != "discovery-tow-native") {
    if (mode == "discovery-tow-reject-arity")
      const_cast<MethodInfo*>(tow_native_plan_method)->parameters_count = 6;
    else if (mode == "discovery-tow-reject-unsigned")
      const_cast<const Il2CppType**>(tow_native_submit_method->parameters)[0] = T(unsigned_long_class);
    else if (mode == "discovery-tow-reject-generic")
      tow_native_callbacks_generic.arguments[0] = T(long_class);
    else if (mode == "discovery-tow-reject-vector")
      const_cast<const Il2CppType**>(tow_native_plan_method->parameters)[2] = T(foreign_vector_class);
    else if (mode == "discovery-tow-reject-null-plan")
      const_cast<MethodInfo*>(tow_native_plan_method)->methodPointer = nullptr;
    else if (mode == "discovery-tow-reject-null-start")
      TowNativeMetadata("MonoBehaviour", "StartCoroutine")->methodPointer = nullptr;
    else if (mode == "discovery-tow-reject-null-instance")
      TowNativeMetadata("MonoBehaviour", "StartCoroutine")->virtualMethodPointer = nullptr;
    else if (mode == "discovery-tow-reject-null-tow")
      const_cast<MethodInfo*>(tow_native_submit_method)->methodPointer = nullptr;
    else if (mode == "discovery-tow-reject-null-ctor")
      TowNativeMetadata("CallbackContainer<Int32>", ".ctor")->methodPointer = nullptr;
    else if (mode == "discovery-tow-reject-null-support")
      TowNativeMetadata("NodeAddress", "Clone")->methodPointer = nullptr;
    else if (mode == "discovery-tow-reject-byref-void") {
      auto& result = field_types.emplace_back(*tow_native_submit_method->return_type);
      result.byref = 1;
      const_cast<MethodInfo*>(tow_native_submit_method)->return_type = &result;
    } else if (mode == "discovery-tow-reject-shared-invoker") {
      auto* method                               = TowNativeMetadata("CallbackContainer<Int32>", ".ctor");
      method->has_full_generic_sharing_signature = 1;
      method->invoker_method                     = nullptr;
    } else
      Require(false, "unknown Discovery towing fixture mode");
    Require(!discovery_tow_binding::Native().ready && !discovery_tow_binding::Callbacks() && tow_native_plans == 0
                && tow_native_starts == 0 && tow_native_submits == 0 && warp_roots.empty(),
            "drifted tow metadata refuses before native work or callback construction");
    return;
  }
  namespace binding = discovery_tow_binding;
  Require(binding::Native().ready, "retained seven-argument planner, iterator and closed callbacks qualify");
  const auto roots = warp_roots.size();
  {
    auto callbacks = binding::Callbacks();
    Require(callbacks && warp_roots.size() == roots + 1,
            "actual callback adapter retains one complete managed object graph");
    auto* value = reinterpret_cast<TowNativeCallbacks*>(callbacks.Get());
    Require(value->success && value->error && value->mode == 2 && !value->retry,
            "real callback constructor fields survive native readback");
    il2cpp::InvokeBound<void>(reinterpret_cast<Il2CppDelegate*>(value->success), int32_t{19});
    il2cpp::InvokeBound<void>(reinterpret_cast<Il2CppDelegate*>(value->error), static_cast<binding::Error*>(nullptr));
    Require(tow_native_success_calls == 1 && tow_native_error_calls == 1,
            "both actual closed delegates dispatch to the native owner's exact handlers");
  }
  Require(warp_roots.size() == roots, "completed callback probe releases temporary native roots");
  tow_native_owner_available = false;
  Require(!binding::Callbacks(), "known null singleton cannot create fake callbacks");
  tow_native_owner_available = true;
  tow_native_callback_throw  = true;
  Require(!binding::Callbacks(), "native callback constructor failure is contained");
  tow_native_callback_throw = false;
  tow_native_bad_readback   = true;
  Require(!binding::Callbacks(), "callback readback mismatch cannot authorize a request");
  tow_native_bad_readback       = false;
  tow_native_allocation_failure = true;
  Require(!binding::Callbacks(), "managed allocation failure refuses without borrowed callback pointers");
  tow_native_allocation_failure = false;
  Require(warp_roots.size() == roots, "all callback failures release their acquired roots");
  auto* bar = reinterpret_cast<FleetBarViewController*>(&tow_native_bar);
  Require(BeginDiscoveryTow(bar, 0) && DiscoveryTowing().run && tow_native_plans == 1 && tow_native_starts == 1
              && DiscoveryTowing().run->request.discovery_index == 12,
          "actual acquisition scans the complete typed fleet array and selects the lowest Discovery native index");
  Require(binding::State(reinterpret_cast<binding::Routine*>(&tow_native_iterator)).value == 1,
          "only the controlled native scheduler advances the iterator to its first yielded state");
  TowNativePoll(1);
  TowNativePoll(2);
  Require(DiscoveryTowing().run && tow_native_submits == 0, "yield states wait without advancing or submitting");
  tow_native_planned = &tow_native_course;
  TowNativePoll(-1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1 && tow_native_stops == 1 && warp_roots.size() == roots,
          "terminal fresh course stops and submits exactly once then releases owned roots");
  TowNativePoll(-1);
  Require(tow_native_submits == 1, "later frames cannot repeat the completed native command");

  const auto begin = [&] {
    tow_native_planned = nullptr;
    Require(BeginDiscoveryTow(bar, 0), "next independent tow begins from a known null baseline");
  };
  begin();
  tow_native_address.location = 18;
  TowNativePoll(1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1,
          "mutated original address cannot match the immutable cloned request destination");
  tow_native_address.location = 17;
  begin();
  tow_native_section = SectionID::Navigation_System;
  TowNativePoll(1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1, "section changes retire publication");
  tow_native_section = SectionID::Navigation_Galaxy;
  begin();
  tow_native_current_owner = &tow_native_replacement_owner;
  TowNativePoll(1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1, "owner replacement cannot publish the original request");
  tow_native_current_owner = &warp_deployment;
  begin();
  fleet_native_capture = true;
  TowNativePoll(1);
  fleet_native_capture = false;
  Require(!DiscoveryTowing().run && tow_native_submits == 1, "Settings capture cancels publication and roots");
  begin();
  DiscoveryTowing().run->request.deadline_ms = TowMilliseconds();
  TowNativePoll(1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1, "absolute deadline cancels without synthetic completion");
  begin();
  tow_native_stop_throw = true;
  CancelDiscoveryTow();
  tow_native_stop_throw = false;
  tow_native_planned    = &tow_native_course;
  TowNativePoll(-1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1 && warp_roots.size() == roots,
          "unconfirmed scheduler stop still retires publication and releases native request ownership");

  const auto starts  = tow_native_starts;
  tow_native_on_plan = [] { CancelDiscoveryTow(); };
  Require(!BeginDiscoveryTow(bar, 0) && !DiscoveryTowing().run && tow_native_starts == starts,
          "cancellation reentering Plan never starts or submits its returned old iterator");
  tow_native_on_start = [] { CancelDiscoveryTow(); };
  const auto stops    = tow_native_stops;
  tow_native_planned  = nullptr;
  Require(!BeginDiscoveryTow(bar, 0) && !DiscoveryTowing().run && tow_native_stops > stops,
          "cancellation reentering Start retires its actual returned Coroutine without touching a replacement");
  const auto iterator_stops = tow_native_iterator_stops;
  tow_native_start_throw    = true;
  Require(!BeginDiscoveryTow(bar, 0) && !DiscoveryTowing().run && tow_native_iterator_stops > iterator_stops,
          "native Start mutation followed by failure is stopped through its original iterator overload");
  tow_native_start_throw = false;
  tow_native_start_null  = true;
  Require(!BeginDiscoveryTow(bar, 0) && !DiscoveryTowing().run,
          "observed null scheduler handle cannot become a running owned request");
  tow_native_start_null = false;

  // Native getters may run managed callbacks. Check the actual command boundary,
  // not only a separately reconstructed policy input.
  tow_native_planned     = nullptr;
  auto before_plan       = tow_native_plans;
  tow_native_on_baseline = [] { Config::Get().control.hotkeys_enabled = false; };
  Require(!BeginDiscoveryTow(bar, 0) && tow_native_plans == before_plan && !DiscoveryTowing().run,
          "input disabled by a baseline getter cannot reach native planning");
  Config::Get().control.hotkeys_enabled = true;
  tow_native_on_baseline                = [] { tow_native_section = SectionID::Navigation_System; };
  Require(!BeginDiscoveryTow(bar, 0) && tow_native_plans == before_plan && !DiscoveryTowing().run,
          "section changed during baseline acquisition cannot reach native planning");
  tow_native_section               = SectionID::Navigation_Galaxy;
  const auto starts_before_reentry = tow_native_starts;
  tow_native_on_plan               = [] { Config::Get().control.hotkeys_enabled = false; };
  Require(!BeginDiscoveryTow(bar, 0) && tow_native_starts == starts_before_reentry && !DiscoveryTowing().run,
          "input disabled by Plan cannot reach StartCoroutine");
  Config::Get().control.hotkeys_enabled = true;
  tow_native_on_plan                    = [] { tow_native_section = SectionID::Navigation_System; };
  Require(!BeginDiscoveryTow(bar, 0) && tow_native_starts == starts_before_reentry && !DiscoveryTowing().run,
          "section changed by Plan cannot reach StartCoroutine");
  tow_native_section = SectionID::Navigation_Galaxy;

  begin();
  tow_native_planned = &tow_native_course;
  tow_native_on_stop = [bar] {
    tow_native_planned = nullptr;
    Require(BeginDiscoveryTow(bar, 0), "reentrant replacement obtains its own owned request");
  };
  TowNativePoll(-1);
  Require(DiscoveryTowing().run && tow_native_submits == 1,
          "replacement during terminal Stop wins and cannot receive the old tow publication");
  CancelDiscoveryTow();
  Require(warp_roots.size() == roots, "reentrant replacement cleanup releases all old and new roots");

  begin();
  tow_native_planned  = &tow_native_course;
  int valid_reads     = 0;
  tow_native_on_valid = [&] {
    if (++valid_reads == 1) {
      tow_native_on_valid = [] { tow_native_section = SectionID::Navigation_System; };
    }
  };
  TowNativePoll(-1);
  Require(!DiscoveryTowing().run && tow_native_submits == 1,
          "section changed by the final course getter cannot reuse earlier source evidence to submit");
  tow_native_section = SectionID::Navigation_Galaxy;

  tow_native_can_tow = false;
  Require(!BeginDiscoveryTow(bar, 0), "observed false tow eligibility declines");
  tow_native_can_tow       = true;
  tow_native_can_tow_throw = true;
  Require(!BeginDiscoveryTow(bar, 0), "unavailable tow eligibility declines independently from observed false");
  tow_native_can_tow_throw      = false;
  const auto before_failed_root = tow_native_plans;
  warp_root_failure             = true;
  Require(!BeginDiscoveryTow(bar, 0) && tow_native_plans == before_failed_root,
          "failed initial managed custody cannot start native planning");
  warp_root_failure = false;
  Require(warp_roots.size() == roots && tow_native_submits == 1,
          "tow qualification has no leaked roots or unexpected native commands");
  tow_object_new_override        = nullptr;
  tow_static_field_read_override = nullptr;
  tow_runtime_invoke_override    = nullptr;
  std::cout
      << "Production Discovery tow acquisition, exact native bindings, scheduler and publication lifecycle passed\n";
}
} // namespace
