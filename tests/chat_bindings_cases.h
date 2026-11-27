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

struct StreamerPreviewFixtureWidget {
  Il2CppObject   header;
  TextLocalizer* label;
  ChatMessage*   message = nullptr;
};
int streamer_chat_preview_refreshes = 0;
struct StreamerPreviewFixtureGraphic {
  Il2CppObject header;
  bool         enabled = true;
};
struct StreamerColourFixtureLabel {
  Il2CppObject header;
  Il2CppObject* graphic = nullptr;
  il2cpp::ArrayObject* colours = nullptr;
  il2cpp::ArrayObject* original = nullptr;
};

struct StreamerChatFixtureIdentity {
  Il2CppObject  header;
  Il2CppString* id    = nullptr;
  bool          fails = false;
};
StreamerChatFixtureIdentity streamer_chat_local{}, streamer_chat_sender{}, streamer_chat_other{};
Il2CppObject                streamer_chat_profile_manager{};
bool                        streamer_chat_manager_present = true;
std::array<Il2CppClass*, 1> streamer_chat_profile_interfaces{};

void StreamerChatIdentityRuntime(FakeClass& widget)
{
  auto& profile           = Define("Digit.PrimeServer.Models", "UserProfile", IL2CPP_TYPE_CLASS, &object_class);
  auto& message           = Define("Digit.PrimeServer.Models", "Message", IL2CPP_TYPE_CLASS, &object_class);
  auto& manager           = Define("Digit.Prime.PlayerProfile", "UserProfileManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& profile_interface = Define("Digit.PrimeServer.Models", "IUserProfile", IL2CPP_TYPE_CLASS, &object_class);
  profile_interface.klass.flags |= TYPE_ATTRIBUTE_INTERFACE;
  streamer_chat_profile_interfaces[0] = &profile_interface.klass;
  profile.klass.implementedInterfaces = streamer_chat_profile_interfaces.data();
  profile.klass.interfaces_count      = 1;
  AddMethod(profile, "get_UserId", reinterpret_cast<void*>(+[](StreamerChatFixtureIdentity* self, const MethodInfo*) {
              Require(ChatNativeRooted(self), "local profile remains rooted during ID classification");
              if (self->fails)
                throw Il2CppExceptionWrapper(&thrown);
              return self->id;
            }),
            T(string_class), {});
  AddMethod(message, "get_SenderId", reinterpret_cast<void*>(+[](StreamerChatFixtureIdentity* self, const MethodInfo*) {
              Require(ChatNativeRooted(self), "bound message remains rooted while reading its sender ID");
              if (self->fails)
                throw Il2CppExceptionWrapper(&thrown);
              return self->id;
            }),
            T(string_class), {});
  AddMethod(manager, "get_Instance", reinterpret_cast<void*>(+[](const MethodInfo*) {
              return streamer_chat_manager_present ? &streamer_chat_profile_manager : nullptr;
            }),
            T(manager), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(manager, "GetLocalUserProfile", reinterpret_cast<void*>(+[](Il2CppObject* self, const MethodInfo*) {
              Require(ChatNativeRooted(self), "native profile manager is rooted while acquiring the local profile");
              return &streamer_chat_local.header;
            }),
            T(profile), {});
  AddMethod(widget, "get_Context", reinterpret_cast<void*>(+[](StreamerPreviewFixtureWidget* self, const MethodInfo*) {
              return self->message;
            }),
            T(message), {});
  streamer_chat_local.header.klass    = &profile.klass;
  streamer_chat_other.header.klass    = &profile.klass;
  streamer_chat_sender.header.klass   = &message.klass;
  streamer_chat_profile_manager.klass = &manager.klass;
  streamer_chat_local.id              = il2cpp::NewString("local-account-id");
  streamer_chat_other.id              = il2cpp::NewString("other-account-id");
  streamer_chat_sender.id             = il2cpp::NewString("other-account-id");
  streamer_chat_manager_present       = true;
}

struct StreamerChatFixtureProfile {
  Il2CppObject   header;
  TextLocalizer* label   = nullptr;
  int32_t        style   = 1;
  ChatProfile*   profile = nullptr;
};
struct StreamerChatFixtureRow {
  Il2CppObject       header;
  UserProfileWidget* profile = nullptr;
  ChatMessage*       message = nullptr;
};
struct StreamerChatFixtureHeaderContext {
  Il2CppObject  header;
  Il2CppString* channel = nullptr;
  UserProfile*  profile = nullptr;
};
struct StreamerChatFixtureHeader {
  Il2CppObject        header;
  TextLocalizer*      label   = nullptr;
  ChatSectionContext* context = nullptr;
  bool fails = false;
};
il2cpp::ArrayObject*                                 streamer_chat_name_arguments   = nullptr;
il2cpp::ArrayObject*                                 streamer_chat_transient_arguments = nullptr;
il2cpp::ArrayObject*                                 streamer_chat_header_arguments = nullptr;
std::map<TextLocalizer*, std::vector<Il2CppObject*>> streamer_chat_rendered;
std::vector<TextLocalizer*> streamer_chat_cover_attempts;
int                                                  streamer_chat_name_refreshes = 0;

void StreamerChatFixtureRender(TextLocalizer* label, bool, il2cpp::ArrayObject* arguments)
{
  if (!arguments)
    return;
  auto* input = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(arguments);
  streamer_chat_rendered[label].assign(input->data(), input->data() + input->size());
}

void StreamerChatFixtureRefreshName(StreamerChatFixtureProfile* self, const MethodInfo*)
{
  ++streamer_chat_name_refreshes;
  // SetContext can first send empty parameters before the native final five-slot identity render.
  TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, self->label, false, nullptr);
  if (streamer_chat_transient_arguments)
    TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, self->label, false,
                                                          streamer_chat_transient_arguments);
  TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, self->label, true,
                                                        streamer_chat_name_arguments);
}

