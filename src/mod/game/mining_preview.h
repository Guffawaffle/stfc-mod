#pragma once

// Native observations for the existing node/occupant/cargo ActionView cycle.
#include "game/viewers.h"

#include <type_traits>
#include <unity/tracker.h>

namespace mining_preview_binding
{
struct Poi : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "POI">;
};
struct Slot : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MiningSlotData">;
};
struct Loader : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "ParentObjectViewerLoadAndShow">;
};

template <class Owner, class Value> FieldInfo* Field(const char* name)
{
  auto* field = il2cpp::detail::ResolveField(Owner::Class::Raw(), name, il2cpp::detail::SlotOf<Value>(), false);
  if (!field || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return nullptr;
  const auto size = il2cpp_class_instance_size(Owner::Class::Raw());
  if (size < sizeof(Value) || static_cast<uint32_t>(field->offset) > size - sizeof(Value))
    return nullptr;
  if constexpr (std::is_same_v<Value, bool> || std::is_same_v<Value, int32_t> || std::is_same_v<Value, int64_t>)
    if (!il2cpp::detail::ExactScalarMatches(field->type, il2cpp::detail::Primitive<Value>()))
      return nullptr;
  return field;
}

struct Api {
  FieldInfo *context, *poi, *node_slot, *slot_fleet, *id, *type, *parent, *loaded, *parent_visibility;
  FieldInfo *loader, *active_viewer, *active_poi, *data_state, *local_fleet, *occupied, *visibility, *visibility_state;
  FieldInfo *battle, *target_fleet;
  il2cpp::Method find, select, can_select, rewards_click;
  bool           ready = false;
};

inline const Api& Native()
{
  static const Api api = [] {
    Api a{};
    a.context    = Field<ObjectViewerBaseWidget, NavigationInteractionUIContext*>("<CanvasContext>k__BackingField");
    a.poi        = Field<NavigationInteractionUIContext, Poi*>("Poi");
    a.node_slot  = Field<MiningObjectViewerWidget, Slot*>("_nodeSlot");
    a.slot_fleet = Field<Slot, FleetDeployedData*>("_fleetData");
    a.id         = Field<Poi, int64_t>("<ID>k__BackingField");
    a.type       = Field<Poi, int32_t>("_interactableType");
    a.parent     = Field<ObjectViewerBaseWidget, ParentObjectViewerViewController*>("<Parent>k__BackingField");
    a.loaded     = Field<ParentObjectViewerViewController, bool>("_objectViewerLoadComplete");
    a.parent_visibility = Field<ParentObjectViewerViewController, VisibilityController*>("_visibilityController");
    a.loader            = Field<NavigationInteractionUIViewController, Loader*>("_objectViewerLoadAndShow");
    a.active_viewer     = Field<Loader, ObjectViewerBaseWidget*>("_activeViewer");
    a.active_poi        = Field<Loader, Poi*>("_activePOI");
    a.data_state        = Field<NavigationInteractionUIContext, int32_t>("_dataState");
    a.local_fleet       = Field<FleetDeployedData, bool>("_localPlayerFleet");
    a.occupied          = Field<MiningObjectViewerWidget, int32_t>("_occupiedState");
    a.visibility        = Field<ObjectViewerBaseWidget, VisibilityController*>("_visibilityController");
    a.visibility_state  = Field<VisibilityController, int32_t>("_state");
    a.battle            = Field<PreScanTargetWidget, BattleTargetData*>("_battleTargetData");
    a.target_fleet      = Field<BattleTargetData, FleetDeployedData*>("TargetFleetDeployedData");
    a.find              = Poi::Class::StaticMethod<Poi*(int64_t, int32_t)>("GetOfType");
    a.select            = NavigationInteractionUIViewController::Class::Method<void(Poi*)>("SelectPOI");
    a.can_select        = NavigationInteractionUIViewController::Class::Method<bool()>("CanSelectPOI");
    a.rewards_click     = RewardsButtonWidget::Class::Method<void()>("RewardsClicked");
    a.ready = a.context && a.poi && a.node_slot && a.slot_fleet && a.id && a.type && a.parent && a.loaded
              && a.parent_visibility && a.loader && a.active_viewer && a.active_poi && a.data_state && a.local_fleet
              && a.occupied && a.visibility && a.visibility_state && a.battle && a.target_fleet && a.find && a.select
              && a.can_select && a.rewards_click;
    return a;
  }();
  return api;
}

template <class Value> Value Read(Il2CppObject* owner, FieldInfo* field)
{ return il2cpp::detail::ReadField<Value>(field, owner); }

struct Viewer {
  il2cpp::GcHandle source, context, poi;
  int64_t          id   = 0;
  int32_t          type = 0;
  explicit         operator bool() const
  { return source && context && poi; }
};

inline Viewer Observe(ObjectViewerBaseWidget* widget)
{
  const auto& a = Native();
  Viewer      result;
  if (!a.ready || !widget)
    return result;
  result.source = il2cpp::GcHandle::Strong(widget);
  if (!result.source)
    return {};
  auto visibility = il2cpp::GcHandle::Strong(Read<VisibilityController*>(result.source.Get(), a.visibility));
  if (!visibility
      || Read<int32_t>(visibility.Get(), a.visibility_state) != std::to_underlying(VisibilityState::Visible))
    return {};
  result.context = il2cpp::GcHandle::Strong(Read<NavigationInteractionUIContext*>(result.source.Get(), a.context));
  if (!result.context || Read<int32_t>(result.context.Get(), a.data_state) != 0)
    return {};
  result.poi  = il2cpp::GcHandle::Strong(Read<Poi*>(result.context.Get(), a.poi));
  auto parent = il2cpp::GcHandle::Strong(Read<ParentObjectViewerViewController*>(result.source.Get(), a.parent));
  if (!result.poi || !parent || !Read<bool>(parent.Get(), a.loaded))
    return {};
  auto parent_visibility = il2cpp::GcHandle::Strong(Read<VisibilityController*>(parent.Get(), a.parent_visibility));
  if (!parent_visibility
      || Read<int32_t>(parent_visibility.Get(), a.visibility_state) != std::to_underlying(VisibilityState::Visible))
    return {};
  for (auto* nav : unity::Instances<NavigationInteractionUIViewController>()) {
    auto       controller = il2cpp::GcHandle::Strong(nav);
    const auto current    = controller ? nav->TryContext() : il2cpp::CallResult<NavigationInteractionUIContext*>{};
    if (!current || current.value != result.context.Get())
      continue;
    auto loader = il2cpp::GcHandle::Strong(Read<Loader*>(controller.Get(), a.loader));
    if (!loader || Read<ObjectViewerBaseWidget*>(loader.Get(), a.active_viewer) != result.source.Get()
        || Read<Poi*>(loader.Get(), a.active_poi) != result.poi.Get())
      return {};
    result.id   = Read<int64_t>(result.poi.Get(), a.id);
    result.type = Read<int32_t>(result.poi.Get(), a.type);
    return result.id != 0 && result.id != -1 ? std::move(result) : Viewer{};
  }
  return {};
}

inline il2cpp::GcHandle Fleet(PreScanTargetWidget* widget)
{
  const auto& a      = Native();
  auto        source = il2cpp::GcHandle::Strong(widget);
  auto battle = source ? il2cpp::GcHandle::Strong(Read<BattleTargetData*>(source.Get(), a.battle)) : il2cpp::GcHandle{};
  return battle ? il2cpp::GcHandle::Strong(Read<FleetDeployedData*>(battle.Get(), a.target_fleet)) : il2cpp::GcHandle{};
}

inline bool Select(NavigationInteractionUIContext* context, int64_t expected, int64_t id, int32_t type)
{
  const auto& a     = Native();
  auto        owner = il2cpp::GcHandle::Strong(context);
  if (!a.ready || !owner)
    return false;
  void*      args[]{&id, &type};
  const auto found = il2cpp::detail::TryInvokeBoxed(a.find.Info(), nullptr, args);
  auto       poi   = found ? il2cpp::GcHandle::Strong(found.value) : il2cpp::GcHandle{};
  if (!poi)
    return false;
  for (auto* nav : unity::Instances<NavigationInteractionUIViewController>()) {
    auto       controller = il2cpp::GcHandle::Strong(nav);
    const auto current    = controller ? nav->TryContext() : il2cpp::CallResult<NavigationInteractionUIContext*>{};
    if (!current || current.value != owner.Get())
      continue;
    const auto allowed = nav->TryCall<"CanSelectPOI", bool>();
    const auto after   = nav->TryContext();
    auto current_poi   = after && after.value == owner.Get() ? il2cpp::GcHandle::Strong(Read<Poi*>(owner.Get(), a.poi))
                                                             : il2cpp::GcHandle{};
    if (!allowed || !allowed.value || !current_poi || Read<int64_t>(current_poi.Get(), a.id) != expected)
      return false;
    void* select_args[]{poi.Get()};
    return il2cpp::detail::TryInvokeBoxed(a.select.Info(), controller.Get(), select_args).succeeded;
  }
  return false;
}
} // namespace mining_preview_binding
