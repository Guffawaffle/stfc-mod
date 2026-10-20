#include "battle_log_summary_data.h"
#include "config.h"
#include "patches/screen_update_hook.h"
#include "prime/Color.h"
#include "prime/Vector2.h"

#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{
// All Unity work runs on ScreenManager's main-thread dispatcher. No additional detours.
// The shared class lookup dereferences missing assemblies. These optional UI
// classes can be absent after a client update, so check each lookup step.
Il2CppClass* Class(const char* assembly, const char* ns, const char* name)
{
  auto* domain = il2cpp_domain_get();
  auto* a      = domain ? il2cpp_domain_assembly_open(domain, assembly) : nullptr;
  auto* image  = a ? il2cpp_assembly_get_image(a) : nullptr;
  return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
}

bool TryInvoke(const MethodInfo* method, void* target, void** args, Il2CppObject** result = nullptr)
{
  if (!method)
    return false;
  Il2CppException* exception = nullptr;
  auto*            value     = il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception)
    return false;
  if (result)
    *result = value;
  return true;
}

// Unity has Type/string and generic overloads with the same argument count.
const MethodInfo* TypeMethod(Il2CppClass* cls, const char* name)
{
  // The shared filtered lookup cannot inspect is_generic and only searches
  // one class. Keep the overload check here so a generic overload cannot win.
  void* iter = nullptr;
  while (cls) {
    while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
      if (method->is_generic || std::strcmp(method->name, name) || method->parameters_count != 1)
        continue;
      auto* param = il2cpp_class_from_type(il2cpp_method_get_param(method, 0));
      if (param && !std::strcmp(il2cpp_class_get_namespace(param), "System")
          && !std::strcmp(il2cpp_class_get_name(param), "Type"))
        return method;
    }
    cls  = il2cpp_class_get_parent(cls);
    iter = nullptr;
  }
  return nullptr;
}

Il2CppObject* Get(Il2CppObject* object, const char* name)
{
  // Callers pass getter method names. Use the shared property helper for its
  // virtual dispatch and exception handling, while accepting absent objects.
  if (!object || std::strncmp(name, "get_", 4))
    return nullptr;
  return IL2CppClassHelper(object->klass).GetProperty(name + 4).GetRaw<Il2CppObject>(object);
}

template <typename T> bool Value(Il2CppObject* object, const char* name, T& value)
{
  if (!object)
    return false;
  Il2CppObject* boxed = nullptr;
  if (!std::strncmp(name, "get_", 4))
    boxed = Get(object, name);
  else if (!TryInvoke(IL2CppClassHelper(object->klass).GetMethodInfo(name, 0), object, nullptr, &boxed))
    return false;
  uint32_t alignment = 0;
  if (!boxed || !il2cpp_class_is_valuetype(boxed->klass)
      || il2cpp_class_value_size(boxed->klass, &alignment) != sizeof(T))
    return false;
  std::memcpy(&value, il2cpp_object_unbox(boxed), sizeof(T));
  return true;
}

bool Set(Il2CppObject* object, const char* name, void* value)
{
  void* args[] = {value};
  return object && TryInvoke(IL2CppClassHelper(object->klass).GetMethodInfo(name, 1), object, args);
}

Il2CppObject* Field(Il2CppObject* object, const char* name)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  if (!field || il2cpp_type_is_byref(il2cpp_field_get_type(field)))
    return nullptr;
  auto* cls = il2cpp_class_from_type(il2cpp_field_get_type(field));
  if (!cls || il2cpp_class_is_valuetype(cls))
    return nullptr;
  Il2CppObject* result = nullptr;
  il2cpp_field_get_value(object, field, &result);
  return result;
}

Il2CppObject* WithType(const MethodInfo* method, Il2CppObject* target, Il2CppClass* type)
{
  if (!type)
    return nullptr;
  void*         args[] = {il2cpp_type_get_object(il2cpp_class_get_type(type))};
  Il2CppObject* result = nullptr;
  TryInvoke(method, target, args, &result);
  return result;
}

