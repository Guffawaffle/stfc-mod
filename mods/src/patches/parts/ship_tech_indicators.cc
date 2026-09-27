#include "patches/ship_tech_indicators.h"
#include "patches/ship_identity.h"
#include "patches/pinned_ship_sort.h"
#include "patches/swap_ship_tile.h"
#include "patches/swap_ship_pin_input.h"
#include "config.h"
#include "il2cpp/method_contract.h"
#include "prime/GameObject.h"
#include "prime/ShipTileWidget.h"
#include "prime/Transform.h"

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace
{
constexpr int64_t     kForbiddenSlot      = 3502081615;
constexpr int64_t     kChaosSlot          = 953301906;
constexpr const char* kForbiddenIndicator = "CommunityMod_SwapShipFT";
constexpr const char* kChaosIndicator     = "CommunityMod_SwapShipCT";
constexpr const char* kForbiddenBacking   = "CommunityMod_SwapShipFTBacking";
constexpr const char* kChaosBacking       = "CommunityMod_SwapShipCTBacking";
constexpr const char* kPinBadge           = "CommunityMod_SwapShipPinBadge";
constexpr const char* kPinLabel           = "CommunityMod_SwapShipPinLabel";
constexpr const char* kPinGroupBand       = "CommunityMod_SwapShipPinGroup";
constexpr const char* kPinDragEdges[]     = {"CommunityMod_PinDragTop", "CommunityMod_PinDragBottom",
                                           "CommunityMod_PinDragLeft", "CommunityMod_PinDragRight"};
constexpr float       kBackingMargin       = 4.0f;
bool                  g_available         = false;

struct Vector2 {
  float x, y;
};

struct Vector4 {
  float x, y, z, w;
};

struct Color {
  float r, g, b, a;
};

struct Methods {
  const MethodInfo* context               = nullptr;
  const MethodInfo* manager_instance      = nullptr;
  const MethodInfo* active_slots          = nullptr;
  const MethodInfo* tech_by_id            = nullptr;
  const MethodInfo* tech_spec             = nullptr;
  const MethodInfo* id_refs               = nullptr;
  const MethodInfo* art_id                = nullptr;
  const MethodInfo* set_identifier_params = nullptr;
  const MethodInfo* slot_spec             = nullptr;
  const MethodInfo* slot_item             = nullptr;
  const MethodInfo* selector_target       = nullptr;
  FieldInfo*        selector_target_field = nullptr;
  FieldInfo*        ship_image            = nullptr;
  FieldInfo*        token_image           = nullptr;
  Il2CppClass*      token_widget_class    = nullptr;
  Il2CppClass*      int64_class           = nullptr;
} methods;

bool LogUiDiagnostic()
{
  static unsigned count = 0;
  return count++ < 16;
}

Il2CppObject* Invoke(const MethodInfo* method, void* instance, void** args = nullptr)
{
  if (!method || (!(method->flags & METHOD_ATTRIBUTE_STATIC) && !instance))
    return nullptr;
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, instance, args, &exception);
  if (exception) {
    spdlog::warn("[ShipTechIndicators] accessor={} threw; value unavailable", method->name);
    return nullptr;
  }
  return result;
}

bool InvokeVoid(const MethodInfo* method, void* instance, void** args = nullptr)
{
  if (!method || (!(method->flags & METHOD_ATTRIBUTE_STATIC) && !instance))
    return false;
  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(method, instance, args, &exception);
  if (exception) {
    spdlog::warn("[ShipTechIndicators] operation={} threw", method->name);
    return false;
  }
  return true;
}

template <typename T> T Value(const MethodInfo* method, void* instance, T unknown)
{
  auto* result = Invoke(method, instance);
  return result ? *static_cast<T*>(il2cpp_object_unbox(result)) : unknown;
}

Il2CppObject* Get(Il2CppObject* object, const char* name)
{ return object ? Invoke(IL2CppClassHelper(object->klass).GetMethodInfo(name, 0), object) : nullptr; }

bool Set(Il2CppObject* object, const char* name, void* value)
{
  void* args[]{value};
  return object && InvokeVoid(IL2CppClassHelper(object->klass).GetMethodInfo(name, 1), object, args);
}

const MethodInfo* TypeMethod(Il2CppClass* cls, const char* name)
{
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

const MethodInfo* ObjectMethod(Il2CppClass* cls, const char* name)
{
  void* iter = nullptr;
  while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
    if (method->is_generic || std::strcmp(method->name, name) || method->parameters_count != 1
        || !(method->flags & METHOD_ATTRIBUTE_STATIC))
      continue;
    auto* param = il2cpp_class_from_type(il2cpp_method_get_param(method, 0));
    if (param && !std::strcmp(il2cpp_class_get_namespace(param), "UnityEngine")
        && !std::strcmp(il2cpp_class_get_name(param), "Object"))
      return method;
  }
  return nullptr;
}

Il2CppObject* WithType(const MethodInfo* method, Il2CppObject* target, Il2CppClass* type)
{
  if (!method || !type)
    return nullptr;
  void* args[]{il2cpp_type_get_object(il2cpp_class_get_type(type))};
  return Invoke(method, target, args);
}

Il2CppObject* Component(Il2CppObject* object, Il2CppClass* type)
{ return object ? WithType(TypeMethod(object->klass, "GetComponent"), object, type) : nullptr; }

