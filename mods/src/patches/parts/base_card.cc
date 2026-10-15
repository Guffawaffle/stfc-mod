#include "config.h"
#include "patches/base_card.h"
#include "settings/preview_settings.h"

#include <atomic>

// As in action_queue.cc, the 24-byte managed value uses the host ABI: indirect on Windows/ARM64,
// by value on the Mac Intel stack. Unsupported architectures keep native behavior.
#if (defined(_WIN32) && defined(_M_X64)) || (defined(__APPLE__) && (defined(__arm64__) || defined(__x86_64__)))
#include "prime/ObjectViewerBaseWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/PreScanTargetWidget.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <tuple>

namespace
{
std::atomic_bool ready{false};

struct CourseResponse {
  std::int64_t  fleet;
  bool          success, recall;
  unsigned char padding[6];
  Il2CppObject* target;
};
static_assert(sizeof(CourseResponse) == 24 && offsetof(CourseResponse, target) == 16);

Il2CppClass *     battleClass{}, *navigationClass{}, *int64Class{}, *stringClass{};
FieldInfo *       previewBattle{}, *navigationPoi{}, *navigationUser{};
const MethodInfo *previewContext{}, *navigationContext{}, *scanningInstance{}, *targetCache{};
const MethodInfo *isStarbase{}, *isOwnStation{}, *isValid{}, *playerId{}, *targetId{}, *targetUser{};

Il2CppObject* Invoke(const MethodInfo* method, Il2CppObject* object)
{
  Il2CppObject* result{};
  return Il2CppRuntime::TryInvoke(method, object, nullptr, &result) ? result : nullptr;
}

bool Boolean(const MethodInfo* method, Il2CppObject* object, bool expected)
{
  bool value{};
  return Il2CppRuntime::TryBoolean(Invoke(method, object), value) && value == expected;
}

bool Integer(const MethodInfo* method, Il2CppObject* object, std::int64_t& value)
{
  auto* boxed = Invoke(method, object);
  if (!boxed || il2cpp_object_get_class(boxed) != int64Class)
    return false;
  std::memcpy(&value, il2cpp_object_unbox(boxed), sizeof(value));
  return true;
}

Il2CppObject* ReadObject(Il2CppObject* object, FieldInfo* field)
{
  Il2CppObject* result{};
  if (object && field)
    il2cpp_field_get_value(object, field, &result);
  return result;
}

bool SameUser(Il2CppObject* left, Il2CppObject* right)
{
  if (!left || !right || il2cpp_object_get_class(left) != stringClass || il2cpp_object_get_class(right) != stringClass)
    return false;
  const auto* a = reinterpret_cast<Il2CppString*>(left);
  const auto* b = reinterpret_cast<Il2CppString*>(right);
  return a->length > 0 && a->length == b->length
         && std::memcmp(a->chars, b->chars, a->length * sizeof(Il2CppChar)) == 0;
}

bool Enabled(const CourseResponse& response)
{
  return ready.load() && Config::Get().keep_base_card_open && response.success && response.fleet != 0
         && (response.recall || response.target);
}

bool MatchesBaseCourse(Il2CppObject* data, const CourseResponse& response)
{
  if (!data || il2cpp_object_get_class(data) != battleClass || !Boolean(isStarbase, data, true)
      || !Boolean(isOwnStation, data, false) || !Boolean(isValid, data, true))
    return false;
  std::int64_t fleet{};
  if (!Integer(playerId, data, fleet) || fleet != response.fleet)
    return false;
  // Recall targets home, not the viewed station, and may have no target payload.
  // The active preview/context checks still identify the base whose card we retain.
  if (response.recall)
    return true;
  // Station courses can identify their target by its hashed ID or by its player UID. Never retain
  // a card for a different attack target, an empty-space course, or an unrecognized response.
  if (il2cpp_object_get_class(response.target) == int64Class) {
    std::int64_t actual{}, expected{};
    std::memcpy(&actual, il2cpp_object_unbox(response.target), sizeof(actual));
    return Integer(targetId, data, expected) && expected != 0 && expected != -1 && actual == expected;
  }
  return SameUser(response.target, Invoke(targetUser, data));
}

bool MatchesPreview(Il2CppObject* context, Il2CppObject* data)
{
  return context && il2cpp_object_get_class(context) == navigationClass && ReadObject(context, navigationPoi)
         && SameUser(ReadObject(context, navigationUser), Invoke(targetUser, data));
}

void PreviewResponse(auto original, Il2CppObject* widget, CourseResponse response)
{
  if (Enabled(response)) {
    auto* data = ReadObject(widget, previewBattle);
    if (MatchesBaseCourse(data, response) && MatchesPreview(Invoke(previewContext, widget), data))
      return;
  }
  original(widget, response);
}

void NavigationResponse(auto original, Il2CppObject* controller, CourseResponse response)
{
  if (Enabled(response)) {
    auto* context = Invoke(navigationContext, controller);
    if (context && il2cpp_object_get_class(context) == navigationClass && ReadObject(context, navigationPoi)) {
      auto* manager = Invoke(scanningInstance, nullptr);
      auto* data    = manager ? Invoke(targetCache, manager) : nullptr;
      // This native handler only resets interaction data or schedules that reset after a semaphore
      // release. Do not schedule a delayed close either; fleet commands/semaphores run independently.
      if (MatchesBaseCourse(data, response) && MatchesPreview(context, data))
        return;
    }
  }
  original(controller, response);
}

FieldInfo* ReferenceField(Il2CppClass* cls, const char* name, const char* type)
{
  auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field && field->type && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC)
                 && method_contract::Type(field->type, type)
             ? field
             : nullptr;
}

