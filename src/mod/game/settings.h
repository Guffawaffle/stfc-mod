#pragma once

// Native Settings contexts and preference access. Observing an FC confirmation must not create a singleton or
// initialize account preference storage merely to render a control.
#include "ui.h"
#include <il2cpp/il2cpp.h>

namespace settings_binding
{
// Object* also accepts opaque IntPtr in the general library. These require managed references before GC use.
struct ReferenceObject : il2cpp::Object {
  struct Class {
    static Il2CppClass* Raw()
    { return il2cpp::ObjectClass(); }
  };
};
struct ReferenceDelegate : il2cpp::Object {
  using Class = il2cpp::Class<"mscorlib", "System", "Delegate">;
};

inline bool Reference(const Il2CppType* type)
{
  if (!type || type->byref)
    return false;
  if (type->type == IL2CPP_TYPE_CLASS || type->type == IL2CPP_TYPE_OBJECT)
    return true;
  auto* kind = type->type == IL2CPP_TYPE_GENERICINST ? il2cpp_class_from_type(type) : nullptr;
  return kind && !il2cpp_class_is_valuetype(kind);
}

template <class T> il2cpp::CallResult<T*> ExistingSingleton()
{
  auto* owner = T::Class::Raw();
  auto* base  = owner ? il2cpp_class_get_parent(owner) : nullptr;
  while (base && std::string_view(il2cpp_class_get_name(base)) != "MonoSingleton`1")
    base = il2cpp_class_get_parent(base);
  auto* instance    = base ? il2cpp_class_get_field_from_name(base, "s_instance") : nullptr;
  auto* initialized = base ? il2cpp_class_get_field_from_name(base, "s_initialized") : nullptr;
  if (!instance || !Reference(instance->type) || !(instance->type->attrs & FIELD_ATTRIBUTE_STATIC) || !initialized
      || !initialized->type || initialized->type->byref || initialized->type->type != IL2CPP_TYPE_BOOLEAN
      || !(initialized->type->attrs & FIELD_ATTRIBUTE_STATIC))
    return {};
  // Deliberately avoid ResolveField/Class::Field: those initialize static classes before reading.
  bool ready = false;
  il2cpp_field_static_get_value(initialized, &ready);
  if (!ready)
    return {true, nullptr};
  T* value = nullptr;
  il2cpp_field_static_get_value(instance, &value);
  return !value || il2cpp_class_is_assignable_from(owner, value->klass) ? il2cpp::CallResult<T*>{true, value}
                                                                        : il2cpp::CallResult<T*>{};
}
} // namespace settings_binding

struct PersistentPrefsManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PersistentPrefs", "PersistentPrefsManager">;
  static bool ConfirmationAvailable()
  {
    static const bool available = Class::HasField<"_isCloudFileLoaded", bool>() && Class::HasField<"_isLoading", bool>()
                                  && Class::HasField<"_savedData", settings_binding::ReferenceObject*>()
                                  && Class::Method<bool(Il2CppString*, bool, bool)>("GetBool");
    return available;
  }
  bool CloudLoaded()
  { return Field<"_isCloudFileLoaded", bool>(); }
  bool Loading()
  { return Field<"_isLoading", bool>(); }
  il2cpp::Object* SavedData()
  { return Field<"_savedData", settings_binding::ReferenceObject*>(); }
};

struct FleetCommanderManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.FleetCommander", "FleetCommanderManager">;
};