Il2CppObject* ReferenceField(Il2CppObject* object, FieldInfo* field)
{
  if (!object || !field || field->type->byref || (field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return nullptr;
  auto* cls = il2cpp_class_from_type(field->type);
  if (!cls || il2cpp_class_is_valuetype(cls))
    return nullptr;
  Il2CppObject* result = nullptr;
  il2cpp_field_get_value(object, field, &result);
  return result;
}

bool InstanceClassField(const FieldInfo* field, const char* namespaze, const char* name)
{
  if (!field || !field->type || field->type->byref || (field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject)))
    return false;
  auto* cls = il2cpp_class_from_type(field->type);
  return cls && !il2cpp_class_is_valuetype(cls) && !std::strcmp(il2cpp_class_get_namespace(cls), namespaze)
         && !std::strcmp(il2cpp_class_get_name(cls), name);
}

FieldInfo* FindInstanceClassField(Il2CppClass* cls, const char* namespaze, const char* name)
{
  while (cls) {
    void* iter = nullptr;
    while (auto* field = il2cpp_class_get_fields(cls, &iter)) {
      if (InstanceClassField(field, namespaze, name))
        return field;
    }
    cls = il2cpp_class_get_parent(cls);
  }
  return nullptr;
}

const MethodInfo* FindInstanceMethod(Il2CppClass* cls, const char* name, const char* result, const char* parameter)
{
  while (cls) {
    void* iter = nullptr;
    while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
      if (method->methodPointer && !method->is_generic && !(method->flags & METHOD_ATTRIBUTE_STATIC)
          && method->parameters_count == 1 && !std::strcmp(method->name, name)
          && method_contract::Type(method->return_type, result)
          && method_contract::Type(method->parameters[0], parameter))
        return method;
    }
    cls = il2cpp_class_get_parent(cls);
  }
  return nullptr;
}

Il2CppObject* SelectorTarget(Il2CppObject* selector)
{
  auto* target = Invoke(methods.selector_target, selector);
  return target ? target : ReferenceField(selector, methods.selector_target_field);
}

Transform* ComponentTransform(Il2CppObject* component)
{ return reinterpret_cast<Transform*>(Get(component, "get_transform")); }

GameObject* TokenImageSource()
{
  static auto  resources = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Resources");
  static auto* find_all  = method_contract::Resolve(resources.get_cls(), "FindObjectsOfTypeAll", true,
                                                    "UnityEngine.Object[]", {"System.Type"});
  if (!find_all || !methods.token_widget_class || !methods.token_image)
    return nullptr;
  void* args[]{il2cpp_type_get_object(il2cpp_class_get_type(methods.token_widget_class))};
  auto* array = reinterpret_cast<Il2CppArraySize*>(Invoke(find_all, nullptr, args));
  if (!array)
    return nullptr;
  for (il2cpp_array_size_t index = 0; index < array->max_length; ++index) {
    auto* widget    = static_cast<Il2CppObject*>(array->vector[index]);
    auto* selector  = ReferenceField(widget, methods.token_image);
    auto* transform = ComponentTransform(selector);
    if (transform && transform->gameObject)
      return transform->gameObject;
  }
  return nullptr;
}

Transform* DirectChild(Transform* parent, const char* name)
{
  if (!parent)
    return nullptr;
  for (int32_t index = 0; index < parent->childCount; ++index) {
    auto* child  = parent->GetChild(index);
    auto* object = child ? child->gameObject : nullptr;
    if (object && object->Name() == name)
      return child;
  }
  return nullptr;
}

Transform* SelectedIndicatorTransform(Transform* tile)
{
  if (!tile)
    return nullptr;
  auto* raw = reinterpret_cast<Il2CppObject*>(tile);
  void* args[]{il2cpp_string_new("OverlayElements/TickItem")};
  return reinterpret_cast<Transform*>(Invoke(IL2CppClassHelper(raw->klass).GetMethodInfo("Find", 1), raw, args));
}

Vector2 SelectedIndicatorSize(Transform* tile, float inset = 0)
{
  auto* transform = SelectedIndicatorTransform(tile);
  auto  size = Value<Vector2>(transform ? IL2CppClassHelper(reinterpret_cast<Il2CppObject*>(transform)->klass)
                                             .GetMethodInfo("get_sizeDelta", 0)
                                       : nullptr,
                              transform, {46, 46});
  if (size.x < 12 || size.x > 128 || size.y < 12 || size.y > 128)
    size = {46, 46};
  size.x -= inset * 2;
  size.y -= inset * 2;
  return size;
}

bool SelectedCard(Transform* tile)
{
  auto* tick = SelectedIndicatorTransform(tile);
  auto* object = tick ? reinterpret_cast<Il2CppObject*>(tick->gameObject) : nullptr;
  return object && Value<bool>(IL2CppClassHelper(object->klass).GetMethodInfo("get_activeInHierarchy", 0), object,
                               false);
}

Il2CppObject* SelectedBackingSprite(Transform* tile)
{
  static auto image = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*       tick  = SelectedIndicatorTransform(tile);
  auto*       graphic = tick ? Component(reinterpret_cast<Il2CppObject*>(tick->gameObject), image.get_cls()) : nullptr;
  if (!graphic)
    return nullptr;
  auto* sprite = Get(graphic, "get_overrideSprite");
  if (!sprite)
    sprite = Get(graphic, "get_sprite");
  return sprite;
}

