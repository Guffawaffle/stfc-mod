// Local Windows client263 science instrumentation. Observe only; never change claim/UI state.
#include "patches/claim_trace.h"
#include "patches/key.h"
#include "patches/screen_update_hook.h"
#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using Json  = nlohmann::json;
using Clock = std::chrono::steady_clock;
struct Target {
  const char *            assembly, *ns, *cls, *method;
  int                     arguments;
  uintptr_t               rva;
  size_t                  windowSize;
  std::array<uint8_t, 32> window;
};
#include "claim_trace_targets.h"
std::array<const MethodInfo*, std::size(kTargets)> methods{};
std::shared_ptr<spdlog::logger>                    logger;
std::atomic_bool                                   ready{false};
std::atomic_uint64_t                               nextSpan{1};
const auto                                         started = Clock::now();
std::mutex                                         stateMutex;
Il2CppGCHandle                                     latestClaim{}, latestReward{};
Il2CppGCHandle                                     latestChest{}, latestMessage{};
Il2CppGCHandle                                     latestSummary{};
Clock::time_point                                  activeUntil{}, nextPulse{}, rateStart{};
uint64_t                                           eventSequence{}, suppressed{};
size_t                                             rateCount{};
// Only valid during the original dispatch call on this thread; never retained across frames.
thread_local void* dispatchJob{};
struct DispatchScope {
  void* previous;
  explicit DispatchScope(void* job)
      : previous(std::exchange(dispatchJob, job))
  {
  }
  ~DispatchScope()
  { dispatchJob = previous; }
};

Il2CppClass* Resolve(const char* assembly, const char* ns, const char* name)
{
  auto* domain = il2cpp_domain_get();
  auto* loaded = domain ? il2cpp_domain_assembly_open(domain, assembly) : nullptr;
  auto* image  = loaded ? il2cpp_assembly_get_image(loaded) : nullptr;
  return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
}

// Read only allowlisted instance fields with compatible storage. Missing metadata is unknown, never false/zero.
FieldInfo* Field(void* object, const char* name)
{
  if (!object)
    return nullptr;
  auto* cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(object));
  for (auto* parent = cls; parent; parent = il2cpp_class_get_parent(parent)) {
    auto* field = il2cpp_class_get_field_from_name(parent, name);
    if (field && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC) && field->offset >= sizeof(Il2CppObject)
        && !field->type->byref)
      return field;
  }
  return nullptr;
}

template <typename T> bool Read(void* object, const char* name, T& result)
{
  auto* field = Field(object, name);
  if (!field)
    return false;
  auto  type       = il2cpp_type_get_type(field->type);
  auto* fieldClass = il2cpp_class_from_type(field->type);
  bool  compatible = false;
  if constexpr (std::is_same_v<T, bool>)
    compatible = type == IL2CPP_TYPE_BOOLEAN;
  if constexpr (std::is_same_v<T, int64_t>)
    compatible = type == IL2CPP_TYPE_I8;
  if constexpr (std::is_same_v<T, float>)
    compatible = type == IL2CPP_TYPE_R4;
  if constexpr (std::is_same_v<T, int32_t>) {
    compatible = type == IL2CPP_TYPE_I4;
    if (fieldClass && il2cpp_class_is_enum(fieldClass))
      compatible = il2cpp_type_get_type(il2cpp_class_enum_basetype(fieldClass)) == IL2CPP_TYPE_I4;
  }
  if constexpr (std::is_same_v<T, void*>)
    compatible = type == IL2CPP_TYPE_CLASS || type == IL2CPP_TYPE_OBJECT || type == IL2CPP_TYPE_SZARRAY
                 || type == IL2CPP_TYPE_STRING
                 || (type == IL2CPP_TYPE_GENERICINST && fieldClass && !il2cpp_class_is_valuetype(fieldClass));
  if (!compatible
      || field->offset + sizeof(T)
             > il2cpp_class_instance_size(il2cpp_object_get_class(static_cast<Il2CppObject*>(object))))
    return false;
  std::memcpy(&result, static_cast<const char*>(object) + field->offset, sizeof(T));
  return true;
}

template <typename T> void Scalar(Json& result, void* object, const char* field, const char* label)
{
  T value{};
  if (Read(object, field, value))
    result[label] = value;
  else
    result[label] = nullptr;
}

// Session-local aliases: never write raw order IDs, semaphore identifiers or object addresses.
// Bounded storage; evicted values receive a NEW token if seen again, never a false match.
std::mutex                                       tokenMutex;
std::vector<std::pair<std::u16string, uint64_t>> orderTokens;
std::vector<std::pair<int64_t, uint64_t>>        semaphoreTokens;
std::vector<std::pair<std::u16string, uint64_t>> requestTokens;
uint64_t                                         nextToken = 1;
template <typename T> uint64_t                   Token(std::vector<std::pair<T, uint64_t>>& tokens, const T& value)
{
  std::lock_guard lock(tokenMutex);
  for (const auto& entry : tokens)
    if (entry.first == value)
      return entry.second;
  if (tokens.size() >= 512)
    tokens.erase(tokens.begin());
  const auto token = nextToken++;
  tokens.emplace_back(value, token);
  return token;
}

bool Text(void* object, const char* field, std::u16string& value)
{
  auto* metadata = Field(object, field);
  void* raw{};
  if (!metadata || il2cpp_type_get_type(metadata->type) != IL2CPP_TYPE_STRING || !Read(object, field, raw) || !raw)
    return false;
  auto*      text   = static_cast<Il2CppString*>(raw);
  const auto length = il2cpp_string_length(text);
  if (length < 0 || length > 512)
    return false;
  value.assign(reinterpret_cast<const char16_t*>(il2cpp_string_chars(text)), length);
  return true;
}

Json Request(void* job)
{
  Json  result = {{"dispatch_scope_present", job != nullptr}};
  void* request{};
  if (!Read(job, "Request", request) || !request)
    return result;
  result["class"] = il2cpp_class_get_name(il2cpp_object_get_class(static_cast<Il2CppObject*>(request)));
  Scalar<int32_t>(result, request, "<Method>k__BackingField", "method");
  Scalar<int32_t>(result, request, "_currentRetries", "retries");
  Scalar<bool>(result, request, "IgnoreCallbackOrdering", "ignore_callback_ordering");
  std::u16string value;
  if (Text(request, "<Uid>k__BackingField", value))
    result["uid_token"] = Token(requestTokens, u"uid:" + value);
  if (Text(request, "_rawPath", value))
    result["route_token"] = Token(requestTokens, u"route:" + value);
  void* response{};
  if (Read(job, "Response", response) && response) {
    Scalar<int32_t>(result, response, "<ResponseCode>k__BackingField", "response_code");
    Scalar<bool>(result, response, "<IsCachedResponse>k__BackingField", "cached_response");
  }
  return result;
}

Json GameCallers()
{
  void*      frames[48]{};
  const auto count  = CaptureStackBackTrace(0, 48, frames, nullptr);
  const auto base   = GetModuleHandleA("GameAssembly.dll");
  Json       result = Json::array();
  if (!base)
    return result;
  for (USHORT index = 0; index < count; ++index) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(frames[index], &memory, sizeof(memory)) && memory.AllocationBase == base)
      result.push_back(reinterpret_cast<uintptr_t>(frames[index]) - reinterpret_cast<uintptr_t>(base));
  }
  return result;
}

