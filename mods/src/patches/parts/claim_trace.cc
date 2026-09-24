// Local Windows client263 science instrumentation. Observe only; never change claim/UI state.
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
Clock::time_point                                  activeUntil{}, nextPulse{}, rateStart{};
uint64_t                                           eventSequence{}, suppressed{};
size_t                                             rateCount{};

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
std::mutex tokenMutex;
std::vector<std::pair<std::u16string, uint64_t>> orderTokens;
std::vector<std::pair<int64_t, uint64_t>> semaphoreTokens;
uint64_t nextToken = 1;
template <typename T> uint64_t Token(std::vector<std::pair<T, uint64_t>>& tokens, const T& value)
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

Json Orders(void* collection)
{
  Json result = {{"present", collection != nullptr}, {"supported", false}};
  if (!collection)
    return result;
  auto* cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(collection));
  void* array = collection;
  int32_t count{};
  if (il2cpp_class_get_rank(cls) != 1) {
    if (std::strcmp(il2cpp_class_get_namespace(cls), "System.Collections.Generic") != 0
        || std::strcmp(il2cpp_class_get_name(cls), "List`1") != 0
        || !Read(collection, "_size", count) || !Read(collection, "_items", array) || !array)
      return result;
    cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(array));
  } else {
    count = static_cast<int32_t>(il2cpp_array_length(static_cast<Il2CppArray*>(array)));
  }
  auto* element = il2cpp_class_get_element_class(cls);
  if (il2cpp_class_get_rank(cls) != 1 || !element
      || std::strcmp(il2cpp_class_get_namespace(element), "System") != 0
      || std::strcmp(il2cpp_class_get_name(element), "String") != 0
      || il2cpp_array_element_size(cls) != sizeof(void*) || count < 0 || count > 128
      || static_cast<uint32_t>(count) > il2cpp_array_length(static_cast<Il2CppArray*>(array)))
    return result;
  result["supported"] = true;
  result["count"] = count;
  result["tokens"] = Json::array();
  auto* items = reinterpret_cast<Il2CppString**>(static_cast<Il2CppArraySize*>(array)->vector);
  for (int32_t index = 0; index < count; ++index) {
    auto* item = items[index];
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
  Scalar<int32_t>(result, object, "_presentationHint", "presentation_hint");
  void* orders{};
  if (Read(object, "_orderIds", orders))
    result["orders"] = Orders(orders);
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
    if (Read(listener, "_button", button) && button)
      Scalar<bool>(result, button, "m_Interactable", "interactable");
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
  auto result = View(object);
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

enum class Kind { none, claim, reward, chest, storyboard, orders, error, semaphore, lock, message };
Json Snapshot(Kind kind, void* object)
{
  switch (kind) {
    case Kind::claim:
      return Claim(object);
    case Kind::reward:
      return Reward(object);
    case Kind::chest:
      return Chest(object);
    case Kind::storyboard:
      return Storyboard(object);
    case Kind::orders:
      return Orders(object);
    case Kind::message: {
      auto result = View(object);
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
      }
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

void Pulse()
{
  if (!ready.load())
    return;
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
             {"latest_message", Snapshot(Kind::message, latestMessage ? il2cpp_gchandle_get_target(latestMessage) : nullptr)},
             {"shortcut_capture", Key::shortcutCaptureActive},
             {"shortcut_popup", Key::shortcutPopupActive}};
  }
  Write(std::move(event));
}

bool Preflight()
{
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
  if (!base)
    return false;
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
  Span       span(0, Kind::claim, claim, -1);
  const bool result = original(self, claim, choices, callback);
  span.result       = result;
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
  Span span(5, Kind::claim, claim, -1);
  original(self, claim);
}

void Hook6(auto original, void* self, void* claim)
{
  Span span(6, Kind::claim, claim, -1);
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
  original(self, error);
}

void Hook32(auto original, void* self, int32_t category, int64_t identifier, int32_t timeout)
{
  Span span(32, Kind::semaphore, self, category);
  try {
    if (span.id)
      span.Details({{"category", category}, {"identifier_token", Token(semaphoreTokens, identifier)},
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
  if (installed != std::size(kTargets) || !register_screen_manager_update_callback(Pulse)) {
    Write({{"event", "install_incomplete"}, {"hooks", installed}});
    spdlog::error("[ClaimTrace] incomplete install ({}/{}); installed wrappers remain pass-through", installed,
                  std::size(kTargets));
    return;
  }
  ready.store(true);
  Write(
      {{"event", "session"},
       {"schema", 2},
       {"client", 263},
       {"hooks", installed},
       {"file", filename},
       {"request_states",
        "0 unknown; 1 purchasing; 2 succeeded; 3 failed; 4 cancelled; 5 deferred; 6 timed out; 7 abandoned"},
       {"acquisition_states", "0 unknown; 1 not required; 2 waiting; 3 receiving; 4 granted; 5 failed"},
       {"presentation_states",
        "0 not required; 1 not started; 2 pending; 3 presenting; 4 up to date; 5 completed; 6 skipped"},
       {"pulse", "5 seconds for 5 minutes after claim/reward/error/message activity; latest weakly held objects only"},
       {"limits",
        "1200 events/minute; 2 MiB per file plus 2 rotations; no request bodies, raw order IDs or account data"},
       {"correlation", "session-local tokens; bounded to 512 orders and 512 semaphore identifiers; unsupported collections are explicit"}});
  spdlog::warn("[ClaimTrace] local client263 science probe active: {} ({} hooks)", filename, installed);
#endif
}
