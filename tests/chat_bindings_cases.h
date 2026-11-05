#pragma once

// Actual chat callbacks and shortcut consumers. The existing dispatcher fixture
// keeps its named chat sinks; rename only these two exports in this test TU.
#define OpenChatShortcut ChatNativeOpenShortcut
#define ChatChannelShortcuts ChatNativeChannelShortcuts
#include "../src/mod/features/chat.cc"
#undef ChatChannelShortcuts
#undef OpenChatShortcut

namespace
{
struct ChatNativeReference {
  Il2CppObject  header;
  Il2CppObject* value = nullptr;
};
struct ChatNativeView {
  Il2CppObject  header;
  Il2CppObject* tabs = nullptr;
  Il2CppObject* list = nullptr;
};
struct ChatNativeScroller {
  Il2CppObject header;
  int32_t      index = 0;
};
struct ChatNativePreview {
  Il2CppObject        header;
  Il2CppObject*       scroller = nullptr;
  ChatChannelCategory focused  = ChatChannelCategory::Global;
};
struct ChatNativeButton {
  Il2CppObject header;
  bool         interactable = true;
};
Il2CppObject                    chat_native_manager{}, chat_native_list{}, chat_native_input{};
ChatNativeView                  chat_native_view{};
ChatNativeReference             chat_native_tabs{}, chat_native_bar{}, chat_native_messages{};
ChatNativeScroller              chat_native_scroller{};
ChatNativePreview               chat_native_preview{};
std::array<ChatNativeButton, 4> chat_native_buttons;
bool                            chat_native_manager_present = true, chat_native_cadet = false, chat_native_veil = false,
                                chat_native_availability_throws = false, chat_native_side_open = false, chat_native_side_throws = false,
                                chat_native_mode_throws = false;
int                             chat_native_focuses = 0, chat_native_swipes = 0, chat_native_opens = 0;
ChatViewMode                    chat_native_mode    = ChatViewMode::Fullscreen;
ChatChannelCategory             chat_native_channel = ChatChannelCategory::None;

bool ChatNativeRooted(const void* object)
{
  return std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; });
}
Il2CppObject* ChatNativeInstance(const MethodInfo*)
{ return chat_native_manager_present ? &chat_native_manager : nullptr; }
bool ChatNativeCadet(Il2CppObject* self, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "chat availability retains its manager through native callbacks");
  if (chat_native_availability_throws)
    throw std::runtime_error("chat availability unavailable");
  return chat_native_cadet;
}
bool ChatNativeVeil(Il2CppObject*, const MethodInfo*)
{ return chat_native_veil; }
bool ChatNativeSide(Il2CppObject* self, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "side-chat observation retains its manager");
  if (chat_native_side_throws)
    throw std::runtime_error("side-chat state unavailable");
  return chat_native_side_open;
}
void ChatNativeSetMode(Il2CppObject*, ChatViewMode mode, const MethodInfo*)
{
  if (chat_native_mode_throws)
    throw std::runtime_error("chat view mode unavailable");
  chat_native_mode = mode;
}
void ChatNativeOpen(Il2CppObject* self, ChatChannelCategory channel, Il2CppString*, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "native channel request retains its manager");
  ++chat_native_opens;
  chat_native_channel = channel;
}
void ChatNativeFocus(Il2CppObject* self, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "native input focus retains the acquired input");
  ++chat_native_focuses;
}
int32_t ChatNativeButtonCount(Il2CppObject* self, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "chat tab traversal retains its acquired list");
  return static_cast<int32_t>(chat_native_buttons.size());
}
Il2CppObject* ChatNativeButtonItem(Il2CppObject*, int32_t index, const MethodInfo*)
{ return &chat_native_buttons.at(index).header; }
void ChatNativeInteractable(ChatNativeButton* self, bool enabled, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "chat tab mutation retains the exact acquired button");
  self->interactable = enabled;
}
void ChatNativeSwipe(ChatNativeScroller* self, int32_t index, const MethodInfo*)
{
  Require(ChatNativeRooted(self), "preview focus retains its scroller");
  ++chat_native_swipes;
  self->index = index;
}