void ChatNativeStreamerRows(FakeClass& label_class, FakeClass& colour_class, FakeClass& multi_class,
                           TextLocalizer* label, TextLocalizer* header_label)
{
  auto& state     = StreamerChat();
  state           = StreamerChatState{};
  state.installed = state.formatter = true;
  state.game_thread                 = std::this_thread::get_id();
  auto& ui                          = Config::Get().ui;
  ui.streamer_mode                  = true;
  ui.streamer_hide_my_name = ui.streamer_hide_my_alliance = false;
  ui.streamer_hide_chat_sender_names = ui.streamer_hide_chat_alliances = false;
  auto& model                                             = Fake(UserProfile::Class::Raw());
  auto& model_interface                                   = Fake(ChatProfile::Class::Raw());
  auto& message_model                                     = Fake(ChatMessage::Class::Raw());
  AddField(chat_profile_class, "_name", label_class, offsetof(StreamerChatFixtureProfile, label));
  AddField(chat_profile_class, "_nameStyle", int_class, offsetof(StreamerChatFixtureProfile, style));
  AddMethod(chat_profile_class, "get_Context",
            reinterpret_cast<void*>(+[](StreamerChatFixtureProfile* self, const MethodInfo*) { return self->profile; }),
            T(model_interface), {});
  AddMethod(chat_profile_class, "UpdatePlayerName", reinterpret_cast<void*>(&StreamerChatFixtureRefreshName),
            T(void_class), {});
  AddField(chat_message_class, "_userProfileWidget", chat_profile_class, offsetof(StreamerChatFixtureRow, profile));
  AddMethod(chat_message_class, "get_Context",
            reinterpret_cast<void*>(+[](StreamerChatFixtureRow* self, const MethodInfo*) { return self->message; }),
            T(message_model), {});
  StreamerChatFixtureProfile profile{
      {&chat_profile_class.klass}, label, 3, reinterpret_cast<ChatProfile*>(&streamer_chat_other)};
  StreamerChatFixtureRow row{{&chat_message_class.klass},
                             reinterpret_cast<UserProfileWidget*>(&profile),
                             reinterpret_cast<ChatMessage*>(&streamer_chat_sender)};
  auto*                  receiver = reinterpret_cast<ChatMessageWidget*>(&row);
  auto*                  args = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 5));
  std::array<Il2CppObject*, 5> values{};
  for (size_t i = 0; i < values.size(); ++i) {
    values[i] = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native identity parameter"));
    args->Set(i, values[i]);
  }
  streamer_chat_name_arguments = reinterpret_cast<il2cpp::ArrayObject*>(args);
  const auto expect            = [&](TextLocalizer* target, std::span<const bool> mask) {
    const auto& output = streamer_chat_rendered.at(target);
    Require(output.size() == mask.size(), "native rendering retains its parameter shape");
    for (size_t i = 0; i < mask.size(); ++i)
      Require(
          (*args)[i] == values[i]
              && (mask[i] ? il2cpp::Equals(reinterpret_cast<Il2CppString*>(output[i]), "") : output[i] == values[i]),
          "chat masks only requested identity slots and preserves native source arguments");
  };
  streamer_chat_sender.id = streamer_chat_other.id;
  const auto off_refreshes = streamer_chat_name_refreshes;
  for (const bool enabled : {false, true}) {
    ui.streamer_mode = enabled; // Includes Player ID-only privacy with no chat protection selected.
    int originals    = 0;
    ChatMessageWidget_OnDidBindContext_StreamerHook([&](auto*) { ++originals; }, receiver);
    Require(originals == 1 && state.views.size() == 1 && state.labels.empty()
                && streamer_chat_name_refreshes == off_refreshes,
            "disabled and ID-only privacy retain only the weak owner without refreshing native identity text");
  }
  ui.streamer_hide_chat_sender_names = true;
  StreamerChatFrame();
  Require(!state.labels.empty() && state.views.front()->healthy,
          "enabling privacy discovers the label of an existing passively observed row without a native rebind");
  for (int32_t style = 1; style <= 7; ++style)
    for (const bool names : {false, true})
      for (const bool alliances : {false, true}) {
        profile.style                      = style;
        ui.streamer_hide_chat_sender_names = names;
        ui.streamer_hide_chat_alliances    = alliances;
        StreamerChatFrame();
        int originals                      = 0;
        ChatMessageWidget_OnDidBindContext_StreamerHook(
            [&](auto*) {
              ++originals;
              StreamerChatFixtureRefreshName(&profile, nullptr);
            },
            receiver);
        std::array<bool, 5> mask{};
        const size_t        name = style == 3 || style == 4 || style == 6 ? 1 : 0;
        mask[name]               = names;
        if (style == 3 || style == 6)
          mask[0] = mask[2] = alliances;
        else if (style == 4)
          mask[0] = alliances;
        else if (style == 7)
          mask[2] = mask[3] = alliances;
        expect(label, mask);
        Require(originals == 1 && state.views.size() == 1 && state.views.front()->healthy,
                "all native name styles use one pooled row and run its original bind once");
      }
  profile.style                      = 3;
  ui.streamer_hide_chat_sender_names = ui.streamer_hide_chat_alliances = false;
  ui.streamer_hide_my_name                                             = true;
  streamer_chat_sender.id                                              = il2cpp::NewString("local-account-id");
  ChatMessageWidget_OnDidBindContext_StreamerHook([](auto*) {}, receiver);
  expect(label, std::array{false, true, false, false, false});
  Require(state.views.front()->healthy,
          "matching ID content masks own messages even when names otherwise remain visible");
  StreamerChatFrame();
  const auto before            = streamer_chat_name_refreshes;
  ui.streamer_hide_my_name     = false;
  ui.streamer_hide_my_alliance = true;
  StreamerChatFrame();
  expect(label, std::array{true, false, true, false, false});
  Require(streamer_chat_name_refreshes == before + 1,
          "changing independent personal protections refreshes existing rows without the broad chat toggle changing");
  streamer_chat_sender.id = streamer_chat_other.id;
  ChatMessageWidget_OnDidBindContext_StreamerHook([](auto*) {}, receiver);
  expect(label, std::array{false, false, false, false, false});
  // Native asynchronous profile refreshes reuse the exact tracked label and read the current message again.
  ui.streamer_hide_my_name     = true;
  ui.streamer_hide_my_alliance = false;
  streamer_chat_sender.id      = streamer_chat_local.id;
  StreamerChatFixtureRefreshName(&profile, nullptr);
  expect(label, std::array{false, true, false, false, false});
  streamer_chat_sender.id = nullptr;
  StreamerChatFixtureRefreshName(&profile, nullptr);
  expect(label, std::array{false, true, false, false, false});
  Require(!state.views.front()->healthy, "unknown current sender remains masked and reports degraded privacy");
  streamer_chat_sender.id = streamer_chat_other.id;
  StreamerChatFixtureRefreshName(&profile, nullptr);
  expect(label, std::array{false, false, false, false, false});
  Require(state.views.front()->healthy,
          "a positively identified other sender restores native presentation on a reused label");
  streamer_chat_sender.id   = streamer_chat_local.id;
  streamer_chat_local.fails = true;
  StreamerChatFixtureRefreshName(&profile, nullptr);
  expect(label, std::array{false, true, false, false, false});
  Require(!state.views.front()->healthy, "unreadable local ID is Unknown rather than permission to show the name");
  streamer_chat_local.fails = false;
  ui.streamer_mode          = false;
  StreamerChatFrame();
  expect(label, std::array{false, false, false, false, false});

  auto* colours = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 2));
  colours->Set(0, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native color A")));
  colours->Set(1, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native color B")));
  auto* expanded = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 7));
  for (size_t i = 0; i < 7; ++i)
    expanded->Set(i, i < 2 ? (*colours)[i] : values[i - 2]);
  StreamerColourFixtureLabel colour_label{{&colour_class.klass}, nullptr,
                                           reinterpret_cast<il2cpp::ArrayObject*>(colours),
                                           streamer_chat_name_arguments};
  auto* colour_target = reinterpret_cast<TextLocalizer*>(&colour_label);
  profile.label = colour_target;
  ui.streamer_mode = true;
  ui.streamer_hide_chat_sender_names = false;
  ui.streamer_hide_chat_alliances = true;
  ui.streamer_hide_my_name = ui.streamer_hide_my_alliance = false;
  streamer_chat_name_arguments = reinterpret_cast<il2cpp::ArrayObject*>(expanded);
  ChatMessageWidget_OnDidBindContext_StreamerHook([](auto*) {}, receiver);
  const auto& coloured = streamer_chat_rendered.at(colour_target);
  for (size_t i = 0; i < 7; ++i)
    Require((i == 2 || i == 4) ? il2cpp::Equals(reinterpret_cast<Il2CppString*>(coloured[i]), "")
                               : coloured[i] == (*expanded)[i],
            "Colour full-row masking shifts only native alliance slots and preserves prefix colors/name");
  Require(state.views.front()->healthy, "qualified color-prefix and original-suffix references permit selective rendering");
  colour_label.original = reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(il2cpp::ObjectClass(), 5));
  StreamerChatFixtureRefreshName(&profile, nullptr);
  Require(!state.views.front()->healthy && streamer_chat_rendered.at(colour_target)[0] == (*expanded)[0],
          "a stale non-color argument cache declines selective formatting and reports degradation");
  // MultiTextLocalizer forwards the original arguments rather than adding a prefix.
  ChatNativeReference multi_label{{&multi_class.klass}};
  profile.label = reinterpret_cast<TextLocalizer*>(&multi_label);
  streamer_chat_name_arguments = reinterpret_cast<il2cpp::ArrayObject*>(args);
  ui.streamer_hide_chat_sender_names = true;
  ui.streamer_hide_chat_alliances = false;
  ChatMessageWidget_OnDidBindContext_StreamerHook([](auto*) {}, receiver);
  expect(profile.label, std::array{false, true, false, false, false});
  Require(state.views.front()->healthy, "Multi-like forwarding keeps the ordinary native five-slot schema");
  profile.label = label;
  ChatMessageWidget_OnDidBindContext_StreamerHook([](auto*) {}, receiver);
  streamer_chat_transient_arguments = reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(il2cpp::ObjectClass(), 2));
  StreamerChatFixtureRefreshName(&profile, nullptr);
  expect(label, std::array{false, true, false, false, false});
  Require(state.views.front()->healthy,
          "the final qualified render clears transient unsupported SetContext degradation on the same native update");
  streamer_chat_transient_arguments = nullptr;

  auto& context_class = Define("Digit.Prime.Chat", "ChatSectionContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& header_class =
      Define("Digit.Prime.Chat", "FullScreenChatViewController", IL2CPP_TYPE_CLASS, &unity_object_class);
  AddField(context_class, "ChannelID", string_class, offsetof(StreamerChatFixtureHeaderContext, channel));
  AddField(context_class, "ChatUserProfile", model, offsetof(StreamerChatFixtureHeaderContext, profile));
  AddField(header_class, "_headerTitle", label_class, offsetof(StreamerChatFixtureHeader, label));
  AddMethod(header_class, "get_CanvasContext",
            reinterpret_cast<void*>(+[](StreamerChatFixtureHeader* self, const MethodInfo*) {
              if (self->fails)
                throw Il2CppExceptionWrapper(&thrown);
              return self->context;
            }),
            T(context_class), {});
  AddMethod(header_class, "RefreshContent",
            reinterpret_cast<void*>(+[](StreamerChatFixtureHeader* self, const MethodInfo*) {
              TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, self->label, false,
                                                                    streamer_chat_header_arguments);
            }),
            T(void_class), {});
  StreamerChatFixtureHeaderContext header_context{{&context_class.klass},
                                                  il2cpp::NewString("private-channel"),
                                                  reinterpret_cast<UserProfile*>(&streamer_chat_other)};
  StreamerChatFixtureHeader        header{
      {&header_class.klass}, header_label, reinterpret_cast<ChatSectionContext*>(&header_context)};
  ui.streamer_mode         = true;
  ui.streamer_hide_my_name = ui.streamer_hide_my_alliance = false;
  for (const size_t count : {1, 2})
    for (const bool names : {false, true})
      for (const bool alliances : {false, true}) {
        ui.streamer_hide_chat_sender_names = names;
        ui.streamer_hide_chat_alliances    = alliances;
        auto* header_args = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), count));
        for (size_t i = 0; i < count; ++i)
          header_args->Set(i, values[i]);
        streamer_chat_header_arguments = reinterpret_cast<il2cpp::ArrayObject*>(header_args);
        FullScreenChatViewController_RefreshContent_Hook(
            [&](auto*) {
              TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, header_label, false,
                                                                    streamer_chat_header_arguments);
            },
            reinterpret_cast<FullScreenChatViewController*>(&header));
        std::array<bool, 2> mask{count == 1 ? names : alliances, names};
        expect(header_label, {mask.data(), count});
        Require(state.views.back()->healthy,
                "private header preserves independently selected name and alliance fields");
      }
  ui.streamer_hide_chat_sender_names = true;
  ui.streamer_hide_chat_alliances = false;
  auto* private_args = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 2));
  private_args->Set(0, values[0]);
  private_args->Set(1, values[1]);
  streamer_chat_header_arguments = reinterpret_cast<il2cpp::ArrayObject*>(private_args);
  header.fails = true;
  streamer_chat_cover_attempts.clear();
  FullScreenChatViewController_RefreshContent_Hook([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, header_label, false,
                                                          streamer_chat_header_arguments);
  }, reinterpret_cast<FullScreenChatViewController*>(&header));
  Require(!state.views.back()->healthy
              && std::ranges::find(streamer_chat_cover_attempts, header_label) != streamer_chat_cover_attempts.end()
              && streamer_chat_rendered.at(header_label)[0] == values[0]
              && streamer_chat_rendered.at(header_label)[1] == values[1],
          "unavailable header context covers the exact label and never guesses the private two-slot schema");
  header.fails = false;
  // A natural async render can recover with unchanged policy after native context becomes readable again.
  TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, header_label, false,
                                                        streamer_chat_header_arguments);
  expect(header_label, std::array{false, true});
  Require(state.views.back()->healthy, "a positively private async header render clears unavailable-context degradation");
  StreamerChatFrame();
  header.fails = true;
  ui.streamer_mode = false;
  StreamerChatFrame();
  Require(!state.views.back()->healthy && state.last_policy.Any(),
          "an unavailable private header keeps native restoration pending after privacy is disabled");
  header.fails = false;
  StreamerChatFrame();
  expect(header_label, std::array{false, false});
  Require(state.views.back()->healthy && !state.last_policy.Any(),
          "a recovered header restores original identity arguments without another policy change or native refresh");
  ui.streamer_mode = true;
  StreamerChatFrame();
  header_context.channel = il2cpp::NewString("");
  auto* public_args = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 1));
  auto* category = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("Alliance Chat"));
  public_args->Set(0, category);
  streamer_chat_header_arguments = reinterpret_cast<il2cpp::ArrayObject*>(public_args);
  streamer_chat_cover_attempts.clear();
  FullScreenChatViewController_RefreshContent_Hook([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook(StreamerChatFixtureRender, header_label, false,
                                                          streamer_chat_header_arguments);
  }, reinterpret_cast<FullScreenChatViewController*>(&header));
  Require(state.views.back()->healthy && streamer_chat_cover_attempts.empty()
              && streamer_chat_rendered.at(header_label)[0] == category && !state.labels.contains(header_label),
          "a confirmed non-private channel header restores native text and retires its privacy label index");
  ChatNativeReference replacement{{&label_class.klass}};
  auto&               pooled = *state.views.front();
  pooled.label.Reset(); // The old managed wrapper may disappear before the native owner is rebound.
  Require(RememberStreamerChatLabel(pooled, reinterpret_cast<TextLocalizer*>(&replacement))
              && !state.labels.contains(label),
          "an expired weak label cannot leave its old raw index key behind during pooled rebinding");
  Require(RememberStreamerChatLabel(pooled, label)
              && !state.labels.contains(reinterpret_cast<TextLocalizer*>(&replacement)),
          "restoring a pooled native label removes the replacement index entry");
  state = StreamerChatState{};
  streamer_chat_rendered.clear();
  Require(warp_roots.empty(), "chat ownership and rendering fixtures release their temporary roots");
}

