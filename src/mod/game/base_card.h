#pragma once

// Qualified observations used to retain the current station or attacked player-ship card.
// Course submission, completion and semaphore ownership remain native.
#include "game/queues.h"
#include "game/viewers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace base_card_binding
{
using Scanning = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.Managers", "ScanningManager">;
using Poi      = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "POI">;
using Loader   = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "ParentObjectViewerLoadAndShow">;

struct Api {
  il2cpp::Method    preview, navigation;
  FieldInfo *       preview_data = nullptr, *poi = nullptr, *user = nullptr;
  const MethodInfo *preview_context = nullptr, *navigation_context = nullptr;
  const MethodInfo *scanning_instance = nullptr, *target_cache = nullptr;
  bool              ready = false;
};

inline FieldInfo* ReferenceField(Il2CppClass* owner, const char* name, Il2CppClass* expected)
{
  auto* field = owner ? il2cpp_class_get_field_from_name(owner, name) : nullptr;
  if (!field || !field->type || field->type->byref || !expected || (field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return nullptr;
  const auto kind = field->type->type;
  if (kind != IL2CPP_TYPE_CLASS && kind != IL2CPP_TYPE_STRING && kind != IL2CPP_TYPE_GENERICINST)
    return nullptr;
  const auto size = il2cpp_class_instance_size(owner);
  return size >= sizeof(void*) && static_cast<uint32_t>(field->offset) <= size - sizeof(void*)
                 && il2cpp_class_from_type(field->type) == expected
             ? field
             : nullptr;
}

inline const MethodInfo* ContextGetter(Il2CppClass* owner, const char* name)
{
  auto* property = owner ? il2cpp_class_get_property_from_name(owner, name) : nullptr;
  auto* method   = property ? property->get : nullptr;
  // These are inherited closed-generic getters. Invoke through the runtime and
  // qualify the actual reference before reading fields, including shared code.
  return method && method->invoker_method && method->parameters_count == 0 && !(method->flags & METHOD_ATTRIBUTE_STATIC)
                 && method->return_type && !method->return_type->byref
             ? method
             : nullptr;
}

inline bool ResponseLayout(il2cpp::Method candidate)
{ return queue_binding::ResponseLayout(candidate); }

inline const Api& Native()
{
  static const Api api = [] {
    Api result;
    result.preview = PreScanTargetWidget::Class::Method<void(SetCourseResponse)>("OnSetCourseResponse");
    result.navigation =
        NavigationInteractionUIViewController::Class::Method<void(SetCourseResponse)>("OnSetCourseResponse");
    result.preview_data =
        ReferenceField(PreScanTargetWidget::Class::Raw(), "_battleTargetData", BattleTargetData::Class::Raw());
    result.poi  = ReferenceField(NavigationInteractionUIContext::Class::Raw(), "Poi", Poi::Raw());
    result.user = ReferenceField(NavigationInteractionUIContext::Class::Raw(), "UserId", il2cpp::detail::StringClass());
    result.preview_context    = ContextGetter(PreScanTargetWidget::Class::Raw(), "Context");
    result.navigation_context = ContextGetter(NavigationInteractionUIViewController::Class::Raw(), "CanvasContext");
    for (auto* owner = Scanning::Raw(); owner; owner = il2cpp_class_get_parent(owner)) {
      auto* method = il2cpp_class_get_method_from_name(owner, "get_Instance", 0);
      if (method && method->return_type && !method->return_type->byref && method->invoker_method
          && (method->flags & METHOD_ATTRIBUTE_STATIC) && method->parameters_count == 0
          && il2cpp_class_from_type(method->return_type) == Scanning::Raw()) {
        result.scanning_instance = method;
        break;
      }
    }
    result.target_cache = Scanning::Method<BattleTargetData*()>("get_TargetDataCache").Info();
    auto starbase       = BattleTargetData::Class::Method<bool()>("get_IsStarbase");
    auto own            = BattleTargetData::Class::Method<bool()>("get_IsOwnStation");
    auto valid          = BattleTargetData::Class::Method<bool()>("get_IsValid");
    auto player         = BattleTargetData::Class::Method<int64_t()>("get_PlayerID");
    auto target         = BattleTargetData::Class::Method<int64_t()>("get_TargetID");
    auto target_user    = BattleTargetData::Class::Method<Il2CppString*()>("get_TargetUserID");
    result.ready = ResponseLayout(result.preview) && ResponseLayout(result.navigation) && result.preview_data
                   && result.poi && result.user && result.preview_context && result.navigation_context
                   && result.scanning_instance && result.target_cache && starbase && own && valid && player && target
                   && target_user && il2cpp::detail::ExactScalarMatches(player.Info()->return_type, IL2CPP_TYPE_I8)
                   && il2cpp::detail::ExactScalarMatches(target.Info()->return_type, IL2CPP_TYPE_I8);
    return result;
  }();
  return api;
}

inline Il2CppObject* Invoke(const MethodInfo* method, Il2CppObject* source)
{
  const auto result = il2cpp::detail::TryInvokeBoxed(method, source, nullptr);
  return result ? result.value : nullptr;
}

inline bool SameUser(Il2CppObject* left, Il2CppObject* right)
{
  if (!left || !right || left->klass != il2cpp::detail::StringClass() || right->klass != il2cpp::detail::StringClass())
    return false;
  auto* a = reinterpret_cast<Il2CppString*>(left);
  auto* b = reinterpret_cast<Il2CppString*>(right);
  return a->length > 0 && a->length == b->length
         && std::memcmp(a->chars, b->chars, static_cast<size_t>(a->length) * sizeof(Il2CppChar)) == 0;
}

inline bool Matches(BattleTargetData* data, NavigationInteractionUIContext* context, const SetCourseResponse& response)
{
  if (!data || data->klass != BattleTargetData::Class::Raw() || !context
      || context->klass != NavigationInteractionUIContext::Class::Raw())
    return false;
  const auto base   = data->TryCall<"get_IsStarbase", bool>();
  const auto own    = data->TryCall<"get_IsOwnStation", bool>();
  const auto valid  = data->TryCall<"get_IsValid", bool>();
  const auto player = planned_warp_binding::Scalar<"get_PlayerID", int64_t>(data);
  if (!base || !base.value || !own || own.value || !valid || !valid.value || !player
      || player.value != response.fleet_id)
    return false;
  auto target_user = il2cpp::GcHandle::Strong(
      reinterpret_cast<Il2CppObject*>(planned_warp_binding::Reference<"get_TargetUserID", Il2CppString>(data).value));
  auto context_user = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(Native().user, context));
  if (!il2cpp::detail::ReadField<Il2CppObject*>(Native().poi, context)
      || !SameUser(context_user.Get(), target_user.Get()))
    return false;
  if (response.is_recall)
    return true;
  auto target = il2cpp::GcHandle::Strong(response.target_id);
  if (!target)
    return false;
  if (target.Get()->klass == il2cpp::detail::PrimitiveClass(IL2CPP_TYPE_I8)) {
    const auto expected = planned_warp_binding::Scalar<"get_TargetID", int64_t>(data);
    auto*      actual   = static_cast<int64_t*>(il2cpp_object_unbox(target.Get()));
    return actual && expected && expected.value != 0 && expected.value != -1 && *actual == expected.value;
  }
  return SameUser(target.Get(), target_user.Get());
}

// Scalar diagnostics are copied only when the debug hooks request them. Nothing persists between responses.
enum class RetentionGate : uint8_t {
  WidgetAlive,
  PoiAlive,
  NotStarbase,
  Valid,
  PlayerFleet,
  OtherPlayer,
  NotDestroyed,
  Visible,
  CurrentContext,
  CurrentData,
  CurrentFleet,
  CurrentPoi,
  CurrentVisibility,
  CurrentLoader,
  CurrentViewer,
  CurrentActivePoi,
  OtherPlayerOccupied,
};

enum class RetentionViewKind : uint8_t { Unknown, PreScan, Mining };

struct RetentionObservation {
  const char*       phase = "entry";
  RetentionViewKind view  = RetentionViewKind::Unknown;
  uint32_t          seen = 0, known = 0, passed = 0;
};

inline void RetentionPhase(RetentionObservation* observation, const char* phase) noexcept
{
  if (observation)
    observation->phase = phase;
}

inline bool RetentionCheck(RetentionObservation* observation, RetentionGate gate, bool value,
                           bool known = true) noexcept
{
  if (observation) {
    const auto bit = uint32_t{1} << std::to_underlying(gate);
    observation->seen |= bit;
    observation->known  = (observation->known & ~bit) | (known ? bit : 0);
    observation->passed = (observation->passed & ~bit) | (known && value ? bit : 0);
  }
  return value;
}

inline FieldInfo* Int32Field(Il2CppClass* owner, const char* name)
{
  auto* field = owner ? il2cpp_class_get_field_from_name(owner, name) : nullptr;
  if (!field || !field->type || field->type->byref
      || (field->type->attrs & (FIELD_ATTRIBUTE_STATIC | FIELD_ATTRIBUTE_LITERAL))
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return nullptr;
  const auto size = il2cpp_class_instance_size(owner);
  return size >= sizeof(int32_t) && static_cast<uint32_t>(field->offset) <= size - sizeof(int32_t)
                 && static_cast<uint32_t>(field->offset) % alignof(int32_t) == 0
                 && il2cpp::detail::ExactScalarMatches(field->type, IL2CPP_TYPE_I4)
             ? field
             : nullptr;
}

struct ShipApi {
  FieldInfo *loader = nullptr, *active_viewer = nullptr, *active_poi = nullptr;
  FieldInfo *target_fleet = nullptr, *mining_fleet = nullptr, *visibility = nullptr, *visibility_state = nullptr;
  FieldInfo* mining_occupied = nullptr;
  bool       preview_ready = false, mining_ready = false, ready = false;
};

inline const ShipApi& ShipNative()
{
  static const ShipApi api = [] {
    ShipApi result;
    result.loader =
        ReferenceField(NavigationInteractionUIViewController::Class::Raw(), "_objectViewerLoadAndShow", Loader::Raw());
    result.active_viewer = ReferenceField(Loader::Raw(), "_activeViewer", ObjectViewerBaseWidget::Class::Raw());
    result.active_poi    = ReferenceField(Loader::Raw(), "_activePOI", Poi::Raw());
    result.target_fleet =
        ReferenceField(BattleTargetData::Class::Raw(), "TargetFleetDeployedData", FleetDeployedData::Class::Raw());
    result.mining_fleet =
        ReferenceField(MiningObjectViewerWidget::Class::Raw(), "_targetFleet", FleetDeployedData::Class::Raw());
    result.visibility       = ReferenceField(ObjectViewerBaseWidget::Class::Raw(), "_visibilityController",
                                             VisibilityController::Class::Raw());
    result.visibility_state = Int32Field(VisibilityController::Class::Raw(), "_state");
    result.mining_occupied  = Int32Field(MiningObjectViewerWidget::Class::Raw(), "_occupiedState");
    result.preview_ready    = result.target_fleet && result.visibility && result.visibility_state;
    result.mining_ready = result.mining_fleet && result.mining_occupied && result.visibility && result.visibility_state;
    result.ready =
        (result.preview_ready || result.mining_ready) && result.loader && result.active_viewer && result.active_poi;
    return result;
  }();
  return api;
}

inline bool RetainBasePreview(PreScanTargetWidget* widget, const SetCourseResponse& response)
{
  const auto& api = Native();
  if (!api.ready || !widget || !response.success || response.fleet_id == 0
      || (!response.is_recall && !response.target_id))
    return false;
  auto source = il2cpp::GcHandle::Strong(widget);
  auto target = il2cpp::GcHandle::Strong(response.target_id);
  if (!source || (response.target_id && !target))
    return false;
  auto data    = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, source.Get()));
  auto context = il2cpp::GcHandle::Strong(Invoke(api.preview_context, source.Get()));
  // The native preview may rebind while its context getter runs. A matching old
  // target must never suppress the game's response for the replacement card.
  if (!data || !context || il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, source.Get()) != data.Get())
    return false;
  return Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
}

