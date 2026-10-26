#pragma once

#include "settings_text_entry.h"

struct SettingsSearchList : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "MultiTemplateDynamicListContainer">;
};
enum class SettingsSearchFocus { Focused, Pending, Unavailable };
inline SettingsSearchFocus FocusSettingsSearchRow(SettingsPageController* controller, SettingsOptionContext* expected,
                                                  int32_t index)
{
  using namespace settings_text_entry_binding;
  try {
    auto  panel = il2cpp::GcHandle::Strong(controller->Panel());
    auto* list  = panel ? panel.Get<SettingsOptionPanel>()->Field<"_container", SettingsSearchList*>() : nullptr;
    if (!list)
      return SettingsSearchFocus::Unavailable;
    auto list_root = il2cpp::GcHandle::Strong(list);
    if (Checked<"get_SpawnInProgress", bool>(list))
      return SettingsSearchFocus::Pending;
    auto widget =
        il2cpp::GcHandle::Strong(Checked<"GetContentAt", settings_binding::ReferenceObject*(int32_t)>(list, index));
    if (!widget)
      return SettingsSearchFocus::Pending;
    auto* object = widget.Get<il2cpp::Object>();
    if (Checked<"get_Context", SettingsOptionContext*>(object) != expected)
      return SettingsSearchFocus::Unavailable;
    auto* row    = Checked<"get_gameObject", unity::GameObject*>(object);
    auto* rect   = row->GetComponent<unity::RectTransform>();
    auto* scroll = ParentComponent<unity::ScrollRect>(row);
    if (!rect || !scroll)
      return SettingsSearchFocus::Unavailable;
    auto* content  = scroll->content();
    auto* viewport = scroll->viewport();
    if (!content || !viewport)
      return SettingsSearchFocus::Unavailable;
    const auto whole  = Checked<"get_rect", unity::Rect>(content);
    const auto view   = Checked<"get_rect", unity::Rect>(viewport);
    const auto bounds = unity::RectTransformUtility::RelativeBounds(content, rect);
    if (!bounds || !std::isfinite(whole.height) || !std::isfinite(view.height) || !std::isfinite(bounds.value.center.y))
      return SettingsSearchFocus::Unavailable;
    if (whole.height <= 0 || view.height <= 0)
      return SettingsSearchFocus::Pending;
    if (whole.height > view.height) {
      const auto position = std::clamp(
          1.f - ((whole.y + whole.height) - bounds.value.center.y - view.height * .5f) / (whole.height - view.height),
          0.f, 1.f);
      Checked<"set_verticalNormalizedPosition", void(float)>(scroll, position);
    }
    return Checked<"get_Context", SettingsOptionContext*>(object) == expected ? SettingsSearchFocus::Focused
                                                                              : SettingsSearchFocus::Unavailable;
  } catch (...) {
    return SettingsSearchFocus::Unavailable;
  }
}

struct SettingsSearchSurface {
  il2cpp::GcHandle         object, input, controller, panel;
  SettingsTextEntrySurface navigation;
  unity::Vector2           original{}, applied{};
  bool                     moved = false;

