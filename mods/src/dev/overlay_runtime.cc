#include "dev/overlay_runtime.h"

#ifdef _MODDBG

#include "errormsg.h"

#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/CanvasController.h>
#include <prime/CanvasScaler.h>
#include <prime/GameObject.h>
#include <prime/ScreenManager.h>
#include <prime/Transform.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace dev::overlay
{
namespace
{
  constexpr const char* kHostName          = "CommunityDevOverlayHost";
  constexpr auto        kInitialRetryDelay = std::chrono::milliseconds(250);
  constexpr auto        kMaximumRetryDelay = std::chrono::milliseconds(5000);

  struct ProviderRegistration {
    std::string            owner;
    ReservedRegionProvider provider = nullptr;
  };

  struct PanelRegistration {
    std::string owner;
    PanelTick   tick  = nullptr;
    PanelReset  reset = nullptr;
  };

  std::vector<ProviderRegistration>     s_providers;
  std::vector<PanelRegistration>        s_panels;
  NativeElement                         s_host;
  Transform*                            s_parent           = nullptr;
  Transform*                            s_candidate_parent = nullptr;
  Vec2                                  s_candidate_size;
  std::chrono::steady_clock::time_point s_candidate_since;
  std::chrono::steady_clock::time_point s_host_retry_after;
  std::chrono::milliseconds             s_host_retry_delay;
  bool                                  s_enabled = false;

  Il2CppObject* invoke(const MethodInfo* method, void* target, void** args, std::string_view operation)
  {
    if (!method) {
      return nullptr;
    }

    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(method, target, args, &exception);
    if (exception) {
      spdlog::warn("[DevOverlay] {} failed", operation);
      return nullptr;
    }
    return result;
  }

  bool invoke_void(const MethodInfo* method, void* target, void** args, std::string_view operation)
  {
    if (!method) {
      return false;
    }

    Il2CppException* exception = nullptr;
    il2cpp_runtime_invoke(method, target, args, &exception);
    if (exception) {
      spdlog::warn("[DevOverlay] {} failed", operation);
      return false;
    }
    return true;
  }

  template <typename T> T* target(void* handle)
  { return handle ? reinterpret_cast<T*>(il2cpp_gchandle_get_target(handle)) : nullptr; }

  void reset_panels()
  {
    for (const auto& panel : s_panels) {
      if (panel.reset) {
        panel.reset();
      }
    }
  }

  const MethodInfo* resolve_get_top_canvas()
  {
    static const auto* method = [] {
      auto  helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
      auto* result = helper.GetMethodInfoSpecial("GetTopCanvas", [](int count, const Il2CppType** params) {
        return count == 1 && params && params[0] && !params[0]->byref && params[0]->type == IL2CPP_TYPE_BOOLEAN;
      });
      if (!result || !(result->flags & METHOD_ATTRIBUTE_STATIC) || !result->methodPointer || !result->return_type) {
        return static_cast<const MethodInfo*>(nullptr);
      }

      auto*       return_class = il2cpp_class_from_type(result->return_type);
      const auto* name         = return_class ? il2cpp_class_get_name(return_class) : nullptr;
      const auto* namespc      = return_class ? il2cpp_class_get_namespace(return_class) : nullptr;
      return name && namespc && std::strcmp(name, "CanvasController") == 0
                     && std::strcmp(namespc, "Digit.Client.UI") == 0
                 ? result
                 : static_cast<const MethodInfo*>(nullptr);
    }();
    return method;
  }

  CanvasScaler* canvas_root_scaler(ScreenManager* screen_manager)
  {
    if (!screen_manager) {
      return nullptr;
    }

    static auto  helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
    static auto* field =
        helper.get_cls() ? il2cpp_class_get_field_from_name(helper.get_cls(), "m_canvasRootScaler") : nullptr;
    if (!field || field->offset < 0) {
      return nullptr;
    }
    return *reinterpret_cast<CanvasScaler**>(reinterpret_cast<std::byte*>(screen_manager) + field->offset);
  }

  Transform* current_parent(ScreenManager* screen_manager)
  {
    static bool warned_missing_top_canvas = false;
    if (const auto* method = resolve_get_top_canvas()) {
      bool  visible_only = true;
      void* args[1]      = {&visible_only};
      auto* top_canvas   = invoke(method, nullptr, args, "ScreenManager.GetTopCanvas");
      if (auto* transform = GetComponentTransform(top_canvas)) {
        return transform;
      }
    } else if (!warned_missing_top_canvas) {
      warned_missing_top_canvas = true;
      spdlog::warn("[DevOverlay] ScreenManager.GetTopCanvas signature unavailable; using the root-canvas fallback");
    }
    return GetComponentTransform(canvas_root_scaler(screen_manager));
  }

  void release_host()
  {
    reset_panels();
    ReleaseElement(s_host);
    s_parent = nullptr;
  }

  void reset_host_retry()
  {
    s_host_retry_after = {};
    s_host_retry_delay = {};
  }

  void defer_host_retry()
  {
    release_host();
    const bool first_failure = s_host_retry_delay == std::chrono::milliseconds::zero();
    s_host_retry_delay = first_failure ? kInitialRetryDelay : std::min(s_host_retry_delay * 2, kMaximumRetryDelay);
    s_host_retry_after = std::chrono::steady_clock::now() + s_host_retry_delay;
    if (first_failure) {
      spdlog::warn("[DevOverlay] native host setup failed; retrying with bounded backoff");
    }
  }

  bool measure_layout(Transform* parent, Vec2& size)
  {
    if (TryGetRectSize(parent, size)) {
      return true;
    }

    static auto get_width  = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_width()");
    static auto get_height = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_height()");
    size.x                 = get_width ? static_cast<float>(get_width()) : 0.0f;
    size.y                 = get_height ? static_cast<float>(get_height()) : 0.0f;
    return size.x > 0.0f && size.y > 0.0f;
  }

  bool layout_has_settled(Transform* parent, Vec2 size)
  {
    constexpr auto settle_time    = std::chrono::milliseconds(250);
    const auto     now            = std::chrono::steady_clock::now();
    const bool     parent_changed = parent != s_candidate_parent;
    if (parent_changed || std::fabs(size.x - s_candidate_size.x) > 1.0f
        || std::fabs(size.y - s_candidate_size.y) > 1.0f) {
      if (parent_changed) {
        reset_host_retry();
      }
      s_candidate_parent = parent;
      s_candidate_size   = size;
      s_candidate_since  = now;
      release_host();
      return false;
    }
    return now - s_candidate_since >= settle_time;
  }

  bool ensure_host(Transform* parent)
  {
    if (!parent) {
      return false;
    }
    if (std::chrono::steady_clock::now() < s_host_retry_after) {
      return false;
    }
    if (s_parent != parent || !IsAlive(s_host)) {
      release_host();
      s_host = CreateElement(kHostName, parent, "UnityEngine.UI", "UnityEngine.UI", "Image");
      if (!IsAlive(s_host)) {
        defer_host_retry();
        return false;
      }
      SetActive(s_host, false);
      s_parent = parent;
      if (!ConfigureRect(s_host, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}, {}, {}) || !SetRaycastTarget(s_host, false)
          || !SetGraphicColor(s_host, {0.0f, 0.0f, 0.0f, 0.0f})) {
        defer_host_retry();
        return false;
      }
      reset_host_retry();
      spdlog::info("[DevOverlay] native host mounted on the settled top canvas");
    }
    SetActive(s_host, true);
    BringToFront(s_host);
    return true;
  }
} // namespace