Json Orders(void* collection)
{
  Json result = {{"present", collection != nullptr}, {"supported", false}};
  if (!collection)
    return result;
  auto*   cls   = il2cpp_object_get_class(static_cast<Il2CppObject*>(collection));
  void*   array = collection;
  int32_t count{};
  if (il2cpp_class_get_rank(cls) != 1) {
    if (std::strcmp(il2cpp_class_get_namespace(cls), "System.Collections.Generic") != 0
        || std::strcmp(il2cpp_class_get_name(cls), "List`1") != 0 || !Read(collection, "_size", count)
        || !Read(collection, "_items", array) || !array)
      return result;
    cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(array));
  } else {
    count = static_cast<int32_t>(il2cpp_array_length(static_cast<Il2CppArray*>(array)));
  }
  auto* element = il2cpp_class_get_element_class(cls);
  if (il2cpp_class_get_rank(cls) != 1 || !element || std::strcmp(il2cpp_class_get_namespace(element), "System") != 0
      || std::strcmp(il2cpp_class_get_name(element), "String") != 0 || il2cpp_array_element_size(cls) != sizeof(void*)
      || count < 0 || count > 128
      || static_cast<uint32_t>(count) > il2cpp_array_length(static_cast<Il2CppArray*>(array)))
    return result;
  result["supported"] = true;
  result["count"]     = count;
  result["tokens"]    = Json::array();
  auto* items         = reinterpret_cast<Il2CppString**>(static_cast<Il2CppArraySize*>(array)->vector);
  for (int32_t index = 0; index < count; ++index) {
    auto*      item   = items[index];
    const auto length = item ? il2cpp_string_length(item) : 0;
    if (length <= 0 || length > 256) {
      result["tokens"].push_back(nullptr);
      continue;
    }
    const auto* chars = reinterpret_cast<const char16_t*>(il2cpp_string_chars(item));
    result["tokens"].push_back(Token(orderTokens, std::u16string(chars, length)));
  }
  return result;
}

Json Claim(void* object)
{
  Json result = {{"present", object != nullptr}};
  if (!object)
    return result;
  // ShopClaimKey is a client-local monotonically assigned number, not an order/account identifier.
  if (auto* field = Field(object, "_key")) {
    auto* cls   = il2cpp_class_from_type(field->type);
    auto* value = cls ? il2cpp_class_get_field_from_name(cls, "_value") : nullptr;
    if (cls && il2cpp_class_is_valuetype(cls) && value && il2cpp_type_get_type(value->type) == IL2CPP_TYPE_I8
        && il2cpp_class_value_size(cls, nullptr) == sizeof(int64_t)
        && field->offset + sizeof(int64_t)
               <= il2cpp_class_instance_size(il2cpp_object_get_class(static_cast<Il2CppObject*>(object)))) {
      int64_t key{};
      std::memcpy(&key, static_cast<const char*>(object) + field->offset, sizeof(key));
      result["local_claim_key"] = key;
    }
  }
  Scalar<int32_t>(result, object, "_mode", "mode");
  Scalar<int32_t>(result, object, "_requestState", "request");
  Scalar<int32_t>(result, object, "_rewardAcquisitionState", "acquisition");
  Scalar<int32_t>(result, object, "_rewardPresentationState", "presentation");
  Scalar<bool>(result, object, "_expectsRewardsFromSync", "expects_sync");
  Scalar<bool>(result, object, "_hasDirectGrant", "direct_grant");
  Scalar<bool>(result, object, "_expectedOrderIdsFinalized", "orders_finalized");
  Scalar<bool>(result, object, "_resultPublished", "result_published");
  void* orders{};
  if (Read(object, "_orderIds", orders))
    result["orders"] = Orders(orders);
  return result;
}