Il2CppClass* UnityObject()
{
  static auto* cls = Class("UnityEngine.CoreModule", "UnityEngine", "Object");
  return cls;
}

bool Alive(Il2CppObject* object)
{
  if (!object)
    return false;
  void*         args[] = {object};
  Il2CppObject* result = nullptr;
  static auto*  method = IL2CppClassHelper(UnityObject()).GetMethodInfo("op_Implicit", 1);
  if (!TryInvoke(method, nullptr, args, &result) || !result || !result->klass)
    return false;
  const auto* type = il2cpp_class_get_type(result->klass);
  if (!type || type->byref || type->type != IL2CPP_TYPE_BOOLEAN)
    return false;
  auto* alive = static_cast<bool*>(il2cpp_object_unbox(result));
  return alive && *alive;
}

struct Root {
  Il2CppGCHandle handle{};
  Il2CppObject*  get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
  void reset(Il2CppObject* value = nullptr)
  {
    if (handle)
      il2cpp_gchandle_free(handle);
    handle = value ? il2cpp_gchandle_new(value, false) : 0;
  }
};

Il2CppObject* Find(Il2CppClass* type)
{
  static auto* method = TypeMethod(UnityObject(), "FindObjectOfType");
  return WithType(method, nullptr, type);
}

Il2CppObject* Component(Il2CppObject* object, Il2CppClass* type)
{ return object ? WithType(TypeMethod(object->klass, "GetComponent"), object, type) : nullptr; }

Il2CppObject* NewObject(const char* name, Il2CppObject* parent, Root& root)
{
  static auto* go = Class("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto* rt = Class("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  if (!go || !rt)
    return nullptr;
  auto* object = il2cpp_object_new(go);
  root.reset(object);
  void* args[] = {il2cpp_string_new(name)};
  if (!TryInvoke(IL2CppClassHelper(go).GetMethodInfo(".ctor", 1), object, args))
    return nullptr;
  bool active = false;
  if (!Set(object, "SetActive", &active))
    return nullptr;
  auto* transform          = WithType(TypeMethod(go, "AddComponent"), object, rt);
  bool  worldPositionStays = false;
  void* parentArgs[]       = {parent, &worldPositionStays};
  if (!transform
      || !TryInvoke(IL2CppClassHelper(transform->klass).GetMethodInfo("SetParent", 2), transform, parentArgs))
    return nullptr;
  return transform;
}

// SmartScroller owns/recycles its content. Keep the summary in a separate,
// resizable band above the viewport. All UI and data reads use the dispatcher.
constexpr float                         kExpandedHeight = 180, kCollapsedHeight = 44, kGap = 10;
Root                                    report, viewport, panel, label, sourceResult, sourceEntries;
vec2                                    originalMax{}, reservedMax{};
bool                                    reserved = false, expanded = true, complete = false, failed = false;
std::int64_t                            battleId   = 0;
int                                     entryCount = 0, nextEntry = 0;
battle_summary::Summary                 summary;
std::unordered_map<Il2CppObject*, bool> identities;

template <typename T> bool FieldValue(Il2CppObject* object, const char* name, Il2CppTypeEnum expected, T& value)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  auto* type  = field ? il2cpp_field_get_type(field) : nullptr;
  if (!type || type->byref || (il2cpp_field_get_flags(field) & 0x10)) // static field
    return false;
  auto* cls = il2cpp_class_from_type(type);
  if (cls && il2cpp_class_is_enum(cls))
    type = il2cpp_class_enum_basetype(cls);
  if (!type || type->type != expected)
    return false;
  // The API accepts boxed structs and applies their runtime field offsets.
  il2cpp_field_get_value(object, field, &value);
  return true;
}

bool ReadLogShowing(Il2CppObject* view)
{
  bool showing = false;
  return FieldValue(view, "_logShowing", IL2CPP_TYPE_BOOLEAN, showing) && showing;
}

