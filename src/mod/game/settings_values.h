#pragma once

#include "settings_pages.h"

namespace settings_values_binding
{
inline bool ClosedValue(const Il2CppType* type, Il2CppClass* definition, Il2CppClass* value)
{
  const auto* generic =
      type && !type->byref && type->type == IL2CPP_TYPE_GENERICINST ? type->data.generic_class : nullptr;
  const auto* arguments = generic ? generic->context.class_inst : nullptr;
  return definition && value && generic && generic->type && il2cpp_class_from_type(generic->type) == definition
         && arguments && arguments->type_argc == 1 && arguments->type_argv && arguments->type_argv[0]
         && il2cpp_class_from_type(arguments->type_argv[0]) == value;
}
inline bool ReferenceVector(const Il2CppType* type, Il2CppClass* element)
{
  auto* klass = type && !type->byref && type->type == IL2CPP_TYPE_SZARRAY ? il2cpp_class_from_type(type) : nullptr;
  return klass && klass->rank == 1 && element && klass->element_class == element && !il2cpp_class_is_valuetype(element);
}

// SafeFunc/SafeAction are native ownership wrappers, not delegates. Compare the supplied delegate identities;
// labels and indices alone never establish that a row belongs to the mod.
inline std::optional<bool> Callbacks(SettingsOptionContext* context, Il2CppObject* getter, Il2CppObject* setter)
{
  using Ref       = settings_binding::ReferenceObject;
  using Delegate  = settings_binding::ReferenceDelegate;
  using Selection = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SelectionItemOptionContext">;
  using Slider    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SliderOptionContext">;
  using Func      = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Core", "SafeFunc`1">;
  using Action    = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Core", "SafeAction`1">;
  using List      = il2cpp::Class<"mscorlib", "System.Collections.Generic", "List`1">;
  if (!context || !getter || !setter)
    return false;
  auto* value     = context->klass == Selection::Raw() ? il2cpp::PrimitiveClass<int32_t>()
                    : context->klass == Slider::Raw()  ? il2cpp::PrimitiveClass<float>()
                                                       : nullptr;
  auto* get_field = il2cpp::detail::ResolveField(context->klass, "GetCallback", il2cpp::detail::SlotOf<Ref*>(), false);
  auto* set_field = il2cpp::detail::ResolveField(context->klass, "SetCallback", il2cpp::detail::SlotOf<Ref*>(), false);
  if (!value || !get_field || !set_field || !ClosedValue(get_field->type, Func::Raw(), value)
      || !ClosedValue(set_field->type, Action::Raw(), value))
    return {};
  // context is an OptionContext facade; these fields live on the closed
  // ValueOptionContext<T> base of its runtime class, not on OptionContext.
  auto  get_root = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Ref*>(get_field, context));
  auto  set_root = il2cpp::GcHandle::Strong(il2cpp::detail::ReadField<Ref*>(set_field, context));
  auto* get      = get_root.Get<il2cpp::Object>();
  auto* set      = set_root.Get<il2cpp::Object>();
  if (!get || !set || get->klass != il2cpp_class_from_type(get_field->type)
      || set->klass != il2cpp_class_from_type(set_field->type))
    return {};
  if (!il2cpp::detail::ResolveField(get->klass, "_callback", il2cpp::detail::SlotOf<Delegate*>(), false)
      || !il2cpp::detail::ResolveField(set->klass, "_callbacks", il2cpp::detail::SlotOf<Ref*>(), false))
    return {};
  if (get->Field<"_callback", Delegate*>() != getter)
    return false;
  auto  callbacks_root = il2cpp::GcHandle::Strong(set->Field<"_callbacks", Ref*>());
  auto* callbacks      = callbacks_root.Get<il2cpp::List<Delegate*>>();
  if (!callbacks || !ClosedValue(&callbacks->klass->byval_arg, List::Raw(), setter->klass))
    return {};
  const auto count = callbacks ? callbacks->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return {};
  if (count.value != 1)
    return false; // Our setter plus another owner's action is not our row.
  const auto callback = callbacks->TryCall<"get_Item", Delegate*(int32_t)>(int32_t{0});
  return callback ? std::optional<bool>(callback.value == setter) : std::nullopt;
}

inline Il2CppObject* DelegateFor(il2cpp::Method method, int argument, SettingsSectionDirector* owner,
                                 const MethodInfo* callback)
{
  return method && argument >= 0 && argument < method.Info()->parameters_count
             ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(method.Info()->parameters[argument]), owner,
                                         il2cpp::Method(callback))
             : nullptr;
}
} // namespace settings_values_binding

struct SettingsValueSlider : unity::Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "Slider">;
};

struct SettingsSelectionContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SelectionItemOptionContext">;
};
struct SettingsSliderContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SliderOptionContext">;
};
struct SettingsSelectionWidget : Widget<SettingsSelectionContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SelectionItemOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
  unity::Toggle* Toggle()
  { return Field<"_toggle", unity::Toggle*>(); }
};
struct SettingsSliderWidget : Widget<SettingsSliderContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SliderOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
  TextLocalizer* ValueLabel()
  { return Field<"_valueLabel", TextLocalizer*>(); }
  SettingsValueSlider* Slider()
  { return Field<"_slider", SettingsValueSlider*>(); }
};

struct SettingsValueContext : SettingsContext {
  using Ref      = settings_binding::ReferenceObject;
  using Delegate = settings_binding::ReferenceDelegate;
  static il2cpp::Method AddChoiceMethod()
  {
    const auto method =
        Class::Method<void(SettingsContainerContext*, Il2CppString*, Ref*, Delegate*, Delegate*, Il2CppString*)>(
            "AddSelection");
    return method
                   && settings_values_binding::ReferenceVector(method.Info()->parameters[2],
                                                               il2cpp::detail::StringClass())
               ? method
               : il2cpp::Method{};
  }
  static il2cpp::Method AddSliderMethod()
  {
    const auto method = Class::Method<void(SettingsContainerContext*, Il2CppString*, Delegate*, Delegate*, Delegate*,
                                           Ref*, bool, float, float)>("AddSlider");
    return method
                   && settings_values_binding::ReferenceVector(method.Info()->parameters[5],
                                                               LocaleTextContext::Class::Raw())
               ? method
               : il2cpp::Method{};
  }
};
