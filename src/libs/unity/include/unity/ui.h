#pragma once

// Unity UI and TextMeshPro: canvases, images, buttons, toggles, text, input fields and the event system, plus the
// small helpers for building UI from native code and keeping a decoded image alive across scene reloads.

#include "core.h"

#include <il2cpp/il2cpp.h>

#include <cmath>
#include <cstdint>
#include <string_view>

namespace unity
{
struct CanvasScaler : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "CanvasScaler">;

  // The property, which the scaler applies on its next Update; not the managed SetScaleFactor method, which writes
  // Canvas.scaleFactor at once.
  void SetScaleFactor(float value)
  { Set<"scaleFactor">(value); }
};

// UnityEngine.UI.Image; color and raycastTarget come from Graphic.
struct Image : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "Image">;

  enum class Type : int32_t {
    Simple = 0,
    Sliced = 1,
    Tiled  = 2,
    Filled = 3,
  };

  Sprite* sprite()
  { return Get<"sprite", Sprite*>(); }
  void SetSprite(Sprite* value)
  { Set<"sprite">(value); }
  Sprite* overrideSprite()
  { return Get<"overrideSprite", Sprite*>(); }
  void SetOverrideSprite(Sprite* value)
  { Set<"overrideSprite">(value); }
  void SetType(Type value)
  { Set<"type">(value); }
  void SetPreserveAspect(bool value)
  { Set<"preserveAspect">(value); }
  void SetPixelsPerUnitMultiplier(float value)
  { Set<"pixelsPerUnitMultiplier">(value); }
  void SetColor(Color value)
  { Set<"color">(value); }
  void SetRaycastTarget(bool value)
  { Set<"raycastTarget">(value); }
  void SetVerticesDirty()
  { Call<"SetVerticesDirty">(); }
};

struct Button : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "Button">;

  void Press()
  { Call<"Press">(); }
};

struct EventSystem : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem">;

  GameObject* currentSelectedGameObject()
  { return Get<"currentSelectedGameObject", GameObject*>(); }
  void SetSelectedGameObject(GameObject* object)
  { Call<"SetSelectedGameObject">(object); }

  static EventSystem* current()
  { return Class::Get<"current", EventSystem*>(); }
};

struct LayoutElement : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "LayoutElement">;

  bool ExcludeFromLayout()
  {
    if (!TryCall<"set_ignoreLayout", void(bool)>(true))
      return false;
    const auto ignored = TryCall<"get_ignoreLayout", bool>();
    return ignored && ignored.value;
  }
};

struct HorizontalLayoutGroup : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "HorizontalLayoutGroup">;
};

struct PointerEventData : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.EventSystems", "PointerEventData">;

  il2cpp::CallResult<Vector2> Position()
  { return TryCall<"get_position", Vector2>(); }
  il2cpp::CallResult<Vector2> PressPosition()
  { return TryCall<"get_pressPosition", Vector2>(); }
  il2cpp::CallResult<int32_t> PointerId()
  { return TryCall<"get_pointerId", int32_t>(); }
  il2cpp::CallResult<int32_t> Button()
  { return TryCall<"get_button", int32_t>(); }
};

struct ScrollRect : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "ScrollRect">;

  RectTransform* content()
  { return Get<"content", RectTransform*>(); }
  RectTransform* viewport()
  { return Get<"viewport", RectTransform*>(); }
};

struct Bounds {
  Vector3 center, extents;
};
struct RectTransformUtility {
  using Class = il2cpp::Class<"UnityEngine.UIModule", "UnityEngine", "RectTransformUtility">;

  static il2cpp::CallResult<bool> Contains(RectTransform* rect, Vector2 point, Camera* camera)
  {
    return Class::TryCall<"RectangleContainsScreenPoint", bool(RectTransform*, Vector2, Camera*)>(rect, point, camera);
  }
  static il2cpp::CallResult<Bounds> RelativeBounds(Transform* root, Transform* child)
  { return Class::TryCall<"CalculateRelativeRectTransformBounds", Bounds(Transform*, Transform*)>(root, child); }

  static bool OwnBoundsAvailable()
  { return OwnRectGetter() && OwnPointMethod(false) && OwnPointMethod(true); }