inline bool RetainBaseNavigation(NavigationInteractionUIViewController* controller, const SetCourseResponse& response)
{
  const auto& api = Native();
  if (!api.ready || !controller || !response.success || response.fleet_id == 0
      || (!response.is_recall && !response.target_id))
    return false;
  auto source = il2cpp::GcHandle::Strong(controller);
  auto target = il2cpp::GcHandle::Strong(response.target_id);
  if (!source || (response.target_id && !target))
    return false;
  auto context = il2cpp::GcHandle::Strong(Invoke(api.navigation_context, source.Get()));
  auto manager = il2cpp::GcHandle::Strong(Invoke(api.scanning_instance, nullptr));
  auto data    = manager ? il2cpp::GcHandle::Strong(Invoke(api.target_cache, manager.Get())) : il2cpp::GcHandle{};
  if (!context || !data)
    return false;
  return Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
}

inline bool ShipShown(Il2CppObject* visibility)
{
  const auto state = il2cpp::detail::ReadField<VisibilityState>(ShipNative().visibility_state, visibility);
  return state == VisibilityState::Show || state == VisibilityState::Visible;
}

inline RetentionViewKind ShipView(Il2CppObject* widget)
{
  if (widget && widget->klass == PreScanTargetWidget::Class::Raw())
    return RetentionViewKind::PreScan;
  if (widget && widget->klass == MiningObjectViewerWidget::Class::Raw())
    return RetentionViewKind::Mining;
  return RetentionViewKind::Unknown;
}

