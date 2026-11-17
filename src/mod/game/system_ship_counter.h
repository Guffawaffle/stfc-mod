#pragma once

// Model observations for the viewed-system counter. No rendered-ship widgets,
// inventory requests, fleet mutations, or per-frame collection scans live here.
#include "fleet.h"
#include "queues.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace system_ship_counter_binding
{
using Service   = DeploymentService;
using FleetList = il2cpp::List<FleetDeployedData*>;

struct ServerContext : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Networking.Core", "ServerContext">;
};
struct Set : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "DeployedFleetSet">;
};
struct Model : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IDeployedFleet">;
};
struct WorldService : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "GameWorldService">;
};
struct EventContainer : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "FleetEventContainer">;
};

inline bool FleetListClass(Il2CppClass* klass)
{
  if (!klass || il2cpp_class_is_valuetype(klass)
      || std::strcmp(il2cpp_class_get_namespace(klass), "System.Collections.Generic")
      || std::strcmp(il2cpp_class_get_name(klass), "List`1"))
    return false;
  auto* items = il2cpp::detail::ResolveField(klass, "_items", il2cpp::detail::SlotOf<il2cpp::ArrayObject*>(), false);
  const auto* type    = items ? items->type : nullptr;
  const auto* element = type && !type->byref && type->type == IL2CPP_TYPE_SZARRAY ? type->data.type : nullptr;
  return element && !element->byref && il2cpp_class_from_type(element) == FleetDeployedData::Class::Raw();
}

template <class Signature> il2cpp::Method ServiceMethod(std::string_view name)
{
  const auto method = Service::Class::Method<Signature>(name);
  return method && method.Info()->klass == Service::Class::Raw() ? method : il2cpp::Method{};
}

inline il2cpp::Method ActiveSystemChangeMethod()
{ return ServiceMethod<void(int64_t)>("HandleActiveSystemChange"); }
inline il2cpp::Method ResetMethod()
{ return ServiceMethod<void(ServerContext*)>("Reset"); }
inline il2cpp::Method RemoveMethod()
{ return ServiceMethod<void(Set*, int64_t, FleetRemovalReason, bool)>("RemoveDeployedFleet"); }
inline il2cpp::Method UpdateMethod()
{ return ServiceMethod<void(Set*, int64_t, Model*)>("UpdateDeployedFleet"); }
inline il2cpp::Method AddMethod()
{ return ServiceMethod<void(Set*, int64_t, Model*)>("AddDeployedFleet"); }
inline il2cpp::Method FireEventsMethod()
{
  const auto method = EventContainer::Class::Method<void(WorldService*)>("FireEvents");
  return method && method.Info()->klass == EventContainer::Class::Raw() ? method : il2cpp::Method{};
}

template <class Visitor> bool VisitBatch(FleetList* fleets, Visitor&& visit)
{
  auto root = il2cpp::GcHandle::Strong(fleets);
  if (!root || !FleetListClass(root.Get()->klass))
    return false;
  auto*      list  = root.Get<FleetList>();
  const auto count = list->TryCall<"get_Count", int32_t>();
  if (!count || count.value < 0)
    return false;
  const auto version = list->Field<"_version", int32_t>();
  for (int32_t index = 0; index < count.value; ++index) {
    const auto item   = list->TryCall<"get_Item", FleetDeployedData*(int32_t)>(index);
    auto       member = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
    if (!item || !member || !visit(member.Get<FleetDeployedData>()))
      return false;
  }
  const auto after = list->TryCall<"get_Count", int32_t>();
  return after && after.value == count.value && list->Field<"_version", int32_t>() == version;
}

// Client274 inlines static Trigger* dispatch into this substantive flush. Read
// its pending lists before the original fires events and clears the container.
template <class Visitor> bool VisitPending(EventContainer* events, Visitor&& visit)
{
  auto root = il2cpp::GcHandle::Strong(events);
  if (!root)
    return false;
  auto*      container = root.Get<EventContainer>();
  const auto batches   = {container->Field<"_fleetsAdded", ServerManagedReference*>(),
                          container->Field<"_fleetsDisposed", ServerManagedReference*>(),
                          container->Field<"_fleetsUpdated", ServerManagedReference*>(),
                          container->Field<"_stateChangedFleets", ServerManagedReference*>(),
                          container->Field<"_playerFleetsUpdated", ServerManagedReference*>()};
  for (auto* batch : batches)
    if (!VisitBatch(reinterpret_cast<FleetList*>(batch), visit))
      return false;
  return true;
}

inline il2cpp::CallResult<bool> ShouldTrack(Service* service, Set* set, Model* received)
{
  if (!set || !received)
    return {};
  const auto segment = set->TryCall<"get_Segment", int32_t>();
  if (!segment || (segment.value != 0 && segment.value != 1))
    return {};
  return service->TryCall<"ShouldTrackFleet", bool(Model*, bool)>(received, segment.value == 0);
}

