#pragma once

// Actual address-guard hooks and bindings; only the managed bodies/storage are substituted.
namespace
{
struct QueueAddressValue {
  Il2CppObject header{};
  int64_t      galaxy = 0, system = 0, planet = 0;
  int32_t      instance = 0;
};
struct QueueAddressDeployment {
  Il2CppObject       header{};
  bool               local   = true;
  QueueAddressValue* address = nullptr;
  Il2CppObject*      model   = nullptr;
};
int                   queue_address_fault = 0;
std::function<void()> queue_address_on_recall;
void                  QueueAddressFault(int id)
{
  if (queue_address_fault == id)
    throw Il2CppExceptionWrapper(&thrown);
}
int32_t QueueAddressIndex(FleetPlayerData*, const MethodInfo*)
{
  QueueAddressFault(1);
  return 0;
}
bool QueueAddressRecall(FleetDeployedData*, const MethodInfo*)
{
  QueueAddressFault(2);
  if (queue_address_on_recall)
    queue_address_on_recall();
  return false;
}
FleetState QueueAddressPlayerState(FleetPlayerData*, const MethodInfo*)
{
  QueueAddressFault(3);
  return FleetState::IdleInSpace;
}
DeployedFleetState QueueAddressDeployedState(FleetDeployedData*, const MethodInfo*)
{
  QueueAddressFault(4);
  return DeployedFleetState::Idle;
}
FleetRemovalReason QueueAddressRemoval(FleetDeployedData*, const MethodInfo*)
{
  QueueAddressFault(5);
  return static_cast<FleetRemovalReason>(0);
}
int64_t QueueAddressGalaxy(QueueAddressValue* value, const MethodInfo*)
{
  QueueAddressFault(6);
  return value->galaxy;
}
int64_t QueueAddressSystem(QueueAddressValue* value, const MethodInfo*)
{
  QueueAddressFault(7);
  return value->system;
}
int64_t QueueAddressPlanet(QueueAddressValue* value, const MethodInfo*)
{
  QueueAddressFault(8);
  return value->planet;
}
int32_t QueueAddressInstance(QueueAddressValue* value, const MethodInfo*)
{
  QueueAddressFault(9);
  return value->instance;
}
int64_t QueueAddressModelId(Il2CppObject*, const MethodInfo*)
{ return 999; }

void QueueAddressCases(std::string_view mode)
{
  auto& action_type    = Define("Prime.ActionQueue", "QueueableAction", IL2CPP_TYPE_CLASS, &object_class);
  auto& queue_type     = Define("Prime.ActionQueue", "ActionQueueInstance", IL2CPP_TYPE_CLASS, &object_class);
  auto& manager_type   = Define("Prime.ActionQueue", "ActionQueueManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& list_type      = Define("Fixture", "AddressQueueList", IL2CPP_TYPE_CLASS, &object_class);
  auto& vector_type    = Define("Fixture", "AddressQueueVector", IL2CPP_TYPE_SZARRAY, &array_class);
  auto& deployed_type  = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& address_type   = Define("Digit.PrimeServer.Models", "NodeAddress", IL2CPP_TYPE_CLASS, &object_class);
  auto& model_type     = Define("Fixture", "AddressDeploymentModel", IL2CPP_TYPE_CLASS, &object_class);
  auto& player_state   = Define("Digit.PrimeServer.Models", "FleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& deployed_state = Define("Digit.PrimeServer.Models", "DeployedFleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& removal        = Define("Digit.PrimeServer.Models", "FleetRemovalReason", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  player_state.is_enum = deployed_state.is_enum = removal.is_enum = true;
  vector_type.klass.byval_arg.data.type                           = T(queue_type);
  AddField(action_type, "<FleetId>k__BackingField", long_class, offsetof(QueueNativeAction, target));
  AddField(queue_type, "<PlayerFleetId>k__BackingField", long_class, offsetof(QueueNativeInstance, fleet));
  AddField(queue_type, "_actionQueue", list_type, offsetof(QueueNativeInstance, actions));
  AddField(manager_type, "_battleQueue", vector_type, offsetof(QueueNativeManager, queues));
  manager_type.fields.back().type = T(vector_type);
  AddMethod(list_type, "get_Count", reinterpret_cast<void*>(&QueueNativeCount), T(int_class), {});
  AddMethod(list_type, "get_Item", reinterpret_cast<void*>(&QueueNativeItem), T(action_type), {T(int_class)});
  const auto& index =
      AddMethod(fleet_data_class, "get_Index", reinterpret_cast<void*>(&QueueAddressIndex), T(int_class), {});
  AddProperty(fleet_data_class, "Index", &index);
  AddMethod(fleet_data_class, "get_CurrentState", reinterpret_cast<void*>(&QueueAddressPlayerState), T(player_state),
            {});
  AddMethod(deployed_type, "get_IsPlanningRecallCourse", reinterpret_cast<void*>(&QueueAddressRecall), T(bool_class),
            {});
  AddMethod(deployed_type, "get_CurrentState", reinterpret_cast<void*>(&QueueAddressDeployedState), T(deployed_state),
            {});
  AddMethod(deployed_type, "get_RemovalReason", reinterpret_cast<void*>(&QueueAddressRemoval), T(removal), {});
  AddField(deployed_type, "_localPlayerFleet", bool_class, offsetof(QueueAddressDeployment, local));
  AddField(deployed_type, "_address", address_type, offsetof(QueueAddressDeployment, address));
  AddField(deployed_type, "_deploymentFleet", model_type, offsetof(QueueAddressDeployment, model));
  const auto& model_id =
      AddMethod(model_type, "get_FleetId", reinterpret_cast<void*>(&QueueAddressModelId), T(long_class), {});
  AddProperty(model_type, "FleetId", &model_id);
  AddMethod(address_type, "get_Galaxy", reinterpret_cast<void*>(&QueueAddressGalaxy), T(long_class), {});
  AddMethod(address_type, "get_System", reinterpret_cast<void*>(&QueueAddressSystem), T(long_class), {});
  AddMethod(address_type, "get_Planet", reinterpret_cast<void*>(&QueueAddressPlanet), T(long_class), {});
  AddMethod(address_type, "get_Instance", reinterpret_cast<void*>(&QueueAddressInstance), T(int_class), {});

  QueueAddressValue      reported{{&address_type.klass}, 0, 71088, 71237, 710};
  QueueAddressValue      deployed{{&address_type.klass}, 0, 925162490, 0, 710};
  Il2CppObject           model{&model_type.klass};
  QueueAddressDeployment own{{&deployed_type.klass}, true, &deployed, &model};
  QueueAddressDeployment target{{&deployed_type.klass}, false, &deployed, nullptr};
  QueueNativeAction      action{{&action_type.klass}, 100}, invalid{{&action_type.klass}, 0};
  QueueNativeList        actions{{&list_type.klass}, {&action.header}};
  QueueNativeInstance    queue{{&queue_type.klass}, false, 2.5f, 0, 0, &actions, 999};
  QueueNativeVector      vector{{&vector_type.klass}, nullptr, 1, {&queue, nullptr}};
  QueueNativeManager     manager{{&manager_type.klass}, &vector};
  auto*                  owner           = reinterpret_cast<ActionQueueManager*>(&manager);
  auto*                  fleet           = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
  auto*                  native_address  = reinterpret_cast<NodeAddress*>(&reported);
  auto*                  correct_address = reinterpret_cast<NodeAddress*>(&deployed);
  Config::Get().control.queue_enabled = Config::Get().control.kirshara_fixes = true;
  const auto mismatch                                                        = [&](auto before_address) {
    NodeAddress* observed        = nullptr;
    int          native_mismatch = 0, native_address_calls = 0, target_lookups = 0;
    const bool   result = ActionQueueManager_DoesFleetQueueContainMismatchedAddressTargets_Hook(
        [&](auto*, auto*) {
          ++native_mismatch;
          DeploymentService_GetDeployedFleet_Hook(
              [&](auto*, int64_t id) {
                ++target_lookups;
                return reinterpret_cast<FleetDeployedData*>(id == 999 ? &own : &target);
              },
              static_cast<DeploymentService*>(nullptr), 100);
          observed = FleetPlayerData_get_Address_Hook(
              [&](auto*) {
                ++native_address_calls;
                before_address();
                return native_address;
              },
              fleet);
          return observed != correct_address;
        },
        owner, fleet);
    Require(native_mismatch == 1 && native_address_calls == 1 && target_lookups >= 1,
            "all native handlers still run once; deployed lookup remains reachable");
    Require(result == (observed != correct_address), "native mismatch result is preserved");
    return observed;
  };
  const auto event = [&](auto before_address) {
    NodeAddress* observed  = nullptr;
    int          originals = 0;
    ActionQueueManager_OnPlayerFleetStateChangedEventHandler_Hook(
        [&](auto*, auto*) {
          ++originals;
          observed = mismatch(before_address);
        },
        owner, static_cast<il2cpp::List<FleetPlayerData*>*>(nullptr));
    Require(originals == 1 && !in_player_state_event && !current_address_check,
            "native event is delivered once and both borrowed scopes unwind");
    return observed;
  };
  const auto noop = [] {};
  if (mode == "queue-address-scan") {
    // A second unavailable Count used to turn all_of into an empty successful scan.
    actions.items         = {&invalid.header};
    actions.fail_count_at = 2;
    Require(event(noop) == native_address, "uninspected invalid action cannot authorize deployed-address substitution");
    Require(actions.reads == 1, "one qualified Count governs queue validation");
    return;
  }
  if (mode == "queue-address-disable") {
    Require(event([] { Config::Get().control.queue_enabled = false; }) == native_address,
            "disabling during native address acquisition retains native address");
    return;
  }
  if (mode == "queue-address-index") {
    queue_address_fault = 1;
    Require(event(noop) == native_address, "unavailable dock index cannot be interpreted as dock zero");
    return;
  }
  Require(event(noop) == correct_address, "complete matching evidence preserves the frozen address correction");
  Require(mismatch(noop) == native_address, "correction is scoped to the native player-state event");
  for (int fault = 1; fault <= 9; ++fault) {
    queue_address_fault = fault;
    actions.reads       = 0;
    Require(event(noop) == native_address,
            "each unavailable index/state/recall/removal/address observation retains native");
  }
  queue_address_fault   = 0;
  actions.reads         = 0;
  actions.fail_count_at = 1;
  Require(event(noop) == native_address, "failed first Count retains native");
  actions.fail_count_at = 0;
  actions.fail_item_at  = 0;
  Require(event(noop) == native_address, "failed item retains native");
  actions.fail_item_at = -1;
  actions.items        = {};
  Require(event(noop) == native_address, "empty queue retains native");
  actions.items = {&action.header};
  own.local     = false;
  Require(event(noop) == native_address, "foreign deployment retains native");
  own.local               = true;
  queue_address_on_recall = [] { Config::Get().control.kirshara_fixes = false; };
  Require(event(noop) == native_address, "disabling during evidence acquisition prevents substitution");
  queue_address_on_recall              = {};
  Config::Get().control.kirshara_fixes = true;
  Require(event(noop) == correct_address, "later healthy observation recovers without retained state");
  std::cout << "Queue address production scope, checked reads, disable and recovery passed\n";
}
} // namespace
