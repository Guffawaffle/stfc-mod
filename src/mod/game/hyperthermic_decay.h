#pragma once

// Read the game's current player-wide automatic buffs. No service creation,
// scene discovery, network requests, or combat-history reconstruction.
#include "settings.h"

#include <il2cpp/il2cpp.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace hyperthermic_decay_binding
{
struct BuffData;
using BuffList = il2cpp::List<BuffData*>;

using DateTimeClass = il2cpp::Class<"mscorlib", "System", "DateTime">;
struct NativeDateTime {
  uint64_t date_data = 0;

  int64_t Ticks() const
  { return static_cast<int64_t>(date_data & uint64_t{0x3fffffffffffffff}); }
};
static_assert(sizeof(NativeDateTime) == 8);

struct DateTimeUtils {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Networking.Core", "DateTimeUtils">;
};

// Native HasExpired uses this server-adjusted clock, rather than the PC clock.
inline il2cpp::CallResult<int64_t> TryServerTicks()
{
  static const auto* getter = [] {
    const auto* method =
        il2cpp::detail::MethodCall<NativeDateTime()>::Resolve(DateTimeUtils::Class::Raw(), "get_UtcNow", true);
    return method
                   && il2cpp::detail::ValueClassMatches(method->return_type, DateTimeClass::Raw(),
                                                        sizeof(NativeDateTime))
               ? method
               : nullptr;
  }();
  if (!getter)
    return {};
  const auto value = il2cpp::detail::TryInvoke<NativeDateTime>(getter, nullptr);
  return value ? il2cpp::CallResult<int64_t>{true, value.value.Ticks()} : il2cpp::CallResult<int64_t>{};
}

struct BuffData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BuffData">;

  il2cpp::CallResult<int64_t> TryId()
  { return TryCall<"get_Id", int64_t>(); }
  il2cpp::CallResult<int32_t> TryRank()
  { return TryCall<"get_Rank", int32_t>(); }
  il2cpp::CallResult<double> TryValue()
  { return TryCall<"get_Value", double>(); }
  il2cpp::CallResult<bool> TryHasExpired()
  { return TryCall<"get_HasExpired", bool>(); }

  il2cpp::CallResult<std::optional<int64_t>> TryExpiryTicks()
  {
    using Expiry       = il2cpp::Nullable<NativeDateTime>;
    static auto* field = [] {
      auto* candidate = il2cpp::detail::ResolveField(Class::Raw(), "<ExpiresAt>k__BackingField",
                                                     il2cpp::detail::SlotOf<Expiry>(), false);
      return candidate
                     && il2cpp::detail::NullableLayoutMatches(candidate->type, DateTimeClass::Raw(), sizeof(Expiry),
                                                              alignof(Expiry), offsetof(Expiry, has_value),
                                                              offsetof(Expiry, value))
                 ? candidate
                 : nullptr;
    }();
    if (!field)
      return {};
    // Client 275's generated getter only reads this backing field. Qualify the
    // exact Nullable<DateTime> storage before copying it; no box or aggregate
    // return ABI is needed. Metadata drift remains an unavailable observation.
    const auto expiry = il2cpp::detail::ReadField<Expiry>(field, this);
    if (!expiry.has_value)
      return {true, std::nullopt};
    return {true, expiry.value.Ticks()};
  }
};

struct Service : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService">;
};

inline bool BuffListClass(const il2cpp::Object& object)
{
  return il2cpp::detail::Lookup<BuffList>(object, [](Il2CppClass* klass) {
    if (!klass || il2cpp_class_is_valuetype(klass)
        || std::strcmp(il2cpp_class_get_namespace(klass), "System.Collections.Generic")
        || std::strcmp(il2cpp_class_get_name(klass), "List`1"))
      return false;
    auto* field = il2cpp::detail::ResolveField(klass, "_items", il2cpp::detail::SlotOf<il2cpp::ArrayObject*>(), false);
    const auto* type    = field ? field->type : nullptr;
    const auto* element = type && !type->byref && type->type == IL2CPP_TYPE_SZARRAY ? type->data.type : nullptr;
    return element && !element->byref && il2cpp_class_from_type(element) == BuffData::Class::Raw();
  });
}

struct Manager : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Buffs", "BuffsManager">;

  il2cpp::CallResult<Service*> TryExistingService()
  {
    struct CachedService {
      Service* service = nullptr;
    };
    static auto* field =
        il2cpp::detail::ResolveField(Class::Raw(), "_cachedService", il2cpp::detail::SlotOf<CachedService>(), false);
    static const bool qualified = [] {
      auto* type = field && field->type && !field->type->byref ? il2cpp_class_from_type(field->type) : nullptr;
      if (!type || !il2cpp_class_is_valuetype(type) || il2cpp::ClassName(type) != "Digit.Client.UI.CachedService`1")
        return false;
      uint32_t alignment = 0;
      if (il2cpp_class_value_size(type, &alignment) != sizeof(CachedService))
        return false;
      auto* cached = il2cpp::detail::ResolveField(type, "_cachedService", il2cpp::detail::SlotOf<Service*>(), false);
      return cached && cached->offset == sizeof(Il2CppObject);
    }();
    if (!qualified)
      return {};
    auto* service = il2cpp::detail::ReadField<CachedService>(field, this).service;
    return !service || service->As<Service>() ? il2cpp::CallResult<Service*>{true, service}
                                              : il2cpp::CallResult<Service*>{};
  }

  // The returned list is borrowed. Root it while traversing, and never retain
  // its elements beyond that observation. A missing baseline is unavailable,
  // not an observed empty buff set.
  il2cpp::CallResult<BuffList*> TryActiveBuffs()
  {
    // The native facade looks up an existing registry service and warms its
    // own cache. It never constructs services or requests server data.
    const auto value = TryCall<"GetAllActiveUserBuffs", settings_binding::ReferenceObject*>();
    if (!value || !value.value || !BuffListClass(*value.value))
      return {};
    auto list_root = il2cpp::GcHandle::Strong(value.value);
    if (!list_root)
      return {};
    const auto service = TryExistingService();
    auto       root    = il2cpp::GcHandle::Strong(service ? service.value : nullptr);
    if (!root)
      return {};
    auto*      current = root.Get<Service>();
    const auto ready   = current->TryCall<"HasData", bool>();
    if (!ready || !ready.value)
      return {};
    return {true, reinterpret_cast<BuffList*>(value.value)};
  }
};

inline il2cpp::CallResult<Manager*> TryCurrentManager()
{ return settings_binding::ExistingSingleton<Manager>(); }
} // namespace hyperthermic_decay_binding