Il2CppObject* RoundedBackingSprite()
{
  static constexpr uint8_t rounded_png[]{
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x00,
      0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
      0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc,
      0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
      0x0e, 0xc3, 0x00, 0x00, 0x0e, 0xc3, 0x01, 0xc7, 0x6f, 0xa8, 0x64, 0x00,
      0x00, 0x00, 0x48, 0x49, 0x44, 0x41, 0x54, 0x58, 0x47, 0xed, 0xd7, 0xb1,
      0x0d, 0x00, 0x30, 0x08, 0x03, 0x41, 0xf6, 0x5f, 0xda, 0x91, 0x52, 0x05,
      0xfa, 0x60, 0x17, 0xff, 0x12, 0xb5, 0xaf, 0xa5, 0x24, 0xd5, 0xb8, 0xdf,
      0xb5, 0xbd, 0xcd, 0xe1, 0x59, 0x03, 0xb8, 0xca, 0x00, 0x58, 0x03, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04,
      0xc0, 0x89, 0xc8, 0xf8, 0x8c, 0x5c, 0x88, 0xbb, 0xfb, 0x02, 0xb6, 0x20,
      0x6d, 0xef, 0x00, 0xba, 0xbb, 0xd1, 0x02, 0x42, 0x5d, 0x7b, 0xec, 0x00,
      0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
  };
  static Il2CppGCHandle sprite_handle = nullptr;
  static bool attempted = false;
  if (attempted)
    return sprite_handle ? il2cpp_gchandle_get_target(sprite_handle) : nullptr;
  attempted = true;

  static auto texture = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
  static auto sprite = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Sprite");
  static auto conversion = il2cpp_get_class_helper("UnityEngine.ImageConversionModule", "UnityEngine", "ImageConversion");
  static auto byte = il2cpp_get_class_helper("mscorlib", "System", "Byte");
  auto* ctor = texture.GetMethodInfo(".ctor", 4);
  auto* load = conversion.GetMethodInfoSpecial("LoadImage", [](int count, const Il2CppType** parameters) {
    return count == 3 && parameters[1]->type == IL2CPP_TYPE_SZARRAY && parameters[2]->type == IL2CPP_TYPE_BOOLEAN;
  });
  auto* create = sprite.GetMethodInfo("Create", 7);
  if (!ctor || !load || !create || !byte.isValidHelper())
    return nullptr;

  auto* tex = il2cpp_object_new(texture.get_cls());
  auto* pixels = il2cpp_array_new(byte.get_cls(), sizeof(rounded_png));
  auto tex_handle = tex ? il2cpp_gchandle_new(tex, false) : nullptr;
  auto pixels_handle = pixels ? il2cpp_gchandle_new(reinterpret_cast<Il2CppObject*>(pixels), false) : nullptr;
  if (!tex_handle || !pixels_handle) {
    if (tex_handle)
      il2cpp_gchandle_free(tex_handle);
    if (pixels_handle)
      il2cpp_gchandle_free(pixels_handle);
    return nullptr;
  }

  std::memcpy(reinterpret_cast<Il2CppArraySize*>(pixels)->vector, rounded_png, sizeof(rounded_png));

  int width = 32, height = 32, format = 4;
  bool mipmaps = false;
  void* ctor_args[]{&width, &height, &format, &mipmaps};
  bool mark_non_readable = false;
  void* load_args[]{il2cpp_gchandle_get_target(tex_handle), il2cpp_gchandle_get_target(pixels_handle),
                    &mark_non_readable};
  const bool initialized = InvokeVoid(ctor, il2cpp_gchandle_get_target(tex_handle), ctor_args);
  auto* load_result = initialized ? Invoke(load, nullptr, load_args) : nullptr;
  const bool loaded = load_result && *static_cast<bool*>(il2cpp_object_unbox(load_result));
  if (loaded) {
    struct RectValue { float x, y, width, height; } rect{0, 0, 32, 32};
    Vector2 pivot{0.5f, 0.5f};
    float pixels_per_unit = 100;
    uint32_t extrude = 0;
    int mesh_type = 1;
    Vector4 border{6, 6, 6, 6};
    void* create_args[]{il2cpp_gchandle_get_target(tex_handle), &rect, &pivot, &pixels_per_unit, &extrude, &mesh_type,
                        &border};
    if (auto* created = Invoke(create, nullptr, create_args))
      sprite_handle = il2cpp_gchandle_new(created, false);
  }
  il2cpp_gchandle_free(pixels_handle);
  il2cpp_gchandle_free(tex_handle);
  return sprite_handle ? il2cpp_gchandle_get_target(sprite_handle) : nullptr;
}

bool Rect(Il2CppObject* transform, Vector2 anchor, Vector2 pivot, Vector2 size, Vector2 position)
{
  return Set(transform, "set_anchorMin", &anchor) && Set(transform, "set_anchorMax", &anchor)
         && Set(transform, "set_pivot", &pivot) && Set(transform, "set_sizeDelta", &size)
         && Set(transform, "set_anchoredPosition", &position);
}

void Destroy(Il2CppObject* object)
{
  if (!object)
    return;
  static auto unity_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  void*       args[]{object};
  InvokeVoid(unity_object.GetMethodInfo("Destroy", 1), nullptr, args);
}

