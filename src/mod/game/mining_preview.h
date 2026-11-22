#pragma once

// Native observations for the node/occupant/cargo ActionView cycle and OPC indicators.
#include "game/fleet_opc.h"
#include "game/viewers.h"

#include <cmath>
#include <type_traits>
#include <unity/tracker.h>
#include <unity/ui.h>
#include <utility>

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
struct ThreatLevel : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "ThreatLevelWidget">;
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
  FieldInfo *battle, *target_fleet, *card_bounds;
  FieldInfo *node_threat, *occupied_text;
  il2cpp::Method find, select, can_select, rewards_click;
  bool           viewer_ready = false, ready = false;
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
    a.card_bounds       = Field<PreScanTargetWidget, unity::RectTransform*>("_inputBlocker");
    a.target_fleet      = Field<BattleTargetData, FleetDeployedData*>("TargetFleetDeployedData");
    a.node_threat       = Field<MiningObjectViewerWidget, ThreatLevel*>("_threatLevelWidget");
    a.occupied_text     = Field<ThreatLevel, TextLocalizer*>("_occupiedText");
    a.find              = Poi::Class::StaticMethod<Poi*(int64_t, int32_t)>("GetOfType");
    a.select            = NavigationInteractionUIViewController::Class::Method<void(Poi*)>("SelectPOI");
    a.can_select        = NavigationInteractionUIViewController::Class::Method<bool()>("CanSelectPOI");
    a.rewards_click     = RewardsButtonWidget::Class::Method<void()>("RewardsClicked");
    a.viewer_ready = a.context && a.poi && a.id && a.type && a.parent && a.loaded && a.parent_visibility && a.loader
                     && a.active_viewer && a.active_poi && a.data_state && a.visibility && a.visibility_state;
    a.ready = a.viewer_ready && a.node_slot && a.slot_fleet && a.local_fleet && a.occupied && a.battle && a.target_fleet
              && a.find && a.select && a.can_select && a.rewards_click;
    return a;
  }();
  return api;
}

template <class Value> Value Read(Il2CppObject* owner, FieldInfo* field)
{ return il2cpp::detail::ReadField<Value>(field, owner); }

struct Viewer {
  il2cpp::GcHandle source, context, poi, loader;
  int64_t          id   = 0;
  int32_t          type = 0;
  explicit         operator bool() const
  { return source && context && poi; }
};

inline bool ViewerVisible(ObjectViewerBaseWidget* widget)
{
  const auto& a          = Native();
  auto        visibility = il2cpp::GcHandle::Strong(Read<VisibilityController*>(widget, a.visibility));
  return visibility
         && Read<int32_t>(visibility.Get(), a.visibility_state) == std::to_underlying(VisibilityState::Visible);
}

inline Viewer Observe(ObjectViewerBaseWidget* widget)
{
  const auto& a = Native();
  Viewer      result;
  if (!a.viewer_ready || !widget)
    return result;
  result.source = il2cpp::GcHandle::Strong(widget);
  if (!result.source || !ViewerVisible(result.source.Get<ObjectViewerBaseWidget>()))
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
    result.loader = std::move(loader);
    result.id     = Read<int64_t>(result.poi.Get(), a.id);
    result.type   = Read<int32_t>(result.poi.Get(), a.type);
    return result.id != 0 && result.id != -1 ? std::move(result) : Viewer{};
  }
  return {};
}

// The active POI changes before an old viewer has necessarily completed its hide transition.
inline bool StillSelected(ObjectViewerBaseWidget* widget, NavigationInteractionUIContext* context, Loader* loader,
                          Poi* poi)
{
  const auto& a = Native();
  return widget && context && loader && poi && Read<NavigationInteractionUIContext*>(widget, a.context) == context
         && Read<int32_t>(context, a.data_state) == 0 && Read<Poi*>(context, a.poi) == poi
         && Read<ObjectViewerBaseWidget*>(loader, a.active_viewer) == widget && Read<Poi*>(loader, a.active_poi) == poi;
}

struct Occupant {
  il2cpp::GcHandle slot, fleet;
  int64_t          id = 0;
  explicit         operator bool() const
  { return slot && fleet && id != 0 && id != -1; }
};

