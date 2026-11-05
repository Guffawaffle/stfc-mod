#pragma once

// PLAY's native Settings text-entry surface: the existing Canvas/font with Unity UI and TMP components.
// Managed lookup, construction, callback connection and navigation holds stay at this binding boundary.
#include "settings/text_entry.h"
#include "settings_actions.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unity/application.h>

struct SettingsEditorToken : il2cpp::Object {
  using Class = il2cpp::Class<"mscorlib", "System", "Object">;
};
struct SettingsEditorBack : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "BackButtonViewController">;
};
struct SettingsEditorMask : unity::Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "RectMask2D">;
};

namespace settings_text_entry_binding
{
template <il2cpp::Name N, class F = void, class T, class... A> auto Checked(T* object, A&&... args)
{
  if (!object)
    throw std::runtime_error("native text-entry object unavailable");
  const auto result = object->template TryCall<N, F>(std::forward<A>(args)...);
  if (!result)
    throw std::runtime_error("native text-entry member unavailable");
  if constexpr (!std::is_same_v<decltype(result), const il2cpp::CallResult<void>>)
    return result.value;
}
template <class T> T* Add(unity::GameObject* object)
{
  auto* type  = T::Class::TypeObject();
  auto* value = type ? Checked<"AddComponent", T*(Il2CppReflectionType*)>(object, type) : nullptr;
  if (!value)
    throw std::runtime_error("native text-entry component unavailable");
  return value;
}
template <class T> T* ParentComponent(unity::GameObject* object)
{
  auto* type = T::Class::TypeObject();
  return type ? Checked<"GetComponentInParent", T*(Il2CppReflectionType*, bool)>(object, type, false) : nullptr;
}
inline void Layout(unity::RectTransform* rect, unity::Vector2 size, unity::Vector2 position)
{
  Checked<"set_anchorMin", void(unity::Vector2)>(rect, unity::Vector2{.5f, .5f});
  Checked<"set_anchorMax", void(unity::Vector2)>(rect, unity::Vector2{.5f, .5f});
  Checked<"set_pivot", void(unity::Vector2)>(rect, unity::Vector2{.5f, .5f});
  Checked<"set_sizeDelta", void(unity::Vector2)>(rect, size);
  Checked<"set_anchoredPosition", void(unity::Vector2)>(rect, position);
}
struct UiObject {
  il2cpp::GcHandle      root;
  unity::GameObject*    object = nullptr;
  unity::RectTransform* rect   = nullptr;
};
inline UiObject Object(std::string_view name, unity::Transform* parent)
{
  UiObject result;
  result.root   = il2cpp::GcHandle::Strong(il2cpp::Allocate<unity::GameObject>());
  result.object = result.root.Get<unity::GameObject>();
  try {
    Checked<".ctor", void(Il2CppString*)>(result.object, il2cpp::NewString(name));
    Checked<"SetActive", void(bool)>(result.object, false);
    result.rect = Add<unity::RectTransform>(result.object);
    Checked<"SetParent", void(unity::Transform*, bool)>(result.rect, parent, false);
    Checked<"set_layer", void(int32_t)>(result.object, 5);
  } catch (...) {
    unity::Destroy(result.object);
    throw;
  }
  return result;
}
inline unity::Image* Image(UiObject& object, unity::Color color)
{
  auto* image = Add<unity::Image>(object.object);
  Checked<"set_color", void(unity::Color)>(image, color);
  return image;
}
inline unity::TMP_Text* Label(unity::Transform* parent, il2cpp::Object* font, std::string_view name,
                              unity::Vector2 size, unity::Vector2 position, float font_size, std::string_view text)
{
  auto child = Object(name, parent);
  Layout(child.rect, size, position);
  auto* label = Add<unity::TextMeshProUGUI>(child.object);
  Checked<"set_font", void(il2cpp::Object*)>(label, font);
  Checked<"set_fontSize", void(float)>(label, font_size);
  Checked<"set_alignment", void(int32_t)>(label, 0x202);
  Checked<"set_color", void(unity::Color)>(label, unity::Color{.88f, .95f, .97f, 1});
  Checked<"set_raycastTarget", void(bool)>(label, false);
  Checked<"set_richText", void(bool)>(label, false);
  Checked<"set_enableWordWrapping", void(bool)>(label, true);
  Checked<"set_text", void(Il2CppString*)>(label, il2cpp::NewString(text));
  Checked<"SetActive", void(bool)>(child.object, true);
  return label;
}
inline bool RestoreBoolean(il2cpp::Object* object, bool previous, bool navigation)
{
  const auto alive = unity::TryAlive(object);
  if (!alive)
    return false;
  if (!alive.value)
    return true;
  const auto current =
      navigation ? object->TryCall<"get_sendNavigationEvents", bool>() : object->TryCall<"get_enabled", bool>();
  if (!current)
    return false;
  if (current.value)
    return true; // Another owner already changed the disabled state; do not overwrite it.
  const auto restored = navigation ? object->TryCall<"set_sendNavigationEvents", void(bool)>(previous)
                                   : object->TryCall<"set_enabled", void(bool)>(previous);
  const auto observed =
      navigation ? object->TryCall<"get_sendNavigationEvents", bool>() : object->TryCall<"get_enabled", bool>();
  return restored && observed && observed.value == previous;
}
} // namespace settings_text_entry_binding