GameObject* CreateIndicator(const char* name, Transform* parent, GameObject* source, Vector2 anchor, Vector2 pivot,
                            Vector2 position)
{
  static auto unity_object   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto image_selector = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ImageSelector");
  static auto image          = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto instantiate    = ObjectMethod(unity_object.get_cls(), "Instantiate");
  if (!parent || !source || !unity_object.isValidHelper() || !image_selector.isValidHelper() || !image.isValidHelper()
      || !instantiate)
    return nullptr;

  void*      clone_args[]{source};
  auto*      object = Invoke(instantiate, nullptr, clone_args);
  const bool named  = object && Set(object, "set_name", il2cpp_string_new(name));
  if (!object || !named) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] create={} clone={} named={}", name, object != nullptr, named);
    return nullptr;
  }

  bool active = false;
  if (!Set(object, "SetActive", &active)) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] create={} deactivate failed", name);
    Destroy(object);
    return nullptr;
  }

  auto*      transform            = reinterpret_cast<Il2CppObject*>(ComponentTransform(object));
  bool       world_position_stays = false;
  void*      parent_args[]{parent, &world_position_stays};
  auto*      selector = Component(object, image_selector.get_cls());
  auto*      graphic  = SelectorTarget(selector);
  bool       no       = false;
  bool       yes      = true;
  const bool parented =
      transform
      && InvokeVoid(IL2CppClassHelper(transform->klass).GetMethodInfo("SetParent", 2), transform, parent_args);
  const bool rect =
      transform && Rect(transform, anchor, pivot, SelectedIndicatorSize(parent, kBackingMargin), position);
  const bool raycast  = graphic && Set(graphic, "set_raycastTarget", &no);
  const bool preserve = graphic && Set(graphic, "set_preserveAspect", &yes);
  const bool ok = transform && selector && graphic && parented && rect && raycast && preserve;
  if (!ok) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] create={} transform={} selector={} target={} parented={} rect={} raycast={} "
                   "preserve={}",
                   name, transform != nullptr, selector != nullptr, graphic != nullptr, parented, rect, raycast,
                   preserve);
    Destroy(object);
    return nullptr;
  }
  return reinterpret_cast<GameObject*>(object);
}

GameObject* CreateBacking(const char* name, Transform* parent, Vector2 anchor, Vector2 pivot, Vector2 position,
                          bool use_selected_sprite = true, bool use_rounded_sprite = false)
{
  static auto game_object    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto rect_transform = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto image           = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto constructor     = game_object.GetMethodInfo(".ctor", 1);
  if (!parent || !game_object.isValidHelper() || !rect_transform.isValidHelper() || !image.isValidHelper()
      || !constructor)
    return nullptr;

  auto* object = reinterpret_cast<GameObject*>(il2cpp_object_new(game_object.get_cls()));
  if (!object)
    return nullptr;
  auto*      raw    = reinterpret_cast<Il2CppObject*>(object);
  const auto handle = il2cpp_gchandle_new(raw, false);
  if (!handle) {
    Destroy(raw);
    return nullptr;
  }

  void* name_args[]{il2cpp_string_new(name)};
  bool  active = false;
  bool  ok     = InvokeVoid(constructor, raw, name_args) && Set(raw, "SetActive", &active);
  auto* transform = ok ? WithType(TypeMethod(raw->klass, "AddComponent"), raw, rect_transform.get_cls()) : nullptr;
  auto* graphic   = transform ? WithType(TypeMethod(raw->klass, "AddComponent"), raw, image.get_cls()) : nullptr;
  bool  world_position_stays = false;
  void* parent_args[]{parent, &world_position_stays};
  bool  no = false;
  Color color{0.01f, 0.02f, 0.03f, 0.46f};
  position.x += (pivot.x - 0.5f) * kBackingMargin * 2;
  ok = transform && graphic
       && InvokeVoid(IL2CppClassHelper(transform->klass).GetMethodInfo("SetParent", 2), transform, parent_args)
       && Rect(transform, anchor, pivot, SelectedIndicatorSize(parent), position) && Set(graphic, "set_color", &color)
       && Set(graphic, "set_raycastTarget", &no);
  auto* sprite = ok ? (use_rounded_sprite ? RoundedBackingSprite()
                                         : use_selected_sprite ? SelectedBackingSprite(parent) : nullptr)
                    : nullptr;
  if (ok && sprite) {
    ok = Set(graphic, "set_sprite", sprite);
    if (use_rounded_sprite) {
      int sliced = 1;
      ok = ok && Set(graphic, "set_type", &sliced);
    }
  } else if (ok && (use_selected_sprite || use_rounded_sprite) && LogUiDiagnostic()) {
    spdlog::warn("[ShipTechIndicators] backing sprite unavailable; using square backing");
  }
  if (!ok) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] backing={} creation failed", name);
    Destroy(raw);
    object = nullptr;
  }
  il2cpp_gchandle_free(handle);
  return object;
}

int64_t ArtId(int64_t tech_id)
{
  if (!tech_id)
    return 0;
  auto* manager = Invoke(methods.manager_instance, nullptr);
  void* id_args[]{&tech_id};
  auto* tech = Invoke(methods.tech_by_id, manager, id_args);
  auto* spec = Invoke(methods.tech_spec, tech);
  auto* refs = Invoke(methods.id_refs, spec);
  return Value<int64_t>(methods.art_id, refs, 0);
}

