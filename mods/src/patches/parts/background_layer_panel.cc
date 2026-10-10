#include "patches/background_layer_science.h"
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
#include "settings/native/ui_helpers.h"
#include "settings/native_boolean_callback.h"
#include <spdlog/spdlog.h>

namespace background_layer_science
{
namespace
{
  using namespace mod_settings::native;
  using namespace mod_settings::native::ui;
  struct Vec2 {
    float x, y;
  };
  struct Color {
    float r, g, b, a;
  };
  Il2CppGCHandle                     panel = nullptr, status_text = nullptr;
  Il2CppGCHandle                     next_token = nullptr, sky_token = nullptr;
  mod_settings::NativeCallback<void> callback;
  bool                               initialized = false, failed = false;
  std::string                        rendered;

  void Close()
  {
    if (Alive(Target(panel))) {
      void *args[]{Target(panel)};
      Static(IL2CppClassHelper(UnityClass("Object")).GetMethodInfo("Destroy", 1), args);
    }
    Free(panel);
    Free(status_text);
    Free(next_token);
    Free(sky_token);
    rendered.clear();
  }

  Il2CppObject *Object(const char *name, Il2CppObject *parent, Vec2 size, Vec2 position)
  {
    auto *cls = UnityClass("GameObject");
    Root  object(il2cpp_object_new(cls));
    Root  label(reinterpret_cast<Il2CppObject *>(il2cpp_string_new(name)));
    auto *ctor = IL2CppClassHelper(cls).GetMethodInfoSpecial(
        ".ctor", [](auto count, auto params) { return count == 1 && Type(params[0], IL2CPP_TYPE_STRING); });
    void *args[]{label.get()};
    Invoke(ctor, object.get(), args);
    if (!parent)
      Retain(panel, object.get());
    WithType(object.get(), "AddComponent", UnityClass("RectTransform"));
    Root transform(UiCall(object.get(), "get_transform"));
    if (parent) {
      bool  world = false;
      void *parent_args[]{parent, &world};
      UiCall(transform.get(), "SetParent", 2, parent_args);
    }
    const Vec2 anchor = parent ? Vec2{.5f, .5f} : Vec2{.5f, 1.f};
    Value(transform.get(), "set_anchorMin", anchor);
    Value(transform.get(), "set_anchorMax", anchor);
    Value(transform.get(), "set_pivot", Vec2{.5f, .5f});
    Value(transform.get(), "set_sizeDelta", size);
    Value(transform.get(), "set_anchoredPosition", position);
    Value(object.get(), "set_layer", 5);
    return object.get();
  }