bool RegisterReservedRegionProvider(std::string_view owner, ReservedRegionProvider provider)
{
  if (owner.empty() || !provider) {
    return false;
  }
  if (std::ranges::any_of(s_providers, [owner](const auto& item) { return item.owner == owner; })) {
    return false;
  }
  s_providers.push_back({std::string{owner}, provider});
  return true;
}

bool RegisterPanel(std::string_view owner, PanelTick tick, PanelReset reset)
{
  if (owner.empty() || !tick || !reset) {
    return false;
  }
  if (std::ranges::any_of(s_panels, [owner](const auto& item) { return item.owner == owner; })) {
    return false;
  }
  s_panels.push_back({std::string{owner}, tick, reset});
  return true;
}

void SetEnabled(bool enabled)
{
  if (s_enabled == enabled) {
    return;
  }
  s_enabled = enabled;
  Reset();
}

bool IsEnabled()
{ return s_enabled; }

void Tick(ScreenManager* screen_manager)
{
  if (!s_enabled) {
    return;
  }

  auto* parent = current_parent(screen_manager);
  Vec2  layout_size;
  if (!parent || !measure_layout(parent, layout_size) || !layout_has_settled(parent, layout_size)) {
    return;
  }
  if (!ensure_host(parent)) {
    return;
  }

  LayoutContext context;
  context.width  = layout_size.x;
  context.height = layout_size.y;
  for (const auto& provider : s_providers) {
    const auto reserved     = provider.provider(context.width, context.height);
    context.reserved.left   = std::max(context.reserved.left, reserved.left);
    context.reserved.top    = std::max(context.reserved.top, reserved.top);
    context.reserved.right  = std::max(context.reserved.right, reserved.right);
    context.reserved.bottom = std::max(context.reserved.bottom, reserved.bottom);
  }

  auto* host = GetTransform(s_host);
  for (const auto& panel : s_panels) {
    panel.tick(host, context);
  }
}