// A nonpositive result is not a valid system identity. Features must hide or
// defer the counter rather than present a zero count for that observation.
inline int64_t ActiveSystem(Service* service)
{ return service->Field<"_activeSystem", int64_t>(); }
inline EventContainer* PendingEvents(Service* service)
{ return service->Field<"_fleetEventsContainer", EventContainer*>(); }
inline il2cpp::CallResult<FleetDeployedData*> TryFleet(Service* service, int64_t id)
{ return service->TryCall<"GetDeployedFleet", FleetDeployedData*(int64_t)>(id); }
inline il2cpp::CallResult<FleetRemovalReason> TryRemovalReason(FleetDeployedData* fleet)
{ return fleet->TryCall<"get_RemovalReason", FleetRemovalReason>(); }

// Only bootstrap/system-boundary work calls this. Both native sets are needed:
// the local set may contain off-system fleets; the nonlocal set includes NPCs.
// Retained queue contacts remain native members, so eligibility/tombstones are
// deliberately left to the feature's owned model.
inline std::optional<std::vector<il2cpp::GcHandle>> Snapshot(Service* service)
{
  auto owner = il2cpp::GcHandle::Strong(service);
  if (!owner)
    return std::nullopt;
  auto sets_root  = il2cpp::GcHandle::Strong(owner.Get<Service>()->Field<"_deployedFleetSets", il2cpp::ArrayObject*>());
  const auto sets = il2cpp::TryReferenceArray<Set*>(sets_root.Get());
  if (!sets || !sets.value)
    return std::nullopt;
  std::vector<il2cpp::GcHandle> result;
  for (auto* set : *sets.value) {
    if (!set)
      return std::nullopt;
    auto list_root = il2cpp::GcHandle::Strong(set->Field<"_fleets", ServerManagedReference*>());
    if (!list_root || !FleetListClass(list_root.Get()->klass))
      return std::nullopt;
    auto*      list  = list_root.Get<FleetList>();
    const auto count = list->TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return std::nullopt;
    const auto version = list->Field<"_version", int32_t>();
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item = list->TryCall<"get_Item", FleetDeployedData*(int32_t)>(index);
      auto       root = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
      if (!item || !root)
        return std::nullopt;
      result.push_back(std::move(root));
    }
    const auto after = list->TryCall<"get_Count", int32_t>();
    if (!after || after.value != count.value || list->Field<"_version", int32_t>() != version)
      return std::nullopt;
  }
  return result;
}

template <class Scope> std::optional<Scope> ReadAddress(NodeAddress* address)
{
  if (!address)
    return std::nullopt;
  const auto galaxy   = planned_warp_binding::Scalar<"get_Galaxy", int64_t>(address);
  const auto system   = address->TrySystem();
  const auto instance = address->TryInstance();
  if (!galaxy || !system || !instance)
    return std::nullopt;
  Scope value;
  value.galaxy   = galaxy.value;
  value.system   = system.value;
  value.instance = instance.value;
  return value;
}

// Observation is the feature's game-free value type with id,address,type,state,
// destroyed,isActive,isLocal and optional cloakstatus,detected,removalReason.
// cloakstatus is an observed binary flag (1 active cloak,0 no active cloak),
// not an inference from IsDetected and not a fabricated unknown-as-zero value.
template <class Observation> std::optional<Observation> ReadFleet(FleetDeployedData* fleet)
{
  auto owner = il2cpp::GcHandle::Strong(fleet);
  if (!owner)
    return std::nullopt;
  fleet                   = owner.Get<FleetDeployedData>();
  const auto id           = fleet->TryId();
  const auto type         = fleet->TryFleetType();
  const auto state        = fleet->TryCall<"get_CurrentState", DeployedFleetState>();
  const auto destroyed    = fleet->TryCall<"get_IsDestroyed", bool>();
  const auto local        = fleet->TryIsLocalPlayer();
  const auto address      = fleet->TryCall<"get_Address", NodeAddress*>();
  auto       address_root = il2cpp::GcHandle::Strong(address ? address.value : nullptr);
  const auto model        = fleet->TryCall<"get_DeployedFleet", Model*>();
  auto       model_root   = il2cpp::GcHandle::Strong(model ? model.value : nullptr);
  const auto active =
      model_root ? model_root.Get<Model>()->TryCall<"get_IsActive", bool>() : il2cpp::CallResult<bool>{};
  if (!id || !type || !state || !destroyed || !local || !address || !model || !active)
    return std::nullopt;
  Observation value;
  auto        scope = ReadAddress<decltype(value.address)>(address_root.Get<NodeAddress>());
  if (!scope)
    return std::nullopt;
  value.id           = id.value;
  value.address      = *scope;
  value.type         = static_cast<int32_t>(type.value);
  value.state        = static_cast<int32_t>(state.value);
  value.destroyed    = destroyed.value;
  value.isActive     = active.value;
  value.isLocal      = local.value;
  const auto cloaked = fleet->TryCall<"HasActiveStatusEffect", bool(int32_t)>(int32_t{1}); // native Cloaked
  if (cloaked)
    value.cloakstatus = cloaked.value ? 1 : 0;
  const auto detected = fleet->TryCall<"get_IsDetected", bool>();
  if (detected)
    value.detected = detected.value;
  const auto removal = TryRemovalReason(fleet);
  if (removal)
    value.removalReason = static_cast<int32_t>(removal.value);
  return value;
}
} // namespace system_ship_counter_binding
