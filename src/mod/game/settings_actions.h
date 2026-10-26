#pragma once

#include "settings_pages.h"

struct SettingsActionToken : il2cpp::Object {
  using Class = il2cpp::Class<"mscorlib", "System", "Object">;
};
struct SettingsActionContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "ButtonAndTextOptionContext">;

  std::optional<bool> CreatedWith(Il2CppObject* expected)
  {
    using Ref         = settings_binding::ReferenceObject;
    using Delegate    = settings_binding::ReferenceDelegate;
    const auto safe   = TryCall<"get_Callback", Ref*>();
    auto       root   = il2cpp::GcHandle::Strong(safe ? safe.value : nullptr);
    auto*      action = root.Get<il2cpp::Object>();
    if (!action || !il2cpp::detail::ResolveField(action->klass, "_callbacks", il2cpp::detail::SlotOf<Ref*>(), false))
      return {};
    auto       callbacks_root = il2cpp::GcHandle::Strong(action->Field<"_callbacks", Ref*>());
    auto*      callbacks      = callbacks_root.Get<il2cpp::List<Delegate*>>();
    const auto count          = callbacks ? callbacks->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
    if (!count || count.value < 0)
      return {};
    if (count.value != 1)
      return false;
    const auto callback = callbacks->TryCall<"get_Item", Delegate*(int32_t)>(0);
    return callback ? std::optional<bool>(callback.value == expected) : std::nullopt;
  }
};
struct SettingsActionButton : il2cpp::Object {
  il2cpp::CallResult<unity::GameObject*> TryObject()
  { return TryCall<"get_gameObject", unity::GameObject*>(); }
  il2cpp::CallResult<bool> TryInteractable()
  { return TryCall<"get_interactable", bool>(); }
  il2cpp::CallResult<void> TryOverride(bool value)
  { return TryCall<"OverrideInteractable", void(bool)>(value); }
  il2cpp::CallResult<void> TryClear()
  { return TryCall<"ClearInteractable", void>(); }
};
struct SettingsActionWidget : Widget<SettingsActionContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "ButtonAndTextOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
  TextLocalizer* ButtonLabel()
  { return Field<"_buttonLabel", TextLocalizer*>(); }
  TextLocalizer* ValueLabel()
  { return Field<"_valueLabel", TextLocalizer*>(); }
  SettingsActionButton* Button()
  { return reinterpret_cast<SettingsActionButton*>(Field<"_button", settings_binding::ReferenceObject*>()); }
  static bool SurfaceAvailable()
  {
    using Ref          = settings_binding::ReferenceObject;
    const auto* field  = il2cpp::detail::ResolveField(Class::Raw(), "_button", il2cpp::detail::SlotOf<Ref*>(), false);
    auto*       button = field ? il2cpp_class_from_type(field->type) : nullptr;
    return Class::HasField<"_label", TextLocalizer*>() && Class::HasField<"_buttonLabel", TextLocalizer*>()
           && Class::HasField<"_valueLabel", TextLocalizer*>() && button
           && il2cpp::detail::ResolveMethod(button, "OverrideInteractable", il2cpp::detail::SignatureOf<void, bool>(),
                                            false)
           && il2cpp::detail::ResolveMethod(button, "ClearInteractable", il2cpp::detail::SignatureOf<void>(), false)
           && il2cpp::detail::ResolveMethod(button, "get_interactable", il2cpp::detail::SignatureOf<bool>(), false)
           && il2cpp::detail::ResolveMethod(button, "get_gameObject", il2cpp::detail::SignatureOf<unity::GameObject*>(),
                                            false);
  }
};
struct SettingsActionOwner : SettingsContext {
  static il2cpp::Method AddMethod()
  {
    using Delegate = settings_binding::ReferenceDelegate;
    return Class::Method<void(SettingsContainerContext*, Il2CppString*, Il2CppString*, Delegate*, Delegate*,
                              Il2CppString*)>("AddButtonAndText");
  }
  static Il2CppObject* DelegateFor(il2cpp::Method method, int argument, il2cpp::Object* target,
                                   const MethodInfo* callback)
  {
    return method && argument >= 0 && argument < method.Info()->parameters_count
               ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(method.Info()->parameters[argument]), target,
                                           il2cpp::Method(callback))
               : nullptr;
  }
};

namespace mod_settings::native
{
// Valid only during the synchronous invocation. Popup owners must root their own reference and page visit token.
il2cpp::Object* InvokingActionWidget();
void            RefreshActions();
} // namespace mod_settings::native