// Managed read-only calls report missing/destroyed objects as unknown. No UI names/text are recorded.
Il2CppObject* Invoke(void* object, const char* method, int count = 0, void** args = nullptr)
{
  if (!object)
    return nullptr;
  auto*            cls  = il2cpp_object_get_class(static_cast<Il2CppObject*>(object));
  auto*            info = il2cpp_class_get_method_from_name(cls, method, count);
  Il2CppException* error{};
  auto*            result = info ? il2cpp_runtime_invoke(info, object, args, &error) : nullptr;
  return error ? nullptr : result;
}
Json BoolProperty(void* object, const char* method)
{
  auto* result = Invoke(object, method);
  if (!result || il2cpp_class_get_type(result->klass)->type != IL2CPP_TYPE_BOOLEAN)
    return nullptr;
  return *static_cast<bool*>(il2cpp_object_unbox(result));
}
std::vector<std::pair<int64_t, uint64_t>> uiTokens;
Json                                      UiIdentity(void* object)
{
  if (!object)
    return {{"present", false}};
  Json  result = {{"present", true},
                  {"class", il2cpp_class_get_name(il2cpp_object_get_class(static_cast<Il2CppObject*>(object)))}};
  auto* id     = Invoke(object, "GetInstanceID");
  if (id && il2cpp_class_get_type(id->klass)->type == IL2CPP_TYPE_I4)
    result["object_token"] = Token(uiTokens, static_cast<int64_t>(*static_cast<int32_t*>(il2cpp_object_unbox(id))));
  return result;
}
// Bounded reference-list reader, rejecting value arrays and invalid collection layouts.
std::vector<void*> References(void* list, int limit = 64)
{
  std::vector<void*> result;
  if (!list)
    return result;
  void*   array = list;
  int32_t count{};
  auto*   cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(list));
  if (!il2cpp_class_get_rank(cls)) {
    if (std::strcmp(il2cpp_class_get_namespace(cls), "System.Collections.Generic")
        || std::strcmp(il2cpp_class_get_name(cls), "List`1"))
      return result;
    if (!Read(list, "_items", array) || !array || !Read(list, "_size", count))
      return result;
  } else
    count = static_cast<int32_t>(il2cpp_array_length(static_cast<Il2CppArray*>(list)));
  cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(array));
  if (il2cpp_class_get_rank(cls) != 1 || count < 0 || count > limit
      || static_cast<uint32_t>(count) > il2cpp_array_length(static_cast<Il2CppArray*>(array))
      || il2cpp_class_is_valuetype(il2cpp_class_get_element_class(cls))
      || il2cpp_array_element_size(cls) != sizeof(void*))
    return result;
  for (int32_t i = 0; i < count; ++i)
    result.push_back(reinterpret_cast<void**>(static_cast<Il2CppArraySize*>(array)->vector)[i]);
  return result;
}
Json AncestorGates(void* component)
{
  Json         result     = Json::array();
  auto*        transform  = Invoke(component, "get_transform");
  static auto* groupClass = Resolve("UnityEngine.UIModule", "UnityEngine", "CanvasGroup");
  static auto* goClass    = Resolve("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  // Select the Type overload, never the generic/list overload.
  static const MethodInfo* typedComponents = []() -> const MethodInfo* {
    void* iter{};
    if (!goClass)
      return nullptr;
    while (auto* m = il2cpp_class_get_methods(goClass, &iter)) {
      if (std::strcmp(m->name, "GetComponents") || m->parameters_count != 1 || m->is_generic)
        continue;
      auto* c = il2cpp_class_from_type(il2cpp_method_get_param(m, 0));
      if (c && !std::strcmp(il2cpp_class_get_name(c), "Type"))
        return m;
    }
    return nullptr;
  }();
  for (int depth = 0; transform && depth < 16; ++depth) {
    auto* go   = Invoke(transform, "get_gameObject");
    Json  node = {{"depth", depth}, {"object", UiIdentity(go)}, {"active", BoolProperty(go, "get_activeInHierarchy")}};
    Json  groups = Json::array();
    if (groupClass && typedComponents && go) {
      void*            type   = il2cpp_type_get_object(il2cpp_class_get_type(groupClass));
      void*            args[] = {type};
      Il2CppException* error{};
      auto*            found = il2cpp_runtime_invoke(typedComponents, go, args, &error);
      if (!error)
        for (auto* group : References(found, 32)) {
          groups.push_back({{"object", UiIdentity(group)},
                            {"enabled", BoolProperty(group, "get_enabled")},
                            {"interactable", BoolProperty(group, "get_interactable")},
                            {"blocks_raycasts", BoolProperty(group, "get_blocksRaycasts")},
                            {"ignore_parent_groups", BoolProperty(group, "get_ignoreParentGroups")}});
        }
      node["groups_read_ok"] = !error;
    } else
      node["groups_read_ok"] = false;
    node["canvas_groups"] = std::move(groups);
    result.push_back(std::move(node));
    transform = Invoke(transform, "get_parent");
  }
  if (transform)
    result.push_back({{"truncated", true}});
  return result;
}
Json Selectable(void* button, bool hierarchy)
{
  Json result = UiIdentity(button);
  Scalar<bool>(result, button, "m_Interactable", "interactable");
  Scalar<bool>(result, button, "m_GroupsAllowInteraction", "groups_allow_interaction");
  Scalar<bool>(result, button, "<isPointerInside>k__BackingField", "pointer_inside");
  Scalar<bool>(result, button, "<isPointerDown>k__BackingField", "pointer_down");
  result["effective_interactable"] = BoolProperty(button, "IsInteractable");
  result["active_enabled"]         = BoolProperty(button, "get_isActiveAndEnabled");
  auto* go                         = Invoke(button, "get_gameObject");
  result["game_object"]            = UiIdentity(go);
  result["active_hierarchy"]       = BoolProperty(go, "get_activeInHierarchy");
  void* graphic{};
  if (Read(button, "m_TargetGraphic", graphic) && graphic) {
    result["graphic"] = UiIdentity(graphic);
    Scalar<bool>(result["graphic"], graphic, "m_RaycastTarget", "raycast_target");
  }
  if (hierarchy)
    result["ancestors"] = AncestorGates(button);
  return result;
}

Json Button(void* object)
{
  Json result = {{"present", object != nullptr}};
  if (!object)
    return result;
  Scalar<bool>(result, object, "_defaultInteractableFlag", "default_interactable");
  Scalar<bool>(result, object, "_overrideLocked", "override_locked");
  void* listener{};
  if (Read(object, "_semaphoreButtonListener", listener) && listener) {
    Scalar<int32_t>(result, listener, "_currentCategory", "semaphore_category");
    Scalar<bool>(result, listener, "_fallbackInteractableState", "fallback_interactable");
    void* button{};
    if (Read(listener, "_button", button) && button) {
      Scalar<bool>(result, button, "m_Interactable", "interactable");
      result["selectable"] = Selectable(button, false);
    }
  }
  return result;
}

Json View(void* object)
{
  Json result = {{"present", object != nullptr}};
  if (!object)
    return result;
  result["class"] = il2cpp_class_get_name(il2cpp_object_get_class(static_cast<Il2CppObject*>(object)));
  Scalar<bool>(result, object, "_wasShown", "was_shown");
  void* context{};
  result["context_present"] = Read(object, "m_context", context) && context;
  return result;
}

Json Reward(void* object)
{
  Json result = View(object);
  if (!object)
    return result;
  void* context{};
  if (Read(object, "m_context", context) && context) {
    Scalar<bool>(result, context, "_isClaimed", "claimed");
    Scalar<bool>(result, context, "_exitButtonActive", "exit_active");
    Scalar<bool>(result, context, "_bottomButtonsActive", "bottom_active");
    Scalar<int32_t>(result, context, "_fetchStatus", "fetch");
    Scalar<int32_t>(result, context, "_claimBehaviour", "claim_behaviour");
    Scalar<int32_t>(result, context, "_semaphoreCategory", "semaphore_category");
  } else
    result["context_present"] = false;
  void* button{};
  if (Read(object, "_collectButtonWidget", button))
    result["collect_button"] = Button(button);
  if (Read(object, "_bundleClaimButtonWidget", button))
    result["claim_button"] = Button(button);
  return result;
}

Json Error(void* object)
{
  Json result = {{"present", object != nullptr}};
  if (object) {
    Scalar<int32_t>(result, object, "<Type>k__BackingField", "type");
    Scalar<int32_t>(result, object, "<Code>k__BackingField", "code");
    Scalar<int32_t>(result, object, "<HttpResponseCode>k__BackingField", "http_status");
    Scalar<int32_t>(result, object, "<CallbackErrorHandling>k__BackingField", "callback_handling");
    std::u16string message;
    if (Text(object, "<Message>k__BackingField", message))
      result["duplicate_request_message"] = message == u"Duplicate request detected";
  }
  return result;
}

Json Storyboard(void* object)
{
  Json result = {{"present", object != nullptr}};
  Scalar<int32_t>(result, object, "_currentItemIndex", "item_index");
  void* items{};
  if (Read(object, "_filteredItemsForReveal", items))
    Scalar<int32_t>(result, items, "_size", "item_count");
  return result;
}

Json Chest(void* object)
{
  auto  result = View(object);
  void* context{};
  if (Read(object, "m_context", context))
    result["storyboard"] = Storyboard(context);
  void* button{};
  if (Read(object, "_skipButton", button))
    result["skip_button"] = Button(button);
  void* routine{};
  if (Read(object, "_unlockRoutine", routine))
    result["unlock_routine_present"] = routine != nullptr;
  return result;
}

Json MessageContext(void* context)
{
  Json result = {{"present", context != nullptr}};
  Scalar<int32_t>(result, context, "ButtonMode", "button_mode");
  Scalar<bool>(result, context, "ShowExitButton", "show_exit");
  Scalar<bool>(result, context, "AutoDismissOnAccept", "auto_dismiss");
  void* error{};
  if (Read(context, "ServerError", error))
    result["server_error"] = Error(error);
  return result;
}

Json ShopContext(void* context)
{
  Json result = {{"present", context != nullptr}};
  Scalar<bool>(result, context, "_transactionInProgress", "transaction_in_progress");
  Scalar<bool>(result, context, "_fetchingBundlesInProgress", "fetching_bundles");
  Scalar<bool>(result, context, "_isInBulkSelectionMode", "bulk_selection");
  Scalar<bool>(result, context, "SkippingReveal", "skipping_reveal");
  Scalar<int32_t>(result, context, "<PurchasedChestsNumber>k__BackingField", "purchased_chests");
  void* orders{};
  if (Read(context, "<RewardPresentationOrderIds>k__BackingField", orders))
    result["orders"] = Orders(orders);
  return result;
}

Json Summary(void* object)
{
  auto result = View(object);
  Scalar<bool>(result, object, "_ignoreContextUpdate", "ignore_context_update");
  Scalar<int32_t>(result, object, "_purchasedChests", "purchased_chests");
  void* value{};
  if (Read(object, "m_context", value))
    result["context"] = ShopContext(value);
  if (Read(object, "_rewardPresentationOrderIds", value))
    result["orders"] = Orders(value);
  for (const char* field : {"_leftScreenOpenChestButton", "_middleScreenOpenChestButton", "_rightScreenOpenChestButton",
                            "_repurchaseButton"})
    if (Read(object, field, value))
      result[field] = Button(value);
  return result;
}

enum class Kind { none, claim, reward, chest, summary, shop, storyboard, orders, error, semaphore, lock, message };
Json Snapshot(Kind kind, void* object)
{
  switch (kind) {
    case Kind::claim:
      return Claim(object);
    case Kind::reward:
      return Reward(object);
    case Kind::chest:
      return Chest(object);
    case Kind::summary:
      return Summary(object);
    case Kind::shop:
      return ShopContext(object);
    case Kind::storyboard:
      return Storyboard(object);
    case Kind::orders:
      return Orders(object);
    case Kind::message: {
      auto  result = View(object);
      void* context{};
      if (Read(object, "m_context", context))
        result["context"] = MessageContext(context);
      return result;
    }
    case Kind::error:
      return Error(object);
    case Kind::lock: {
      Json result;
      Scalar<int32_t>(result, object, "category", "category");
      Scalar<int32_t>(result, object, "state", "state");
      Scalar<int32_t>(result, object, "reason", "reason");
      Scalar<float>(result, object, "timer", "timer");
      int64_t identifier{};
      if (Read(object, "identifier", identifier))
        result["identifier_token"] = Token(semaphoreTokens, identifier);
      return result;
    }
    default:
      return Json::object();
  }
}

void Write(Json event) noexcept
{
  try {
    std::lock_guard lock(stateMutex);
    const auto      now = Clock::now();
    if (now - rateStart >= std::chrono::minutes(1)) {
      rateStart = now;
      rateCount = 0;
    }
    if (rateCount++ >= 1200) {
      ++suppressed;
      return;
    }
    event["seq"]        = ++eventSequence;
    event["elapsed_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count();
    event["utc_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    event["thread"]     = GetCurrentThreadId();
    event["suppressed"] = std::exchange(suppressed, 0);
    logger->info("{}", event.dump());
  } catch (...) { /* Diagnostics must not throw into the game. */
  }
}

void Remember(Il2CppGCHandle& handle, void* object)
{
  if (!object || (handle && il2cpp_gchandle_get_target(handle) == object))
    return;
  auto next = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(object), false);
  if (!next)
    return;
  if (handle)
    il2cpp_gchandle_free(handle);
  handle = next;
}

struct ObservedClaim {
  Il2CppGCHandle handle;
  Json           last;
};
std::mutex                 observedClaimMutex;
std::vector<ObservedClaim> observedClaims; // Weak handles never keep a claim alive.
void                       TrackClaim(void* claim)
{
  std::lock_guard lock(observedClaimMutex);
  if (!claim)
    return;
  for (const auto& entry : observedClaims)
    if (il2cpp_gchandle_get_target(entry.handle) == claim)
      return;
  auto handle = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(claim), false);
  if (!handle)
    return;
  if (observedClaims.size() >= 64) {
    Write({{"event", "claim_tracking_evicted"}, {"last", observedClaims.front().last}});
    il2cpp_gchandle_free(observedClaims.front().handle);
    observedClaims.erase(observedClaims.begin());
  }
  observedClaims.push_back({handle, Claim(claim)});
}
void CheckClaims() noexcept
{
  try {
    std::lock_guard lock(observedClaimMutex);
    for (auto it = observedClaims.begin(); it != observedClaims.end();) {
      auto* object = il2cpp_gchandle_get_target(it->handle);
      if (!object) {
        Write({{"event", "claim_tracking_collected"}, {"last", it->last}});
        il2cpp_gchandle_free(it->handle);
        it = observedClaims.erase(it);
        continue;
      }
      auto current = Claim(object);
      if (current != it->last) {
        Write({{"event", "claim_state_observed"}, {"before", it->last}, {"after", current}});
        it->last = current;
      }
      if (current.value("presentation", 0) == 5 || current.value("presentation", 0) == 6) {
        il2cpp_gchandle_free(it->handle);
        it = observedClaims.erase(it);
      } else
        ++it;
    }
  } catch (...) {
  }
}

struct Span {
  size_t            index;
  Kind              kind;
  void*             object;
  int64_t           tag;
  uint64_t          id{};
  int               exceptions = std::uncaught_exceptions();
  Clock::time_point start      = Clock::now();
  int               result     = -1;
  Span(size_t index, Kind kind, void* object, int64_t tag) noexcept
      : index(index)
      , kind(kind)
      , object(object)
      , tag(tag)
  {
    if (!ready.load())
      return;
    try {
      {
        std::lock_guard lock(stateMutex);
        // Global UI-lock traffic is only useful near claims, rewards or error/confirmation activity.
        if ((kind == Kind::semaphore || kind == Kind::lock) && start > activeUntil)
          return;
        if (kind != Kind::semaphore && kind != Kind::lock)
          activeUntil = start + std::chrono::minutes(5);
        if (kind == Kind::claim)
          Remember(latestClaim, object);
        if (kind == Kind::reward)
          Remember(latestReward, object);
        if (kind == Kind::chest)
          Remember(latestChest, object);
        if (kind == Kind::message)
          Remember(latestMessage, object);
        if (kind == Kind::summary)
          Remember(latestSummary, object);
      }
      if (kind == Kind::claim)
        TrackClaim(object);
      id = nextSpan++;
      Event("enter");
    } catch (...) {
      id = 0;
    }
  }
  void Details(Json details) noexcept
  {
    try {
      if (id)
        Write({{"event", "details"}, {"span", id}, {"details", std::move(details)}});
    } catch (...) {
    }
  }
  void Event(const char* phase) noexcept
  {
    try {
      Json event = {{"event", phase},
                    {"span", id},
                    {"class", kTargets[index].cls},
                    {"method", kTargets[index].method},
                    {"tag", tag},
                    {"state", Snapshot(kind, object)}};
      if (std::strcmp(phase, "enter") != 0) {
        event["duration_us"] = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
        if (result >= 0)
          event["returned"] = result != 0;
      }
      Write(std::move(event));
    } catch (...) {
    }
  }
  ~Span() noexcept
  {
    if (id)
      Event(std::uncaught_exceptions() > exceptions ? "unwind" : "exit");
  }
};

thread_local void*       requestClaim{};
thread_local const char* requestPurpose{};
struct ClaimRequestScope {
  void*       previous;
  const char* previousPurpose;
  ClaimRequestScope(void* claim, const char* purpose) noexcept
      : previous(std::exchange(requestClaim, claim))
      , previousPurpose(std::exchange(requestPurpose, purpose))
  {
  }
  ~ClaimRequestScope()
  {
    requestClaim   = previous;
    requestPurpose = previousPurpose;
  }
};
struct TrackedRequest {
  Il2CppGCHandle    job;
  Json              correlation;
  Clock::time_point sent;
  bool              staleReported{};
  bool              modelObserved{};
};
std::mutex                  requestMutex;
std::vector<TrackedRequest> trackedRequests;
std::atomic_uint64_t        nextRequest{1};
Json                        RequestContext(void* job, bool modelObserved = false)
{
  std::lock_guard lock(requestMutex);
  for (auto it = trackedRequests.begin(); it != trackedRequests.end(); ++it) {
    if (il2cpp_gchandle_get_target(it->job) != job)
      continue;
    auto result             = it->correlation;
    result["since_send_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - it->sent).count();
    if (modelObserved)
      it->modelObserved = true;
    return result;
  }
  return nullptr;
}
void ObserveDispatch(void* job) noexcept
{
  try {
    if (!ready.load() || !job)
      return;
    auto context = RequestContext(job);
    if (context.is_null() && requestClaim) {
      context     = {{"request_trace", nextRequest++}, {"purpose", requestPurpose}, {"claim", Claim(requestClaim)}};
      auto handle = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(job), false);
      if (!handle) {
        Write({{"event", "request_tracking_unavailable"}, {"correlation", context}});
        return;
      }
      std::lock_guard lock(requestMutex);
      if (trackedRequests.size() >= 128) {
        Write({{"event", "request_tracking_evicted"}, {"correlation", trackedRequests.front().correlation}});
        il2cpp_gchandle_free(trackedRequests.front().job);
        trackedRequests.erase(trackedRequests.begin());
      }
      trackedRequests.push_back({handle, context, Clock::now()});
    }
    if (!context.is_null())
      Write({{"event", "claim_request_send"}, {"correlation", context}, {"request", Request(job)}});
  } catch (...) {
  }
}
// HTTPClientError is an embedded value type, not an integer error code.
Json TransportError(void* response)
{
  auto* field     = Field(response, "<Error>k__BackingField");
  auto* cls       = field ? il2cpp_class_from_type(field->type) : nullptr;
  auto* flag      = cls ? il2cpp_class_get_field_from_name(cls, "IsError") : nullptr;
  auto* code      = cls ? il2cpp_class_get_field_from_name(cls, "Code") : nullptr;
  auto* codeClass = code ? il2cpp_class_from_type(code->type) : nullptr;
  if (!cls || !il2cpp_class_is_valuetype(cls) || std::strcmp(il2cpp_class_get_name(cls), "HTTPClientError")
      || std::strcmp(il2cpp_class_get_namespace(cls), "Digit.Engine.HttpClient")
      || il2cpp_class_value_size(cls, nullptr) != 8 || !flag || !code || flag->offset != sizeof(Il2CppObject)
      || code->offset != sizeof(Il2CppObject) + 4 || il2cpp_type_get_type(flag->type) != IL2CPP_TYPE_BOOLEAN
      || !codeClass || !il2cpp_class_is_enum(codeClass)
      || il2cpp_type_get_type(il2cpp_class_enum_basetype(codeClass)) != IL2CPP_TYPE_I4
      || field->offset + 8 > il2cpp_class_instance_size(il2cpp_object_get_class(static_cast<Il2CppObject*>(response))))
    return {{"supported", false}};
  uint8_t isError{};
  int32_t value{};
  std::memcpy(&isError, static_cast<const char*>(response) + field->offset, 1);
  std::memcpy(&value, static_cast<const char*>(response) + field->offset + 4, 4);
  return {{"supported", true}, {"is_error", isError != 0}, {"code", isError ? Json(value) : Json(nullptr)}};
}

void ObserveRequest(void* job, const char* phase, void* response, void* error) noexcept
{
  try {
    if (!ready.load() || !job)
      return;
    auto context = RequestContext(job, std::strcmp(phase, "claim_request_model_callback") == 0);
    if (context.is_null())
      return;
    Json event = {{"event", phase}, {"correlation", context}, {"request", Request(job)}, {"error", Error(error)}};
    if (response) {
      Scalar<int32_t>(event, response, "<ResponseCode>k__BackingField", "response_code");
      event["transport_error"] = TransportError(response);
      Scalar<bool>(event, response, "<IsCachedResponse>k__BackingField", "cached_response");
    }
    Write(std::move(event));
  } catch (...) {
  }
}
void CheckTrackedRequests() noexcept
{
  try {
    std::lock_guard lock(requestMutex);
    for (auto it = trackedRequests.begin(); it != trackedRequests.end();) {
      const bool expired = il2cpp_gchandle_get_target(it->job) == nullptr;
      const auto age     = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - it->sent).count();
      if (expired || (!it->staleReported && !it->modelObserved && age >= 60)) {
        Write({{"event", expired ? "request_tracking_collected" : "request_no_model_callback_observed_60s"},
               {"correlation", it->correlation},
               {"age_seconds", age}});
        it->staleReported = true;
      }
      if (expired) {
        il2cpp_gchandle_free(it->job);
        it = trackedRequests.erase(it);
      } else
        ++it;
    }
  } catch (...) {
  }
}
Json StringToken(void* value)
{
  if (!value)
    return nullptr;
  auto* str = static_cast<Il2CppString*>(value);
  auto* cls = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(str));
  if (il2cpp_class_get_type(cls)->type != IL2CPP_TYPE_STRING)
    return nullptr;
  const auto n = il2cpp_string_length(str);
  if (n < 0 || n > 256)
    return nullptr;
  return Token(orderTokens, std::u16string(reinterpret_cast<const char16_t*>(il2cpp_string_chars(str)), n));
}
Json ClaimKeys(void* list)
{
  Json    result = {{"supported", false}};
  void*   array{};
  int32_t count{};
  if (!Read(list, "_items", array) || !array || !Read(list, "_size", count) || count < 0 || count > 128)
    return result;
  auto* cls     = il2cpp_object_get_class(static_cast<Il2CppObject*>(array));
  auto* element = il2cpp_class_get_element_class(cls);
  if (il2cpp_class_get_rank(cls) != 1 || !element || std::strcmp(il2cpp_class_get_name(element), "ShopClaimKey")
      || !il2cpp_class_is_valuetype(element) || il2cpp_class_value_size(element, nullptr) != 8
      || il2cpp_array_element_size(cls) != 8
      || static_cast<uint32_t>(count) > il2cpp_array_length(static_cast<Il2CppArray*>(array)))
    return result;
  auto* field = il2cpp_class_get_field_from_name(element, "_value");
  if (!field || field->offset != sizeof(Il2CppObject) || il2cpp_type_get_type(field->type) != IL2CPP_TYPE_I8)
    return result;
  result["supported"] = true;
  result["count"]     = count;
  result["keys"]      = Json::array();
  for (int32_t i = 0; i < count; ++i)
    result["keys"].push_back(reinterpret_cast<int64_t*>(static_cast<Il2CppArraySize*>(array)->vector)[i]);
  return result;
}
void* BoundSummary()
{
  auto* object = latestSummary ? il2cpp_gchandle_get_target(latestSummary) : nullptr;
  void* context{};
  return object && Read(object, "m_context", context) && context ? object : nullptr;
}