void ChatNativeStreamerPreview()
{
  auto& widget  = Define("Digit.Prime.Chat", "ChatPreviewMessageWidget", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& label   = Define("Digit.Client.UI", "TextLocalizer", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& graphic = Define("Fixture", "StreamerPreviewGraphic", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& colour_class = Define("Digit.Client.UI", "ColourTextLocalizer", IL2CPP_TYPE_CLASS, &label);
  auto& multi_class = Define("Digit.Client.UI", "MultiTextLocalizer", IL2CPP_TYPE_CLASS, &label);
  AddField(colour_class, "_colorHexes", object_array_class, offsetof(StreamerColourFixtureLabel, colours));
  AddField(colour_class, "_noneColourArgs", object_array_class, offsetof(StreamerColourFixtureLabel, original));
  StreamerChatIdentityRuntime(widget);
  AddField(widget, "_text", label, offsetof(StreamerPreviewFixtureWidget, label));
  AddMethod(
      widget, "SetWidgetData", reinterpret_cast<void*>(+[](StreamerPreviewFixtureWidget* self, const MethodInfo*) {
        ++streamer_chat_preview_refreshes;
        ChatPreviewMessageWidget_SetWidgetData_Hook([](auto*) {}, reinterpret_cast<ChatPreviewMessageWidget*>(self));
      }),
      T(void_class), {});
  auto& target =
      AddMethod(label, "get_Target",
                reinterpret_cast<void*>(+[](ChatNativeReference* self, const MethodInfo*) { return self->value; }),
                T(graphic), {});
  AddProperty(label, "Target", &target);
  AddMethod(label, "get_gameObject", reinterpret_cast<void*>(+[](TextLocalizer* self, const MethodInfo*) {
              streamer_chat_cover_attempts.push_back(self);
              return static_cast<unity::GameObject*>(nullptr);
            }), T(game_object_class), {});

  AddMethod(
      graphic, "get_enabled",
      reinterpret_cast<void*>(+[](StreamerPreviewFixtureGraphic* self, const MethodInfo*) { return self->enabled; }),
      T(bool_class), {});
  StreamerPreviewFixtureGraphic rendered{{&graphic.klass}};
  ChatNativeReference           text{{&label.klass}, &rendered.header}, unrelated{{&label.klass}, &rendered.header};
  StreamerPreviewFixtureWidget  row{{&widget.klass}, reinterpret_cast<TextLocalizer*>(&text)};
  row.message                            = reinterpret_cast<ChatMessage*>(&streamer_chat_sender);
  auto*                         receiver = reinterpret_cast<ChatPreviewMessageWidget*>(&row);
  auto&                         state    = StreamerChat();
  state.installed                        = true;
  state.game_thread                      = std::this_thread::get_id();
  auto& ui                               = Config::Get().ui;
  ui.streamer_mode                       = true;
  ui.streamer_hide_chat_sender_names = ui.streamer_hide_chat_alliances = true;
  ui.streamer_hide_my_name = ui.streamer_hide_my_alliance                    = false;
  const auto bind                                                            = [&](auto original) {
    ChatPreviewMessageWidget_SetWidgetData_Hook(original, receiver);
    return state.views.front()->healthy;
  };

  for (const size_t count : {4, 5}) {
    auto* source = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), count));
    auto* input  = reinterpret_cast<il2cpp::ArrayObject*>(source);
    std::array<Il2CppObject*, 5> values{};
    for (size_t index = 0; index < count; ++index) {
      values[index] = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native parameter"));
      source->Set(index, values[index]);
    }
    int formats = 0;
    Require(bind([&](auto*) {
              TextLocalizer_SetLocalTextParameters_StreamerChatHook(
                  [&](auto*, bool parse_id, auto* output) {
                    Require(output == input, "unrelated native formatting passes through unchanged");
                  },
                  reinterpret_cast<TextLocalizer*>(&unrelated), true, input);
              TextLocalizer_SetLocalTextParameters_StreamerChatHook(
                  [&](auto*, bool parse_id, auto* output) {
                    ++formats;
                    Require(parse_id, "shared formatter preserves the native parse-ID flag");
                    Require(output != input, "streamer preview never mutates the original native array");
                    auto* masked = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(output);
                    for (size_t index = 0; index < count; ++index) {
                      const bool identity = index > 0 && index < count - 2;
                      Require((*source)[index] == values[index]
                                  && (identity ? il2cpp::Equals(reinterpret_cast<Il2CppString*>((*masked)[index]), "")
                                               : (*masked)[index] == values[index]),
                              "preview replaces only its sender and optional alliance, keeping the color and body");
                    }
                  },
                  row.label, true, input);
            }) && formats == 1,
            "native preview reports success after rewriting the exact formatter inputs");
  }
  for (const auto subject :
       {streamer_mode::Subject::Self, streamer_mode::Subject::Other, streamer_mode::Subject::Unknown})
    for (const bool names : {false, true})
      for (const bool alliances : {false, true})
        for (const bool my_name : {false, true})
          for (const bool my_alliance : {false, true}) {
            ui.streamer_hide_chat_sender_names = names;
            ui.streamer_hide_chat_alliances    = alliances;
            ui.streamer_hide_my_name           = my_name;
            ui.streamer_hide_my_alliance       = my_alliance;
            streamer_chat_sender.id = subject == streamer_mode::Subject::Self    ? streamer_chat_local.id
                                      : subject == streamer_mode::Subject::Other ? il2cpp::NewString("other-account-id")
                                                                                 : nullptr;
            const bool hide_name    = names || (my_name && subject != streamer_mode::Subject::Other);
            const bool hide_alliance = alliances || (my_alliance && subject != streamer_mode::Subject::Other);
            auto*      input = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 5));
            std::array<Il2CppObject*, 5> values{};
            for (size_t i = 0; i < 5; ++i) {
              values[i] = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native parameter"));
              input->Set(i, values[i]);
            }
            int        originals = 0;
            const bool healthy   = bind([&](auto*) {
              TextLocalizer_SetLocalTextParameters_StreamerChatHook(
                  [&](auto*, bool parse_id, auto* output) {
                    ++originals;
                    auto* result = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(output);
                    for (size_t i = 0; i < 5; ++i) {
                      const bool masked = (i == 1 && hide_alliance) || (i == 2 && hide_name);
                      Require((*input)[i] == values[i]
                                  && (masked ? il2cpp::Equals(reinterpret_cast<Il2CppString*>((*result)[i]), "")
                                             : (*result)[i] == values[i]),
                              "independent native preview masks preserve every unselected name, tag, color and body");
                    }
                  },
                  row.label, true, reinterpret_cast<il2cpp::ArrayObject*>(input));
            });
            Require(originals == 1, "each native preview formatter is called exactly once");
            Require(healthy == (subject != streamer_mode::Subject::Unknown || (!my_name && !my_alliance)),
                    "unknown ownership conservatively masks selected personal fields and reports degradation");
          }
  streamer_chat_sender.id            = il2cpp::NewString("other-account-id");
  ui.streamer_hide_chat_sender_names = ui.streamer_hide_chat_alliances = true;
  ui.streamer_hide_my_name = ui.streamer_hide_my_alliance = false;
  auto* bare = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 4));
  auto* prefix = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 1));
  auto* expanded = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(il2cpp::ObjectClass(), 5));
  prefix->Set(0, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native prefix color")));
  expanded->Set(0, (*prefix)[0]);
  for (size_t i = 0; i < 4; ++i) {
    bare->Set(i, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("native preview parameter")));
    expanded->Set(i + 1, (*bare)[i]);
  }
  StreamerColourFixtureLabel coloured{{&colour_class.klass}, &rendered.header,
                                       reinterpret_cast<il2cpp::ArrayObject*>(prefix),
                                       reinterpret_cast<il2cpp::ArrayObject*>(bare)};
  auto* plain_label = row.label;
  row.label = reinterpret_cast<TextLocalizer*>(&coloured);
  ui.streamer_hide_chat_sender_names = false;
  ui.streamer_hide_chat_alliances = true;
  Require(bind([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook([&](auto*, bool parse_id, auto* output) {
      Require(!parse_id, "Colour preview forwarding preserves the native false parse-ID flag");
      auto* result = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(output);
      for (size_t i = 0; i < 5; ++i)
        Require((*result)[i] == (*expanded)[i],
                "four preview arguments plus one color cannot be mistaken for a bare five-argument alliance preview");
    }, row.label, false, reinterpret_cast<il2cpp::ArrayObject*>(expanded));
  }), "the qualified Colour prefix is decoded before choosing the native preview arity");
  ui.streamer_hide_chat_sender_names = true;
  ui.streamer_hide_chat_alliances = false;
  Require(bind([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook([&](auto*, bool, auto* output) {
      auto* result = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(output);
      for (size_t i = 0; i < 5; ++i)
        Require(i == 2 ? il2cpp::Equals(reinterpret_cast<Il2CppString*>((*result)[i]), "")
                       : (*result)[i] == (*expanded)[i],
                "prefixed preview name masking preserves every color and the native message body");
    }, row.label, false, reinterpret_cast<il2cpp::ArrayObject*>(expanded));
  }), "Colour previews retain selective name masking");
  expanded->Set(0, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("foreign prefix")));
  Require(!bind([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook([&](auto*, bool, auto* output) {
      Require(output == reinterpret_cast<il2cpp::ArrayObject*>(expanded),
              "malformed prefix fallback does not rewrite native arguments using a guessed bare schema");
    }, row.label, false, reinterpret_cast<il2cpp::ArrayObject*>(expanded));
  }), "malformed color-prefix references retain coarse protection and degraded status");
  expanded->Set(0, (*prefix)[0]);
  coloured.original = reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(il2cpp::ObjectClass(), 4));
  Require(!bind([&](auto*) {
    TextLocalizer_SetLocalTextParameters_StreamerChatHook([](auto*, bool, auto*) {}, row.label, false,
                                                         reinterpret_cast<il2cpp::ArrayObject*>(expanded));
  }), "a stale original-suffix cache cannot qualify the overlapping four-plus-one shape");
  for (auto* colors : {static_cast<il2cpp::ArrayObject*>(nullptr),
                       reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(il2cpp::ObjectClass(), 0))}) {
    coloured.colours = colors;
    Require(bind([&](auto*) {
      TextLocalizer_SetLocalTextParameters_StreamerChatHook([&](auto*, bool, auto* output) {
        auto* result = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(output);
        Require(il2cpp::Equals(reinterpret_cast<Il2CppString*>((*result)[1]), "")
                    && (*result)[3] == (*bare)[3],
                "an unexpanded Colour preview masks its name and preserves the body");
      }, row.label, false, reinterpret_cast<il2cpp::ArrayObject*>(bare));
    }), "null and empty color arrays skip their stale original-argument cache");
  }
  row.label = plain_label;
  ui.streamer_hide_chat_sender_names = ui.streamer_hide_chat_alliances = true;
  Require(!bind([](auto*) {}), "a visible preview with no intercepted formatter is degraded");
  rendered.enabled = false;
  Require(bind([](auto*) {}), "the disabled native preview graphic needs no formatter");
  streamer_chat_sender.id = nullptr;
  ui.streamer_hide_my_name = true;
  Require(bind([](auto*) {}), "a disabled native preview contains no visible identity even when its sender is unknown");
  streamer_chat_sender.id = streamer_chat_other.id;
  ui.streamer_hide_my_name = false;
  rendered.enabled     = true;
  auto* original_label = row.label;
  row.label            = nullptr;
  Require(!bind([](auto*) {}), "a missing preview label cannot report verified privacy");
  row.label                      = original_label;
  StreamerChatFrame();
  const auto preview_refreshes   = streamer_chat_preview_refreshes;
  Config::Get().ui.streamer_mode = false;
  int off_originals              = 0;
  ChatPreviewMessageWidget_SetWidgetData_Hook([&](auto*) { ++off_originals; }, receiver);
  Require(off_originals == 1 && streamer_chat_preview_refreshes == preview_refreshes,
          "turning Streamer Mode off leaves native previews alone");
  StreamerChatFrame();
  Require(state.views.front()->healthy && !state.last_policy.Any()
              && streamer_chat_preview_refreshes == preview_refreshes + 1,
          "the frame handler restores a previously degraded preview after its successful native Off refresh");
  state = StreamerChatState{};
  Require(warp_roots.empty(), "preview fixture releases all temporary roots");
  ChatNativeStreamerRows(label, colour_class, multi_class, row.label, reinterpret_cast<TextLocalizer*>(&unrelated));
  std::cout << "Streamer Mode: exact preview formatting, visible failure and disabled preview passed\n";
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
  else if (mode == "chat-native-streamer")
    ChatNativeStreamerPreview();
  else
    Require(false, "unknown chat-native mode");
  std::cout << "Actual production chat consumer " << mode << " passed\n";
}
} // namespace