const MethodInfo* ContextGetter(Il2CppClass* cls, const char* name)
{
  // These getters belong to closed Widget<T>/ViewController<T> parents. runtime_invoke handles the
  // inflated MethodInfo; check the returned managed object's class before reading any of its fields.
  auto* property = cls ? il2cpp_class_get_property_from_name(cls, name) : nullptr;
  auto* getter   = property ? property->get : nullptr;
  return getter && getter->parameters_count == 0 && !(getter->flags & METHOD_ATTRIBUTE_STATIC) ? getter : nullptr;
}

const MethodInfo* SingletonGetter(Il2CppClass* cls)
{
  for (auto* parent = cls; parent; parent = il2cpp_class_get_parent(parent)) {
    const auto* method = il2cpp_class_get_method_from_name(parent, "get_Instance", 0);
    if (method && (method->flags & METHOD_ATTRIBUTE_STATIC) && method->parameters_count == 0
        && il2cpp_class_from_type(method->return_type) == cls)
      return method;
  }
  return nullptr;
}

bool ResponseLayout(const MethodInfo* method)
{
  if (!method || method->has_full_generic_sharing_signature || method->parameters[0]->type != IL2CPP_TYPE_VALUETYPE)
    return false;
  auto*         cls = il2cpp_class_from_type(method->parameters[0]);
  std::uint32_t alignment{};
  if (!cls || il2cpp_class_value_size(cls, &alignment) != sizeof(CourseResponse))
    return false;
  for (const auto& [name, offset, type] : {std::tuple{"<FleetId>k__BackingField", 0x10, IL2CPP_TYPE_I8},
                                           std::tuple{"<Success>k__BackingField", 0x18, IL2CPP_TYPE_BOOLEAN},
                                           std::tuple{"<IsRecall>k__BackingField", 0x19, IL2CPP_TYPE_BOOLEAN},
                                           std::tuple{"<TargetId>k__BackingField", 0x20, IL2CPP_TYPE_OBJECT}}) {
    auto* field = il2cpp_class_get_field_from_name(cls, name);
    if (!field || il2cpp_field_get_offset(field) != offset || !field->type || field->type->type != type)
      return false;
  }
  return true;
}
} // namespace