struct ShipCard {
  il2cpp::GcHandle  widget, data, context, poi, fleet, visibility;
  RetentionViewKind view        = RetentionViewKind::Unknown;
  FieldInfo*        fleet_field = nullptr;
  explicit          operator bool() const
  {
    return widget && (view == RetentionViewKind::Mining || data) && context && poi && fleet && visibility
           && fleet_field;
  }

  bool Current(RetentionObservation* observation) const
  {
    const auto& api  = Native();
    const auto& ship = ShipNative();
    // All managed observations finish before these raw references and UI states are rechecked.
    if (view == RetentionViewKind::Mining
        && !RetentionCheck(observation, RetentionGate::OtherPlayerOccupied,
                           il2cpp::detail::ReadField<OccupiedState>(ship.mining_occupied, widget.Get())
                               == OccupiedState::OtherPlayerOccupied))
      return false;
    if (view == RetentionViewKind::PreScan
        && !RetentionCheck(observation, RetentionGate::CurrentData,
                           il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, widget.Get()) == data.Get()))
      return false;
    auto* fleet_owner = view == RetentionViewKind::PreScan ? data.Get() : widget.Get();
    return RetentionCheck(observation, RetentionGate::CurrentFleet,
                          il2cpp::detail::ReadField<Il2CppObject*>(fleet_field, fleet_owner) == fleet.Get())
           && RetentionCheck(observation, RetentionGate::CurrentPoi,
                             il2cpp::detail::ReadField<Il2CppObject*>(api.poi, context.Get()) == poi.Get())
           && RetentionCheck(observation, RetentionGate::CurrentVisibility,
                             il2cpp::detail::ReadField<Il2CppObject*>(ship.visibility, widget.Get())
                                 == visibility.Get())
           && RetentionCheck(observation, RetentionGate::Visible, ShipShown(visibility.Get()));
  }
};

