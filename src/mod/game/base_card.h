#pragma once

// Qualified observations used to retain the currently viewed enemy station's
// card. Course submission, completion and semaphore ownership remain native.
#include "game/queues.h"
#include "game/viewers.h"

#include <cstddef>
#include <cstring>

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

inline bool RetainPreview(PreScanTargetWidget* widget, const SetCourseResponse& response)
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
  return data && context
         && il2cpp::detail::ReadField<Il2CppObject*>(api.preview_data, source.Get()) == data.Get()
         && Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
}

inline bool RetainNavigation(NavigationInteractionUIViewController* controller, const SetCourseResponse& response)
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
  return context && data
         && Matches(data.Get<BattleTargetData>(), context.Get<NavigationInteractionUIContext>(), response);
}

} // namespace base_card_binding