bool base_card::KeepOpenForRecall(std::int64_t fleet)
{
  const CourseResponse response{fleet, true, true, {}, nullptr};
  if (!Enabled(response))
    return false;
  for (auto* widget : ObjectFinder<PreScanTargetWidget>::GetAll()) {
    auto* visibility = widget ? widget->_visibilityController : nullptr;
    if (!visibility || (visibility->_state != VisibilityState::Show && visibility->_state != VisibilityState::Visible))
      continue;
    auto* object = reinterpret_cast<Il2CppObject*>(widget);
    auto* data   = ReadObject(object, previewBattle);
    if (MatchesBaseCourse(data, response) && MatchesPreview(Invoke(previewContext, object), data))
      return true;
  }
  return false;
}

void InstallBaseCardHooks()
{
  auto* preview = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget").get_cls();
  auto* nav =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController")
          .get_cls();
  auto* scanning =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation.Managers", "ScanningManager").get_cls();
  navigationClass =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIContext").get_cls();
  battleClass = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData")
                    .get_cls();

  using method_contract::Resolve;
  const auto* pre        = Resolve(preview, "OnSetCourseResponse", false, "System.Void",
                                   {"Digit.PrimeServer.Events.SetCourseResponseEventArgs"});
  const auto* navigation = Resolve(nav, "OnSetCourseResponse", false, "System.Void",
                                   {"Digit.PrimeServer.Events.SetCourseResponseEventArgs"});
  previewBattle          = ReferenceField(preview, "_battleTargetData", "Digit.PrimeServer.Models.BattleTargetData");
  navigationPoi          = ReferenceField(navigationClass, "Poi", "Digit.Prime.Navigation.POI");
  navigationUser         = ReferenceField(navigationClass, "UserId", "System.String");
  previewContext         = ContextGetter(preview, "Context");
  navigationContext      = ContextGetter(nav, "CanvasContext");
  scanningInstance       = SingletonGetter(scanning);
  targetCache  = Resolve(scanning, "get_TargetDataCache", false, "Digit.PrimeServer.Models.BattleTargetData", {});
  isStarbase   = Resolve(battleClass, "get_IsStarbase", false, "System.Boolean", {});
  isOwnStation = Resolve(battleClass, "get_IsOwnStation", false, "System.Boolean", {});
  isValid      = Resolve(battleClass, "get_IsValid", false, "System.Boolean", {});
  playerId     = Resolve(battleClass, "get_PlayerID", false, "System.Int64", {});
  targetId     = Resolve(battleClass, "get_TargetID", false, "System.Int64", {});
  targetUser   = Resolve(battleClass, "get_TargetUserID", false, "System.String", {});
  int64Class   = playerId ? il2cpp_class_from_type(playerId->return_type) : nullptr;
  stringClass  = targetUser ? il2cpp_class_from_type(targetUser->return_type) : nullptr;
  if (!ResponseLayout(pre) || !ResponseLayout(navigation) || !previewBattle || !navigationPoi || !navigationUser
      || !previewContext || !navigationContext || !scanningInstance || !targetCache || !isStarbase || !isOwnStation
      || !isValid || !playerId || !targetId || !targetUser || !int64Class || !stringClass) {
    spdlog::warn("[BaseCard] native card API unavailable; preserving native dismissal");
    return;
  }
  const bool a = SPUD_STATIC_DETOUR(pre->methodPointer, PreviewResponse) != nullptr;
  const bool b = SPUD_STATIC_DETOUR(navigation->methodPointer, NavigationResponse) != nullptr;
  ready        = a && b;
  spdlog::info("[BaseCard] ready={} preview={} navigation={}", ready.load(), a, b);
}
#else
namespace
{
std::atomic_bool ready{false};
}
void InstallBaseCardHooks() {}
bool base_card::KeepOpenForRecall(std::int64_t)
{ return false; }
#endif

namespace mod_settings
{
bool KeepBaseCardAvailable()
{ return ready.load(); }
} // namespace mod_settings