bool Local(Il2CppObject* profile)
{
  if (!profile)
    return false; // NPCs may not have a profile.
  if (auto found = identities.find(profile); found != identities.end())
    return found->second;
  bool local = false;
  if (!Value(profile, "get_IsLocalPlayer", local))
    failed = true;
  identities.emplace(profile, local);
  return local;
}

bool Named(Il2CppObject* object, const char* name)
{
  return object && !std::strcmp(il2cpp_class_get_name(object->klass), name)
         && !std::strcmp(il2cpp_class_get_namespace(object->klass), "Digit.PrimeServer.Models");
}

void ReadEntry(Il2CppObject* entry)
{
  std::int32_t type = 0;
  if (!Named(entry, "BattleAction") || !FieldValue(entry, "ActionType", IL2CPP_TYPE_I4, type)) {
    failed = true;
    return;
  }
  auto* data = Field(entry, "ActionData");
  if (type == 6) { // RoundTitle
    std::int32_t rounds = 0;
    if (!Named(data, "BattleRoundDivider") || !FieldValue(data, "MaxRounds", IL2CPP_TYPE_I4, rounds) || rounds < 1
        || rounds > 100000) {
      failed = true;
      return;
    }
    summary.rounds = std::max(summary.rounds, rounds);
  } else if (type == 1) { // Attack. IsPlayerAction means a SIDE, not an Armada participant.
    if (!Named(data, "BattleActionAttackData")) {
      failed = true;
      return;
    }
    bool outgoing = Local(Field(data, "<AttackerUserProfile>k__BackingField"));
    bool incoming = Local(Field(data, "<TargetUserProfile>k__BackingField"));
    if (!outgoing && !incoming)
      return;
    std::int64_t hull = 0, shield = 0;
    bool         critical = false;
    if (!FieldValue(data, "HullDamageTaken", IL2CPP_TYPE_I8, hull)
        || !FieldValue(data, "ShieldDamageTaken", IL2CPP_TYPE_I8, shield)
        || !FieldValue(data, "CriticalHit", IL2CPP_TYPE_BOOLEAN, critical)
        || !summary.attack(outgoing, incoming, critical, hull, shield))
      failed = true;
  } else if (type == 8) { // HullRepair; reported magnitude, not inferred effective healing.
    bool  local  = Local(Field(data, "<AttackerUserProfile>k__BackingField"))
                   || Local(Field(data, "<TargetUserProfile>k__BackingField"));
    float amount = 0;
    if (local
        && (!FieldValue(data, "<AbilityValue>k__BackingField", IL2CPP_TYPE_R4, amount)
            || !summary.repair(true, amount)))
      failed = true;
  }
}

std::string Number(long double value)
{
  if (value >= 1e12L)
    return fmt::format("{:.2f}T", value / 1e12L);
  if (value >= 1e9L)
    return fmt::format("{:.2f}B", value / 1e9L);
  if (value >= 1e6L)
    return fmt::format("{:.2f}M", value / 1e6L);
  if (value >= 1e3L)
    return fmt::format("{:.2f}K", value / 1e3L);
  return fmt::format("{:.0f}", value);
}