struct SettingsOptionContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "OptionContext">;
  LocaleTextContext* Label()
  { return Get<"LabelContext", LocaleTextContext*>(); }
  bool HasLabel(std::string_view id)
  {
    auto  label = il2cpp::GcHandle::Strong(Label());
    auto* value = label.Get<LocaleTextContext>();
    return value && il2cpp::Equals(value->Get<"Identifier", Il2CppString*>(), id);
  }
};
struct SettingsContainerContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "ContainerOptionContext">;
  // ReadOnlyCollection<OptionContext>: use its checked Count/indexer, never mutate the facade.
  il2cpp::List<SettingsOptionContext*>* Children()
  {
    return reinterpret_cast<il2cpp::List<SettingsOptionContext*>*>(
        Get<"Children", settings_binding::ReferenceObject*>());
  }
};
struct SettingsContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SettingsContext">;
  SettingsOptionContext* RootOption()
  { return Get<"RootOption", SettingsOptionContext*>(); }
  static il2cpp::Method AddToggleMethod()
  {
    return Class::Method<void(SettingsContainerContext*, Il2CppString*, il2cpp::Object*, il2cpp::Object*)>("AddToggle");
  }
  static Il2CppObject* ToggleDelegate(int32_t argument, il2cpp::Object* owner, const MethodInfo* callback)
  {
    const auto add = AddToggleMethod();
    return add && (argument == 2 || argument == 3)
               ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(add.Info()->parameters[argument]), owner,
                                           il2cpp::Method(callback))
               : nullptr;
  }
};
struct SettingsSectionDirector : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "SettingsSectionDirector">;
  static bool QualifiedQueryState()
  {
    const auto  query      = Class::Method<int32_t()>("QueryShouldShowGenericPcSetting");
    auto*       kind       = query ? il2cpp_class_from_type(query.Info()->return_type) : nullptr;
    const auto* underlying = kind && il2cpp_class_is_enum(kind) ? il2cpp_class_enum_basetype(kind) : nullptr;
    return underlying && underlying->type == IL2CPP_TYPE_I4;
  }
};
struct ToggleOptionContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "ToggleOptionContext">;
  il2cpp::Object* Query()
  { return Field<"<QueryOptionState>k__BackingField", settings_binding::ReferenceDelegate*>(); }
  il2cpp::Object* Getter()
  {
    auto  root    = il2cpp::GcHandle::Strong(Field<"GetCallback", settings_binding::ReferenceObject*>());
    auto* wrapper = root.Get<il2cpp::Object>();
    return wrapper && il2cpp::ClassName(wrapper->klass) == "Digit.Client.Core.SafeFunc`1"
               ? wrapper->Field<"_callback", settings_binding::ReferenceDelegate*>()
               : nullptr;
  }
  static Il2CppObject* QueryDelegate(il2cpp::Object* owner, const MethodInfo* callback)
  {
    const auto setter = Class::Method<void(il2cpp::Object*)>("set_QueryOptionState");
    return setter ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(setter.Info()->parameters[0]), owner,
                                              il2cpp::Method(callback))
                  : nullptr;
  }
  // Do not invoke SafeAction.get_Callback: it is a null stub in retained client273. The native AddToggle stores
  // our getter and setter in these wrappers before calling AddChild, which can mutate and then throw.
  std::optional<bool> CreatedWith(Il2CppObject* getter, Il2CppObject* setter)
  {
    if (!Class::HasField<"GetCallback", settings_binding::ReferenceObject*>()
        || !Class::HasField<"SetCallback", settings_binding::ReferenceObject*>())
      return std::nullopt;
    auto  get_root = il2cpp::GcHandle::Strong(Field<"GetCallback", settings_binding::ReferenceObject*>());
    auto  set_root = il2cpp::GcHandle::Strong(Field<"SetCallback", settings_binding::ReferenceObject*>());
    auto* get      = get_root.Get<il2cpp::Object>();
    auto* set      = set_root.Get<il2cpp::Object>();
    if (!get || !set || il2cpp::ClassName(get->klass) != "Digit.Client.Core.SafeFunc`1"
        || il2cpp::ClassName(set->klass) != "Digit.Client.Core.SafeAction`1")
      return std::nullopt;
    if (!il2cpp::detail::ResolveField(get->klass, "_callback",
                                      il2cpp::detail::SlotOf<settings_binding::ReferenceDelegate*>(), false))
      return std::nullopt;
    if (get->Field<"_callback", settings_binding::ReferenceDelegate*>() != getter)
      return false;
    if (!il2cpp::detail::ResolveField(set->klass, "_callbacks",
                                      il2cpp::detail::SlotOf<settings_binding::ReferenceObject*>(), false))
      return std::nullopt;
    auto  callbacks_root = il2cpp::GcHandle::Strong(set->Field<"_callbacks", settings_binding::ReferenceObject*>());
    auto* callbacks      = callbacks_root.Get<il2cpp::List<il2cpp::Object*>>();
    const auto count     = callbacks ? callbacks->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
    if (!count || count.value < 0)
      return std::nullopt;
    for (int32_t i = 0; i < count.value; ++i) {
      const auto value = callbacks->TryCall<"get_Item", il2cpp::Object*(int32_t)>(i);
      if (!value)
        return std::nullopt;
      if (value.value == setter)
        return true;
    }
    return false;
  }
};
struct ToggleOptionWidget : Widget<ToggleOptionContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "ToggleOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
  unity::Toggle* Toggle()
  { return Field<"_toggle", unity::Toggle*>(); }
  unity::Component* StateAnimator()
  { return Field<"_toggleStateAnimator", unity::Component*>(); }
};