void UpdateIndicator(Transform* parent, GameObject* source, const char* name, const char* backing_name, Vector2 anchor,
                     Vector2 pivot, Vector2 position, int64_t art_id)
{
  static auto image_selector = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ImageSelector");
  auto*       child          = DirectChild(parent, name);
  auto*       object         = child ? child->gameObject : nullptr;
  auto*       backing_child  = DirectChild(parent, backing_name);
  auto*       backing        = backing_child ? backing_child->gameObject : nullptr;
  if (object)
    object->SetActive(false);
  if (backing)
    backing->SetActive(false);
  if (!art_id)
    return;

  if (!object) {
    auto* token_source = TokenImageSource();
    object             = CreateIndicator(name, parent, token_source ? token_source : source, anchor, pivot, position);
  }
  auto* selector = Component(reinterpret_cast<Il2CppObject*>(object), image_selector.get_cls());
  auto* graphic  = SelectorTarget(selector);
  if (!object || !selector || !graphic) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] update={} object={} selector={} target={}", name, object != nullptr,
                   selector != nullptr, graphic != nullptr);
    return;
  }
  auto* object_transform = ComponentTransform(reinterpret_cast<Il2CppObject*>(object));
  if (!object_transform
      || !Rect(reinterpret_cast<Il2CppObject*>(object_transform), anchor, pivot,
               SelectedIndicatorSize(parent, kBackingMargin), position))
    return;

  auto* boxed_art_id = il2cpp_value_box(methods.int64_class, &art_id);
  void* identifier_args[]{boxed_art_id};
  if (!boxed_art_id || !InvokeVoid(methods.set_identifier_params, selector, identifier_args)) {
    if (LogUiDiagnostic())
      spdlog::warn("[ShipTechIndicators] update={} identifier assignment failed", name);
    return;
  }

  static auto transform = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  const bool  show_backing = Config::Get().show_ship_tech_indicator_backgrounds;
  if (show_backing && !backing)
    backing = CreateBacking(backing_name, parent, anchor, pivot, position, false, true);
  if (backing) {
    auto backing_position = position;
    backing_position.x += (pivot.x - 0.5f) * kBackingMargin * 2;
    backing_position.y += kBackingMargin;
    auto* backing_transform = ComponentTransform(reinterpret_cast<Il2CppObject*>(backing));
    if (!backing_transform || !Rect(reinterpret_cast<Il2CppObject*>(backing_transform), anchor, pivot,
                                    SelectedIndicatorSize(parent), backing_position))
      return;
    backing->SetActive(show_backing);
    if (show_backing)
      InvokeVoid(transform.GetMethodInfo("SetAsLastSibling", 0),
                 ComponentTransform(reinterpret_cast<Il2CppObject*>(backing)));
  }
  object->SetActive(true);
  InvokeVoid(transform.GetMethodInfo("SetAsLastSibling", 0),
             ComponentTransform(reinterpret_cast<Il2CppObject*>(object)));
}

void UpdateIndicators(Transform* parent, GameObject* source, int64_t ft_art = 0, int64_t ct_art = 0)
{
  const bool selected = SelectedCard(parent);
  UpdateIndicator(parent, source, kForbiddenIndicator, kForbiddenBacking, {0, 1}, {0, 1},
                  selected ? Vector2{16, -3} : Vector2{8, -3}, ft_art);
  UpdateIndicator(parent, source, kChaosIndicator, kChaosBacking, {1, 1}, {1, 1},
                  selected ? Vector2{-56, -3} : Vector2{-8, -3}, ct_art);
}

bool CreatePinPiece(Transform* parent, const char* name, Vector2 size, Vector2 position, Color color)
{
  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto* piece = CreateBacking(name, parent, {0, 0}, {0.5f, 0.5f}, position, false);
  if (!piece)
    return false;
  auto* raw = reinterpret_cast<Il2CppObject*>(piece);
  auto* transform = reinterpret_cast<Il2CppObject*>(ComponentTransform(raw));
  auto* image = Component(raw, image_helper.get_cls());
  if (!transform || !image || !Rect(transform, {0, 0}, {0.5f, 0.5f}, size, position)
      || !Set(image, "set_color", &color)) {
    Destroy(raw);
    return false;
  }
  piece->SetActive(true);
  return true;
}

void UpdatePinBadge(Transform* parent, FleetPlayerData* fleet)
{
  static auto text_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*       child = DirectChild(parent, kPinBadge);
  auto*       badge = child ? child->gameObject : nullptr;
  const auto  rank = pinned_ship_sort::Rank(fleet);
  if (!fleet || !fleet->HasShip || !ship_identity::InstanceId(fleet)) {
    if (badge)
      badge->SetActive(false);
    return;
  }

  if (!badge) {
    badge = CreateBacking(kPinBadge, parent, {1, 0}, {1, 0}, {-3, 8});
    if (!badge)
      return;
    auto* raw = reinterpret_cast<Il2CppObject*>(badge);
    auto* transform = reinterpret_cast<Il2CppObject*>(ComponentTransform(raw));
    auto* background = Component(raw, image_helper.get_cls());
    auto* badge_transform = ComponentTransform(raw);
    static auto game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    static auto rect_transform = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
    auto* label_raw = game_object.isValidHelper() ? il2cpp_object_new(game_object.get_cls()) : nullptr;
    const auto handle = label_raw ? il2cpp_gchandle_new(label_raw, false) : 0;
    auto* constructor = game_object.GetMethodInfo(".ctor", 1);
    void* name_args[]{il2cpp_string_new(kPinLabel)};
    bool world_position_stays = false;
    void* parent_args[]{ComponentTransform(raw), &world_position_stays};
    auto* label_transform = handle && constructor && rect_transform.isValidHelper()
                                && InvokeVoid(constructor, label_raw, name_args)
                                ? WithType(TypeMethod(label_raw->klass, "AddComponent"), label_raw,
                                           rect_transform.get_cls())
                                : nullptr;
    auto* label = label_transform && text_helper.isValidHelper()
                      ? WithType(TypeMethod(label_raw->klass, "AddComponent"), label_raw, text_helper.get_cls())
                      : nullptr;
    Color backing{0.06f, 0.10f, 0.12f, 0.82f};
    Color foreground{0.97f, 0.84f, 0.48f, 1.0f};
    float font_size = 13;
    int alignment = 0x202;
    bool no = false;
    const bool ready = transform && background && badge_transform && label && label_transform
        && InvokeVoid(IL2CppClassHelper(label_transform->klass).GetMethodInfo("SetParent", 2), label_transform,
                      parent_args)
        && Rect(transform, {1, 0}, {1, 0}, {34, 24}, {-3, 8})
        && Rect(label_transform, {0, 0.5f}, {0, 0.5f}, {18, 22}, {15, 0})
        && Set(background, "set_color", &backing) && Set(label, "set_fontSize", &font_size)
        && Set(label, "set_alignment", &alignment) && Set(label, "set_color", &foreground)
        && Set(label, "set_raycastTarget", &no) && Set(label, "set_enableWordWrapping", &no)
        && CreatePinPiece(badge_transform, "CommunityMod_PinCap", {11, 3}, {9, 17}, foreground)
        && CreatePinPiece(badge_transform, "CommunityMod_PinHead", {7, 6}, {9, 13}, foreground)
        && CreatePinPiece(badge_transform, "CommunityMod_PinStem", {2, 9}, {9, 6}, foreground);
    if (handle)
      il2cpp_gchandle_free(handle);
    if (!ready) {
      if (label_raw)
        Destroy(label_raw);
      Destroy(raw);
      return;
    }
  }

  auto* label_child = DirectChild(ComponentTransform(reinterpret_cast<Il2CppObject*>(badge)), kPinLabel);
  auto* label = label_child ? Component(reinterpret_cast<Il2CppObject*>(label_child->gameObject), text_helper.get_cls())
                            : nullptr;
  const auto text = rank ? *rank <= 99 ? std::to_string(*rank) : std::string{"+"} : std::string{};
  if (!label || !Set(label, "set_text", il2cpp_string_new(text.c_str())))
    return;
  badge->SetActive(true);
}