struct SettingsTextEntrySurface {
  il2cpp::GcHandle                object, owner, input, status, scroll, apply, back, navigation;
  std::array<il2cpp::GcHandle, 3> tokens;
  bool  back_previous = false, navigation_previous = false, back_held = false, navigation_held = false;
  float status_height = 130;

  static bool Available()
  {
    static const il2cpp::ICall<bool()>               any_key{"UnityEngine.Input::get_anyKey()"};
    static const il2cpp::ICall<bool(unity::KeyCode)> key_down{"UnityEngine.Input::GetKeyDownInt(UnityEngine.KeyCode)"};
    return SettingsEditorToken::Class::Method<void()>(".ctor") && any_key && key_down
           && unity::Application::Class::StaticMethod<bool()>("get_isFocused")
           && unity::GameObject::Class::Method<void(Il2CppString*)>(".ctor")
           && SettingsEditorBack::Class::HasProperty<"enabled", bool>()
           && unity::EventSystem::Class::HasProperty<"sendNavigationEvents", bool>()
           && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current")
           && unity::RectTransform::Class::Raw() && unity::Image::Class::Raw() && unity::Button::Class::Raw()
           && unity::TextMeshProUGUI::Class::Raw() && unity::TMP_InputField::Class::Raw()
           && unity::ScrollRect::Class::Raw() && SettingsEditorMask::Class::Raw();
  }
  bool HasInputHold() const
  { return back_held || navigation_held; }
  bool RestoreInput()
  {
    using namespace settings_text_entry_binding;
    if (back_held && RestoreBoolean(back.Get<il2cpp::Object>(), back_previous, false))
      back_held = false;
    if (navigation_held && RestoreBoolean(navigation.Get<il2cpp::Object>(), navigation_previous, true))
      navigation_held = false;
    if (!back_held)
      back.Reset();
    if (!navigation_held)
      navigation.Reset();
    return !HasInputHold();
  }
  void HoldInput(SettingsEditorBack* settings_back)
  {
    using namespace settings_text_entry_binding;
    back               = il2cpp::GcHandle::Strong(settings_back);
    const auto current = unity::EventSystem::Class::TryCall<"get_current", unity::EventSystem*>();
    navigation         = il2cpp::GcHandle::Strong(current ? current.value : nullptr);
    if (!back || !navigation)
      throw std::runtime_error("native text-entry navigation unavailable");
    back_previous       = Checked<"get_enabled", bool>(settings_back);
    navigation_previous = Checked<"get_sendNavigationEvents", bool>(navigation.Get<unity::EventSystem>());
    back_held = true; // Acquisition may mutate before throwing. Keep restoration custody before either setter.
    Checked<"set_enabled", void(bool)>(settings_back, false);
    navigation_held = true;
    Checked<"set_sendNavigationEvents", void(bool)>(navigation.Get<unity::EventSystem>(), false);
    if (Checked<"get_enabled", bool>(settings_back)
        || Checked<"get_sendNavigationEvents", bool>(navigation.Get<unity::EventSystem>()))
      throw std::runtime_error("native text-entry navigation hold unverified");
  }
  bool DestroySurface()
  {
    auto*      root  = object.Get<unity::GameObject>();
    const auto alive = unity::TryAlive(root);
    if (!alive)
      return false;
    if (alive.value) {
      (void)root->TryCall<"SetActive", void(bool)>(false);
      const auto hidden = root->TryCall<"get_activeSelf", bool>();
      if (!hidden || hidden.value)
        return false;
      const auto destroyed = unity::UnityObject::Class::TryCall<"Destroy", void(unity::UnityObject*)>(
          reinterpret_cast<unity::UnityObject*>(root));
      if (!destroyed)
        return false;
    }
    object.Reset();
    owner.Reset();
    input.Reset();
    status.Reset();
    scroll.Reset();
    apply.Reset();
    for (auto& token : tokens)
      token.Reset();
    return true;
  }
  bool OwnerActive()
  {
    auto*      value  = owner.Get<unity::GameObject>();
    const auto alive  = unity::TryAlive(value);
    const auto active = alive && alive.value ? value->TryActiveInHierarchy() : il2cpp::CallResult<bool>{};
    return active && active.value;
  }
  std::string Read()
  {
    auto* value = settings_text_entry_binding::Checked<"get_text", Il2CppString*>(input.Get<unity::TMP_InputField>());
    if (!value)
      throw std::runtime_error("native text-entry value unavailable");
    return il2cpp::ToUtf8(value);
  }
  void Write(std::string_view value)
  {
    settings_text_entry_binding::Checked<"set_text", void(Il2CppString*)>(input.Get<unity::TMP_InputField>(),
                                                                          il2cpp::NewString(value));
  }
  void Status(std::string_view text, bool enabled)
  {
    using namespace settings_text_entry_binding;
    auto* label = status.Get<unity::TMP_Text>();
    Checked<"set_text", void(Il2CppString*)>(label, il2cpp::NewString(text));
    const auto height = Checked<"get_preferredHeight", float>(label);
    if (!std::isfinite(height))
      throw std::runtime_error("native text-entry help size unavailable");
    auto* rect = Checked<"get_transform", unity::RectTransform*>(label);
    Checked<"set_sizeDelta", void(unity::Vector2)>(rect, unity::Vector2{590, std::max(status_height, height)});
    Checked<"set_verticalNormalizedPosition", void(float)>(scroll.Get<unity::ScrollRect>(), 1.f);
    Checked<"set_interactable", void(bool)>(apply.Get<unity::Button>(), enabled);
  }
  void Button(unity::Transform* parent, il2cpp::Object* font, std::size_t index, std::string_view title, float x,
              const MethodInfo* callback)
  {
    using namespace settings_text_entry_binding;
    auto child = Object(title, parent);
    Layout(child.rect, {190, 52}, {x, -166});
    auto* image  = Image(child, {.18f, .34f, .43f, 1});
    auto* button = Add<unity::Button>(child.object);
    Checked<"set_targetGraphic", void(unity::Image*)>(button, image);
    Label(child.rect, font, "Label", {178, 46}, {0, 0}, 24, title);
    if (index == 0)
      apply = il2cpp::GcHandle::Strong(button);
    tokens[index]      = il2cpp::GcHandle::Strong(il2cpp::Allocate<SettingsEditorToken>());
    auto        event  = il2cpp::GcHandle::Strong(Checked<"get_onClick", settings_binding::ReferenceObject*>(button));
    auto*       target = event.Get<il2cpp::Object>();
    const auto* add    = target ? il2cpp::detail::MethodCall<void(settings_binding::ReferenceDelegate*)>::Resolve(
                                      target->klass, "AddListener", false)
                                : nullptr;
    if (!tokens[index] || !add)
      throw std::runtime_error("native text-entry click event unavailable");
    auto listener = il2cpp::GcHandle::Strong(il2cpp::NewClosedDelegate(il2cpp_class_from_type(add->parameters[0]),
                                                                       tokens[index].Get<SettingsEditorToken>(),
                                                                       il2cpp::Method(callback)));
    if (!listener || !il2cpp::detail::TryInvoke<void>(add, target, listener.Get<settings_binding::ReferenceDelegate>()))
      throw std::runtime_error("native text-entry click binding unavailable");
    Checked<"SetActive", void(bool)>(child.object, true);
  }
  void Build(il2cpp::Object* invoking, const mod_settings::TextEntryRequest& request, const MethodInfo* callback)
  {
    using namespace settings_text_entry_binding;
    auto  host        = il2cpp::GcHandle::Strong(invoking);
    auto* host_object = Checked<"get_gameObject", unity::GameObject*>(invoking);
    auto  settings    = il2cpp::GcHandle::Strong(ParentComponent<SettingsPageController>(host_object));
    auto  canvas      = il2cpp::GcHandle::Strong(ParentComponent<unity::Canvas>(host_object));
    if (!settings || !canvas || !host)
      throw std::runtime_error("native Settings editor owner unavailable");
    auto* controller    = settings.Get<SettingsPageController>();
    auto* settings_back = controller->Field<"_backButtonViewController", SettingsEditorBack*>();
    owner               = il2cpp::GcHandle::Weak(Checked<"get_gameObject", unity::GameObject*>(controller));
    auto* localizer     = reinterpret_cast<SettingsActionWidget*>(invoking)->Label();
    auto* label_object  = Checked<"get_gameObject", unity::GameObject*>(localizer);
    auto* source        = label_object->GetComponent<unity::TMP_Text>();
    auto  font          = il2cpp::GcHandle::Strong(Checked<"get_font", settings_binding::ReferenceObject*>(source));
    auto* parent        = Checked<"get_transform", unity::RectTransform*>(canvas.Get<unity::Canvas>());
    if (!owner || !font || !settings_back || request.value.size() > std::numeric_limits<int32_t>::max())
      throw std::runtime_error("native text-entry assets unavailable");
    auto root = Object("CommunityMod_TextEditor", parent);
    object    = std::move(root.root); // Retain before any following construction can fail.
    Checked<"set_anchorMin", void(unity::Vector2)>(root.rect, unity::Vector2{0, 0});
    Checked<"set_anchorMax", void(unity::Vector2)>(root.rect, unity::Vector2{1, 1});
    Checked<"set_sizeDelta", void(unity::Vector2)>(root.rect, unity::Vector2{0, 0});
    Checked<"set_anchoredPosition", void(unity::Vector2)>(root.rect, unity::Vector2{0, 0});
    Checked<"SetAsLastSibling", void>(root.rect);
    Image(root, {0, 0, 0, .5f});
    auto panel = Object("Panel", root.rect);
    Layout(panel.rect, {680, 430}, {0, 0});
    Image(panel, {.10f, .18f, .24f, 1});
    const auto bounds = Checked<"get_rect", unity::Rect>(parent);
    const auto scale  = std::min({1.f, bounds.width / 710.f, bounds.height / 460.f});
    if (!std::isfinite(scale) || scale <= 0)
      throw std::runtime_error("native text-entry canvas size unavailable");
    Checked<"set_localScale", void(unity::Vector3)>(panel.rect, unity::Vector3{scale, scale, 1});
    Label(panel.rect, font.Get<il2cpp::Object>(), "Title", {620, 60}, {0, 162}, 28, request.title);
    auto entry = Object("Input", panel.rect);
    Layout(entry.rect, {610, 80}, {0, 72});
    auto* background = Image(entry, {.06f, .12f, .16f, 1});
    auto  viewport   = Object("Text Area", entry.rect);
    Layout(viewport.rect, {580, 64}, {0, 0});
    Add<SettingsEditorMask>(viewport.object);
    auto* text = Label(viewport.rect, font.Get<il2cpp::Object>(), "Text", {580, 64}, {0, 0}, 25, "");
    Checked<"set_alignment", void(int32_t)>(text, 0x201);
    Checked<"set_enableWordWrapping", void(bool)>(text, false);
    auto* field = Add<unity::TMP_InputField>(entry.object);
    input       = il2cpp::GcHandle::Strong(field);
    Checked<"set_targetGraphic", void(unity::Image*)>(field, background);
    Checked<"set_textViewport", void(unity::RectTransform*)>(field, viewport.rect);
    Checked<"set_textComponent", void(unity::TMP_Text*)>(field, text);
    Checked<"set_characterLimit", void(int32_t)>(field,
                                                 std::max<int32_t>(32768, static_cast<int32_t>(request.value.size())));
    Checked<"set_lineType", void(int32_t)>(field, 0);
    Checked<"set_richText", void(bool)>(field, false);
    Checked<"set_restoreOriginalTextOnEscape", void(bool)>(field, false);
    Checked<"set_readOnly", void(bool)>(field, request.read_only);
    Checked<"set_text", void(Il2CppString*)>(field, il2cpp::NewString(request.value));
    const bool details = request.read_only && request.value.empty() && !request.record_frame;
    status_height      = details ? 250.f : 130.f;
    auto help          = Object("Help", panel.rect);
    Layout(help.rect, {610, status_height}, {0, details ? 2.f : -58.f});
    Add<SettingsEditorMask>(help.object);
    Image(help, {0, 0, 0, .01f});
    auto* status_label =
        Label(help.rect, font.Get<il2cpp::Object>(), "Status", {590, status_height}, {0, 0}, 21, request.help);
    auto* status_rect = Checked<"get_transform", unity::RectTransform*>(status_label);
    Checked<"set_anchorMin", void(unity::Vector2)>(status_rect, unity::Vector2{.5f, 1});
    Checked<"set_anchorMax", void(unity::Vector2)>(status_rect, unity::Vector2{.5f, 1});
    Checked<"set_pivot", void(unity::Vector2)>(status_rect, unity::Vector2{.5f, 1});
    Checked<"set_alignment", void(int32_t)>(status_label, 0x102);
    status         = il2cpp::GcHandle::Strong(status_label);
    auto* scroller = Add<unity::ScrollRect>(help.object);
    scroll         = il2cpp::GcHandle::Strong(scroller);
    Checked<"set_viewport", void(unity::RectTransform*)>(scroller, help.rect);
    Checked<"set_content", void(unity::RectTransform*)>(scroller, status_rect);
    Checked<"set_horizontal", void(bool)>(scroller, false);
    Checked<"set_vertical", void(bool)>(scroller, true);
    Checked<"set_movementType", void(int32_t)>(scroller, 2);
    Checked<"set_scrollSensitivity", void(float)>(scroller, 24.f);
    if (request.record_frame && request.record_again) {
      Button(panel.rect, font.Get<il2cpp::Object>(), 1, "Cancel", -210, callback);
      Button(panel.rect, font.Get<il2cpp::Object>(), 2, "Record again", 0, callback);
      Button(panel.rect, font.Get<il2cpp::Object>(), 0, request.apply_label, 210, callback);
    } else {
      Button(panel.rect, font.Get<il2cpp::Object>(), 1, "Cancel", -124, callback);
      Button(panel.rect, font.Get<il2cpp::Object>(), 0, request.apply_label, 124, callback);
    }
    HoldInput(settings_back);
    Checked<"SetActive", void(bool)>(viewport.object, true);
    Checked<"SetActive", void(bool)>(entry.object, !details);
    Checked<"SetActive", void(bool)>(help.object, true);
    Checked<"SetActive", void(bool)>(panel.object, true);
    Checked<"SetActive", void(bool)>(object.Get<unity::GameObject>(), true);
    if (!request.read_only) {
      Checked<"Select", void>(field);
      Checked<"ActivateInputField", void>(field);
    }
  }
};