  // The two opposite corners of this rectangle in root space, excluding all descendants. This preserves the
  // axis-aligned card measurement used by PLAY; RelativeBounds intentionally includes descendant geometry.
  static il2cpp::CallResult<Bounds> OwnBounds(Transform* root, RectTransform* child)
  {
    if (!root || !child || !OwnBoundsAvailable())
      return {};
    auto root_owner  = il2cpp::GcHandle::Strong(root);
    auto child_owner = il2cpp::GcHandle::Strong(child);
    if (!root_owner || !child_owner)
      return {};
    const auto observed = il2cpp::detail::TryInvokeBoxed(OwnRectGetter(), child, nullptr);
    auto       box      = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
    using RectClass     = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Rect">;
    if (!box || !box.Get()->klass
        || !il2cpp::detail::ValueClassMatches(&box.Get()->klass->byval_arg, RectClass::Raw(), sizeof(Rect)))
      return {};
    const auto rect = il2cpp::detail::FromBoxed<Rect>(box.Get());
    if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) || !std::isfinite(rect.height))
      return {};
    const Vector3 corners[]{{rect.x, rect.y, 0}, {rect.x + rect.width, rect.y + rect.height, 0}};
    Vector3       local[2]{};
    for (size_t i = 0; i < 2; ++i) {
      auto point = corners[i];
      if (!std::isfinite(point.x) || !std::isfinite(point.y))
        return {};
      void*      point_args[]{&point};
      const auto world = TryReadVector3(OwnPointMethod(false), child, point_args);
      if (!world || !std::isfinite(world.value.x) || !std::isfinite(world.value.y) || !std::isfinite(world.value.z))
        return {};
      auto       world_point = world.value;
      void*      world_args[]{&world_point};
      const auto relative = TryReadVector3(OwnPointMethod(true), root, world_args);
      if (!relative || !std::isfinite(relative.value.x) || !std::isfinite(relative.value.y)
          || !std::isfinite(relative.value.z) || std::abs(relative.value.x) > 1'000'000
          || std::abs(relative.value.y) > 1'000'000)
        return {};
      local[i] = relative.value;
    }
    const auto low_x = std::min(local[0].x, local[1].x), high_x = std::max(local[0].x, local[1].x);
    const auto low_y = std::min(local[0].y, local[1].y), high_y = std::max(local[0].y, local[1].y);
    return {
        true,
        {{(low_x + high_x) * 0.5f, (low_y + high_y) * 0.5f, 0}, {(high_x - low_x) * 0.5f, (high_y - low_y) * 0.5f, 0}}};
  }

private:
  static const MethodInfo* OwnRectGetter()
  {
    static const auto* method = [] {
      using RectClass      = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Rect">;
      const auto candidate = RectTransform::Class::Method<Rect()>("get_rect");
      return candidate && candidate.Info()->invoker_method
                     && il2cpp::detail::ValueClassMatches(candidate.Info()->return_type, RectClass::Raw(), sizeof(Rect))
                 ? candidate.Info()
                 : nullptr;
    }();
    return method;
  }
  static const MethodInfo* OwnPointMethod(bool inverse)
  {
    static const auto resolve = [](const char* name) -> const MethodInfo* {
      using TransformClass = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Transform">;
      const auto candidate = TransformClass::Method<Vector3(Vector3)>(name);
      return candidate && candidate.Info()->invoker_method
                     && il2cpp::detail::ValueClassMatches(candidate.Info()->parameters[0], Vector3Class(),
                                                          sizeof(Vector3))
                     && il2cpp::detail::ValueClassMatches(candidate.Info()->return_type, Vector3Class(),
                                                          sizeof(Vector3))
                 ? candidate.Info()
                 : nullptr;
    };
    static const auto* point         = resolve("TransformPoint");
    static const auto* inverse_point = resolve("InverseTransformPoint");
    return inverse ? inverse_point : point;
  }
};

struct Toggle : Behaviour {
  using Class = il2cpp::Class<"UnityEngine.UI", "UnityEngine.UI", "Toggle">;

  bool isOn()
  { return Get<"isOn", bool>(); }

  // Goes through the pointer handler instead of the isOn setter, so interactable checks and listeners behave as for
  // a real click.
  void SimulateClick()
  {
    if (auto* event = il2cpp::New<PointerEventData>(EventSystem::current()))
      Call<"OnPointerClick">(event);
  }
};

struct TMP_Text : Behaviour {
  using Class = il2cpp::Class<"Unity.TextMeshPro", "TMPro", "TMP_Text">;

