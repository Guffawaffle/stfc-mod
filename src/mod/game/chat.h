#pragma once

// The chat manager, the full-screen and side chat views, and the chat message rows.

#include "ui.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>
#include <type_traits>

enum class ChatChannelCategory : int32_t {
  None            = -1,
  Newbie          = 0,
  Global          = 1,
  Alliance        = 2,
  Private         = 3,
  Private_Message = 4,
  Block           = 5,
  Regional        = 6,
};

enum class ChatViewMode : int32_t {
  Fullscreen = 0,
  Side       = 1,
};

struct ChatManager : MonoSingleton<ChatManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Chat", "ChatManager">;

  bool IsSideChatOpen()
  { return Get<"IsSideChatOpen", bool>(); }
  void SetViewMode(ChatViewMode value)
  { Set<"ViewMode">(value); }

  bool CanSeeNewbieChat()
  { return Call<"CanSeeNewbieChat", bool>(); }
  bool CanSeeRegionalChat()
  { return Call<"CanSeeRegionalChat", bool>(); }
  void OpenChannel(ChatChannelCategory category = ChatChannelCategory::Alliance)
  { Call<"OpenChannel">(category, il2cpp::NewString("")); }

  void OpenChannel(ChatChannelCategory category, ChatViewMode viewMode)
  {
    SetViewMode(viewMode);
    OpenChannel(category);
  }
};

struct SwipeScroller : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "SwipeScroller">;

  int32_t currentContentIndex()
  { return Field<"_currentContentIndex", int32_t>(); }
  void FocusOnInstantly(int32_t index)
  { Call<"FocusOnInstantly">(index); }
};

struct ChatPreviewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Chat", "ChatPreviewController">;

  SwipeScroller* swipeScroller()
  { return Field<"_swipeScroller", SwipeScroller*>(); }
  void SetFocusedPanel(ChatChannelCategory value)
  { SetField<"_focusedPanel">(value); }
};

struct ChatMessageListLocalViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Chat", "ChatMessageListLocalViewController">;

  unity::TMP_InputField* inputField()
  { return Field<"_inputField", unity::TMP_InputField*>(); }
};

struct FullScreenChatViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Chat", "FullScreenChatViewController">;

  ChatMessageListLocalViewController* messageList()
  { return Field<"_messageList", ChatMessageListLocalViewController*>(); }
  TabBarViewController* categoriesTabBarViewController()
  { return Field<"_categoriesTabBarViewController", TabBarViewController*>(); }
};

struct PlayerTitleWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerAvatars", "PlayerTitleWidget">;
};

struct FrameAndAvatarWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerAvatars", "FrameAndAvatarWidget">;
};

// A player's name, avatar and title, on chat rows and other screens.
struct UserProfileWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "UserProfileWidget">;

  PlayerTitleWidget* playerTitleWidget()
  { return Field<"_playerTitleWidget", PlayerTitleWidget*>(); }
};

struct ChatMessageWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Chat", "ChatMessageWidget">;

  enum class RetentionStatus { Unavailable, NativeBehavior, Ready };
  static RetentionStatus AvatarRetentionStatus();
  static il2cpp::Method  AvatarRetentionTarget();
  // Retention is set before native child binding and remains set for asynchronous reloads.
  // Null children need no change; false reports a runtime reference that failed qualification.
  bool RetainAvatars();
};

