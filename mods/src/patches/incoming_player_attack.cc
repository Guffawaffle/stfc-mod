#include "incoming_player_attack.h"
#include "fleet_notification_settings.h"
#include "incoming_player_attack_tracker.h"
#include "str_utils.h"
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <mutex>
#include <prime/Toast.h>
#include <spdlog/spdlog.h>

namespace
{
Il2CppClass* Model(const char* name)
{
  auto* cls = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", name).get_cls();
  return cls ? cls : il2cpp_get_class_helper("Assembly-CSharp", "Digit.PrimeServer.Models", name).get_cls();
}
FieldInfo* Field(Il2CppClass* cls, const char* name, const char* type)
{
  auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field && field->type && field->offset >= sizeof(Il2CppObject) && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC)
                 && method_contract::Type(field->type, type)
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
    toast        = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "Toast").get_cls();
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
    has_ship     = method_contract::Resolve(fleet, "get_HasShip", false, "System.Boolean", {});
    is_local     = method_contract::Resolve(profile, "get_IsLocalPlayer", false, "System.Boolean", {});
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
  Il2CppObject* result = nullptr;
  bool          value  = false;
  return object && Il2CppRuntime::TryInvoke(method, object, nullptr, &result)
         && Il2CppRuntime::TryBoolean(result, value) && value;
}
std::string String(Il2CppObject* object, FieldInfo* field)
{
  auto* value = Read<Il2CppString*>(object, field);
  return value && value->length > 0 && value->length <= 256 ? to_string(value) : std::string{};
}
} // namespace

bool IncomingPlayerAttackAvailable()
{
  const bool  available = ToastAudioAvailable() && NativeApi().Valid();
  static bool reported  = false;
  if (!reported) {
    spdlog::info("[IncomingPlayerAttack] ships-only payload available={}", available);
    reported = true;
  }
  return available;
}

std::optional<IncomingPlayerAttack> ParseIncomingPlayerAttack(Toast* toast)
{
  auto& api    = NativeApi();
  auto* object = reinterpret_cast<Il2CppObject*>(toast);
  if (!api.Valid() || !Is(object, api.toast))
    return {};
  auto* payload = Read<Il2CppObject*>(object, api.data);
  if (!Is(payload, api.payload) || Read<int>(payload, api.attack_type) != api.player_type)
    return {};
  auto* fleet = Read<Il2CppObject*>(payload, api.target);
  if (!Is(fleet, api.fleet) || !True(api.has_ship, fleet))
    return {}; // Station attacks have no targeted ship.
  auto* owner = Read<Il2CppObject*>(fleet, api.owner);
  if (!Is(owner, api.profile) || !True(api.is_local, owner))
    return {};
  IncomingPlayerAttack result;
  result.fleet_id = Read<long long>(fleet, api.fleet_id);
  if (result.fleet_id <= 0)
    return {};
  result.slot    = Read<int>(fleet, api.slot);
  auto* attacker = Read<Il2CppObject*>(payload, api.attacker);
  if (Is(attacker, api.profile)) {
    if (True(api.is_local, attacker))
      return {};
    result.attacker = String(attacker, api.name);
  }
  auto* event = Read<Il2CppObject*>(payload, api.event);
  if (Is(event, api.notification))
    result.event_id = String(event, api.uuid);
  return result;
}

bool FirstIncomingPlayerAttack(const IncomingPlayerAttack& attack)
{
  static IncomingPlayerAttackTracker tracker;
  static std::mutex                  mutex;
  std::lock_guard                    lock(mutex);
  return tracker.Accept(attack.event_id, attack.fleet_id, attack.attacker, IncomingPlayerAttackTracker::Clock::now());
}
