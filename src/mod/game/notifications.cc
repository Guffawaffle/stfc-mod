// Qualified native incoming-attack payload adapter, preserving PLAY's identity namespaces and assembly fallback.
#include "notifications.h"
#include <cstdint>
#include <il2cpp/il2cpp.h>
#include <optional>
#include <string>

namespace incoming_attack_binding
{
Il2CppClass* Model(const char* name)
{
  auto* cls = il2cpp::detail::FindClass("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", name);
  return cls ? cls : il2cpp::detail::FindClass("Assembly-CSharp", "Digit.PrimeServer.Models", name);
}
FieldInfo* Field(Il2CppClass* cls, const char* name, const char* type)
{
  auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field && field->type && field->offset >= sizeof(Il2CppObject) && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC)
                 && il2cpp::ClassName(il2cpp_class_from_type(field->type)) == type
             ? field
             : nullptr;
}
struct Api {
  Il2CppClass *     toast = nullptr, *payload = nullptr, *fleet = nullptr, *profile = nullptr, *notification = nullptr;
  FieldInfo *       data = nullptr, *attack_type = nullptr, *target = nullptr, *attacker = nullptr, *event = nullptr;
  FieldInfo *       owner = nullptr, *fleet_id = nullptr, *slot = nullptr, *uuid = nullptr, *name = nullptr;
  const MethodInfo *has_ship = nullptr, *is_local = nullptr;
  int               player_type = -1;
  Api()
  {
    toast        = Toast::Class::Raw();
    payload      = Model("IncomingAttackNotificationData");
    fleet        = Model("FleetPlayerData");
    profile      = Model("UserProfile");
    notification = Model("Notification");
    data         = Field(toast, "<Data>k__BackingField", "System.Object");
    target       = Field(payload, "<TargetedFleet>k__BackingField", "Digit.PrimeServer.Models.FleetPlayerData");
    attacker     = Field(payload, "<AttackerUserData>k__BackingField", "Digit.PrimeServer.Models.UserProfile");
    event        = Field(payload, "<Notification>k__BackingField", "Digit.PrimeServer.Models.Notification");
    owner        = Field(fleet, "<User>k__BackingField", "Digit.PrimeServer.Models.UserProfile");
    fleet_id     = Field(fleet, "<ID>k__BackingField", "System.Int64");
    slot         = Field(fleet, "<Index>k__BackingField", "System.Int32");
    uuid         = Field(notification, "uuid_", "System.String");
    name         = Field(profile, "name_", "System.String");
    has_ship     = il2cpp::detail::ResolveMethod(fleet, "get_HasShip", il2cpp::detail::SignatureOf<bool>(), false);
    is_local = il2cpp::detail::ResolveMethod(profile, "get_IsLocalPlayer", il2cpp::detail::SignatureOf<bool>(), false);
    // Read the runtime enum literal rather than assuming Player remains zero after a client update.
    auto*       kind = payload ? il2cpp_class_get_field_from_name(payload, "<AttackType>k__BackingField") : nullptr;
    auto*       kind_class = kind && kind->type ? il2cpp_class_from_type(kind->type) : nullptr;
    const auto* base =
        kind_class && il2cpp_class_is_enum(kind_class) ? il2cpp_class_enum_basetype(kind_class) : nullptr;
    auto* player  = kind_class ? il2cpp_class_get_field_from_name(kind_class, "Player") : nullptr;
    auto* faction = kind_class ? il2cpp_class_get_field_from_name(kind_class, "Faction") : nullptr;
    if (kind && kind->offset >= sizeof(Il2CppObject) && !(kind->type->attrs & FIELD_ATTRIBUTE_STATIC) && base
        && base->type == IL2CPP_TYPE_I4 && player && faction && (player->type->attrs & FIELD_ATTRIBUTE_LITERAL)
        && (faction->type->attrs & FIELD_ATTRIBUTE_LITERAL)) {
      int faction_type = -1;
      il2cpp_field_static_get_value(player, &player_type);
      il2cpp_field_static_get_value(faction, &faction_type);
      if (player_type != faction_type)
        attack_type = kind;
    }
  }
  bool Valid() const
  {
    return data && attack_type && target && attacker && event && owner && fleet_id && slot && uuid && name && has_ship
           && is_local;
  }
};
Api& NativeApi()
{
  static Api api;
  return api;
}
template <typename T> T Read(Il2CppObject* object, FieldInfo* field)
{
  T value{};
  if (object && field)
    il2cpp_field_get_value(object, field, &value);
  return value;
}
bool Is(Il2CppObject* object, Il2CppClass* cls)
{ return object && cls && il2cpp_object_get_class(object) == cls; }
bool True(const MethodInfo* method, Il2CppObject* object)
{
  if (!object || !method)
    return false;
  const auto observed = reinterpret_cast<il2cpp::Object*>(object)->TryCall<"get_IsLocalPlayer", bool>();
  return observed && observed.value;
}
std::string String(Il2CppObject* object, FieldInfo* field)
{
  auto* value = Read<Il2CppString*>(object, field);
  return value && value->length > 0 && value->length <= 256 ? il2cpp::ToUtf8(value) : std::string{};
}
} // namespace incoming_attack_binding

bool IncomingPlayerAttackAvailable()
{ return incoming_attack_binding::NativeApi().Valid(); }

std::optional<IncomingPlayerAttack> ReadIncomingPlayerAttack(Toast* toast)
{
  using namespace incoming_attack_binding;
  auto& api        = NativeApi();
  auto  toast_root = il2cpp::GcHandle::Strong(toast);
  auto* object     = reinterpret_cast<Il2CppObject*>(toast);
  if (!api.Valid() || !Is(object, api.toast))
    return {};
  auto* payload      = Read<Il2CppObject*>(object, api.data);
  auto  payload_root = il2cpp::GcHandle::Strong(payload);
  if (!Is(payload, api.payload) || Read<int>(payload, api.attack_type) != api.player_type)
    return {};
  auto*      fleet      = Read<Il2CppObject*>(payload, api.target);
  auto       fleet_root = il2cpp::GcHandle::Strong(fleet);
  const auto ship =
      fleet ? reinterpret_cast<il2cpp::Object*>(fleet)->TryCall<"get_HasShip", bool>() : il2cpp::CallResult<bool>{};
  if (!Is(fleet, api.fleet) || !ship || !ship.value)
    return {}; // Station attacks have no targeted ship.
  auto* owner      = Read<Il2CppObject*>(fleet, api.owner);
  auto  owner_root = il2cpp::GcHandle::Strong(owner);
  if (!Is(owner, api.profile) || !True(api.is_local, owner))
    return {};
  IncomingPlayerAttack result;
  result.fleet_id = Read<long long>(fleet, api.fleet_id);
  if (result.fleet_id <= 0)
    return {};
  result.slot         = Read<int>(fleet, api.slot);
  auto* attacker      = Read<Il2CppObject*>(payload, api.attacker);
  auto  attacker_root = il2cpp::GcHandle::Strong(attacker);
  if (Is(attacker, api.profile)) {
    if (True(api.is_local, attacker))
      return {};
    result.attacker = String(attacker, api.name);
  }
  auto* event      = Read<Il2CppObject*>(payload, api.event);
  auto  event_root = il2cpp::GcHandle::Strong(event);
  if (Is(event, api.notification))
    result.event_id = String(event, api.uuid);
  return result;
}