inline Occupant ForeignOccupant(MiningObjectViewerWidget* widget)
{
  const auto& a = Native();
  if (!widget || !a.node_slot || !a.slot_fleet || !a.local_fleet || !a.occupied
      || Read<int32_t>(widget, a.occupied) != std::to_underlying(OccupiedState::OtherPlayerOccupied))
    return {};
  auto slot  = il2cpp::GcHandle::Strong(Read<Slot*>(widget, a.node_slot));
  auto fleet = slot ? il2cpp::GcHandle::Strong(Read<FleetDeployedData*>(slot.Get(), a.slot_fleet)) : il2cpp::GcHandle{};
  if (!fleet || Read<bool>(fleet.Get(), a.local_fleet))
    return {};
  const auto id = fleet.Get<FleetDeployedData>()->TryId();
  return id ? Occupant{std::move(slot), std::move(fleet), id.value} : Occupant{};
}

inline il2cpp::GcHandle Fleet(PreScanTargetWidget* widget)
{
  const auto& a      = Native();
  auto        source = il2cpp::GcHandle::Strong(widget);
  auto battle = source ? il2cpp::GcHandle::Strong(Read<BattleTargetData*>(source.Get(), a.battle)) : il2cpp::GcHandle{};
  return battle ? il2cpp::GcHandle::Strong(Read<FleetDeployedData*>(battle.Get(), a.target_fleet)) : il2cpp::GcHandle{};
}

// The foreign fleet's quick scan is the source of the game's player-ship rewards.
enum class DockPointType : int32_t { Mining = 0, Capturing = 1, Outpost = 2 };

struct QuickScan : il2cpp::Object {
  using Class =
      il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "QuickScanDeployedFleetData">;
};
struct ScanResource : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceData">;
};

inline bool TargetOpcAvailable()
{
  const auto& a = Native();
  using Mining  = fleet_opc_binding::Mining::Class;
  using Fleet   = FleetDeployedData::Class;
  return a.viewer_ready && a.battle && a.target_fleet && a.card_bounds
         && Fleet::HasField<"_deploymentFleet", DeployedFleetModel*>() && Fleet::HasProperty<"ID", int64_t>()
         && Fleet::HasProperty<"FleetType", DeployedFleetType>() && Fleet::HasProperty<"IsLocalPlayer", bool>()
         && Fleet::HasProperty<"IsMining", bool>() && Fleet::HasProperty<"IsQuickScanDataTooOld", bool>()
         && Fleet::HasProperty<"LatestQuickScanData", QuickScan*>()
         && DeployedFleetInterface::Class::HasProperty<"MiningSlot", fleet_opc_binding::Mining*>()
         && Mining::HasProperty<"DockPointType", DockPointType>() && Mining::HasProperty<"IsOccupied", bool>()
         && Mining::HasProperty<"DeployedFleetId", int64_t>() && Mining::HasProperty<"ResourceId", int64_t>()
         && QuickScan::Class::HasProperty<"ResourceDataList", il2cpp::List<ScanResource*>*>()
         && ScanResource::Class::HasProperty<"ID", int64_t>() && ScanResource::Class::HasProperty<"Amount", int64_t>()
         && ScanResource::Class::HasProperty<"IsFactionPoint", bool>();
}

inline il2cpp::GcHandle VisibleRect(unity::RectTransform* rect)
{
  auto transform = il2cpp::GcHandle::Strong(rect);
  if (!unity::Alive(rect))
    return {};
  auto       object = il2cpp::GcHandle::Strong(rect->gameObject());
  const auto active = object ? object.Get<unity::GameObject>()->TryActiveInHierarchy() : il2cpp::CallResult<bool>{};
  const auto size   = rect->TryCall<"get_rect", unity::Rect>();
  if (!active || !active.value || !size || !std::isfinite(size.value.width) || !std::isfinite(size.value.height)
      || size.value.width <= 0 || size.value.height <= 0)
    return {};
  return transform;
}

inline il2cpp::GcHandle CardAnchor(PreScanTargetWidget* widget)
{
  // The native ship card uses this rectangle for its own screen hit test.
  return VisibleRect(Read<unity::RectTransform*>(widget, Native().card_bounds));
}

struct NodeStatus {
  il2cpp::GcHandle anchor, font;
};

inline NodeStatus OccupiedStatus(MiningObjectViewerWidget* widget)
{
  const auto& a = Native();
  if (!a.node_threat || !a.occupied_text)
    return {};
  auto threat = il2cpp::GcHandle::Strong(Read<ThreatLevel*>(widget, a.node_threat));
  auto text =
      threat ? il2cpp::GcHandle::Strong(Read<TextLocalizer*>(threat.Get(), a.occupied_text)) : il2cpp::GcHandle{};
  auto* component = text ? text.Get<TextLocalizer>()->As<unity::Component>() : nullptr;
  auto* object    = component ? component->gameObject() : nullptr;
  auto  label     = il2cpp::GcHandle::Strong(object ? object->GetComponent<unity::TextMeshProUGUI>() : nullptr);
  auto* transform = label ? label.Get<unity::TextMeshProUGUI>()->transform() : nullptr;
  auto* parent    = transform ? transform->parent() : nullptr;
  // The text fills the occupied row. Its parent leaves room for a separate badge at the right edge.
  auto anchor = VisibleRect(parent ? parent->As<unity::RectTransform>() : nullptr);
  if (!anchor)
    return {};
  return {std::move(anchor), il2cpp::GcHandle::Strong(label.Get<unity::TextMeshProUGUI>()->font())};
}

