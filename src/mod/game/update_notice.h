#pragma once

#include "ui.h"
#include <optional>

namespace update_notice
{
struct CallbackTarget : il2cpp::Object {
  // The donor supplies the typed ButtonResult signature. Its code is never invoked.
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "AllianceInviteErrorMessageCreator">;
};

struct ScriptableObject : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "ScriptableObject">;
};

enum class ButtonLayout : int32_t { Custom = 3 };

inline bool IsSelectionType(const Il2CppType* type)
{
  using NativeResult  = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "MessageBox/ButtonResult">;
  auto*       klass   = il2cpp_class_from_type(type);
  const auto* backing = klass && klass == NativeResult::Raw() && il2cpp_class_is_enum(klass)
                            ? il2cpp_class_enum_basetype(klass)
                            : nullptr;
  return backing && backing->type == IL2CPP_TYPE_I4;
}

inline std::optional<bool> Ready()
{
  const auto visible = GameMessageBox::TryCall<"get_IsMessageBoxVisible", bool>();
  if (!visible)
    return std::nullopt;
  if (visible.value)
    return false;
  const auto ready = GameMessageBox::TryCall<"CanShow", bool>();
  return ready ? std::optional{ready.value} : std::nullopt;
}

inline il2cpp::GcHandle Text(std::string_view value)
{
  auto label    = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(value)));
  auto category = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString("shared_common")));
  if (!label || !category)
    return {};
  return il2cpp::GcHandle::Strong(il2cpp::New<LocaleTextContext>(reinterpret_cast<Il2CppString*>(label.Get()),
                                                                 reinterpret_cast<Il2CppString*>(category.Get())));
}

inline il2cpp::GcHandle Button(std::string_view label)
{
  auto text   = Text(label);
  auto button = il2cpp::GcHandle::Strong(il2cpp::New<GenericButtonContext>());
  if (!text || !button
      || !button.Get<GenericButtonContext>()->TryCall<"set_MainTextContext">(text.Get<LocaleTextContext>())
      || !button.Get<GenericButtonContext>()->TryCall<"set_OverrideLabelConfiguration">(true)
      || !button.Get<GenericButtonContext>()->TryCall<"set_Interactable">(true))
    return {};
  return button;
}

inline bool Show(std::string_view title, std::string_view body, std::string_view left, std::string_view right,
                 CallbackTarget* target, const MethodInfo* select)
{
  using Box                = GameMessageBox;
  using Fields             = MessageBoxContext::Class;
  static const auto show   = Box::StaticMethod<void(MessageBoxContext*, Il2CppDelegate*)>("Show");
  static const bool fields = Fields::HasField<"TitleContext", LocaleTextContext*>()
                             && Fields::HasField<"DescriptionContext", LocaleTextContext*>()
                             && Fields::HasField<"CustomButtonLeftKey", GenericButtonContext*>()
                             && Fields::HasField<"CustomButtonRightKey", GenericButtonContext*>()
                             && Fields::HasField<"ButtonMode", ButtonLayout>()
                             && Fields::HasField<"ShowExitButton", bool>()
                             && Fields::HasField<"AutoDismissOnAccept", bool>();
  if (!show || !fields || !target || !select)
    return false;
  auto callback = il2cpp::GcHandle::Strong(
      il2cpp::NewClosedDelegate(il2cpp_class_from_type(show.Info()->parameters[1]), target, il2cpp::Method(select)));
  auto heading = Text(title);
  auto detail  = Text(body);
  auto first   = Button(left);
  auto second  = Button(right);
  if (!callback || !heading || !detail || !first || !second)
    return false;

  auto* type = Fields::TypeObject();
  auto  context =
      type ? il2cpp::GcHandle::Strong(ScriptableObject::Class::Call<"CreateInstance", ScriptableObject*>(type))
           : il2cpp::GcHandle{};
  if (!context || !context.Get<il2cpp::Object>()->Is<MessageBoxContext>())
    return false;
  auto* box = context.Get<MessageBoxContext>();
  box->SetField<"TitleContext">(heading.Get<LocaleTextContext>());
  box->SetField<"DescriptionContext">(detail.Get<LocaleTextContext>());
  box->SetField<"CustomButtonLeftKey">(first.Get<GenericButtonContext>());
  box->SetField<"CustomButtonRightKey">(second.Get<GenericButtonContext>());
  box->SetField<"ButtonMode">(ButtonLayout::Custom);
  box->SetField<"ShowExitButton">(true);
  box->SetField<"AutoDismissOnAccept">(true);
  if (Box::TryCall<"Show", void(MessageBoxContext*, Il2CppDelegate*)>(
          box, reinterpret_cast<Il2CppDelegate*>(callback.Get())))
    return true;
  unity::Destroy(box);
  return false;
}
} // namespace update_notice