void Draw()
{
  if (!Alive(panel.get()) || !Alive(label.get()))
    return;
  std::string title =
      fmt::format("<b>YOUR BATTLE</b>  <size=80%>LOCAL PLAYER ONLY  [{}]</size>", expanded ? "- Collapse" : "+ Expand");
  std::string text;
  if (!complete)
    text = title + fmt::format("\nReading battle entries: {} / {}", nextEntry, entryCount);
  else if (failed)
    text = title + "\nSummary unavailable: this report's data could not be read completely.";
  else if (!summary.local_identity_seen)
    text = title + "\nNo identifiable local-player combat data in this report.";
  else {
    const auto& out = summary.outgoing;
    const auto& in  = summary.incoming;
    if (!expanded) {
      text = title + fmt::format("   Landed {}   |   Taken {}", Number(out.landed()), Number(in.landed()));
    } else {
      text =
          title
          + fmt::format("  {} rounds\n"
                        "<b>LANDED {}</b>  (Hull {} | Shields {})     <b>TAKEN {}</b>\n"
                        "Shots {}   |   Crit {:.1f}% ({}/{})   |   Avg/shot {}   |   Landed/round {}\n"
                        "Incoming shots {}   |   Hull {}   |   Shields {}   |   Repairs {} <size=80%>(reported)</size>",
                        summary.rounds, Number(out.landed()), Number(out.hull), Number(out.shield), Number(in.landed()),
                        out.shots, out.critical_percent(), out.critical, out.shots, Number(out.average()),
                        summary.rounds ? Number(summary.per_round()) : "n/a", in.shots, Number(in.hull),
                        Number(in.shield), Number(summary.repairs));
    }
  }
  Set(label.get(), "set_text", il2cpp_string_new(text.c_str()));
}

void ResetSummary()
{
  sourceResult.reset();
  sourceEntries.reset();
  identities.clear();
  summary    = {};
  battleId   = 0;
  entryCount = nextEntry = 0;
  complete = failed = false;
}

void Clear()
{
  vec2 current{};
  if (reserved && Alive(viewport.get()) && Value(viewport.get(), "get_offsetMax", current)
      && std::abs(current.x - reservedMax.x) < 0.1f && std::abs(current.y - reservedMax.y) < 0.1f)
    Set(viewport.get(), "set_offsetMax", &originalMax);
  reserved = false;
  if (Alive(panel.get())) {
    bool active = false;
    Set(panel.get(), "SetActive", &active);
    void* args[] = {panel.get()};
    TryInvoke(IL2CppClassHelper(UnityObject()).GetMethodInfo("Destroy", 1), nullptr, args);
  }
  label.reset();
  panel.reset();
  viewport.reset();
  ResetSummary();
}

bool Resize()
{
  float height = expanded ? kExpandedHeight : kCollapsedHeight;
  vec2  min    = originalMax;
  vec2  max    = originalMax;
  vec2  originalMin{}, anchorMin{}, anchorMax{};
  auto* port      = viewport.get();
  auto* transform = Get(panel.get(), "get_transform");
  if (!Value(port, "get_offsetMin", originalMin) || !Value(port, "get_anchorMin", anchorMin)
      || !Value(port, "get_anchorMax", anchorMax))
    return false;
  anchorMin.y = anchorMax.y;
  min         = {originalMin.x + 8, originalMax.y - height};
  max.x -= 8;
  vec2 requestedMax{originalMax.x, originalMax.y - height - kGap};
  if (!Set(transform, "set_anchorMin", &anchorMin) || !Set(transform, "set_anchorMax", &anchorMax)
      || !Set(transform, "set_offsetMin", &min) || !Set(transform, "set_offsetMax", &max)
      || !Set(port, "set_offsetMax", &requestedMax))
    return false;
  reservedMax = requestedMax;
  return true;
}