Json InputSystemState()
{
  static auto*     cls    = Resolve("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
  static auto*     getter = cls ? il2cpp_class_get_method_from_name(cls, "get_current", 0) : nullptr;
  Il2CppException* error{};
  auto*            system = getter ? il2cpp_runtime_invoke(getter, nullptr, nullptr, &error) : nullptr;
  if (error || !system)
    return {{"present", false}};
  Json result = {{"present", true}};
  Scalar<bool>(result, system, "m_HasFocus", "has_focus");
  void* module{};
  if (Read(system, "m_CurrentInputModule", module))
    result["module"] = UiIdentity(module);
  void* selected{};
  if (Read(system, "m_CurrentSelected", selected))
    result["selected"] = UiIdentity(selected);
  return result;
}

Json SummaryInput(void* summary)
{
  Json result  = {{"summary", Summary(summary)}, {"event_system", InputSystemState()}};
  Json buttons = Json::object();
  for (auto name : {"_leftScreenOpenChestButton", "_middleScreenOpenChestButton", "_rightScreenOpenChestButton",
                    "_repurchaseButton"}) {
    void *widget{}, *listener{}, *button{};
    if (Read(summary, name, widget) && Read(widget, "_semaphoreButtonListener", listener)
        && Read(listener, "_button", button))
      buttons[name] = Selectable(button, true);
  }
  result["buttons"] = std::move(buttons);
  DWORD process{};
  GetWindowThreadProcessId(GetForegroundWindow(), &process);
  result["process_foreground"] = process == GetCurrentProcessId();
  result["shortcut_capture"]   = Key::shortcutCaptureActive;
  result["shortcut_popup"]     = Key::shortcutPopupActive;
  return result;
}
void* RaycastObject(void* pointer, const char* name)
{
  auto* field = Field(pointer, name);
  auto* cls   = field ? il2cpp_class_from_type(field->type) : nullptr;
  auto* go    = cls ? il2cpp_class_get_field_from_name(cls, "m_GameObject") : nullptr;
  if (!cls || !il2cpp_class_is_valuetype(cls) || std::strcmp(il2cpp_class_get_name(cls), "RaycastResult") || !go
      || go->offset != sizeof(Il2CppObject) || il2cpp_type_get_type(go->type) != IL2CPP_TYPE_CLASS
      || field->offset + il2cpp_class_value_size(cls, nullptr)
             > il2cpp_class_instance_size(il2cpp_object_get_class(static_cast<Il2CppObject*>(pointer))))
    return nullptr;
  void* object{};
  std::memcpy(&object, static_cast<char*>(pointer) + field->offset, sizeof(object));
  return object;
}
Json Pointer(void* pointer)
{
  Json result = {{"present", pointer != nullptr}};
  for (auto name : {"<pointerEnter>k__BackingField", "m_PointerPress", "<rawPointerPress>k__BackingField",
                    "<pointerClick>k__BackingField"}) {
    void* object{};
    if (Read(pointer, name, object))
      result[name] = UiIdentity(object);
  }
  auto* target                       = RaycastObject(pointer, "<pointerCurrentRaycast>k__BackingField");
  result["raycast_target"]           = UiIdentity(target);
  result["raycast_target_ancestors"] = AncestorGates(target);
  result["press_raycast_target"]     = UiIdentity(RaycastObject(pointer, "<pointerPressRaycast>k__BackingField"));
  Scalar<bool>(result, pointer, "<eligibleForClick>k__BackingField", "eligible_for_click");
  Scalar<bool>(result, pointer, "<dragging>k__BackingField", "dragging");
  Scalar<int32_t>(result, pointer, "<button>k__BackingField", "button");
  return result;
}
void InputPulse() noexcept
{
  try {
    static bool previousMouse{};
    auto*       summary = BoundSummary();
    if (!summary) {
      previousMouse = false;
      return;
    }
    const bool mouseDown = Key::RawDown(KeyCode::Mouse0);
    const bool mouse     = Key::RawPressed(KeyCode::Mouse0);
    const bool escape    = Key::RawDown(KeyCode::Escape);
    const bool released  = previousMouse && !mouse;
    previousMouse        = mouse;
    if (mouseDown || released || escape) {
      auto data                = SummaryInput(summary);
      data["event"]            = "summary_raw_input";
      data["mouse_down"]       = mouseDown;
      data["mouse_up_sampled"] = released;
      data["escape_down"]      = escape;
      Write(std::move(data));
    }
  } catch (...) {
  }
}

void Pulse()
{
  if (!ready.load())
    return;
  InputPulse();
  Json event;
  {
    std::lock_guard lock(stateMutex);
    const auto      now = Clock::now();
    if (now < nextPulse)
      return;
    nextPulse = now + std::chrono::seconds(5);
    if (now > activeUntil)
      return;
    event = {{"event", "game_update"},
             {"latest_claim", Claim(latestClaim ? il2cpp_gchandle_get_target(latestClaim) : nullptr)},
             {"latest_reward", Reward(latestReward ? il2cpp_gchandle_get_target(latestReward) : nullptr)},
             {"latest_chest", Chest(latestChest ? il2cpp_gchandle_get_target(latestChest) : nullptr)},
             {"latest_summary", Summary(latestSummary ? il2cpp_gchandle_get_target(latestSummary) : nullptr)},
             {"latest_message",
              Snapshot(Kind::message, latestMessage ? il2cpp_gchandle_get_target(latestMessage) : nullptr)},
             {"shortcut_capture", Key::shortcutCaptureActive},
             {"shortcut_popup", Key::shortcutPopupActive}};
  }
  Write(std::move(event));
  CheckTrackedRequests();
  CheckClaims();
}

bool Preflight()
{
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
  if (!base)
    return false;
  auto* keyClass = Resolve("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "ShopClaimKey");
  auto* keyField = keyClass ? il2cpp_class_get_field_from_name(keyClass, "_value") : nullptr;
  if (!keyClass || !il2cpp_class_is_valuetype(keyClass) || il2cpp_class_value_size(keyClass, nullptr) != 8 || !keyField
      || keyField->offset != sizeof(Il2CppObject) || il2cpp_type_get_type(keyField->type) != IL2CPP_TYPE_I8) {
    spdlog::error("[ClaimTrace] ShopClaimKey ABI mismatch; no trace hooks installed");
    return false;
  }
  for (size_t index = 0; index < std::size(kTargets); ++index) {
    const auto&       target   = kTargets[index];
    auto*             cls      = Resolve(target.assembly, target.ns, target.cls);
    const MethodInfo* match    = nullptr;
    void*             iterator = nullptr;
    while (cls) {
      auto* method = il2cpp_class_get_methods(cls, &iterator);
      if (!method)
        break;
      if (std::strcmp(method->name, target.method) == 0 && method->parameters_count == target.arguments
          && !(method->flags & METHOD_ATTRIBUTE_STATIC) && !method->is_generic && !method->is_inflated
          && reinterpret_cast<uintptr_t>(method->methodPointer) == base + target.rva) {
        if (match)
          return false;
        match = method;
      }
    }
    if (!match
        || std::memcmp(reinterpret_cast<const void*>(match->methodPointer), target.window.data(), target.windowSize)
               != 0) {
      spdlog::error("[ClaimTrace] client263 identity mismatch: {}.{}; no trace hooks installed", target.cls,
                    target.method);
      return false;
    }
    methods[index] = match;
  }
  return true;
}

// Each wrapper calls the original exactly once, outside every diagnostic mutex/catch.

bool Hook0(auto original, void* self, void* claim, void* choices, void* callback)
{
  ClaimRequestScope requestScope(claim, "purchase");
  Span              span(0, Kind::claim, claim, -1);
  const bool        result = original(self, claim, choices, callback);
  span.result              = result;
  return result;
}

bool Hook1(auto original, void* self, void* bundles, void* quantities, void* callbacks)
{
  Span       span(1, Kind::none, nullptr, -1);
  const bool result = original(self, bundles, quantities, callbacks);
  span.result       = result;
  return result;
}

void Hook2(auto original, void* self, void* claim, void* orders)
{
  Span span(2, Kind::claim, claim, -1);
  original(self, claim, orders);
}

void Hook3(auto original, void* self, void* claim)
{
  Span span(3, Kind::claim, claim, -1);
  original(self, claim);
}

void Hook4(auto original, void* self, void* claim)
{
  Span span(4, Kind::claim, claim, -1);
  original(self, claim);
}

void Hook5(auto original, void* self, void* claim)
{
  ClaimRequestScope requestScope(claim, "sync_rewards");
  Span              span(5, Kind::claim, claim, -1);
  original(self, claim);
}

void Hook6(auto original, void* self, void* claim)
{
  ClaimRequestScope requestScope(claim, "bulk_sync_rewards");
  Span              span(6, Kind::claim, claim, -1);
  original(self, claim);
}

bool Hook7(auto original, void* self, void* claim, int32_t state, void* error)
{
  Span span(7, Kind::claim, claim, state);
  if (span.id && error) {
    try {
      Write({{"event", "claim_error"}, {"span", span.id}, {"error", Error(error)}});
    } catch (...) {
    }
  }
  const bool result = original(self, claim, state, error);
  span.result       = result;
  return result;
}

bool Hook8(auto original, void* self, void* claim)
{
  Span       span(8, Kind::claim, claim, -1);
  const bool result = original(self, claim);
  span.result       = result;
  return result;
}

void Hook9(auto original, void* self, void* notifications)
{
  Span span(9, Kind::none, nullptr, -1);
  original(self, notifications);
}

void Hook10(auto original, void* self, void* orders, int32_t state)
{
  Span span(10, Kind::orders, orders, state);
  original(self, orders, state);
}

bool Hook11(auto original, void* self, void* claim)
{
  Span       span(11, Kind::claim, claim, -1);
  const bool result = original(self, claim);
  span.result       = result;
  return result;
}

void Hook12(auto original, void* self, void* context)
{
  Span span(12, Kind::none, nullptr, -1);
  original(self, context);
}

void Hook13(auto original, void* self)
{
  Span span(13, Kind::reward, self, -1);
  original(self);
}

void Hook14(auto original, void* self)
{
  Span span(14, Kind::reward, self, -1);
  original(self);
}

void Hook15(auto original, void* self)
{
  Span span(15, Kind::reward, self, -1);
  original(self);
}

void Hook16(auto original, void* self)
{
  Span span(16, Kind::reward, self, -1);
  original(self);
}

void Hook17(auto original, void* self)
{
  Span span(17, Kind::reward, self, -1);
  original(self);
}

void Hook18(auto original, void* self)
{
  Span span(18, Kind::reward, self, -1);
  original(self);
}

void Hook19(auto original, void* self)
{
  Span span(19, Kind::reward, self, -1);
  original(self);
}

void Hook20(auto original, void* self)
{
  Span span(20, Kind::reward, self, -1);
  original(self);
}

void Hook21(auto original, void* self)
{
  Span span(21, Kind::reward, self, -1);
  original(self);
}

void Hook22(auto original, void* self)
{
  Span span(22, Kind::chest, self, -1);
  original(self);
}

void Hook23(auto original, void* self)
{
  Span span(23, Kind::chest, self, -1);
  original(self);
}

void Hook24(auto original, void* self)
{
  Span span(24, Kind::chest, self, -1);
  original(self);
}

void Hook25(auto original, void* self)
{
  Span span(25, Kind::chest, self, -1);
  original(self);
}

void Hook26(auto original, void* self, int32_t state)
{
  Span span(26, Kind::reward, self, state);
  original(self, state);
}

void Hook27(auto original, void* self, int32_t state)
{
  Span span(27, Kind::reward, self, state);
  original(self, state);
}

bool Hook28(auto original, void* self)
{
  Span       span(28, Kind::reward, self, -1);
  const bool result = original(self);
  span.result       = result;
  return result;
}

bool Hook29(auto original, void* self)
{
  Span       span(29, Kind::storyboard, self, -1);
  const bool result = original(self);
  span.result       = result;
  return result;
}

bool Hook30(auto original, void* self, bool all)
{
  Span       span(30, Kind::storyboard, self, all);
  const bool result = original(self, all);
  span.result       = result;
  return result;
}

void Hook31(auto original, void* self, void* error)
{
  Span span(31, Kind::error, error, -1);
  try {
    std::u16string message;
    if (span.id && Text(error, "<Message>k__BackingField", message) && message == u"Duplicate request detected")
      span.Details({{"duplicate_request", Request(dispatchJob)}, {"game_callers_rva", GameCallers()}});
  } catch (...) {
  }
  original(self, error);
}

void Hook32(auto original, void* self, int32_t category, int64_t identifier, int32_t timeout)
{
  Span span(32, Kind::semaphore, self, category);
  try {
    if (span.id)
      span.Details({{"category", category},
                    {"identifier_token", Token(semaphoreTokens, identifier)},
                    {"timeout_argument", timeout}});
  } catch (...) {
  }
  original(self, category, identifier, timeout);
}

bool Hook33(auto original, void* self, void* semaphore)
{
  Span       span(33, Kind::lock, semaphore, -1);
  const bool result = original(self, semaphore);
  span.result       = result;
  return result;
}

void Hook34(auto original, void* self, void* visibility, void* context)
{
  Span span(34, Kind::message, self, -1);
  try {
    if (span.id)
      span.Details({{"incoming_context", MessageContext(context)}});
  } catch (...) {
  }
  original(self, visibility, context);
}

void Hook35(auto original, void* self, void* visibility, void* context)
{
  Span span(35, Kind::message, self, -1);
  original(self, visibility, context);
}

void Hook36(auto original, void* self)
{
  Span span(36, Kind::message, self, -1);
  original(self);
}

void Hook37(auto original, void* self)
{
  Span span(37, Kind::message, self, -1);
  original(self);
}

void Hook38(auto original, void* self)
{
  Span span(38, Kind::message, self, -1);
  original(self);
}

void Hook39(auto original, void* self)
{
  Span span(39, Kind::summary, self, -1);
  original(self);
}

void Hook40(auto original, void* self)
{
  Span span(40, Kind::summary, self, -1);
  original(self);
}

void Hook41(auto original, void* self)
{
  Span span(41, Kind::summary, self, -1);
  original(self);
}

void Hook42(auto original, void* self)
{
  Span span(42, Kind::summary, self, -1);
  original(self);
}

void Hook43(auto original, void* self)
{
  Span span(43, Kind::summary, self, -1);
  original(self);
}

bool Hook44(auto original, void* self)
{
  Span       span(44, Kind::summary, self, -1);
  const bool result = original(self);
  span.result       = result;
  return result;
}

bool Hook45(auto original, void* self, void* context)
{
  Span       span(45, Kind::shop, context, -1);
  const bool result = original(self, context);
  span.result       = result;
  return result;
}

// Correlate claim traffic only; retain synchronous context for duplicate errors.
void Hook46(auto original, void* self, void* job)
{
  DispatchScope scope(ready.load() ? job : nullptr);
  ObserveDispatch(job);
  original(self, job);
}

void Hook47(auto original, void* self, void* job)
{
  DispatchScope scope(ready.load() ? job : nullptr);
  original(self, job);
}

void Hook48(auto original, void* self, void* response)
{
  ObserveRequest(self, "claim_request_http_callback", response, nullptr);
  original(self, response);
}
void Hook49(auto original, void* self, void* error, void* messages)
{
  void* job{};
  try {
    if (ready.load())
      Read(self, "_job", job);
  } catch (...) {
  }
  ObserveRequest(job, "claim_request_model_callback", nullptr, error);
  original(self, error, messages);
}
void Hook50(auto original, void* self, void* keys)
{
  original(self, keys);
  try {
    if (!ready.load())
      return;
    auto result = ClaimKeys(keys);
    if (!result.value("supported", false) || result.value("count", 0) > 0)
      Write({{"event", "recovery_timed_out_claims_drained"}, {"claims", result}});
  } catch (...) {
  }
}
void Hook51(auto original, void* self, int64_t key, int32_t state)
{
  try {
    if (ready.load())
      Write({{"event", "recovery_acquisition_changed"}, {"local_claim_key", key}, {"acquisition", state}});
  } catch (...) {
  }
  original(self, key, state);
}
void Hook52(auto original, void* self, int64_t key)
{
  try {
    if (ready.load())
      Write({{"event", "recovery_active_claim_removed"}, {"local_claim_key", key}});
  } catch (...) {
  }
  original(self, key);
}
bool Hook53(auto original, void* self, void* order, void* content)
{
  Span span(53, Kind::claim, self, -1);
  try {
    if (span.id)
      span.Details({{"order_token", StringToken(order)}, {"content_token", StringToken(content)}});
  } catch (...) {
  }
  const bool result = original(self, order, content);
  span.result       = result;
  return result;
}
void PointerEvent(const char* phase, uint64_t id, void* pointer, void* button, int state) noexcept
{
  try {
    if (!ready.load())
      return;
    auto* summary = BoundSummary();
    if (!summary) {
      Write({{"event", phase}, {"input_trace", id}, {"summary_bound", false}});
      return;
    }
    auto data            = SummaryInput(summary);
    data["event"]        = phase;
    data["input_trace"]  = id;
    data["button_state"] = state;
    data["pointer"]      = Pointer(pointer);
    if (button)
      data["clicked_button"] = Selectable(button, true);
    // EventSystem focus/selection are read from the event's own owner.
    void* system{};
    if (Read(pointer, "m_EventSystem", system)) {
      Scalar<bool>(data, system, "m_HasFocus", "event_system_focus");
      void* selected{};
      if (Read(system, "m_CurrentSelected", selected))
        data["selected"] = UiIdentity(selected);
    }
    Write(std::move(data));
  } catch (...) {
  }
}
void Hook54(auto original, void* self, void* data)
{
  void*    pointer{};
  int32_t  state = 3;
  uint64_t id{};
  try {
    if (ready.load() && BoundSummary() && Read(data, "buttonState", state) && state != 3
        && Read(data, "buttonData", pointer)) {
      id = nextSpan++;
      PointerEvent("summary_mouse_press_enter", id, pointer, nullptr, state);
    }
  } catch (...) {
  }
  original(self, data);
  if (id)
    PointerEvent("summary_mouse_press_exit", id, pointer, nullptr, state);
}
void Hook55(auto original, void* self, void* pointer)
{
  uint64_t id{};
  try {
    if (ready.load() && BoundSummary()) {
      id = nextSpan++;
      PointerEvent("summary_button_click_enter", id, pointer, self, -1);
    }
  } catch (...) {
  }
  original(self, pointer);
  if (id)
    PointerEvent("summary_button_click_exit", id, pointer, self, -1);
}

// ShopClaimsService.Tick inlines the drain in client263. Observe the substantive
// recovery Tick before its caller consumes the timeout list.
void Hook56(auto original, void* self)
{
  original(self);
  try {
    if (!ready.load())
      return;
    void*   timedOut{};
    int32_t count{};
    if (!Read(self, "_timedOutClaimKeys", timedOut) || !Read(timedOut, "_size", count) || count <= 0)
      return;
    auto  keys   = ClaimKeys(timedOut);
    Json  event  = {{"event", "recovery_tick_timed_out_claims"}, {"claims", keys}};
    auto* config = Invoke(self, "GetFeatureConfig");
    Scalar<float>(event, config, "SyncRetryIntervalSeconds", "configured_sync_retry_seconds");
    Scalar<float>(event, config, "RecoveryTimeoutSeconds", "configured_recovery_timeout_seconds");
    Write(std::move(event));
  } catch (...) {
  }
}

} // namespace
#endif

void InstallClaimTrace()
{
#if defined(_WIN32) && defined(_M_X64)
  if (!Preflight())
    return;
  const auto stamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
          .count();
  const auto filename =
      "community_claims_" + std::to_string(stamp) + "_" + std::to_string(GetCurrentProcessId()) + ".jsonl";
  try {
    logger = spdlog::rotating_logger_mt("claim-science", filename, 2 * 1024 * 1024, 2, false);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");
    logger->flush_on(spdlog::level::info);
  } catch (const std::exception& error) {
    spdlog::error("[ClaimTrace] could not open trace: {}", error.what());
    return;
  }
  size_t installed = 0;
  if (SPUD_STATIC_DETOUR(methods[0]->methodPointer, Hook0))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[1]->methodPointer, Hook1))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[2]->methodPointer, Hook2))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[3]->methodPointer, Hook3))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[4]->methodPointer, Hook4))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[5]->methodPointer, Hook5))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[6]->methodPointer, Hook6))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[7]->methodPointer, Hook7))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[8]->methodPointer, Hook8))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[9]->methodPointer, Hook9))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[10]->methodPointer, Hook10))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[11]->methodPointer, Hook11))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[12]->methodPointer, Hook12))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[13]->methodPointer, Hook13))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[14]->methodPointer, Hook14))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[15]->methodPointer, Hook15))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[16]->methodPointer, Hook16))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[17]->methodPointer, Hook17))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[18]->methodPointer, Hook18))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[19]->methodPointer, Hook19))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[20]->methodPointer, Hook20))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[21]->methodPointer, Hook21))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[22]->methodPointer, Hook22))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[23]->methodPointer, Hook23))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[24]->methodPointer, Hook24))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[25]->methodPointer, Hook25))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[26]->methodPointer, Hook26))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[27]->methodPointer, Hook27))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[28]->methodPointer, Hook28))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[29]->methodPointer, Hook29))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[30]->methodPointer, Hook30))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[31]->methodPointer, Hook31))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[32]->methodPointer, Hook32))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[33]->methodPointer, Hook33))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[34]->methodPointer, Hook34))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[35]->methodPointer, Hook35))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[36]->methodPointer, Hook36))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[37]->methodPointer, Hook37))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[38]->methodPointer, Hook38))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[39]->methodPointer, Hook39))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[40]->methodPointer, Hook40))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[41]->methodPointer, Hook41))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[42]->methodPointer, Hook42))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[43]->methodPointer, Hook43))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[44]->methodPointer, Hook44))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[45]->methodPointer, Hook45))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[46]->methodPointer, Hook46))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[47]->methodPointer, Hook47))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[48]->methodPointer, Hook48))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[49]->methodPointer, Hook49))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[50]->methodPointer, Hook50))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[51]->methodPointer, Hook51))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[52]->methodPointer, Hook52))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[53]->methodPointer, Hook53))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[54]->methodPointer, Hook54))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[55]->methodPointer, Hook55))
    ++installed;
  if (SPUD_STATIC_DETOUR(methods[56]->methodPointer, Hook56))
    ++installed;
  if (installed != std::size(kTargets) || !register_screen_manager_update_callback(Pulse)) {
    Write({{"event", "install_incomplete"}, {"hooks", installed}});
    spdlog::error("[ClaimTrace] incomplete install ({}/{}); installed wrappers remain pass-through", installed,
                  std::size(kTargets));
    return;
  }
  ready.store(true);
  Write(
      {{"event", "session"},
       {"schema", 6},
       {"client", 263},
       {"hooks", installed},
       {"file", filename},
       {"request_states",
        "0 unknown; 1 purchasing; 2 succeeded; 3 failed; 4 cancelled; 5 deferred; 6 timed out; 7 abandoned"},
       {"acquisition_states", "0 unknown; 1 not required; 2 waiting; 3 receiving; 4 granted; 5 failed"},
       {"presentation_states",
        "0 not required; 1 not started; 2 pending; 3 presenting; 4 up to date; 5 completed; 6 skipped"},
       {"pulse", "5 seconds for 5 minutes after claim/reward/error/message activity; latest weakly held objects only"},
       {"limits", "1200 events/minute; 2 MiB per file plus 2 rotations; 128 weak requests, 64 weak claims; no bodies, "
                  "raw IDs or UI text"},
       {"correlation", "session-local tokens; bounded to 512 orders, 512 semaphore identifiers and 512 request labels; "
                       "unsupported collections are explicit"}});
  spdlog::warn("[ClaimTrace] local client263 science probe active: {} ({} hooks)", filename, installed);
#endif
}

void TraceClaimNavigation(const char* action) noexcept
{
#if defined(_WIN32) && defined(_M_X64)
  try {
    if (!ready.load())
      return;
    auto* summary = BoundSummary();
    if (!summary)
      return;
    auto data      = SummaryInput(summary);
    data["event"]  = "summary_navigation_action";
    data["action"] = action;
    Write(std::move(data));
  } catch (...) {
  }
#endif
}