  // TMPro.TextAlignmentOptions, the ones in use.
  enum class Alignment : int32_t {
    Left   = 0x201,
    Center = 0x202,
  };
  // TMPro.TextOverflowModes, the ones in use.
  enum class Overflow : int32_t {
    Ellipsis = 1,
  };

  // A TMP_FontAsset.
  il2cpp::Object* font()
  { return Get<"font", il2cpp::Object*>(); }
  void SetFont(il2cpp::Object* value)
  { Set<"font">(value); }
  void SetFontSize(float value)
  { Set<"fontSize">(value); }
  void SetAlignment(Alignment value)
  { Set<"alignment">(value); }
  void SetColor(Color value)
  { Set<"color">(value); }
  void SetRaycastTarget(bool value)
  { Set<"raycastTarget">(value); }
  void SetEnableWordWrapping(bool value)
  { Set<"enableWordWrapping">(value); }
  void SetRichText(bool value)
  { Set<"richText">(value); }
  void SetOverflowMode(Overflow value)
  { Set<"overflowMode">(value); }
  void SetEnableAutoSizing(bool value)
  { Set<"enableAutoSizing">(value); }
  void SetText(std::string_view value)
  { Set<"text">(il2cpp::NewString(value)); }
};

struct TextMeshProUGUI : TMP_Text {
  using Class = il2cpp::Class<"Unity.TextMeshPro", "TMPro", "TextMeshProUGUI">;
};

struct TMP_InputField : Behaviour {
  using Class = il2cpp::Class<"Unity.TextMeshPro", "TMPro", "TMP_InputField">;

  bool isFocused()
  { return Get<"isFocused", bool>(); }
  void ActivateInputField()
  { Call<"ActivateInputField">(); }
};

// The nearest ancestor of `transform` (or itself) with a Canvas, so an overlay attached there covers the screen.
// `transform` when there is none.
inline Transform* CanvasRoot(Transform* transform)
{
  auto* canvas = transform ? transform->FindInParents<Canvas>() : nullptr;
  auto* root   = canvas ? canvas->transform() : nullptr;
  return root ? root : transform;
}

// An inactive GameObject with a RectTransform under `parent`, for building UI. `root` keeps it alive while the
// caller builds it; once parented, the hierarchy owns it.
inline RectTransform* CreateUiObject(std::string_view name, Transform* parent, il2cpp::GcHandle& root)
{
  auto* object = GameObject::Create(name);
  root         = il2cpp::GcHandle::Strong(object);
  if (!object)
    return nullptr;
  object->SetActive(false);
  auto* transform = object->AddComponent<RectTransform>();
  if (transform)
    transform->SetParent(parent, false);
  return transform;
}

// An image decoded once and kept while Unity keeps it: a scene reload unloads the texture, and the next use
// decodes it again. Holds strong GC handles, so an instance must not outlive the runtime.
class ImageAsset
{
public:
  // Decodes `data` unless the texture is still loaded. False when it cannot be decoded.
  bool Load(const uint8_t* data, size_t size)
  {
    if (Texture())
      return true;
    sprite_.Reset();
    texture_ = il2cpp::GcHandle::Strong(Texture2D::Decode(data, size));
    return Texture() != nullptr;
  }

  Texture2D* Texture() const
  {
    auto* texture = texture_.Get<Texture2D>();
    return Alive(texture) ? texture : nullptr;
  }

  // A sprite over the whole texture, created on first use.
  unity::Sprite* Sprite()
  {
    if (auto* sprite = sprite_.Get<unity::Sprite>(); Alive(sprite))
      return sprite;
    auto* texture = Texture();
    if (!texture)
      return nullptr;
    const Rect whole{.width = static_cast<float>(texture->width()), .height = static_cast<float>(texture->height())};
    auto*      sprite = unity::Sprite::Create(texture, whole, Vector2{.x = 0.5f, .y = 0.5f});
    sprite_           = il2cpp::GcHandle::Strong(sprite);
    return sprite;
  }

  // Height over width; 1 without a texture.
  float AspectRatio() const
  {
    auto* texture = Texture();
    return texture && texture->width() > 0 ? static_cast<float>(texture->height()) / texture->width() : 1.0f;
  }

private:
  il2cpp::GcHandle texture_;
  il2cpp::GcHandle sprite_;
};
} // namespace unity