  static bool Available()
  { return SettingsTextEntrySurface::Available(); }
  bool Focused()
  { return settings_text_entry_binding::Checked<"get_isFocused", bool>(input.Get<unity::TMP_InputField>()); }
  std::string Read()
  {
    auto* text = settings_text_entry_binding::Checked<"get_text", Il2CppString*>(input.Get<unity::TMP_InputField>());
    if (!text)
      throw std::runtime_error("settings search text unavailable");
    return il2cpp::ToUtf8(text);
  }
  void Write(std::string_view text)
  {
    settings_text_entry_binding::Checked<"set_text", void(Il2CppString*)>(input.Get<unity::TMP_InputField>(),
                                                                          il2cpp::NewString(text));
  }
  void Focus()
  {
    settings_text_entry_binding::Checked<"Select", void>(input.Get<unity::TMP_InputField>());
    settings_text_entry_binding::Checked<"ActivateInputField", void>(input.Get<unity::TMP_InputField>());
  }
  bool Destroy()
  {
    if (moved) {
      auto*      rect  = panel.Get<unity::RectTransform>();
      const auto alive = unity::TryAlive(rect);
      if (!alive)
        return false;
      if (alive.value) {
        const auto current = rect->TryCall<"get_offsetMax", unity::Vector2>();
        if (!current)
          return false;
        if (current.value.x == applied.x && current.value.y == applied.y) {
          (void)rect->TryCall<"set_offsetMax", void(unity::Vector2)>(original);
          const auto after = rect->TryCall<"get_offsetMax", unity::Vector2>();
          if (!after || after.value.x != original.x || after.value.y != original.y)
            return false;
        }
      }
      moved = false;
    }
    auto*      root  = object.Get<unity::GameObject>();
    const auto alive = unity::TryAlive(root);
    if (!alive)
      return false;
    if (alive.value) {
      (void)root->TryCall<"SetActive", void(bool)>(false);
      const auto hidden = root->TryCall<"get_activeSelf", bool>();
      if (!hidden || hidden.value)
        return false;
      if (!unity::UnityObject::Class::TryCall<"Destroy", void(unity::UnityObject*)>(
              reinterpret_cast<unity::UnityObject*>(root)))
        return false;
    }
    object.Reset();
    input.Reset();
    controller.Reset();
    panel.Reset();
    return true;
  }
  void Build(SettingsPageController* owner, std::string_view query)
  {
    using namespace settings_text_entry_binding;
    controller         = il2cpp::GcHandle::Weak(owner);
    auto* native_panel = owner->Panel();
    auto* rect         = Checked<"get_transform", unity::RectTransform*>(native_panel);
    panel              = il2cpp::GcHandle::Weak(rect);
    auto*      parent  = Checked<"get_parent", unity::Transform*>(rect);
    const auto min     = Checked<"get_anchorMin", unity::Vector2>(rect);
    const auto max     = Checked<"get_anchorMax", unity::Vector2>(rect);
    const auto lower   = Checked<"get_offsetMin", unity::Vector2>(rect);
    original           = Checked<"get_offsetMax", unity::Vector2>(rect);
    applied            = {original.x, original.y - 90};
    auto* title        = owner->Title();
    auto* title_object = Checked<"get_gameObject", unity::GameObject*>(title);
    auto* source       = title_object->GetComponent<unity::TMP_Text>();
    auto  font         = il2cpp::GcHandle::Strong(Checked<"get_font", settings_binding::ReferenceObject*>(source));
    if (!controller || !panel || !font)
      throw std::runtime_error("native search assets unavailable");
    auto root = Object("CommunityMod_SettingsSearch", parent);
    object    = std::move(root.root);
    Checked<"set_anchorMin", void(unity::Vector2)>(root.rect, unity::Vector2{min.x, max.y});
    Checked<"set_anchorMax", void(unity::Vector2)>(root.rect, unity::Vector2{max.x, max.y});
    Checked<"set_offsetMin", void(unity::Vector2)>(root.rect, unity::Vector2{lower.x, original.y - 76});
    Checked<"set_offsetMax", void(unity::Vector2)>(root.rect, unity::Vector2{original.x, original.y - 4});
    auto*      image    = Image(root, {.10f, .20f, .25f, 1});
    auto       viewport = Object("Text Area", root.rect);
    const auto stretch  = [](unity::RectTransform* target, unity::Vector2 lower, unity::Vector2 upper) {
      Checked<"set_anchorMin", void(unity::Vector2)>(target, unity::Vector2{0, 0});
      Checked<"set_anchorMax", void(unity::Vector2)>(target, unity::Vector2{1, 1});
      Checked<"set_offsetMin", void(unity::Vector2)>(target, lower);
      Checked<"set_offsetMax", void(unity::Vector2)>(target, upper);
    };
    stretch(viewport.rect, {20, 6}, {-20, -6});
    Add<SettingsEditorMask>(viewport.object);
    const auto label = [&](std::string_view name, std::string_view value, unity::Color color) {
      auto* text      = Label(viewport.rect, font.Get<il2cpp::Object>(), name, {0, 0}, {0, 0}, 26, value);
      auto* transform = Checked<"get_transform", unity::RectTransform*>(text);
      stretch(transform, {0, 0}, {0, 0});
      Checked<"set_alignment", void(int32_t)>(text, 0x201);
      Checked<"set_color", void(unity::Color)>(text, color);
      Checked<"set_enableWordWrapping", void(bool)>(text, false);
      return text;
    };
    auto* text        = label("Text", "", {.88f, .95f, .97f, 1});
    auto* placeholder = label("Placeholder", "Search settings, shortcuts or TOML name...", {.60f, .72f, .76f, 1});
    auto* field       = Add<unity::TMP_InputField>(root.object);
    input             = il2cpp::GcHandle::Strong(field);
    Checked<"set_targetGraphic", void(unity::Image*)>(field, image);
    Checked<"set_textViewport", void(unity::RectTransform*)>(field, viewport.rect);
    Checked<"set_textComponent", void(unity::TMP_Text*)>(field, text);
    Checked<"set_placeholder", void(unity::TMP_Text*)>(field, placeholder);
    Checked<"set_characterLimit", void(int32_t)>(field, 128);
    Checked<"set_lineType", void(int32_t)>(field, 0);
    Checked<"set_richText", void(bool)>(field, false);
    Checked<"set_restoreOriginalTextOnEscape", void(bool)>(field, false);
    Write(query);
    const auto current = Checked<"get_offsetMax", unity::Vector2>(rect);
    if (current.x != original.x || current.y != original.y)
      throw std::runtime_error("native search panel changed");
    moved = true;
    Checked<"set_offsetMax", void(unity::Vector2)>(rect, applied);
    Checked<"SetActive", void(bool)>(viewport.object, true);
    Checked<"SetActive", void(bool)>(object.Get<unity::GameObject>(), true);
  }
};
