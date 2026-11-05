#pragma once

// Qualified observations used to retain the current station or attacked player-ship card.
// Course submission, completion and semaphore ownership remain native.
#include "game/galaxy.h"
#include "game/queues.h"
#include "game/viewers.h"

#include <cstddef>
#include <cstring>
#include <utility>

namespace base_card_binding
{
using Scanning = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.Managers", "ScanningManager">;
using Poi      = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "POI">;

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

inline bool NullableFleetIdLayout(const Il2CppType* type)
{
  // The shared NullableLayoutMatches validates aggregate payloads; its ValueClassMatches intentionally rejects
  // primitive metadata. This course member is Nullable<Int64>, so qualify the real primitive argument and field.
  using Target = il2cpp::Nullable<int64_t>;
  if (!type || type->byref || type->type != IL2CPP_TYPE_GENERICINST)
    return false;
  const auto* generic    = type->data.generic_class;
  const auto* arguments  = generic ? generic->context.class_inst : nullptr;
  auto*       definition = il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Nullable`1");
  auto*       payload    = il2cpp::PrimitiveClass<int64_t>();
  if (!definition || !payload || !generic || !generic->type || il2cpp_class_from_type(generic->type) != definition
      || !arguments || arguments->type_argc != 1 || !arguments->type_argv
      || !il2cpp::detail::ExactScalarMatches(arguments->type_argv[0], IL2CPP_TYPE_I8)
      || il2cpp_class_from_type(arguments->type_argv[0]) != payload)
    return false;
  auto*    actual    = il2cpp_class_from_type(type);
  uint32_t alignment = 0;
  if (!actual || !il2cpp_class_is_valuetype(actual) || il2cpp_class_value_size(actual, &alignment) != sizeof(Target)
      || alignment != alignof(Target))
    return false;
  const auto* flag     = il2cpp_class_get_field_from_name(actual, "hasValue");
  const auto* value    = il2cpp_class_get_field_from_name(actual, "value");
  const auto  instance = [](const FieldInfo* field) {
    return field && field->type && !field->type->byref
           && !(field->type->attrs & (FIELD_ATTRIBUTE_STATIC | FIELD_ATTRIBUTE_LITERAL));
  };
  return instance(flag) && instance(value) && flag->type->type == IL2CPP_TYPE_BOOLEAN
         && flag->offset == sizeof(Il2CppObject) + offsetof(Target, has_value)
         && value->offset == sizeof(Il2CppObject) + offsetof(Target, value)
         && il2cpp::detail::ExactScalarMatches(value->type, IL2CPP_TYPE_I8)
         && il2cpp_class_from_type(value->type) == payload;
}

inline FieldInfo* CourseTargetFleetField()
{
  static auto* field = [] {
    using Target = il2cpp::Nullable<int64_t>;
    auto* owner  = CourseData::Class::Raw();
    auto* value  = owner ? il2cpp_class_get_field_from_name(owner, "<TargetDeployedFleetId>k__BackingField") : nullptr;
    if (!value || !value->type || value->type->byref || (value->type->attrs & FIELD_ATTRIBUTE_STATIC)
        || value->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
      return static_cast<FieldInfo*>(nullptr);
    const auto size = il2cpp_class_instance_size(owner);
    return size >= sizeof(Target) && static_cast<uint32_t>(value->offset) <= size - sizeof(Target)
                   && static_cast<uint32_t>(value->offset) % alignof(Target) == 0 && NullableFleetIdLayout(value->type)
               ? value
               : nullptr;
  }();
  return field;
}

struct PlayerShipProof {
  il2cpp::GcHandle poi, fleet, course, user;
  FieldInfo *      fleet_field = nullptr, *course_field = nullptr, *target_field = nullptr;
  int64_t          target_id = 0;

  explicit operator bool() const
  { return poi && fleet && course && user && fleet_field && course_field && target_field && target_id > 0; }

  bool Current(BattleTargetData* data, NavigationInteractionUIContext* context) const
  {
    // Keep the final references tied to this card after every managed observation, including a generic getter
    // that rebinds the viewer. These roots live only for the response, never across target changes or combat.
    // This last check has no managed calls: a getter cannot rebind the card after its source was compared.
    const auto target = il2cpp::detail::ReadField<il2cpp::Nullable<int64_t>>(target_field, course.Get());
    return target.has_value && target.value == target_id
           && il2cpp::detail::ReadField<Il2CppObject*>(Native().poi, context) == poi.Get()
           && il2cpp::detail::ReadField<Il2CppObject*>(Native().user, context) == user.Get()
           && il2cpp::detail::ReadField<Il2CppObject*>(fleet_field, data) == fleet.Get()
           && il2cpp::detail::ReadField<Il2CppObject*>(course_field, data) == course.Get();
  }
};

inline PlayerShipProof MatchPlayerShip(BattleTargetData* data, NavigationInteractionUIContext* context,
                                       const SetCourseResponse& response)
{
  if (!data || data->klass != BattleTargetData::Class::Raw() || !context
      || context->klass != NavigationInteractionUIContext::Class::Raw() || !response.success || response.is_recall
      || response.fleet_id <= 0 || !response.target_id
      || response.target_id->klass != il2cpp::detail::PrimitiveClass(IL2CPP_TYPE_I8))
    return {};
  auto* boxed = static_cast<int64_t*>(il2cpp_object_unbox(response.target_id));
  if (!boxed || *boxed <= 0)
    return {};
  const int64_t expected = *boxed;
  const auto    base     = data->TryCall<"get_IsStarbase", bool>();
  const auto    valid    = data->TryCall<"get_IsValid", bool>();
  const auto    player   = planned_warp_binding::Scalar<"get_PlayerID", int64_t>(data);
  const auto    target   = planned_warp_binding::Scalar<"get_TargetID", int64_t>(data);
  if (!base || base.value || !valid || !valid.value || !player || player.value != response.fleet_id || !target
      || target.value != expected)
    return {};

  static auto* fleet_field =
      ReferenceField(BattleTargetData::Class::Raw(), "TargetFleetDeployedData", FleetDeployedData::Class::Raw());
  static auto* course_field = ReferenceField(BattleTargetData::Class::Raw(), "<PlannedCourseToTarget>k__BackingField",
                                             CourseData::Class::Raw());
  auto*        target_field = CourseTargetFleetField();
  if (!fleet_field || !course_field || !target_field)
    return {};
  PlayerShipProof proof;
  proof.fleet_field  = fleet_field;
  proof.course_field = course_field;
  proof.target_field = target_field;
  proof.target_id    = expected;
  proof.poi          = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(Native().poi, context));
  proof.fleet        = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(fleet_field, data));
  proof.course       = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(course_field, data));
  proof.user         = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Il2CppObject*>(Native().user, context));
  if (!proof.poi || proof.poi.Get()->klass != Poi::Raw() || !proof.fleet
      || !proof.fleet.Get<il2cpp::Object>()->Is<FleetDeployedData>() || !proof.course
      || !proof.course.Get<il2cpp::Object>()->Is<CourseData>() || !proof.user)
    return {};
  const auto alive = unity::TryAlive(proof.poi.Get());
  if (!alive || !alive.value)
    return {};
  auto*      poi          = proof.poi.Get<galaxy_binding::Poi>();
  auto*      fleet        = proof.fleet.Get<FleetDeployedData>();
  auto*      course       = proof.course.Get<CourseData>();
  const auto poi_id       = planned_warp_binding::Scalar<"get_ID", int64_t>(poi);
  const auto poi_type     = poi->TryCall<"get_InteractableType", galaxy_binding::Interactable>();
  const auto fleet_id     = planned_warp_binding::Scalar<"get_ID", int64_t>(fleet);
  const auto fleet_type   = fleet->TryFleetType();
  const auto own          = fleet->TryIsLocalPlayer();
  const auto destroyed    = fleet->TryCall<"get_IsDestroyed", bool>();
  const auto course_fleet = course->TryFleetId();
  const auto action       = course->TryTargetAction();
  auto       user         = il2cpp::GcHandle::Strong(
      reinterpret_cast<Il2CppObject*>(planned_warp_binding::Reference<"get_TargetUserID", Il2CppString>(data).value));
  // TargetAction.Attack = 2 in the retained client catalog. A course response has no action kind of its own;
  // an absent plan cannot become permission to retain orbit/follow or a card for a different ship of this user.
  if (!poi_id || poi_id.value != expected || !poi_type || poi_type.value != galaxy_binding::Interactable::Fleet
      || !fleet_id || fleet_id.value != expected || !fleet_type || fleet_type.value != DeployedFleetType::Player || !own
      || own.value || !destroyed || destroyed.value || !course_fleet || course_fleet.value != response.fleet_id
      || !action || action.value != 2 || !SameUser(user.Get(), proof.user.Get()) || !proof.Current(data, context))
    return {};
  return proof;
}

inline bool RetainPreview(PreScanTargetWidget* widget, const SetCourseResponse& response, bool keep_base = true,
                          bool keep_player_ship = false)
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
  if (keep_base && Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response))
    return true;
  if (!keep_player_ship)
    return false;
  auto proof = MatchPlayerShip(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
  if (!proof)
    return false;
  const auto alive = unity::TryAlive(proof.poi.Get());
  if (!alive || !alive.value)
    return false;
  auto current = il2cpp::GcHandle::Strong(Invoke(api.preview_context, source.Get()));
  return current.Get() == context.Get()
         && proof.Current(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>())
         && il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, source.Get()) == data.Get();
}

inline bool RetainNavigation(NavigationInteractionUIViewController* controller, const SetCourseResponse& response,
                             bool keep_base = true, bool keep_player_ship = false)
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
  if (keep_base && Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response))
    return true;
  if (!keep_player_ship)
    return false;
  auto proof = MatchPlayerShip(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
  if (!proof)
    return false;
  const auto alive = unity::TryAlive(proof.poi.Get());
  if (!alive || !alive.value)
    return false;
  auto current_context = il2cpp::GcHandle::Strong(Invoke(api.navigation_context, source.Get()));
  auto current_manager = il2cpp::GcHandle::Strong(Invoke(api.scanning_instance, nullptr));
  auto current_data =
      current_manager ? il2cpp::GcHandle::Strong(Invoke(api.target_cache, current_manager.Get())) : il2cpp::GcHandle{};
  return current_context.Get() == context.Get() && current_manager.Get() == manager.Get()
         && current_data.Get() == data.Get()
         && proof.Current(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>());
}

} // namespace base_card_binding