void ChatNativeRuntime()
{
  auto& manager    = Define("Digit.Prime.Chat", "ChatManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& category   = Define("Digit.Prime.Chat", "ChatChannelCategory", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& mode       = Define("Digit.Prime.Chat", "ChatViewMode", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  category.is_enum = mode.is_enum = true;
  auto& instance = AddMethod(manager, "get_Instance", reinterpret_cast<void*>(&ChatNativeInstance), T(manager), {},
                             METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager, "Instance", &instance);
  AddMethod(manager, "CanSeeNewbieChat", reinterpret_cast<void*>(&ChatNativeCadet), T(bool_class), {});
  AddMethod(manager, "CanSeeRegionalChat", reinterpret_cast<void*>(&ChatNativeVeil), T(bool_class), {});
  auto& side = AddMethod(manager, "get_IsSideChatOpen", reinterpret_cast<void*>(&ChatNativeSide), T(bool_class), {});
  AddProperty(manager, "IsSideChatOpen", &side);
  auto& set_mode =
      AddMethod(manager, "set_ViewMode", reinterpret_cast<void*>(&ChatNativeSetMode), T(void_class), {T(mode)});
  AddProperty(manager, "ViewMode", nullptr, &set_mode);
  AddMethod(manager, "OpenChannel", reinterpret_cast<void*>(&ChatNativeOpen), T(void_class),
            {T(category), T(string_class)});
  chat_native_manager.klass = &manager.klass;

  auto& scroller = Define("Digit.Client.UI", "SwipeScroller", IL2CPP_TYPE_CLASS, &object_class);
  auto& preview  = Define("Digit.Prime.Chat", "ChatPreviewController", IL2CPP_TYPE_CLASS, &object_class);
  AddField(scroller, "_currentContentIndex", int_class, offsetof(ChatNativeScroller, index));
  AddMethod(scroller, "FocusOnInstantly", reinterpret_cast<void*>(&ChatNativeSwipe), T(void_class), {T(int_class)});
  AddField(preview, "_swipeScroller", scroller, offsetof(ChatNativePreview, scroller));
  AddField(preview, "_focusedPanel", category, offsetof(ChatNativePreview, focused));
  chat_native_scroller.header.klass = &scroller.klass;
  chat_native_preview.header.klass  = &preview.klass;
  chat_native_preview.scroller      = &chat_native_scroller.header;

  auto& buttons    = Define("Game", "ChatNativeButtons", IL2CPP_TYPE_CLASS, &object_class);
  auto& button     = Define("Digit.Client.UI", "GenericButtonContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& container  = Define("Digit.Client.UI", "BaseListContainer", IL2CPP_TYPE_CLASS, &object_class);
  auto& selectable = Define("Digit.Client.UI", "SelectableList", IL2CPP_TYPE_CLASS, &container);
  auto& bar        = Define("Digit.Client.UI", "TabBar", IL2CPP_TYPE_CLASS, &selectable);
  auto& tabs       = Define("Digit.Client.UI", "TabBarViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& view       = Define("Digit.Prime.Chat", "FullScreenChatViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& messages   = Define("Digit.Prime.Chat", "ChatMessageListLocalViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(buttons, "get_Count", reinterpret_cast<void*>(&ChatNativeButtonCount), T(int_class), {});
  AddMethod(buttons, "get_Item", reinterpret_cast<void*>(&ChatNativeButtonItem), T(object_class), {T(int_class)});
  auto& interactable = AddMethod(button, "set_Interactable", reinterpret_cast<void*>(&ChatNativeInteractable),
                                 T(void_class), {T(bool_class)});
  AddProperty(button, "Interactable", nullptr, &interactable);
  AddField(container, "_data", buttons, offsetof(ChatNativeReference, value));
  AddField(tabs, "_tabBar", bar, offsetof(ChatNativeReference, value));
  AddField(view, "_categoriesTabBarViewController", tabs, offsetof(ChatNativeView, tabs));
  AddField(view, "_messageList", messages, offsetof(ChatNativeView, list));
  AddField(messages, "_inputField", input_field_class, offsetof(ChatNativeReference, value));
  AddMethod(input_field_class, "ActivateInputField", reinterpret_cast<void*>(&ChatNativeFocus), T(void_class), {});
  chat_native_list.klass  = &buttons.klass;
  chat_native_bar         = {{&bar.klass}, &chat_native_list};
  chat_native_tabs        = {{&tabs.klass}, &chat_native_bar.header};
  chat_native_input.klass = &input_field_class.klass;
  chat_native_messages    = {{&messages.klass}, &chat_native_input};
  chat_native_view        = {{&view.klass}, &chat_native_tabs.header, &chat_native_messages.header};
  for (auto& item : chat_native_buttons)
    item.header.klass = &button.klass;
  fleet_native_widgets[&view.klass] = {&chat_native_view.header};
}

void ChatNativeChannels()
{
  ChatNativeRuntime();
  auto* view    = reinterpret_cast<FullScreenChatViewController*>(&chat_native_view);
  auto* preview = reinterpret_cast<ChatPreviewController*>(&chat_native_preview);
  for (const bool cadet : {false, true})
    for (const bool veil : {false, true})
      for (const bool disable_global : {false, true})
        for (const bool disable_veil : {false, true}) {
          chat_native_cadet                    = cadet;
          chat_native_veil                     = veil;
          Config::Get().ui.disable_galaxy_chat = disable_global;
          Config::Get().ui.disable_veil_chat   = disable_veil;
          const int32_t galaxy = cadet ? 1 : 0, regional = veil ? galaxy + 1 : -1;
          const int32_t alliance = galaxy + (veil ? 2 : 1);
          const auto    disabled = [&](int32_t index) {
            return (disable_global && index == galaxy) || (disable_veil && regional >= 0 && index == regional);
          };
          int show_calls = 0;
          FullScreenChatViewController_AboutToShow_Hook(
              [&](auto*) {
                ++show_calls;
                for (auto& button : chat_native_buttons)
                  button.interactable = true;
              },
              view);
          Require(show_calls == 1, "native fullscreen show runs exactly once before tab decoration");
          for (int32_t index = 0; index < 4; ++index) {
            Require(chat_native_buttons[index].interactable == !disabled(index),
                    "actual callback disables only the requested available galaxy/Veil tabs");
            int changed = 0;
            FullScreenChatViewController_OnDidChangeSelectedTab_Hook(
                [&](auto*, int32_t observed, void*) {
                  ++changed;
                  Require(observed == index, "allowed tab index preserved");
                },
                view, index, nullptr);
            Require(changed == (disabled(index) ? 0 : 1),
                    "disabled keyboard tab selection cannot reach native handler");
            chat_native_scroller.index  = index;
            chat_native_preview.focused = ChatChannelCategory::Private;
            ChatPreviewController_AboutToShow_Hook([](auto*) {}, preview);
            Require(chat_native_scroller.index == (disabled(index) ? alliance : index)
                        && chat_native_preview.focused
                               == (disabled(index) ? ChatChannelCategory::Alliance : ChatChannelCategory::Private),
                    "preview reopen redirects only a disabled channel, preserving allowed private/cadet panels");
            int focused = -1, calls = 0;
            ChatPreviewController_OnPanelFocused_Hook(
                [&](auto*, int32_t actual) {
                  ++calls;
                  focused                    = actual;
                  chat_native_scroller.index = actual;
                },
                preview, index);
            Require(calls == 1 && focused == (disabled(index) ? alliance : index),
                    "live preference combinations redirect only disabled swipe panels through one native call");
          }
          int global = 0, regional_messages = 0;
          ChatPreviewController_OnGlobalMessageReceived_Hook([&](auto*, void*) { ++global; }, preview, nullptr);
          ChatPreviewController_OnRegionalMessageReceived_Hook([&](auto*, void*) { ++regional_messages; }, preview,
                                                               nullptr);
          Require(global == (disable_global ? 0 : 1) && regional_messages == (disable_veil ? 0 : 1),
                  "each preview message source reads the current independent preference");
        }
  Config::Get().ui.disable_galaxy_chat = true;
  chat_native_availability_throws      = true;
  int unknown                          = 0;
  FullScreenChatViewController_OnDidChangeSelectedTab_Hook([&](auto*, int32_t, void*) { ++unknown; }, view, 0, nullptr);
  ChatPreviewController_OnPanelFocused_Hook(
      [&](auto*, int32_t index) {
        ++unknown;
        Require(index == 0, "unknown index retained");
      },
      preview, 0);
  Require(unknown == 2, "unavailable channel availability preserves native tab/swipe handlers");
  chat_native_availability_throws = false;
  chat_native_manager_present     = false;
  Require(!ChatTabs::Current(), "absent manager cannot invent channel indices");
  chat_native_manager_present = true;
  warp_root_failure           = true;
  Require(!ChatTabs::Current(), "failed manager custody cannot identify a disabled tab");
  warp_root_failure = false;
  Require(warp_roots.empty(), "channel callbacks release operation-local roots");
}

void ChatNativeAvatars()
{
  auto* message                     = reinterpret_cast<ChatMessageWidget*>(&chat_message);
  int   originals                   = 0;
  chat_profile_avatar.keep_in_cache = chat_admin_avatar.keep_in_cache = false;
  ChatMessageWidget_SetWidgetData_Hook(
      [&](auto* actual) {
        ++originals;
        Require(actual == message && chat_profile_avatar.keep_in_cache && chat_admin_avatar.keep_in_cache,
                "actual avatar hook sets both retention flags before exactly one native row binding");
      },
      message);
  Require(originals == 1, "successful avatar bookkeeping binds the native row once");
  chat_message.admin = &other_object.header;
  ChatMessageWidget_SetWidgetData_Hook([&](auto*) { ++originals; }, message);
  Require(originals == 2, "wrong-class optional portrait cannot suppress the native binding");
  warp_root_failure = true;
  ChatMessageWidget_SetWidgetData_Hook([&](auto*) { ++originals; }, message);
  warp_root_failure = false;
  Require(originals == 3 && warp_roots.empty(), "failed retention custody still binds once without leaking roots");
  chat_message.profile = chat_message.admin = nullptr;
  ChatMessageWidget_SetWidgetData_Hook([&](auto*) { ++originals; }, message);
  Require(originals == 4, "pooled unbound rows retain the original native call");
}

struct ChatNativeTitleProfile {
  Il2CppObject       header;
  Il2CppObject*      avatar = nullptr;
  PlayerTitleWidget* title  = nullptr;
};
struct ChatNativeTransform {
  Il2CppObject         header;
  ChatNativeTransform* parent = nullptr;
  unity::GameObject*   object = nullptr;
};
ChatNativeTitleProfile chat_native_profile{};
Il2CppObject chat_native_title{}, chat_native_title_object{}, chat_native_profile_object{}, chat_native_row_object{};
ChatNativeTransform chat_native_title_transform{}, chat_native_profile_transform{}, chat_native_row_transform{};
bool                chat_native_in_chat = true, chat_native_parent_throws = false, chat_native_title_active = true,
                    chat_native_requested_profile = false;
int                 chat_native_parent_calls = 0, chat_native_walk_calls = 0;
unity::Component*   ChatNativeParent(unity::Component* self, Il2CppReflectionType*, bool inactive, const MethodInfo*)
{
  ++chat_native_parent_calls;
  Require(inactive && ChatNativeRooted(self), "scoped title-parent lookup includes inactive rows with local custody");
  chat_native_requested_profile = reinterpret_cast<Il2CppObject*>(self) == &chat_native_title;
  if (chat_native_parent_throws)
    throw std::runtime_error("scoped parent lookup unavailable");
  return reinterpret_cast<Il2CppObject*>(self) == &chat_native_title
             ? reinterpret_cast<unity::Component*>(&chat_native_profile)
         : chat_native_in_chat ? reinterpret_cast<unity::Component*>(&chat_message)
                               : nullptr;
}
ChatNativeTransform* ChatNativeTransformOf(Il2CppObject* self, const MethodInfo*)
{ return self == &chat_native_title ? &chat_native_title_transform : &chat_native_profile_transform; }
unity::GameObject* ChatNativeTransformObject(ChatNativeTransform* self, const MethodInfo*)
{
  ++chat_native_walk_calls;
  return self->object;
}
ChatNativeTransform* ChatNativeTransformParent(ChatNativeTransform* self, const MethodInfo*)
{ return self->parent; }
unity::Component* ChatNativeComponent(Il2CppObject* self, Il2CppReflectionType*, const MethodInfo*)
{
  if (self == &chat_native_profile_object && chat_native_requested_profile)
    return reinterpret_cast<unity::Component*>(&chat_native_profile);
  if (self == &chat_native_row_object && chat_native_in_chat && !chat_native_requested_profile)
    return reinterpret_cast<unity::Component*>(&chat_message);
  return nullptr;
}
unity::GameObject* ChatNativeTitleObject(Il2CppObject*, const MethodInfo*)
{ return reinterpret_cast<unity::GameObject*>(&chat_native_title_object); }
void ChatNativeTitleActive(Il2CppObject* self, bool enabled, const MethodInfo*)
{
  Require(self == &chat_native_title_object && ChatNativeRooted(self),
          "title display touches only the rooted title object");
  chat_native_title_active = enabled;
}
void ChatNativeTitles()
{
  auto& title     = Define("Digit.Prime.PlayerAvatars", "PlayerTitleWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& transform = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &object_class);
  chat_profile_class.klass.instance_size = sizeof(ChatNativeTitleProfile);
  AddField(chat_profile_class, "_playerTitleWidget", title, offsetof(ChatNativeTitleProfile, title));
  AddMethod(component_class, "GetComponentInParent", reinterpret_cast<void*>(&ChatNativeParent), T(component_class),
            {T(type_class), T(bool_class)});
  auto& component_transform =
      AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&ChatNativeTransformOf), T(transform), {});
  AddProperty(component_class, "transform", &component_transform);
  auto& component_object = AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&ChatNativeTitleObject),
                                     T(game_object_class), {});
  AddProperty(component_class, "gameObject", &component_object);
  auto& transform_object = AddMethod(transform, "get_gameObject", reinterpret_cast<void*>(&ChatNativeTransformObject),
                                     T(game_object_class), {});
  AddProperty(transform, "gameObject", &transform_object);
  auto& parent =
      AddMethod(transform, "get_parent", reinterpret_cast<void*>(&ChatNativeTransformParent), T(transform), {});
  AddProperty(transform, "parent", &parent);
  // The shared runtime's unrelated GetComponent returns TMP_InputField. Replace
  // that metadata in this fresh process with Unity's actual Component result.
  for (auto* method : game_object_class.methods)
    if (std::string_view(method->name) == "GetComponent") {
      const_cast<MethodInfo*>(method)->return_type   = T(component_class);
      const_cast<MethodInfo*>(method)->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&ChatNativeComponent);
      const_cast<MethodInfo*>(method)->virtualMethodPointer =
          reinterpret_cast<Il2CppMethodPointer>(&ChatNativeComponent);
    }
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&ChatNativeTitleActive), T(void_class),
            {T(bool_class)});
  chat_native_title.klass        = &title.klass;
  chat_native_title_object.klass = chat_native_profile_object.klass = chat_native_row_object.klass =
      &game_object_class.klass;
  chat_native_profile = {
      {&chat_profile_class.klass}, nullptr, reinterpret_cast<PlayerTitleWidget*>(&chat_native_title)};
  chat_native_row_transform = {
      {&transform.klass}, nullptr, reinterpret_cast<unity::GameObject*>(&chat_native_row_object)};
  chat_native_profile_transform      = {{&transform.klass},
                                        &chat_native_row_transform,
                                        reinterpret_cast<unity::GameObject*>(&chat_native_profile_object)};
  chat_native_title_transform        = {{&transform.klass},
                                        &chat_native_profile_transform,
                                        reinterpret_cast<unity::GameObject*>(&chat_native_title_object)};
  auto* profile                      = reinterpret_cast<UserProfileWidget*>(&chat_native_profile);
  auto* title_widget                 = reinterpret_cast<PlayerTitleWidget*>(&chat_native_title);
  Config::Get().ui.show_player_title = false;
  int        originals               = 0;
  const auto original                = [&](auto*) {
    ++originals;
    chat_native_title_active = true;
  };
  UserProfileWidget_UpdatePlayerTitle_Hook(original, profile);
  Require(originals == 1 && !chat_native_title_active && chat_native_walk_calls == 0,
          "profile callback hides chat title after original using native scoped lookup");
  PlayerTitleWidget_Bind_Hook(original, title_widget);
  Require(originals == 2 && !chat_native_title_active, "substantive title bind also hides only its chat row");
  chat_native_in_chat = false;
  UserProfileWidget_UpdatePlayerTitle_Hook(original, profile);
  Require(originals == 3 && chat_native_title_active, "ordinary nonchat player profiles retain titles");
  chat_native_in_chat                = true;
  Config::Get().ui.show_player_title = true;
  PlayerTitleWidget_Bind_Hook(original, title_widget);
  Require(originals == 4 && chat_native_title_active, "enabled titles preserve the native bind");
  Config::Get().ui.show_player_title = false;
  chat_native_parent_throws          = true;
  UserProfileWidget_UpdatePlayerTitle_Hook(original, profile);
  Require(originals == 5 && !chat_native_title_active && chat_native_walk_calls > 0,
          "unavailable scoped API uses the retained bounded parent walk");
  chat_native_in_chat                  = false;
  chat_native_profile_transform.parent = &chat_native_profile_transform;
  const auto walks                     = chat_native_walk_calls;
  UserProfileWidget_UpdatePlayerTitle_Hook(original, profile);
  Require(originals == 6 && chat_native_title_active && chat_native_walk_calls - walks == 32,
          "a cyclic nonchat hierarchy terminates after the source's bounded walk");
  warp_root_failure = true;
  UserProfileWidget_UpdatePlayerTitle_Hook(original, profile);
  warp_root_failure = false;
  Require(originals == 7 && chat_native_title_active && warp_roots.empty(),
          "failed title custody preserves native display and releases all roots");
}