inline ShipCard ObservePlayerShip(ObjectViewerBaseWidget* widget, RetentionObservation* observation)
{
  const auto& api  = Native();
  const auto& ship = ShipNative();
  RetentionPhase(observation, "ship_widget");
  ShipCard card;
  card.widget = il2cpp::GcHandle::Strong(widget);
  card.view   = ShipView(card.widget.Get());
  if (observation)
    observation->view = card.view;
  if (card.view == RetentionViewKind::Unknown)
    return {};
  RetentionPhase(observation, "ship_fields");
  if (!api.ready || (card.view == RetentionViewKind::PreScan ? !ship.preview_ready : !ship.mining_ready))
    return {};
  if (card.view == RetentionViewKind::Mining) {
    // UpdateSlotInfo can leave _targetFleet populated when a node becomes empty; occupancy is the current UI state.
    RetentionPhase(observation, "ship_mining_occupied");
    if (!RetentionCheck(observation, RetentionGate::OtherPlayerOccupied,
                        il2cpp::detail::ReadField<OccupiedState>(ship.mining_occupied, card.widget.Get())
                            == OccupiedState::OtherPlayerOccupied))
      return {};
  }
  RetentionPhase(observation, "ship_data_context");
  if (card.view == RetentionViewKind::PreScan)
    card.data = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, card.widget.Get()));
  // Both views inherit the same Widget<NavigationInteractionUIContext>.Context getter.
  card.context = il2cpp::GcHandle::Strong(Invoke(api.preview_context, card.widget.Get()));
  if ((card.view == RetentionViewKind::PreScan
       && (!card.data || card.data.Get()->klass != BattleTargetData::Class::Raw()))
      || !card.context || card.context.Get()->klass != NavigationInteractionUIContext::Class::Raw())
    return {};
  card.poi          = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(api.poi, card.context.Get()));
  card.fleet_field  = card.view == RetentionViewKind::PreScan ? ship.target_fleet : ship.mining_fleet;
  auto* fleet_owner = card.view == RetentionViewKind::PreScan ? card.data.Get() : card.widget.Get();
  card.fleet        = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(card.fleet_field, fleet_owner));
  card.visibility =
      il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(ship.visibility, card.widget.Get()));
  RetentionPhase(observation, "ship_references");
  if (!card.poi || card.poi.Get()->klass != Poi::Raw() || !card.fleet
      || !card.fleet.Get<il2cpp::Object>()->Is<FleetDeployedData>() || !card.visibility
      || !card.visibility.Get<il2cpp::Object>()->Is<VisibilityController>())
    return {};
  RetentionPhase(observation, "ship_classification");
  il2cpp::CallResult<bool> base, valid;
  if (card.view == RetentionViewKind::PreScan) {
    auto* data = card.data.Get<BattleTargetData>();
    base       = data->TryCall<"get_IsStarbase", bool>();
    valid      = data->TryCall<"get_IsValid", bool>();
  }
  auto*      fleet     = card.fleet.Get<FleetDeployedData>();
  const auto kind      = fleet->TryFleetType();
  const auto local     = fleet->TryIsLocalPlayer();
  const auto destroyed = fleet->TryCall<"get_IsDestroyed", bool>();
  if (card.view == RetentionViewKind::PreScan) {
    RetentionCheck(observation, RetentionGate::NotStarbase, !base.value, base.succeeded);
    RetentionCheck(observation, RetentionGate::Valid, valid.value, valid.succeeded);
  }
  RetentionCheck(observation, RetentionGate::PlayerFleet, kind.value == DeployedFleetType::Player, kind.succeeded);
  RetentionCheck(observation, RetentionGate::OtherPlayer, !local.value, local.succeeded);
  RetentionCheck(observation, RetentionGate::NotDestroyed, !destroyed.value, destroyed.succeeded);
  if ((card.view == RetentionViewKind::PreScan && (!base || base.value || !valid || !valid.value)) || !kind
      || kind.value != DeployedFleetType::Player || !local || local.value || !destroyed || destroyed.value)
    return {};
  RetentionPhase(observation, "ship_alive");
  const auto widget_alive = unity::TryAlive(card.widget.Get());
  const auto poi_alive    = unity::TryAlive(card.poi.Get());
  RetentionCheck(observation, RetentionGate::WidgetAlive, widget_alive.value, widget_alive.succeeded);
  RetentionCheck(observation, RetentionGate::PoiAlive, poi_alive.value, poi_alive.succeeded);
  if (!widget_alive || !widget_alive.value || !poi_alive || !poi_alive.value)
    return {};
  RetentionPhase(observation, "ship_final_context");
  auto current_context = il2cpp::GcHandle::Strong(Invoke(api.preview_context, card.widget.Get()));
  if (!RetentionCheck(observation, RetentionGate::CurrentContext, current_context.Get() == card.context.Get())
      || !card.Current(observation))
    return {};
  RetentionPhase(observation, "ship_retained");
  return card;
}