namespace chat_avatar_binding
{
struct Fields {
  ChatMessageWidget::RetentionStatus status  = ChatMessageWidget::RetentionStatus::Unavailable;
  FieldInfo *                        profile = nullptr, *avatar = nullptr, *admin = nullptr, *keep = nullptr;
  Il2CppClass*                       avatar_class = nullptr;
};

template <class Owner, class Value> FieldInfo* Field(const char* name)
{
  auto* owner = Owner::Class::Raw();
  auto* field = owner ? il2cpp_class_get_field_from_name(owner, name) : nullptr;
  if (!field || !field->type || field->type->byref || (field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return nullptr;
  const auto size = il2cpp_class_instance_size(owner);
  if (size < sizeof(Value) || static_cast<uint32_t>(field->offset) > size - sizeof(Value))
    return nullptr;
  if constexpr (std::is_same_v<Value, bool>) {
    if (field->type->type != IL2CPP_TYPE_BOOLEAN)
      return nullptr;
  } else {
    using Binding  = std::remove_pointer_t<Value>;
    auto* expected = Binding::Class::Raw();
    if (field->type->type != IL2CPP_TYPE_CLASS || !expected || il2cpp_class_from_type(field->type) != expected)
      return nullptr;
  }
  return field;
}

inline const Fields& RetentionFields()
{
  static const Fields fields = [] {
    Fields result;
    result.avatar_class = FrameAndAvatarWidget::Class::Raw();
    if (!result.avatar_class)
      return result;
    // Older clients always retained downloaded portraits and have no serialized retention switch.
    if (!il2cpp_class_get_field_from_name(result.avatar_class, "_keepInCache")) {
      result.status = ChatMessageWidget::RetentionStatus::NativeBehavior;
      return result;
    }
    result.profile = Field<ChatMessageWidget, UserProfileWidget*>("_userProfileWidget");
    result.avatar  = Field<UserProfileWidget, FrameAndAvatarWidget*>("_profileFrameAndAvatar");
    result.admin   = Field<ChatMessageWidget, FrameAndAvatarWidget*>("_adminAvatarWidget");
    result.keep    = Field<FrameAndAvatarWidget, bool>("_keepInCache");
    if (result.profile && result.avatar && result.admin && result.keep)
      result.status = ChatMessageWidget::RetentionStatus::Ready;
    return result;
  }();
  return fields;
}
} // namespace chat_avatar_binding

inline ChatMessageWidget::RetentionStatus ChatMessageWidget::AvatarRetentionStatus()
{ return chat_avatar_binding::RetentionFields().status; }

inline il2cpp::Method ChatMessageWidget::AvatarRetentionTarget()
{
  auto        method = Class::Method<void()>("SetWidgetData");
  const auto* info   = method.Info();
  if (!info || info->klass != Class::Raw() || !info->methodPointer || info->is_generic || info->is_inflated
      || (info->flags & METHOD_ATTRIBUTE_STATIC) || info->parameters_count != 0 || !info->return_type
      || info->return_type->byref || info->return_type->type != IL2CPP_TYPE_VOID)
    return {};
  return method;
}

inline bool ChatMessageWidget::RetainAvatars()
{
  const auto& fields = chat_avatar_binding::RetentionFields();
  if (fields.status != RetentionStatus::Ready)
    return fields.status == RetentionStatus::NativeBehavior;
  auto  source          = il2cpp::GcHandle::Strong(this);
  auto* observed_source = source.Get<ChatMessageWidget>();
  if (!observed_source || !observed_source->Is<ChatMessageWidget>())
    return false;

  auto* profile_value = il2cpp::detail::ReadField<UserProfileWidget*>(fields.profile, source.Get());
  auto  profile       = il2cpp::GcHandle::Strong(profile_value);
  bool  qualified     = !profile_value || (profile.Get() && profile.Get<UserProfileWidget>()->Is<UserProfileWidget>());
  auto* avatar_value =
      profile && qualified ? il2cpp::detail::ReadField<FrameAndAvatarWidget*>(fields.avatar, profile.Get()) : nullptr;
  auto  avatar      = il2cpp::GcHandle::Strong(avatar_value);
  auto* admin_value = il2cpp::detail::ReadField<FrameAndAvatarWidget*>(fields.admin, source.Get());
  auto  admin       = il2cpp::GcHandle::Strong(admin_value);
  qualified         = qualified && (!avatar_value || avatar.Get()) && (!admin_value || admin.Get());
  const auto retain = [&](Il2CppObject* object) {
    if (!object)
      return;
    // Preserve the original exact avatar-class guard rather than accepting an unrelated runtime object.
    if (il2cpp_object_get_class(object) != fields.avatar_class) {
      qualified = false;
      return;
    }
    il2cpp::detail::WriteField(fields.keep, object, true);
  };
  retain(avatar.Get());
  if (admin.Get() != avatar.Get())
    retain(admin.Get());
  return qualified;
}
