#pragma once

// The action queue (Kir'Shara): the manager, one queue of target fleets per dock slot, and the course responses
// that drive it.

#include "fleet.h"

#include <il2cpp/il2cpp.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

enum class EngageResult : int32_t {
  Success    = 0,
  SkipTarget = 1,
  Stop       = 2,
};

struct QueueableAction : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Prime.ActionQueue", "QueueableAction">;

  // The queued target's fleet id.
  int64_t FleetId()
  { return Field<"<FleetId>k__BackingField", int64_t>(); }
};

// Qualify managed acquisition before using the runtime IList facade. Native pointer fields are not objects.
struct QueueListReference : il2cpp::Object {
  struct Class {
    static Il2CppClass* Raw()
    { return il2cpp::ObjectClass(); }
  };
};

struct ActionQueueInstance : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance">;

  // Unknown latches cannot be interpreted as an idle queue. Resolve these contracts before any queue fix acts.
  static bool SafetyFieldsAvailable()
  {
    static const bool qualified =
        Class::HasField<"IsEngaging", bool>() && Class::HasField<"LastEngageAttemptTime", float>()
        && Class::HasField<"LastEngagedTargetId", int64_t>() && Class::HasField<"PendingEngageTargetId", int64_t>()
        && Class::HasField<"<PlayerFleetId>k__BackingField", int64_t>()
        && Class::HasField<"_actionQueue", QueueListReference*>()
        && QueueableAction::Class::HasField<"<FleetId>k__BackingField", int64_t>();
    return qualified;
  }

  bool IsEngaging()
  { return Field<"IsEngaging", bool>(); }
  float LastEngageAttemptTime()
  { return Field<"LastEngageAttemptTime", float>(); }
  int64_t LastEngagedTargetId()
  { return Field<"LastEngagedTargetId", int64_t>(); }
  int64_t PendingEngageTargetId()
  { return Field<"PendingEngageTargetId", int64_t>(); }
  int64_t PlayerFleetId()
  { return Field<"<PlayerFleetId>k__BackingField", int64_t>(); }
  // IList<QueueableAction>, a List`1 in practice.
  il2cpp::List<QueueableAction*>* actionQueue()
  { return reinterpret_cast<il2cpp::List<QueueableAction*>*>(Field<"_actionQueue", QueueListReference*>()); }
};

// Digit.PrimeServer.Events.SetCourseResponseEventArgs, passed by value. The method lookup checks its size; the
// field order follows the client's dump.
struct SetCourseResponse {
  int64_t fleet_id;
  bool    success;
  bool    is_recall;
  // A boxed Int64.
  il2cpp::Object* target_id;
};
static_assert(sizeof(SetCourseResponse) == 24 && offsetof(SetCourseResponse, target_id) == 16);

namespace queue_binding
{
inline bool EngageResultLayout(il2cpp::Method candidate)
{
  const auto* method = candidate.Info();
  auto*       type   = method && method->return_type ? il2cpp_class_from_type(method->return_type) : nullptr;
  return type && method->return_type->type == IL2CPP_TYPE_VALUETYPE && !method->return_type->byref
         && il2cpp_class_is_enum(type) && std::strcmp(il2cpp_class_get_namespace(type), "Digit.Prime.Combat") == 0
         && std::strcmp(il2cpp_class_get_name(type), "EngageResult") == 0
         && il2cpp::detail::ExactScalarMatches(il2cpp_class_enum_basetype(type), IL2CPP_TYPE_I4);
}

// Size alone does not qualify a value passed by value: a reordered response can still be 24 bytes.
// Inspect the callback's actual parameter so consumers share one ABI gate without guessing its assembly.
inline bool ResponseLayout(il2cpp::Method candidate)
{
  const auto* method = candidate.Info();
  if (!method || !method->parameters || method->parameters_count != 1 || method->has_full_generic_sharing_signature
      || !method->parameters[0] || method->parameters[0]->byref || method->parameters[0]->type != IL2CPP_TYPE_VALUETYPE)
    return false;
  auto* expected = il2cpp_class_from_type(method->parameters[0]);
  if (!expected || std::strcmp(il2cpp_class_get_namespace(expected), "Digit.PrimeServer.Events") != 0
      || std::strcmp(il2cpp_class_get_name(expected), "SetCourseResponseEventArgs") != 0)
    return false;
  uint32_t alignment = 0;
  if (il2cpp_class_value_size(expected, &alignment) != sizeof(SetCourseResponse))
    return false;
  const auto field = [expected](const char* name, size_t offset, Il2CppTypeEnum type) {
    auto* value = il2cpp_class_get_field_from_name(expected, name);
    return value && value->type && !value->type->byref && !(value->type->attrs & FIELD_ATTRIBUTE_STATIC)
           && value->offset == static_cast<int32_t>(sizeof(Il2CppObject) + offset) && value->type->type == type;
  };
  return field("<FleetId>k__BackingField", offsetof(SetCourseResponse, fleet_id), IL2CPP_TYPE_I8)
         && field("<Success>k__BackingField", offsetof(SetCourseResponse, success), IL2CPP_TYPE_BOOLEAN)
         && field("<IsRecall>k__BackingField", offsetof(SetCourseResponse, is_recall), IL2CPP_TYPE_BOOLEAN)
         && field("<TargetId>k__BackingField", offsetof(SetCourseResponse, target_id), IL2CPP_TYPE_OBJECT);
}
} // namespace queue_binding

struct ActionQueueManager : MonoSingleton<ActionQueueManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager">;

  // One queue per dock slot, indexed by FleetPlayerData.Index; empty slots are null.
  il2cpp::Array<ActionQueueInstance*>* battleQueue()
  {
    // An unbound Array<T> facade alone accepts arbitrary reference/pointer metadata. Qualify the actual vector
    // before reading its header; unavailable storage must not establish an empty set of queues.
    auto* source = Field<"_battleQueue", il2cpp::ArrayObject*>();
    return il2cpp::TryReferenceArray<ActionQueueInstance*>(source).value;
  }

  void ClearQueue(FleetPlayerData* playerData)
  { Call<"ClearQueue">(playerData); }
  bool IsQueueFull(FleetPlayerData* playerData)
  { return Call<"IsQueueFull", bool>(playerData); }
  bool IsQueueUnlocked()
  { return Call<"IsQueueUnlocked", bool>(); }
  EngageResult TryPlanPathAndEngageTarget(FleetPlayerData* fleet, ActionQueueInstance* queue)
  { return Call<"TryPlanPathAndEngageTarget", EngageResult(FleetPlayerData*, ActionQueueInstance*)>(fleet, queue); }
  // Removes the target fleet from every queue that holds it, as the game does when a target goes away.
  void ProcessQueue(int64_t target_fleet_id, bool can_select_new_target)
  { Call<"ProcessQueue", void(int64_t, bool)>(target_fleet_id, can_select_new_target); }
};

struct DeploymentService : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "DeploymentService">;
};