inline bool RetainPreview(PreScanTargetWidget* widget, const SetCourseResponse& response, bool keep_base = true,
                          bool keep_player_ship = false, RetentionObservation* observation = nullptr)
{
  RetentionPhase(observation, "preview_response");
  if (keep_base && RetainBasePreview(widget, response)) {
    RetentionPhase(observation, "base_retained");
    return true;
  }
  if (!keep_player_ship || !response.success || response.is_recall)
    return false;
  return bool(ObservePlayerShip(widget, observation));
}

inline bool RetainNavigation(NavigationInteractionUIViewController* controller, const SetCourseResponse& response,
                             bool keep_base = true, bool keep_player_ship = false,
                             RetentionObservation* observation = nullptr)
{
  RetentionPhase(observation, "navigation_response");
  if (keep_base && RetainBaseNavigation(controller, response)) {
    RetentionPhase(observation, "base_retained");
    return true;
  }
  if (!keep_player_ship || !response.success || response.is_recall)
    return false;
  const auto& ship = ShipNative();
  RetentionPhase(observation, "ship_navigation_fields");
  if (!ship.ready || !controller)
    return false;
  auto source = il2cpp::GcHandle::Strong(controller);
  if (!source || source.Get()->klass != NavigationInteractionUIViewController::Class::Raw())
    return false;
  auto loader = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(ship.loader, source.Get()));
  RetentionPhase(observation, "ship_no_loader");
  if (!loader || !il2cpp_class_is_assignable_from(Loader::Raw(), loader.Get()->klass))
    return false;
  auto viewer = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(ship.active_viewer, loader.Get()));
  RetentionPhase(observation, "ship_no_active_viewer");
  if (!viewer)
    return false;
  RetentionPhase(observation, "ship_viewer_class");
  const auto view = ShipView(viewer.Get());
  if (observation)
    observation->view = view;
  if (view == RetentionViewKind::Unknown)
    return false;
  auto poi = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(ship.active_poi, loader.Get()));
  RetentionPhase(observation, "ship_no_active_poi");
  if (!poi || poi.Get()->klass != Poi::Raw())
    return false;
  auto card = ObservePlayerShip(viewer.Get<ObjectViewerBaseWidget>(), observation);
  if (!card)
    return false;
  RetentionPhase(observation, "ship_navigation_final");
  const bool retained =
      RetentionCheck(observation, RetentionGate::CurrentLoader,
                     il2cpp::detail::ReadField<Il2CppObject*>(ship.loader, source.Get()) == loader.Get())
      && RetentionCheck(observation, RetentionGate::CurrentViewer,
                        il2cpp::detail::ReadField<Il2CppObject*>(ship.active_viewer, loader.Get()) == viewer.Get())
      && RetentionCheck(observation, RetentionGate::CurrentActivePoi,
                        il2cpp::detail::ReadField<Il2CppObject*>(ship.active_poi, loader.Get()) == poi.Get()
                            && card.poi.Get() == poi.Get())
      && card.Current(observation);
  if (retained)
    RetentionPhase(observation, "ship_retained");
  return retained;
}

} // namespace base_card_binding
