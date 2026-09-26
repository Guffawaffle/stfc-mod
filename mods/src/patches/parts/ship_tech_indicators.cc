#include "patches/ship_tech_indicators.h"
#include "config.h"
#include "il2cpp/method_contract.h"
#include "prime/GameObject.h"
#include "prime/ShipTileWidget.h"
#include "prime/Transform.h"

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstdint>
#include <cstring>

namespace
{
constexpr int64_t     kForbiddenSlot      = 3502081615;
constexpr int64_t     kChaosSlot          = 953301906;
constexpr const char* kForbiddenIndicator = "CommunityMod_SwapShipFT";
constexpr const char* kChaosIndicator     = "CommunityMod_SwapShipCT";
constexpr const char* kForbiddenBacking   = "CommunityMod_SwapShipFTBacking";
constexpr const char* kChaosBacking       = "CommunityMod_SwapShipCTBacking";
constexpr float       kBackingMargin       = 4.0f;
bool                  g_available         = false;

struct Vector2 {
  float x, y;
};

struct Color {
  float r, g, b, a;
};

struct Methods {
  const MethodInfo* context               = nullptr;
  const MethodInfo* ship                  = nullptr;
  const MethodInfo* id                    = nullptr;
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

bool IsSwapShipTile(ShipTileWidget* widget)
{
  static auto selection =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "ShipSelectionViewController");
  static auto game_object             = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto get_component_in_parent = game_object.GetMethodInfo("GetComponentInParent", 2);
  auto*       transform               = ComponentTransform(reinterpret_cast<Il2CppObject*>(widget));
  auto*       object                  = transform ? reinterpret_cast<Il2CppObject*>(transform->gameObject) : nullptr;
  if (!object || !selection.isValidHelper() || !get_component_in_parent)
    return false;
  void* type             = selection.GetType();
  bool  include_inactive = true;
  void* args[]{type, &include_inactive};
  return Invoke(get_component_in_parent, object, args) != nullptr;
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

GameObject* CreateBacking(const char* name, Transform* parent, Vector2 anchor, Vector2 pivot, Vector2 position)
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
  auto* sprite = ok ? SelectedBackingSprite(parent) : nullptr;
  if (ok && (!sprite || !Set(graphic, "set_sprite", sprite)) && LogUiDiagnostic())
    spdlog::warn("[ShipTechIndicators] selected-indicator sprite unavailable; using square backing");
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
    backing = CreateBacking(backing_name, parent, anchor, pivot, position);
  if (backing) {
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
  UpdateIndicator(parent, source, kForbiddenIndicator, kForbiddenBacking, {0, 0.42f}, {0, 0.5f}, {8, 8}, ft_art);
  UpdateIndicator(parent, source, kChaosIndicator, kChaosBacking, {1, 0.42f}, {1, 0.5f}, {-8, 8}, ct_art);
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
  if (!IsSwapShipTile(widget) || !Config::Get().show_ship_tech_indicators) {
    UpdateIndicators(tile_parent, image_source);
    return;
  }

  auto*      context = Invoke(methods.context, widget);
  auto*      ship    = Invoke(methods.ship, context);
  const auto active  = ReadActiveSlots(Value<int64_t>(methods.id, ship, 0));
  UpdateIndicators(tile_parent, image_source, ArtId(active.ft), ArtId(active.ct));
}
} // namespace

namespace ship_tech_indicators
{
bool Available()
{ return g_available; }
} // namespace ship_tech_indicators

void InstallShipTechIndicatorHooks()
{
  auto widget = ShipTileWidget::get_class_helper();
  auto fleet  = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
  auto ship   = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Ship");
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
  methods.ship    = method_contract::Resolve(fleet.get_cls(), "get_Ship", false, "Digit.PrimeServer.Models.Ship", {});
  methods.id      = method_contract::Resolve(ship.GetParent("BaseShip").get_cls(), "get_Id", false, "System.Int64", {});
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
  if (!target || !methods.context || !methods.context->methodPointer || !methods.ship || !methods.id || !manager_valid
      || !methods.tech_by_id || !active_slots_valid || !methods.tech_spec || !methods.id_refs || !methods.art_id
      || !methods.slot_spec || !slot_item_valid || !methods.set_identifier_params || !methods.int64_class
      || !image_valid || !token_valid || !target_valid) {
    spdlog::error("[ShipTechIndicators] required binding unavailable: hook={} context={} ship={} id={} manager={} "
                  "active={} tech={} spec={} refs={} art={} slotSpec={} slotItem={} selector={} image={} token={} "
                  "target={}",
                  bool(target), bool(methods.context), bool(methods.ship), bool(methods.id), manager_valid,
                  active_slots_valid, bool(methods.tech_by_id), bool(methods.tech_spec), bool(methods.id_refs),
                  bool(methods.art_id), bool(methods.slot_spec), slot_item_valid, bool(methods.set_identifier_params),
                  image_valid, token_valid, target_valid);
    return;
  }
  if (!SPUD_STATIC_DETOUR(method_contract::Pointer(target), SetWidgetData_Hook)) {
    spdlog::error("[ShipTechIndicators] failed to install ShipTileWidget.SetWidgetData detour");
    return;
  }
  g_available = true;
  spdlog::info("[ShipTechIndicators] installed FT/CT art indicators for Manage Ship > Swap Ship rows");
}