inline bool TargetIsOpc(FleetDeployedData* fleet, int64_t fleet_id, Slot* selected_slot = nullptr)
{
  const auto type   = fleet->TryFleetType();
  const auto local  = fleet->TryIsLocalPlayer();
  const auto mining = fleet->TryCall<"get_IsMining", bool>();
  const auto old    = fleet->TryCall<"get_IsQuickScanDataTooOld", bool>();
  if (!type || type.value != DeployedFleetType::Player || !local || local.value || !mining || !mining.value || !old
      || old.value)
    return false;

  auto  deployed   = il2cpp::GcHandle::Strong(fleet->deploymentFleet());
  auto* deployment = deployed ? deployed.Get<il2cpp::Object>()->As<DeployedFleetInterface>() : nullptr;
  if (!deployment)
    return false;
  const auto observed_slot = deployment->TryCall<"get_MiningSlot", fleet_opc_binding::Mining*>();
  auto       slot          = il2cpp::GcHandle::Strong(observed_slot ? observed_slot.value : nullptr);
  if (!slot)
    return false;
  auto*      mine     = slot.Get<fleet_opc_binding::Mining>();
  const auto point    = mine->TryCall<"get_DockPointType", DockPointType>();
  const auto occupied = mine->TryCall<"get_IsOccupied", bool>();
  const auto occupant = mine->TryCall<"get_DeployedFleetId", int64_t>();
  const auto resource = mine->TryCall<"get_ResourceId", int64_t>();
  if (!point || point.value != DockPointType::Mining || !occupied || !occupied.value || !occupant
      || occupant.value != fleet_id || !resource || resource.value <= 0)
    return false;

  if (selected_slot) {
    using Mining                  = fleet_opc_binding::Mining;
    const auto observed_slot_data = selected_slot->TryCall<"get_SlotData", Mining*>();
    auto       slot_data          = il2cpp::GcHandle::Strong(observed_slot_data ? observed_slot_data.value : nullptr);
    if (!slot_data)
      return false;
    const auto fleet_slot_id = mine->TryCall<"get_SlotId", int64_t>();
    const auto node_slot_id  = slot_data.Get<Mining>()->TryCall<"get_SlotId", int64_t>();
    const auto node_fleet_id = slot_data.Get<Mining>()->TryCall<"get_DeployedFleetId", int64_t>();
    const auto node_resource = slot_data.Get<Mining>()->TryCall<"get_ResourceId", int64_t>();
    if (!fleet_slot_id || !node_slot_id || node_slot_id.value == 0 || node_slot_id.value == -1
        || node_slot_id.value != fleet_slot_id.value || !node_fleet_id || node_fleet_id.value != fleet_id
        || !node_resource || node_resource.value != resource.value)
      return false;
  }

  const auto observed = fleet->TryCall<"get_LatestQuickScanData", QuickScan*>();
  auto       scan     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  if (!scan)
    return false;
  using Resources      = il2cpp::List<ScanResource*>;
  const auto scan_list = scan.Get<QuickScan>()->TryCall<"get_ResourceDataList", Resources*>();
  auto       list      = il2cpp::GcHandle::Strong(scan_list ? scan_list.value : nullptr);
  const auto count     = list ? list.Get<Resources>()->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count)
    return false;

  // Only attack loot matching this node's mined resource is evidence of OPC.
  for (int32_t index = 0; index < count.value; ++index) {
    const auto observed_item = list.Get<Resources>()->TryCall<"get_Item", ScanResource*(int32_t)>(index);
    auto       item          = il2cpp::GcHandle::Strong(observed_item ? observed_item.value : nullptr);
    if (!item)
      continue;
    auto*      loot = item.Get<ScanResource>();
    const auto id   = loot->TryCall<"get_ID", int64_t>();
    if (!id || id.value != resource.value)
      continue;
    const auto amount  = loot->TryCall<"get_Amount", int64_t>();
    const auto faction = loot->TryCall<"get_IsFactionPoint", bool>();
    if (amount && amount.value > 0 && faction && !faction.value)
      return true;
  }
  return false;
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
