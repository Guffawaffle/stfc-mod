#pragma once

// Actual cached-hit binding, chat-owner classifier and FleetActionShortcuts. Native raycast/parent bodies
// and a primary-action consumer are controlled; no chat message or live game action is sent.
namespace
{
struct ChatPointerHit {
  Il2CppObject       header;
  unity::GameObject* object = nullptr;
};
Il2CppObject                                      chat_pointer_list{}, chat_pointer_count_box{}, chat_pointer_rewards{};
ChatPointerHit                                    chat_pointer_hit{};
Il2CppObject                                      chat_pointer_messages{}, chat_pointer_preview{}, chat_pointer_full{};
unity::GameObject                                 chat_pointer_object{};
std::vector<unity::GameObject*>                   chat_pointer_hits;
std::map<const Il2CppType*, Il2CppReflectionType> chat_pointer_types;
const MethodInfo *chat_pointer_raycast = nullptr, *chat_pointer_count = nullptr, *chat_pointer_item = nullptr;
int32_t           chat_pointer_size  = 0;
int               chat_pointer_owner = 0, chat_pointer_queries = 0, chat_pointer_primary_calls = 0;
bool chat_pointer_query_throws = false, chat_pointer_parent_throws = false, chat_pointer_count_throws = false;

Il2CppObject* ChatPointerType(const Il2CppType* type)
{
  auto& reflection        = chat_pointer_types[type];
  reflection.object.klass = &type_class.klass;
  reflection.type         = type;
  return &reflection.object;
}
unity::Component* ChatPointerParent(unity::GameObject* object, Il2CppReflectionType* type, bool inactive,
                                    const MethodInfo*)
{
  Require(!inactive && HavenFixtureRooted(object), "only the rooted top hit's active parent hierarchy is queried");
  if (chat_pointer_parent_throws)
    throw Il2CppExceptionWrapper(&thrown);
  auto* wanted = il2cpp_class_from_type(type->type);
  auto* owner  = chat_pointer_owner == 1   ? &chat_pointer_messages
                 : chat_pointer_owner == 2 ? &chat_pointer_preview
                 : chat_pointer_owner == 3 ? &chat_pointer_full
                                           : nullptr;
  return object == &chat_pointer_object && owner && owner->klass == wanted ? reinterpret_cast<unity::Component*>(owner)
                                                                           : nullptr;
}
Il2CppObject* ChatPointerInvoke(const MethodInfo* method, void* receiver, void** args, Il2CppException** exception,
                                bool& handled)
{
  handled = method == chat_pointer_raycast || method == chat_pointer_count || method == chat_pointer_item;
  if (!handled)
    return nullptr;
  if ((method == chat_pointer_raycast && chat_pointer_query_throws)
      || (method == chat_pointer_count && chat_pointer_count_throws)) {
    *exception = &thrown;
    return nullptr;
  }
  if (method == chat_pointer_raycast) {
    ++chat_pointer_queries;
    Require(!receiver && args[0] == &event_system && HavenFixtureRooted(&event_system),
            "cached raycast uses the rooted current EventSystem");
    return &chat_pointer_list;
  }
  Require(receiver == &chat_pointer_list && HavenFixtureRooted(receiver), "raycast result list stays rooted");
  if (method == chat_pointer_count) {
    chat_pointer_size = static_cast<int32_t>(chat_pointer_hits.size());
    return &chat_pointer_count_box;
  }
  Require(*static_cast<int32_t*>(args[0]) == 0, "only the first sorted hit may own the click");
  chat_pointer_hit.object = chat_pointer_hits.front();
  return &chat_pointer_hit.header;
}
bool ChatPointerRewardActive(Il2CppObject*, const MethodInfo*)
{ return true; }
void ChatPointerRewardPrimary(Il2CppObject*, const MethodInfo*)
{ ++chat_pointer_primary_calls; }

void ChatPointerCases()
{
  auto& vector2  = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& raycasts = Define("Digit.Client.UI", "RaycastExtensions", IL2CPP_TYPE_CLASS, &object_class);
  auto& raycast  = Define("UnityEngine.EventSystems", "RaycastResult", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(void*));
  auto& list     = Define("PointerFixture", "RaycastList", IL2CPP_TYPE_CLASS, &object_class);
  raycast.klass.instance_size = sizeof(ChatPointerHit);
  AddField(raycast, "m_GameObject", game_object_class, offsetof(ChatPointerHit, object));
  auto& query = AddMethod(raycasts, "CachedRaycast", reinterpret_cast<void*>(&Never), T(list),
                          {T(event_system_class), T(vector2)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto& count = AddMethod(list, "get_Count", reinterpret_cast<void*>(&Never), T(int_class), {});
  auto& item  = AddMethod(list, "get_Item", reinterpret_cast<void*>(&Never), T(raycast), {T(int_class)});
  for (auto* method : {&query, &count, &item})
    method->invoker_method = &WarpInvokerSentinel;
  chat_pointer_raycast = &query;
  chat_pointer_count   = &count;
  chat_pointer_item    = &item;
  AddMethod(game_object_class, "GetComponentInParent", reinterpret_cast<void*>(&ChatPointerParent), T(component_class),
            {T(type_class), T(bool_class)});
  auto& messages =
      Define("Digit.Prime.Chat", "ChatMessageListLocalViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& preview = Define("Digit.Prime.Chat", "ChatPreviewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& full    = Define("Digit.Prime.Chat", "FullScreenChatViewController", IL2CPP_TYPE_CLASS, &component_class);
  chat_pointer_messages.klass   = &messages.klass;
  chat_pointer_preview.klass    = &preview.klass;
  chat_pointer_full.klass       = &full.klass;
  chat_pointer_object.klass     = &game_object_class.klass;
  chat_pointer_list.klass       = &list.klass;
  chat_pointer_count_box.klass  = &int_class.klass;
  chat_pointer_hit.header.klass = &raycast.klass;
  extra_value_box               = &chat_pointer_count_box;
  extra_value_payload           = &chat_pointer_size;
  tow_runtime_invoke_override   = &ChatPointerInvoke;
  type_object_override          = &ChatPointerType;
  auto& rewards =
      Define("Digit.Prime.Missions.UI", "AnimatedRewardsScreenViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(rewards, "IsActive", reinterpret_cast<void*>(&ChatPointerRewardActive), T(bool_class), {});
  AddMethod(rewards, "GoBackToLastSection", reinterpret_cast<void*>(&ChatPointerRewardPrimary), T(void_class), {});
  chat_pointer_rewards.klass           = &rewards.klass;
  fleet_native_widgets[&rewards.klass] = {&chat_pointer_rewards};
  selected                             = nullptr;
  has_event_system                     = true;
  ++input_frame;
  unity::Key::Poll();
  fleet_native_keys          = {Action::kActionPrimary};
  fleet_native_primary_mouse = true;
  chat_pointer_hits          = {&chat_pointer_object};
  for (int owner : {1, 2, 3}) {
    chat_pointer_owner = owner;
    const auto before  = chat_pointer_primary_calls;
    Require(!FleetActionShortcuts() && chat_pointer_primary_calls == before && warp_roots.empty(),
            "chat row, preview and full view own mouse primary without consuming native Update");
  }
  chat_pointer_owner         = 1;
  const auto queries         = chat_pointer_queries;
  fleet_native_primary_mouse = false;
  Require(FleetActionShortcuts() && chat_pointer_primary_calls == 1 && chat_pointer_queries == queries,
          "keyboard primary remains independent of pointer position and needs no raycast");
  fleet_native_primary_mouse = true;
  chat_pointer_hits.clear();
  Require(FleetActionShortcuts() && chat_pointer_primary_calls == 2, "empty world retains mouse primary");
  // Chat underneath an unrelated UI hit cannot claim its pointer.
  chat_pointer_hits = {&selected_object, &chat_pointer_object};
  Require(FleetActionShortcuts() && chat_pointer_primary_calls == 3, "occluded chat never steals another top hit");
  chat_pointer_hits = {&chat_pointer_object};
  for (bool* failure : {&chat_pointer_query_throws, &chat_pointer_count_throws, &chat_pointer_parent_throws}) {
    *failure = true;
    Require(!FleetActionShortcuts() && chat_pointer_primary_calls == 3 && warp_roots.empty(),
            "unavailable ownership cannot authorize mouse primary or leak roots");
    *failure = false;
  }
  warp_root_failure = true;
  Require(!FleetActionShortcuts() && chat_pointer_primary_calls == 3, "failed custody reserves mouse primary");
  warp_root_failure = false;
  chat_pointer_hits.clear();
  Require(FleetActionShortcuts() && chat_pointer_primary_calls == 4 && warp_roots.empty(),
          "next known world hit recovers ordinary primary dispatch");
  fleet_native_keys.clear();
  fleet_native_primary_mouse = false;
  fleet_native_widgets.clear();
  tow_runtime_invoke_override = nullptr;
  type_object_override        = nullptr;
  extra_value_box             = nullptr;
  extra_value_payload         = nullptr;
  std::cout
      << "Chat pointer production routing: scoped first hit, keyboard/world retention and failure refusal passed\n";
}
} // namespace