void ChatNativeNavigation()
{
  ChatNativeRuntime();
  for (const auto [action, mode] :
       {std::pair{Action::kShowChat, ChatViewMode::Fullscreen}, std::pair{Action::kShowChatSide1, ChatViewMode::Side},
        std::pair{Action::kShowChatSide2, ChatViewMode::Side}}) {
    fleet_native_keys = {action};
    const auto opens  = chat_native_opens;
    ChatNativeOpenShortcut();
    Require(chat_native_opens == opens + 1 && chat_native_mode == mode
                && chat_native_channel == ChatChannelCategory::Alliance,
            "full/side shortcuts set the requested native view then open Alliance once");
  }
  chat_native_side_open = true;
  ChatNativeOpenShortcut();
  Require(chat_native_focuses == 1 && chat_native_opens == 3,
          "open side chat focuses its existing input without reopening");
  chat_native_view.list = nullptr;
  ChatNativeOpenShortcut();
  Require(chat_native_focuses == 1 && chat_native_opens == 3,
          "missing side-chat input does not open a replacement view");
  chat_native_side_open   = false;
  chat_native_side_throws = true;
  ChatNativeOpenShortcut();
  Require(chat_native_opens == 3, "unknown side-chat state cannot authorize an open request");
  chat_native_side_throws = false;
  chat_native_mode_throws = true;
  ChatNativeOpenShortcut();
  Require(chat_native_opens == 3, "failed requested view mode cannot open an unintended view");
  chat_native_mode_throws = false;
  for (const auto [action, channel] : {std::pair{Action::kSelectChatGlobal, ChatChannelCategory::Global},
                                       std::pair{Action::kSelectChatAlliance, ChatChannelCategory::Alliance},
                                       std::pair{Action::kSelectChatPrivate, ChatChannelCategory::Private}}) {
    fleet_native_keys = {action};
    const auto opens  = chat_native_opens;
    Require(ChatNativeChannelShortcuts() && chat_native_opens == opens + 1 && chat_native_channel == channel,
            "each chat-channel shortcut issues exactly its requested native channel");
  }
  fleet_native_keys = {Action::kSelectChatGlobal, Action::kSelectChatAlliance, Action::kSelectChatPrivate};
  const auto opens  = chat_native_opens;
  Require(ChatNativeChannelShortcuts() && chat_native_opens == opens + 1
              && chat_native_channel == ChatChannelCategory::Global,
          "conflicting channel chords retain frozen Global/Alliance/Private precedence");
  chat_native_manager_present = false;
  Require(!ChatNativeChannelShortcuts(), "absent chat manager leaves channel dispatch unconsumed");
  chat_native_manager_present = true;
  warp_root_failure           = true;
  Require(!ChatNativeChannelShortcuts(), "failed manager custody cannot request a channel");
  warp_root_failure = false;
  fleet_native_keys.clear();
  Require(warp_roots.empty(), "navigation callbacks release all acquired manager/view/input roots");
}

void ChatNativeCases(std::string_view mode)
{
  if (mode == "chat-native-channels")
    ChatNativeChannels();
  else if (mode == "chat-native-avatars")
    ChatNativeAvatars();
  else if (mode == "chat-native-titles")
    ChatNativeTitles();
  else if (mode == "chat-native-navigation")
    ChatNativeNavigation();
  else
    Require(false, "unknown chat-native mode");
  std::cout << "Actual production chat consumer " << mode << " passed\n";
}
} // namespace