bool Create(Il2CppObject* view, Il2CppObject* port)
{
  static auto* tmp    = Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  static auto* image  = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        parent = Get(port, "get_parent");
  auto*        font   = Get(Component(Field(Field(view, "_battleResultWidget"), "_victoryText"), tmp), "get_font");
  if (!font)
    font = Get(WithType(TypeMethod(view->klass, "GetComponentInChildren"), view, tmp), "get_font");
  struct RectValue {
    float x, y, width, height;
  } rect{};
  if (!parent || !font || !Value(port, "get_offsetMax", originalMax) || !Value(port, "get_rect", rect)
      || rect.height < kExpandedHeight + kGap + 150 || rect.width < 300)
    return false;

  auto* transform = NewObject("CommunityMod_BattleLogSummaryPreview", parent, panel);
  auto* background =
      WithType(TypeMethod(panel.get() ? panel.get()->klass : nullptr, "AddComponent"), panel.get(), image);
  color backgroundColor{0.035f, 0.10f, 0.16f, 0.98f};
  bool  no = false, yes = true;
  if (!transform || !background || !Set(background, "set_color", &backgroundColor)
      || !Set(background, "set_raycastTarget", &yes))
    return false;

  Root  textObject;
  auto* textTransform = NewObject("PlayerSummary", transform, textObject);
  auto* text =
      WithType(TypeMethod(textObject.get() ? textObject.get()->klass : nullptr, "AddComponent"), textObject.get(), tmp);
  label.reset(text);
  vec2  zero{0, 0}, one{1, 1}, textMin{20, 8}, textMax{-20, -8};
  float fontSize  = 22;
  int   alignment = 0x101; // TMP: top left.
  color textColor{0.78f, 0.91f, 1, 1};
  bool  ok = textTransform && text && Set(textTransform, "set_anchorMin", &zero)
             && Set(textTransform, "set_anchorMax", &one) && Set(textTransform, "set_offsetMin", &textMin)
             && Set(textTransform, "set_offsetMax", &textMax) && Set(text, "set_font", font)
             && Set(text, "set_fontSize", &fontSize) && Set(text, "set_alignment", &alignment)
             && Set(text, "set_color", &textColor) && Set(text, "set_raycastTarget", &no)
             && Set(text, "set_enableWordWrapping", &no) && Set(textObject.get(), "SetActive", &yes);
  if (!ok && Alive(textObject.get())) {
    void* args[] = {textObject.get()};
    TryInvoke(IL2CppClassHelper(UnityObject()).GetMethodInfo("Destroy", 1), nullptr, args);
  }
  textObject.reset();
  if (!ok)
    return false;
  viewport.reset(port);
  // Track the geometry before changing it so failures can restore the native viewport.
  reservedMax = originalMax;
  reserved    = true;
  if (!Resize() || !Set(panel.get(), "SetActive", &yes))
    return false;
  Draw();
  return true;
}

void RefreshSource(Il2CppObject* view)
{
  auto*        context = Get(view, "get_CanvasContext");
  auto*        result  = Field(context, "BattleResultData");
  auto*        entries = Get(context, "get_BattleLog");
  int          count   = 0;
  std::int64_t id      = 0;
  if (!result || !Value(result, "get_ID", id) || !entries || !Value(entries, "get_Count", count) || count < 1)
    return; // The game's async report request may still be loading.
  if (result == sourceResult.get() && entries == sourceEntries.get() && count == entryCount && id == battleId)
    return;
  ResetSummary();
  sourceResult.reset(result);
  sourceEntries.reset(entries);
  entryCount = count;
  battleId   = id;
  if (count > 200000) {
    complete = failed = true; // Do not present a truncated battle as complete.
  }
  Draw();
}

void AdvanceSummary()
{
  if (complete || !sourceEntries.get())
    return;
  auto*      list       = sourceEntries.get();
  auto*      itemMethod = IL2CppClassHelper(list->klass).GetMethodInfo("get_Item", 1);
  const auto deadline   = std::chrono::steady_clock::now() + std::chrono::milliseconds(3);
  // Bound both managed calls and time per frame; large Armada logs stay responsive.
  for (int batch = 0; nextEntry < entryCount && batch < 128; ++batch) {
    int           index  = nextEntry++;
    void*         args[] = {&index};
    Il2CppObject* entry  = nullptr;
    if (!TryInvoke(itemMethod, list, args, &entry))
      failed = true;
    else
      ReadEntry(entry);
    if ((batch & 15) == 15 && std::chrono::steady_clock::now() >= deadline)
      break;
  }
  complete = nextEntry == entryCount;
  if (complete) {
    spdlog::info("Battle summary: {} entries, {} rounds, local shots {}/{}, read_complete={}", entryCount,
                 summary.rounds, summary.outgoing.shots, summary.incoming.shots, !failed);
    Draw();
  }
}