void Reset()
{
  release_host();
  s_candidate_parent = nullptr;
  s_candidate_size   = {};
  s_candidate_since  = {};
  reset_host_retry();
}

NativeElement CreateElement(std::string_view name, Transform* parent, std::string_view assembly,
                            std::string_view namespc, std::string_view component)
{
  NativeElement result;
  if (!parent || name.empty() || component.empty()) {
    return result;
  }

  auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  auto transform_helper   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  auto component_helper   = il2cpp_get_class_helper(std::string{assembly}.c_str(), std::string{namespc}.c_str(),
                                                    std::string{component}.c_str());
  auto constructor        = game_object_helper.GetMethodInfoSpecial(".ctor", [](int count, const Il2CppType** params) {
    return count == 1 && params[0]->type == IL2CPP_TYPE_STRING;
  });
  auto get_transform      = game_object_helper.GetMethodInfo("get_transform");
  auto set_parent =
      transform_helper.GetMethodInfoSpecial("SetParent", [](int count, const Il2CppType**) { return count == 2; });
  auto add_component = game_object_helper.GetMethodInfo("AddComponent", 1);
  if (!game_object_helper.isValidHelper() || !component_helper.isValidHelper() || !constructor || !get_transform
      || !set_parent || !add_component) {
    return result;
  }

  auto* game_object = reinterpret_cast<GameObject*>(il2cpp_object_new(game_object_helper.get_cls()));
  if (!game_object) {
    return result;
  }
  result.game_object_handle = il2cpp_gchandle_new(reinterpret_cast<Il2CppObject*>(game_object), false);
  if (!result.game_object_handle) {
    return {};
  }

  const auto owned_name   = std::string{name};
  void*      name_args[1] = {il2cpp_string_new(owned_name.c_str())};
  if (!invoke_void(constructor, game_object, name_args, "GameObject.ctor")) {
    ReleaseElement(result);
    return {};
  }

  auto* transform =
      reinterpret_cast<Transform*>(invoke(get_transform, game_object, nullptr, "GameObject.get_transform"));
  bool  world_position_stays = false;
  void* parent_args[2]       = {parent, &world_position_stays};
  if (!transform || !invoke_void(set_parent, transform, parent_args, "Transform.SetParent")) {
    ReleaseElement(result);
    return {};
  }

  void* component_type = component_helper.GetType();
  if (!component_type) {
    ReleaseElement(result);
    return {};
  }
  void* component_args[1] = {component_type};
  auto* native_component  = invoke(add_component, game_object, component_args, "GameObject.AddComponent");
  if (!native_component) {
    ReleaseElement(result);
    return {};
  }
  transform = GetComponentTransform(native_component);
  if (!transform) {
    ReleaseElement(result);
    return {};
  }

  result.transform_handle = il2cpp_gchandle_new(reinterpret_cast<Il2CppObject*>(transform), false);
  result.component_handle = il2cpp_gchandle_new(native_component, false);
  if (!result.transform_handle || !result.component_handle) {
    ReleaseElement(result);
    return {};
  }
  game_object->SetActive(true);
  return result;
}

GameObject* GetGameObject(const NativeElement& element)
{ return target<GameObject>(element.game_object_handle); }

Transform* GetTransform(const NativeElement& element)
{ return target<Transform>(element.transform_handle); }

void* GetComponent(const NativeElement& element)
{ return target<void>(element.component_handle); }

Transform* GetComponentTransform(void* component)
{
  if (!component) {
    return nullptr;
  }
  static auto helper        = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  static auto get_transform = helper.GetMethodInfo("get_transform");
  return reinterpret_cast<Transform*>(invoke(get_transform, component, nullptr, "Component.get_transform"));
}

bool TryGetRectSize(Transform* transform, Vec2& size)
{
  struct Rect {
    float x;
    float y;
    float width;
    float height;
  };

  if (!transform) {
    return false;
  }
  static auto helper   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto get_rect = helper.GetMethodInfo("get_rect", 0);
  auto*       boxed    = invoke(get_rect, transform, nullptr, "RectTransform.get_rect");
  auto*       rect     = boxed ? reinterpret_cast<Rect*>(il2cpp_object_unbox(boxed)) : nullptr;
  if (!rect || rect->width <= 0.0f || rect->height <= 0.0f) {
    return false;
  }
  size = {rect->width, rect->height};
  return true;
}