  Il2CppObject *Label(Il2CppObject *parent, Il2CppObject *font, const char *name, Vec2 size, Vec2 position,
                      const std::string &text)
  {
    Root object(Object(name, parent, size, position));
    Root label(WithType(object.get(), "AddComponent", Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI")));
    Set(label.get(), "set_font", font);
    Value(label.get(), "set_fontSize", 20.f);
    Value(label.get(), "set_alignment", 0x202);
    Value(label.get(), "set_color", Color{1, 1, 1, 1});
    Value(label.get(), "set_raycastTarget", false);
    Value(label.get(), "set_enableWordWrapping", true);
    Text(label.get(), text);
    return label.get();
  }

  void Click(Il2CppObject *token, const MethodInfo *)
  {
    try {
      if (token == Target(next_token))
        Next();
      else if (token == Target(sky_token))
        ToggleAmbient();
    } catch (const std::exception &error) {
      spdlog::warn("[BackgroundLayers] button failed: {}", error.what());
    }
  }

  void Button(Il2CppObject *parent, Il2CppObject *font, const char *caption, Vec2 position, Il2CppGCHandle &token)
  {
    Root object(Object(caption, parent, {270, 38}, position));
    Root transform(UiCall(object.get(), "get_transform"));
    Root image(WithType(object.get(), "AddComponent", Class("UnityEngine.UI", "UnityEngine.UI", "Image")));
    Value(image.get(), "set_color", Color{.12f, .30f, .45f, 1});
    Root button(WithType(object.get(), "AddComponent", Class("UnityEngine.UI", "UnityEngine.UI", "Button")));
    Set(button.get(), "set_targetGraphic", image.get());
    Label(transform.get(), font, "Label", {264, 34}, {0, 0}, caption);
    Root identity(il2cpp_object_new(il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Object")));
    Retain(token, identity.get());
    Root  event(UiCall(button.get(), "get_onClick"));
    auto *add = IL2CppClassHelper(event.get()->klass).GetMethodInfo("AddListener", 1);
    if (!Instance(add, 1, IL2CPP_TYPE_VOID) || !Reference(add->parameters[0]))
      throw std::runtime_error("background science button event");
    Root  listener(MakeDelegate(il2cpp_class_from_type(add->parameters[0]), identity.get(), callback.method()));
    void *args[]{listener.get()};
    Invoke(add, event.get(), args);
  }

  void Create()
  {
    if (!initialized) {
      auto *cls = il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Object");
      if (!callback.Initialize(il2cpp_class_get_method_from_name(cls, ".ctor", 0), Click))
        throw std::runtime_error("background science callback");
      initialized = true;
    }
    auto *settings = Class("Unity.TextMeshPro", "TMPro", "TMP_Settings");
    Root  font(Static(IL2CppClassHelper(settings).GetMethodInfo("get_defaultFontAsset", 0), nullptr));
    if (!font.get())
      throw std::runtime_error("background science font");
    Root object(Object("BackgroundLayerScience", nullptr, {660, 186}, {0, -172}));
    Root canvas(WithType(object.get(), "AddComponent", Class("UnityEngine.UIModule", "UnityEngine", "Canvas")));
    Value(canvas.get(), "set_renderMode", 0); // Screen-space overlay.
    Value(canvas.get(), "set_sortingOrder", 30000);
    Root scaler(WithType(object.get(), "AddComponent", Class("UnityEngine.UI", "UnityEngine.UI", "CanvasScaler")));
    Value(scaler.get(), "set_uiScaleMode", 1);
    Value(scaler.get(), "set_referenceResolution", Vec2{2560, 1440});
    Value(scaler.get(), "set_matchWidthOrHeight", .5f);
    WithType(object.get(), "AddComponent", Class("UnityEngine.UI", "UnityEngine.UI", "GraphicRaycaster"));
    // The Canvas root fills the screen; only this child occupies/raycasts the panel area.
    Root transform(UiCall(object.get(), "get_transform"));
    Root body(Object("LayerControls", transform.get(), {660, 186}, {0, 0}));
    Root body_transform(UiCall(body.get(), "get_transform"));
    Value(body_transform.get(), "set_anchorMin", Vec2{.5f, 1.f});
    Value(body_transform.get(), "set_anchorMax", Vec2{.5f, 1.f});
    Value(body_transform.get(), "set_anchoredPosition", Vec2{0, -172});
    Root image(WithType(body.get(), "AddComponent", Class("UnityEngine.UI", "UnityEngine.UI", "Image")));
    Value(image.get(), "set_color", Color{.015f, .025f, .04f, .9f});
    Root label(Label(body_transform.get(), font.get(), "LayerIdentity", {648, 132}, {0, 22}, "Background science"));
    Retain(status_text, label.get());
    Button(body_transform.get(), font.get(), "Next hidden layer (Alt-F8)", {-145, -68}, next_token);
    Button(body_transform.get(), font.get(), "Toggle orbit sky (Alt-F9)", {145, -68}, sky_token);
    spdlog::info("[BackgroundLayers] panel-created=true");
  }
} // namespace

void UpdatePanel(const std::string &status, bool visible)
{
  if (failed)
    return;
  try {
    if (!visible) {
      Close();
      return;
    }
    if (!Alive(Target(panel))) {
      Close();
      Create();
    }
    if (status != rendered) {
      Text(Target(status_text), status);
      rendered = status;
    }
  } catch (const std::exception &error) {
    spdlog::warn("[BackgroundLayers] panel unavailable: {}; keyboard controls retained", error.what());
    try {
      Close();
    } catch (...) {
    }
    failed = true;
  }
}
} // namespace background_layer_science
#else
namespace background_layer_science
{
void UpdatePanel(const std::string &, bool) {}
} // namespace background_layer_science
#endif