struct ActiveSlots {
  int64_t ft = 0;
  int64_t ct = 0;
};

int64_t NullableInt64(Il2CppObject* value)
{
  if (!value)
    return 0;
  if (!std::strcmp(il2cpp_class_get_namespace(value->klass), "System")
      && !std::strcmp(il2cpp_class_get_name(value->klass), "Int64"))
    return *static_cast<int64_t*>(il2cpp_object_unbox(value));
  static bool warned = false;
  if (!warned) {
    warned = true;
    spdlog::warn("[ShipTechIndicators] unexpected SlotItemId box type={}.{}", il2cpp_class_get_namespace(value->klass),
                 il2cpp_class_get_name(value->klass));
  }
  return 0;
}

ActiveSlots ReadActiveSlots(int64_t ship_id)
{
  ActiveSlots   result;
  auto*         manager = Invoke(methods.manager_instance, nullptr);
  Il2CppObject* list    = nullptr;
  void*         args[]{&ship_id, &list};
  auto*         found = Invoke(methods.active_slots, manager, args);
  if (!found || !*static_cast<bool*>(il2cpp_object_unbox(found)) || !list)
    return result;

  auto      helper = IL2CppClassHelper(list->klass);
  const int count  = Value<int32_t>(helper.GetMethodInfo("get_Count", 0), list, 0);
  auto*     item   = helper.GetMethodInfo("get_Item", 1);
  for (int index = 0; index < count; ++index) {
    void* item_args[]{&index};
    auto* slot = Invoke(item, list, item_args);
    if (!slot)
      continue;
    const auto spec_id  = Value<int64_t>(methods.slot_spec, slot, 0);
    auto*      selected = spec_id == kForbiddenSlot ? &result.ft : spec_id == kChaosSlot ? &result.ct : nullptr;
    if (!selected)
      continue;
    *selected = NullableInt64(Invoke(methods.slot_item, slot));
  }
  return result;
}

void SetWidgetData_Hook(auto original, ShipTileWidget* widget)
{
  original(widget);

  auto* tile_parent  = ComponentTransform(reinterpret_cast<Il2CppObject*>(widget));
  auto* image_parent = ComponentTransform(ReferenceField(reinterpret_cast<Il2CppObject*>(widget), methods.ship_image));
  auto* image_source = image_parent ? image_parent->gameObject : nullptr;
  if (!tile_parent)
    return;
  const bool in_selection = swap_ship_tile::IsInSelection(widget);
  const bool pinning_enabled = in_selection && Config::Get().installPinnedShipSortHooks
                               && pinned_ship_sort::Available();
  if (pinning_enabled)
    swap_ship_pin_input::RegisterTile(widget);
  auto* context = Invoke(methods.context, widget);
  UpdatePinBadge(tile_parent, pinning_enabled ? reinterpret_cast<FleetPlayerData*>(context) : nullptr);
  if (!in_selection || !g_available || !Config::Get().show_ship_tech_indicators) {
    UpdateIndicators(tile_parent, image_source);
    return;
  }

  const auto ship_id = ship_identity::InstanceId(reinterpret_cast<FleetPlayerData*>(context));
  const auto active  = ship_id ? ReadActiveSlots(*ship_id) : ActiveSlots{};
  UpdateIndicators(tile_parent, image_source, ArtId(active.ft), ArtId(active.ct));
}
} // namespace

