#include "config.h"
#include "patches/screen_update_hook.h"
#include "prime/Color.h"
#include "prime/Vector2.h"

#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <cstring>

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

// This is deliberately outside the virtualized content: SmartScroller owns and
// recycles all of its rows. Reserve a band above its viewport instead.
constexpr float kHeight = 104;
constexpr float kGap    = 12;
Root            report, viewport, panel;
vec2            originalMax{}, reservedMax{};
bool            reserved = false;

bool ReadLogShowing(Il2CppObject* view)
{
  auto* field = view ? il2cpp_class_get_field_from_name(view->klass, "_logShowing") : nullptr;
  auto* type  = field ? il2cpp_field_get_type(field) : nullptr;
  if (!type || type->byref || type->type != IL2CPP_TYPE_BOOLEAN)
    return false;
  bool showing = false;
  il2cpp_field_get_value(view, field, &showing);
  return showing;
}

void Clear()
{
  // Restore only geometry we still own; native layout may have replaced it.
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
  panel.reset();
  viewport.reset();
}

bool Create(Il2CppObject* view, Il2CppObject* port)
{
  static auto* tmp    = Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  static auto* image  = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        parent = Get(port, "get_parent");
  // Borrow the game's font asset without cloning a localized widget/controller.
  auto* result     = Field(view, "_battleResultWidget");
  auto* victory    = Field(result, "_victoryText");
  auto* nativeText = Component(victory, tmp);
  auto* font       = Get(nativeText, "get_font");
  if (!font) {
    auto* text = WithType(TypeMethod(view->klass, "GetComponentInChildren"), view, tmp);
    font       = Get(text, "get_font");
  }
  vec2 anchorMin{}, anchorMax{}, offsetMin{}, offsetMax{};
  struct RectValue {
    float x, y, width, height;
  } rect{};
  if (!parent || !font || !Value(port, "get_anchorMin", anchorMin) || !Value(port, "get_anchorMax", anchorMax)
      || !Value(port, "get_offsetMin", offsetMin) || !Value(port, "get_offsetMax", offsetMax)
      || !Value(port, "get_rect", rect) || rect.height < kHeight + kGap + 150 || rect.width < 300)
    return false;

  auto* transform = NewObject("CommunityMod_BattleLogSummaryPreview", parent, panel);
  anchorMin.y     = anchorMax.y;
  vec2  min{offsetMin.x + 8, offsetMax.y - kHeight};
  vec2  max{offsetMax.x - 8, offsetMax.y};
  auto* background =
      WithType(TypeMethod(panel.get() ? panel.get()->klass : nullptr, "AddComponent"), panel.get(), image);
  color backgroundColor{0.035f, 0.10f, 0.16f, 0.98f};
  bool  no = false, yes = true;
  if (!transform || !background || !Set(transform, "set_anchorMin", &anchorMin)
      || !Set(transform, "set_anchorMax", &anchorMax) || !Set(transform, "set_offsetMin", &min)
      || !Set(transform, "set_offsetMax", &max) || !Set(background, "set_color", &backgroundColor)
      || !Set(background, "set_raycastTarget", &no))
    return false;

  Root  textObject;
  auto* textTransform = NewObject("SummaryPlaceholder", transform, textObject);
  auto* label =
      WithType(TypeMethod(textObject.get() ? textObject.get()->klass : nullptr, "AddComponent"), textObject.get(), tmp);
  vec2  zero{0, 0}, one{1, 1}, textMin{20, 10}, textMax{-20, -10};
  float fontSize  = 22;
  int   alignment = 0x201; // TMP: midline, left.
  color textColor{0.78f, 0.91f, 1, 1};
  bool  ok = textTransform && label && Set(textTransform, "set_anchorMin", &zero)
             && Set(textTransform, "set_anchorMax", &one) && Set(textTransform, "set_offsetMin", &textMin)
             && Set(textTransform, "set_offsetMax", &textMax) && Set(label, "set_font", font)
             && Set(label, "set_fontSize", &fontSize) && Set(label, "set_alignment", &alignment)
             && Set(label, "set_color", &textColor) && Set(label, "set_raycastTarget", &no)
             && Set(label, "set_text",
                    il2cpp_string_new("<b>BATTLE SUMMARY</b>  <size=75%>PREVIEW</size>\n"
                                      "Summary space reserved. Battle statistics will appear here."))
             && Set(textObject.get(), "SetActive", &yes);
  if (!ok && Alive(textObject.get())) {
    void* args[] = {textObject.get()};
    TryInvoke(IL2CppClassHelper(UnityObject()).GetMethodInfo("Destroy", 1), nullptr, args);
  }
  textObject.reset();
  if (!ok)
    return false;

  // Do this last so a failed construction cannot leave a blank band in the log.
  viewport.reset(port);
  originalMax = offsetMax;
  reservedMax = {offsetMax.x, offsetMax.y - kHeight - kGap};
  reserved    = Set(port, "set_offsetMax", &reservedMax);
  if (!reserved || !Set(panel.get(), "SetActive", &yes))
    return false;
  spdlog::info("Battle log summary preview: inserted {}-unit block above {}x{} detailed viewport", kHeight, rect.width,
               rect.height);
  return true;
}

void Update()
{
  static auto next = std::chrono::steady_clock::time_point{};
  auto        now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::milliseconds(250);
  if (!Config::Get().battle_log_summary_preview) {
    if (panel.handle || viewport.handle)
      Clear();
    report.reset();
    return;
  }
  if (!Alive(report.get())) {
    Clear();
    static auto* cls = Class("Assembly-CSharp", "Digit.Prime.Combat", "BattleReportViewController");
    report.reset(Find(cls));
  }
  bool  active   = false;
  auto* view     = report.get();
  auto* scroller = Field(view, "_logEntryScroller");
  auto* port     = Field(scroller, "_viewport");
  if (!Alive(view) || !Value(view, "get_isActiveAndEnabled", active) || !active || !ReadLogShowing(view) || !Alive(port)
      || !Value(Get(port, "get_gameObject"), "get_activeInHierarchy", active) || !active) {
    Clear();
    report.reset();
    return;
  }
  if (Alive(panel.get()) && port == viewport.get())
    return;
  Clear();
  if (!Create(view, port)) {
    Clear();
    static bool warned = false;
    if (!warned) {
      spdlog::warn("Battle log summary preview: layout unavailable; retaining the native log");
      warned = true;
    }
    next = now + std::chrono::seconds(3);
  }
}
} // namespace

void InstallBattleLogSummary()
{
  // Shares the existing, single-owner ScreenManager.Update detour on both platforms.
  if (install_screen_manager_update_hook()) {
    register_screen_manager_update_callback(Update);
    spdlog::info("Battle log summary preview registered (opt-in: ui.battle_log_summary_preview)");
  }
}