bool IsAlive(const NativeElement& element)
{
  auto* game_object = GetGameObject(element);
  if (!game_object) {
    return false;
  }

  static auto object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto op_implicit   = object_helper.GetMethodInfo("op_Implicit", 1);
  void*       args[1]       = {game_object};
  auto*       boxed         = invoke(op_implicit, nullptr, args, "Object.op_Implicit");
  auto*       value         = boxed ? il2cpp_object_unbox(boxed) : nullptr;
  return value && *reinterpret_cast<bool*>(value);
}

void ReleaseElement(NativeElement& element)
{
  auto* game_object = GetGameObject(element);
  if (game_object && IsAlive(element)) {
    game_object->SetActive(false);
    static auto object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
    static auto destroy       = object_helper.GetMethodInfo("Destroy", 1);
    void*       args[1]       = {game_object};
    invoke_void(destroy, nullptr, args, "Object.Destroy");
  }

  if (element.component_handle) {
    il2cpp_gchandle_free(element.component_handle);
  }
  if (element.transform_handle) {
    il2cpp_gchandle_free(element.transform_handle);
  }
  if (element.game_object_handle) {
    il2cpp_gchandle_free(element.game_object_handle);
  }
  element = {};
}

void SetActive(const NativeElement& element, bool active)
{
  if (auto* game_object = GetGameObject(element); game_object && IsAlive(element)) {
    game_object->SetActive(active);
  }
}

bool ConfigureRect(const NativeElement& element, Vec2 anchor_min_value, Vec2 anchor_max_value, Vec2 pivot_value,
                   Vec2 size_delta_value, Vec2 anchored_position_value)
{
  auto* transform = GetTransform(element);
  if (!transform) {
    return false;
  }

  static auto helper             = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto anchor_min         = helper.GetMethodInfo("set_anchorMin", 1);
  static auto anchor_max         = helper.GetMethodInfo("set_anchorMax", 1);
  static auto pivot              = helper.GetMethodInfo("set_pivot", 1);
  static auto size_delta         = helper.GetMethodInfo("set_sizeDelta", 1);
  static auto anchored_pos       = helper.GetMethodInfo("set_anchoredPosition", 1);
  void*       anchor_min_args[1] = {&anchor_min_value};
  void*       anchor_max_args[1] = {&anchor_max_value};
  void*       pivot_args[1]      = {&pivot_value};
  void*       size_args[1]       = {&size_delta_value};
  void*       position_args[1]   = {&anchored_position_value};
  return invoke_void(anchor_min, transform, anchor_min_args, "RectTransform.set_anchorMin")
         && invoke_void(anchor_max, transform, anchor_max_args, "RectTransform.set_anchorMax")
         && invoke_void(pivot, transform, pivot_args, "RectTransform.set_pivot")
         && invoke_void(size_delta, transform, size_args, "RectTransform.set_sizeDelta")
         && invoke_void(anchored_pos, transform, position_args, "RectTransform.set_anchoredPosition");
}

bool SetRaycastTarget(const NativeElement& element, bool enabled)
{
  static auto helper  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Graphic");
  static auto method  = helper.GetMethodInfo("set_raycastTarget", 1);
  void*       args[1] = {&enabled};
  return invoke_void(method, GetComponent(element), args, "Graphic.set_raycastTarget");
}

bool SetGraphicColor(const NativeElement& element, Color color)
{
  static auto helper  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Graphic");
  static auto method  = helper.GetMethodInfo("set_color", 1);
  void*       args[1] = {&color};
  return invoke_void(method, GetComponent(element), args, "Graphic.set_color");
}

bool SetText(const NativeElement& element, std::string_view text)
{
  static auto helper  = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
  static auto method  = helper.GetMethodInfo("set_text", 1);
  const auto  owned   = std::string{text};
  void*       args[1] = {il2cpp_string_new(owned.c_str())};
  return invoke_void(method, GetComponent(element), args, "TMP_Text.set_text");
}

bool ConfigureText(const NativeElement& element, float font_size, int32_t alignment, Color color)
{
  static auto text_helper   = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
  static auto font_method   = text_helper.GetMethodInfo("set_fontSize", 1);
  static auto align_method  = text_helper.GetMethodInfo("set_alignment", 1);
  void*       font_args[1]  = {&font_size};
  void*       align_args[1] = {&alignment};
  return invoke_void(font_method, GetComponent(element), font_args, "TMP_Text.set_fontSize")
         && invoke_void(align_method, GetComponent(element), align_args, "TMP_Text.set_alignment")
         && SetGraphicColor(element, color) && SetRaycastTarget(element, false);
}

void BringToFront(const NativeElement& element)
{
  static auto helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto method = helper.GetMethodInfo("SetAsLastSibling", 0);
  invoke_void(method, GetTransform(element), nullptr, "Transform.SetAsLastSibling");
}
} // namespace dev::overlay

#endif