void HandleClick()
{
  if (!Alive(panel.get()))
    return;
  static auto*  input        = Class("UnityEngine.InputLegacyModule", "UnityEngine", "Input");
  static auto*  utility      = Class("UnityEngine.UIModule", "UnityEngine", "RectTransformUtility");
  static auto*  canvas       = Class("UnityEngine.UIModule", "UnityEngine", "Canvas");
  int           button       = 0;
  void*         buttonArgs[] = {&button};
  Il2CppObject* pressed      = nullptr;
  if (!TryInvoke(IL2CppClassHelper(input).GetMethodInfo("GetMouseButtonDown", 1), nullptr, buttonArgs, &pressed)
      || !pressed || !*static_cast<bool*>(il2cpp_object_unbox(pressed)))
    return;
  Il2CppObject* position = nullptr;
  if (!TryInvoke(IL2CppClassHelper(input).GetMethodInfo("get_mousePosition", 0), nullptr, nullptr, &position)
      || !position)
    return;
  struct Vec3 {
    float x, y, z;
  } mouse{};
  uint32_t alignment = 0;
  if (il2cpp_class_value_size(position->klass, &alignment) != sizeof(mouse))
    return;
  std::memcpy(&mouse, il2cpp_object_unbox(position), sizeof(mouse));
  vec2          point{mouse.x, mouse.y};
  auto*         ownerCanvas = WithType(TypeMethod(panel.get()->klass, "GetComponentInParent"), panel.get(), canvas);
  auto*         rootCanvas  = Get(ownerCanvas, "get_rootCanvas");
  auto*         camera      = Get(rootCanvas ? rootCanvas : ownerCanvas, "get_worldCamera");
  void*         args[]      = {Get(panel.get(), "get_transform"), &point, camera};
  Il2CppObject* inside      = nullptr;
  if (TryInvoke(IL2CppClassHelper(utility).GetMethodInfo("RectangleContainsScreenPoint", 3), nullptr, args, &inside)
      && inside && *static_cast<bool*>(il2cpp_object_unbox(inside))) {
    expanded = !expanded;
    if (!Resize()) {
      Clear();
      return;
    }
    Draw();
  }
}

void Update()
{
  static auto next = std::chrono::steady_clock::time_point{};
  auto        now  = std::chrono::steady_clock::now();
  if (!Config::Get().battle_log_summary_preview) {
    if (panel.handle || viewport.handle)
      Clear();
    report.reset();
    return;
  }
  // Input must be checked every frame, rather than at the discovery interval.
  if (ReadLogShowing(report.get())) {
    HandleClick();
    AdvanceSummary();
  }
  if (now < next)
    return;
  next = now + std::chrono::milliseconds(250);
  if (!Alive(report.get())) {
    Clear();
    static auto* cls = Class("Assembly-CSharp", "Digit.Prime.Combat", "BattleReportViewController");
    report.reset(Find(cls));
  }
  bool  active = false;
  auto* view   = report.get();
  auto* port   = Field(Field(view, "_logEntryScroller"), "_viewport");
  if (!Alive(view) || !Value(view, "get_isActiveAndEnabled", active) || !active || !ReadLogShowing(view) || !Alive(port)
      || !Value(Get(port, "get_gameObject"), "get_activeInHierarchy", active) || !active) {
    Clear();
    report.reset();
    return;
  }
  if (!Alive(panel.get()) || port != viewport.get()) {
    Clear();
    if (!Create(view, port)) {
      Clear();
      static bool warned = false;
      if (!warned) {
        spdlog::warn("Battle summary: layout unavailable; retaining the native log");
        warned = true;
      }
      next = now + std::chrono::seconds(3);
      return;
    }
  }
  RefreshSource(view);
}
} // namespace

void InstallBattleLogSummary()
{
  if (install_screen_manager_update_hook()) {
    register_screen_manager_update_callback(Update);
    spdlog::info("Local-player battle summary registered (opt-in: ui.battle_log_summary_preview)");
  }
}