namespace ship_tech_indicators
{
bool Available()
{ return g_available; }

void RefreshPinBadge(ShipTileWidget* widget)
{
  auto* parent = widget ? ComponentTransform(reinterpret_cast<Il2CppObject*>(widget)) : nullptr;
  if (parent)
    UpdatePinBadge(parent, swap_ship_tile::IsInSelection(widget) ? widget->Context : nullptr);
}

void SetPinDragEdge(Transform* parent, const char* name, Vector2 anchor_min, Vector2 anchor_max, Vector2 pivot,
                    Vector2 size, Vector2 position, Color color, bool visible)
{
  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto* child = DirectChild(parent, name);
  auto* object = child ? child->gameObject : nullptr;
  if (!visible) {
    if (object)
      object->SetActive(false);
    return;
  }
  if (!object)
    object = CreateBacking(name, parent, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, false);
  auto* raw = reinterpret_cast<Il2CppObject*>(object);
  auto* transform = ComponentTransform(raw);
  auto* rect = reinterpret_cast<Il2CppObject*>(transform);
  auto* image = Component(raw, image_helper.get_cls());
  if (!rect || !image || !Set(rect, "set_anchorMin", &anchor_min)
      || !Set(rect, "set_anchorMax", &anchor_max) || !Set(rect, "set_pivot", &pivot)
      || !Set(rect, "set_sizeDelta", &size) || !Set(rect, "set_anchoredPosition", &position)
      || !Set(image, "set_color", &color))
    return;
  object->SetActive(true);
}

void UpdatePinGroupBand(Transform* content, std::optional<PinGroupBounds> bounds)
{
  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto layout_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "LayoutElement");
  static auto transform_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  auto* child = DirectChild(content, kPinGroupBand);
  auto* band = child ? child->gameObject : nullptr;
  if (!bounds || bounds->right <= bounds->left || bounds->top <= bounds->bottom
      || bounds->right - bounds->left > 1'000'000 || bounds->top - bounds->bottom > 1'000'000) {
    if (band)
      band->SetActive(false);
    return;
  }

  if (!band) {
    band = CreateBacking(kPinGroupBand, content, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, false, true);
    auto* raw = reinterpret_cast<Il2CppObject*>(band);
    auto* layout = raw ? WithType(TypeMethod(raw->klass, "AddComponent"), raw, layout_helper.get_cls()) : nullptr;
    bool ignore_layout = true;
    if (!layout || !Set(layout, "set_ignoreLayout", &ignore_layout)) {
      Destroy(raw);
      return;
    }
  }

  auto* raw = reinterpret_cast<Il2CppObject*>(band);
  auto* transform = ComponentTransform(raw);
  auto* rect = reinterpret_cast<Il2CppObject*>(transform);
  auto* image = Component(raw, image_helper.get_cls());
  if (!rect || !image)
    return;

  constexpr float margin = 7.0f;
  const Vector2 size{bounds->right - bounds->left + margin * 2, bounds->top - bounds->bottom + margin * 2};
  Vector3 position{(bounds->left + bounds->right) * 0.5f, (bounds->bottom + bounds->top) * 0.5f, 0};
  Color fill{0.06f, 0.17f, 0.20f, 0.72f};
  if (!Rect(rect, {0.5f, 0.5f}, {0.5f, 0.5f}, size, {0, 0})
      || !Set(rect, "set_localPosition", &position) || !Set(image, "set_color", &fill)
      || !InvokeVoid(transform_helper.GetMethodInfo("SetAsFirstSibling", 0), rect))
    return;
  band->SetActive(true);

  const Color rim{0.83f, 0.67f, 0.32f, 0.72f};
  SetPinDragEdge(transform, "CommunityMod_PinGroupTop", {0, 1}, {1, 1}, {0.5f, 1}, {-12, 1.5f}, {0, -1}, rim, true);
  SetPinDragEdge(transform, "CommunityMod_PinGroupBottom", {0, 0}, {1, 0}, {0.5f, 0}, {-12, 1.5f}, {0, 1}, rim, true);
  SetPinDragEdge(transform, "CommunityMod_PinGroupLeft", {0, 0}, {0, 1}, {0, 0.5f}, {1.5f, -12}, {1, 0}, rim, true);
  SetPinDragEdge(transform, "CommunityMod_PinGroupRight", {1, 0}, {1, 1}, {1, 0.5f}, {1.5f, -12}, {-1, 0}, rim, true);
}

void SetPinBadgeHighlight(ShipTileWidget* widget, PinBadgeHighlight highlight)
{
  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto* parent = widget ? ComponentTransform(reinterpret_cast<Il2CppObject*>(widget)) : nullptr;
  auto* badge = DirectChild(parent, kPinBadge);
  auto* image = badge && badge->gameObject
                    ? Component(reinterpret_cast<Il2CppObject*>(badge->gameObject), image_helper.get_cls())
                    : nullptr;
  if (!image)
    return;
  Color color = highlight == PinBadgeHighlight::Source ? Color{0.52f, 0.27f, 0.02f, 0.98f}
                : highlight == PinBadgeHighlight::Target ? Color{0.03f, 0.40f, 0.34f, 0.95f}
                                                         : Color{0.06f, 0.10f, 0.12f, 0.82f};
  Set(image, "set_color", &color);
  const bool visible = highlight != PinBadgeHighlight::None;
  const bool source = highlight == PinBadgeHighlight::Source;
  const float width = source ? 4.5f : 2.5f;
  const Color edge = source ? Color{1.0f, 0.76f, 0.16f, 1.0f}
                                                      : Color{0.10f, 0.82f, 0.66f, 0.9f};
  SetPinDragEdge(parent, kPinDragEdges[0], {0, 1}, {1, 1}, {0.5f, 1}, {-8, width}, {0, -3}, edge, visible);
  SetPinDragEdge(parent, kPinDragEdges[1], {0, 0}, {1, 0}, {0.5f, 0}, {-8, width}, {0, 3}, edge, visible);
  SetPinDragEdge(parent, kPinDragEdges[2], {0, 0}, {0, 1}, {0, 0.5f}, {width, -8}, {3, 0}, edge, visible);
  SetPinDragEdge(parent, kPinDragEdges[3], {1, 0}, {1, 1}, {1, 0.5f}, {width, -8}, {-3, 0}, edge, visible);
}
} // namespace ship_tech_indicators

void InstallShipTechIndicatorHooks()
{
  auto widget = ShipTileWidget::get_class_helper();
  auto parent = widget.GetParent("Widget`1");
  auto manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ForbiddenTechs", "ForbiddenTechManager");
  auto tech    = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ForbiddenTech");
  auto spec = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ForbiddenTechSpec");
  auto entity_slot = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "EntitySlot");
  auto id_refs     = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IdRefs");
  auto image_selector = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ImageSelector");
  auto token_widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ForbiddenTechs", "ForbiddenTechSlotWidget");
  auto int64_type = il2cpp_get_class_helper("mscorlib", "System", "Int64");

  auto* property  = parent.get_cls() ? il2cpp_class_get_property_from_name(parent.get_cls(), "Context") : nullptr;
  methods.context = property ? property->get : nullptr;
  methods.manager_instance = manager.GetParent("MonoSingleton`1").GetMethodInfo("get_Instance", 0);
  methods.active_slots     = manager.GetMethodInfo("TryGetActiveForbiddenTechSlot", 2);
  methods.tech_by_id       = method_contract::Resolve(manager.get_cls(), "GetForbiddenTechById", false,
                                                      "Digit.PrimeServer.Models.ForbiddenTech", {"System.Int64"});
  methods.tech_spec        = method_contract::Resolve(tech.get_cls(), "get_ForbiddenTechSpec", false,
                                                      "Digit.PrimeServer.Models.ForbiddenTechSpec", {});
  methods.id_refs =
      method_contract::Resolve(spec.get_cls(), "get_IdRefs", false, "Digit.PrimeServer.Models.IdRefs", {});
  methods.art_id    = method_contract::Resolve(id_refs.get_cls(), "get_ArtId", false, "System.Int64", {});
  methods.slot_spec = method_contract::Resolve(entity_slot.get_cls(), "get_SlotSpecId", false, "System.Int64", {});
  methods.slot_item = entity_slot.GetMethodInfo("get_SlotItemId", 0);
  methods.set_identifier_params =
      FindInstanceMethod(image_selector.get_cls(), "SetIdentifierParams", "System.Void", "System.Object");
  methods.selector_target       = image_selector.GetMethodInfo("get_Target", 0);
  methods.selector_target_field = FindInstanceClassField(image_selector.get_cls(), "UnityEngine.UI", "Image");
  methods.ship_image = widget.get_cls() ? il2cpp_class_get_field_from_name(widget.get_cls(), "_shipImage") : nullptr;
  methods.token_image =
      token_widget.get_cls() ? il2cpp_class_get_field_from_name(token_widget.get_cls(), "_tokenImage") : nullptr;
  methods.token_widget_class = token_widget.get_cls();
  methods.int64_class        = int64_type.get_cls();

  const bool manager_valid      = methods.manager_instance && methods.manager_instance->methodPointer
                                  && (methods.manager_instance->flags & METHOD_ATTRIBUTE_STATIC)
                                  && methods.manager_instance->parameters_count == 0
                                  && method_contract::Type(methods.manager_instance->return_type,
                                                           "Digit.Prime.ForbiddenTechs.ForbiddenTechManager");
  const bool active_slots_valid = methods.active_slots && methods.active_slots->methodPointer
                                  && !(methods.active_slots->flags & METHOD_ATTRIBUTE_STATIC)
                                  && method_contract::Type(methods.active_slots->return_type, "System.Boolean")
                                  && methods.active_slots->parameters_count == 2
                                  && method_contract::Type(methods.active_slots->parameters[0], "System.Int64")
                                  && methods.active_slots->parameters[1]->byref;
  const bool slot_item_valid =
      methods.slot_item && methods.slot_item->methodPointer && !(methods.slot_item->flags & METHOD_ATTRIBUTE_STATIC)
      && methods.slot_item->parameters_count == 0 && methods.slot_item->return_type->type == IL2CPP_TYPE_GENERICINST;
  const bool image_valid  = InstanceClassField(methods.ship_image, "Digit.Client.UI", "ImageSelector");
  const bool token_valid  = InstanceClassField(methods.token_image, "Digit.Client.UI", "ImageSelector");
  const bool target_valid = methods.selector_target && methods.selector_target->methodPointer
                            && !(methods.selector_target->flags & METHOD_ATTRIBUTE_STATIC)
                            && methods.selector_target->parameters_count == 0
                            && InstanceClassField(methods.selector_target_field, "UnityEngine.UI", "Image");
  auto*      target       = method_contract::Resolve(widget.get_cls(), "SetWidgetData", false, "System.Void", {});
  if (!target || !methods.context || !methods.context->methodPointer || !ship_identity::Available()) {
    spdlog::error("[ShipTechIndicators] tile binding unavailable: hook={} context={} shipId={}", bool(target),
                  bool(methods.context), ship_identity::Available());
    return;
  }
  const bool tech_available = manager_valid && methods.tech_by_id && active_slots_valid && methods.tech_spec
                              && methods.id_refs && methods.art_id && methods.slot_spec && slot_item_valid
                              && methods.set_identifier_params && methods.int64_class && image_valid && token_valid
                              && target_valid;
  if (!SPUD_STATIC_DETOUR(method_contract::Pointer(target), SetWidgetData_Hook)) {
    spdlog::error("[ShipTechIndicators] failed to install ShipTileWidget.SetWidgetData detour");
    return;
  }
  g_available = Config::Get().installShipTechIndicatorHooks && tech_available;
  spdlog::info("[ShipTechIndicators] installed Swap Ship pin badges; FT/CT art available={}", g_available);
}
