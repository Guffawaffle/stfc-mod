#pragma once

// Native Galaxy contracts. Features own selection/composition; this file owns managed access and value layouts.
#include "galaxy_selection_pin_metadata.h"
#include "navigation.h"
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace galaxy_binding
{
struct Reference : il2cpp::Object {
  struct Class {
    static Il2CppClass* Raw()
    { return il2cpp::ObjectClass(); }
  };
};
struct OptimisedGalaxy : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "OptimisedGalaxy">;
};
struct Node {
  OptimisedGalaxy* galaxy = nullptr;
  int32_t          index = 0, padding = 0;
};
inline Il2CppClass* NodeClass()
{ return il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "GalaxyNode">::Raw(); }
inline bool NodeLayout()
{
  static const bool valid = [] {
    auto*    cls       = NodeClass();
    auto*    galaxy    = cls ? il2cpp_class_get_field_from_name(cls, "_galaxy") : nullptr;
    auto*    index     = cls ? il2cpp_class_get_field_from_name(cls, "_index") : nullptr;
    uint32_t alignment = 0;
    if (!cls || !OptimisedGalaxy::Class::Raw() || !il2cpp_class_is_valuetype(cls)
        || il2cpp_class_value_size(cls, &alignment) != sizeof(Node) || !galaxy || !index || !galaxy->type
        || galaxy->type->byref || !index->type || (galaxy->type->attrs & FIELD_ATTRIBUTE_STATIC)
        || (index->type->attrs & FIELD_ATTRIBUTE_STATIC)
        || !il2cpp::detail::ExactScalarMatches(index->type, IL2CPP_TYPE_I4)
        || il2cpp_class_from_type(galaxy->type) != OptimisedGalaxy::Class::Raw())
      return false;
    const auto origin = galaxy->offset;
    return (origin == 0 || origin == sizeof(Il2CppObject)) && index->offset == origin + offsetof(Node, index);
  }();
  return valid;
}
inline bool Finite(unity::Vector2 v)
{ return std::isfinite(v.x) && std::isfinite(v.y); }
inline bool Finite(unity::Vector3 v)
{ return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

struct World : MonoSingleton<World> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "GameWorldManager">;
  static il2cpp::Method MinorMethod()
  {
    static const auto method = [] {
      const auto candidate = Class::Method<bool(Node&)>("IsMinorNode");
      return candidate && NodeLayout() && candidate.Info()->parameters[0]->byref
                     && il2cpp_class_from_type(candidate.Info()->parameters[0]) == NodeClass()
                 ? candidate
                 : il2cpp::Method{};
    }();
    return method;
  }
  il2cpp::CallResult<bool> Minor(Node node)
  { return il2cpp::detail::TryInvoke<bool>(MinorMethod().Info(), this, std::ref(node)); }
  OptimisedGalaxy* Viewing()
  { return Get<"ViewingGalaxy", OptimisedGalaxy*>(); }
};
inline std::optional<int64_t> NodeId(Node node)
{
  if (!NodeLayout() || !node.galaxy || node.index < 0)
    return std::nullopt;
  static const auto* get =
      il2cpp::detail::ResolveProperty(NodeClass(), "ID", il2cpp::detail::SlotOf<int64_t>(), false, false);
  if (!get || !get->methodPointer || !get->invoker_method
      || !il2cpp::detail::ExactScalarMatches(get->return_type, IL2CPP_TYPE_I8))
    return std::nullopt;
  const auto boxed = il2cpp::detail::TryInvokeBoxed(get, &node, nullptr);
  if (!boxed || !boxed.value || boxed.value->klass != il2cpp::PrimitiveClass<int64_t>())
    return std::nullopt;
  return il2cpp::detail::FromBoxed<int64_t>(boxed.value);
}
struct Star : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationStarEntityWidget">;
  static FieldInfo* ContextField()
  {
    static auto* field = [] {
      auto* candidate = il2cpp::detail::ResolveField(Class::Raw(), "m_context", il2cpp::detail::SlotOf<Node>(), false);
      return candidate && candidate->type && !candidate->type->byref
                     && il2cpp_class_from_type(candidate->type) == NodeClass()
                 ? candidate
                 : nullptr;
    }();
    return field;
  }
  Node Context()
  { return il2cpp::detail::ReadField<Node>(ContextField(), this); }
  bool Bound()
  {
    const auto node = Context();
    return node.galaxy && node.index >= 0;
  }
  TextLocalizer* Name()
  { return Field<"_starName", TextLocalizer*>(); }
  TextLocalizer* HostileText()
  { return Field<"_hostilesLevelText", TextLocalizer*>(); }
  BaseListContainer* Resources()
  { return Field<"_resourceList", BaseListContainer*>(); }
  BaseListContainer* Hostiles()
  { return Field<"_hostilesList", BaseListContainer*>(); }
  unity::GameObject* Home()
  { return Field<"_homeIcon", unity::GameObject*>(); }
  unity::GameObject* ResourceContainer()
  { return Field<"_resourceListContainer", unity::GameObject*>(); }
  unity::GameObject* HostileContainer()
  { return Field<"_hostilesListContainer", unity::GameObject*>(); }
  unity::GameObject* HazardContainer()
  { return Field<"_hazardsListContainer", unity::GameObject*>(); }
  unity::Component* Hazard()
  { return Field<"_hazardsWidget", unity::Component*>(); }
  unity::GameObject* Anomaly()
  { return Field<"_galacticAnomalyIcon", unity::GameObject*>(); }
  void Update(ZoomLevels level)
  { Call<"UpdateStarData">(level); }
  void UpdateName(bool resources, bool hostiles, bool level, bool hazards)
  { Call<"UpdateStarName">(resources, hostiles, level, hazards); }
  static bool LayoutReady()
  {
    return NodeLayout() && ContextField() && Class::HasField<"_starName", TextLocalizer*>()
           && Class::HasField<"_hostilesLevelText", TextLocalizer*>()
           && Class::HasField<"_resourceList", BaseListContainer*>()
           && Class::HasField<"_hostilesList", BaseListContainer*>()
           && Class::HasField<"_resourceListContainer", unity::GameObject*>()
           && Class::HasField<"_hostilesListContainer", unity::GameObject*>()
           && Class::HasField<"_hazardsListContainer", unity::GameObject*>()
           && Class::HasField<"_hazardsWidget", unity::Component*>()
           && Class::HasField<"_galacticAnomalyIcon", unity::GameObject*>()
           && Class::HasField<"_homeIcon", unity::GameObject*>();
  }
};
inline Star* StarFor(il2cpp::Object* component)
{
  auto* type = Star::Class::TypeObject();
  return component && type ? component->Call<"GetComponent", Star*>(type) : nullptr;
}
struct ZoomHandler : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoomEventHandler">;
  NavigationLOD* Lod()
  { return Field<"_navigationLod", NavigationLOD*>(); }
};
struct Filter : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Core.Systems", "GalaxyDataFilterSystem">;
  ZoomLevels Current()
  { return Field<"_currentLevel", ZoomLevels>(); }
  void SetCurrent(ZoomLevels level)
  { SetField<"_currentLevel">(level); }
  void Dirty()
  { SetField<"_cullingDirty">(true); }
};
using Hud           = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.UI", "HudNavigationViewController">;
using ToggleHandler = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.UI", "GalaxyToggleHandler">;
struct Director {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationDirector">;
  static FieldInfo* ModeField()
  {
    // Preflight metadata only: resolving a static field through the convenience API runs its class
    // initializer. Hook installation must not initialize NavigationDirector before the native scene.
    static auto* field = [] {
      auto* cls       = Class::Raw();
      auto* candidate = cls ? il2cpp_class_get_field_from_name(cls, "GalaxyViewMode") : nullptr;
      return candidate && candidate->type && (candidate->type->attrs & FIELD_ATTRIBUTE_STATIC)
                     && candidate->offset != THREAD_STATIC_FIELD_OFFSET
                     && il2cpp::detail::ExactScalarMatches(candidate->type, IL2CPP_TYPE_I4)
                 ? candidate
                 : nullptr;
    }();
    return field;
  }
  static std::optional<int32_t> Mode()
  {
    if (!ModeField())
      return std::nullopt;
    auto mode = il2cpp::detail::ReadField<int32_t>(ModeField(), nullptr);
    return mode >= 0 && mode <= 3 ? std::optional(mode) : std::nullopt;
  }
  static void SetMode(int32_t value)
  { il2cpp::detail::WriteField(ModeField(), nullptr, value); }
};
inline unity::Transform* HostileRow(Star* star)
{
  auto* list = star->Hostiles();
  return list ? list->Field<"m_container", unity::Transform*>() : nullptr;
}
struct NavigationCamera : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationCamera">;
  NavigationZoom* Zoom()
  { return Get<"NavZoomCamera", NavigationZoom*>(); }
  unity::Camera* Scene()
  { return Get<"SceneCamera", unity::Camera*>(); }
};
struct Tap : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "", "TKTapRecognizer">;
};
// Retained client 273 catalog, including entries not used by this feature.
enum class Interactable : int32_t {
  NonInteractable                = 0,
  Planet                         = 1,
  Star                           = 2,
  Starbase                       = 3,
  Battle                         = 4,
  Fleet                          = 5,
  MissionLocation                = 6,
  MissionDelivery                = 7,
  LockedPath                     = 8,
  OneWayPath                     = 9,
  Armada                         = 10,
  CelestialObject                = 11,
  AllianceStarbase               = 12,
  WaveDefenseCentralEntity       = 13,
  JumpZoneEntity                 = 14,
  RegionalTransportCentralEntity = 15,
  SurgeCentralEntity             = 16,
  Outpost                        = 17
};
struct Poi : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "POI">;
  int64_t Id()
  { return Get<"ID", int64_t>(); }
  static Poi* Existing(int64_t id)
  { return Class::Call<"GetOfType", Poi*>(id, Interactable::Star); }
  bool Initialize(int64_t id)
  {
    if (!TryCall<"set_ID", void(int64_t)>(id)
        || !TryCall<"set_InteractableType", void(Interactable)>(Interactable::Star))
      return false;
    const auto type   = TryCall<"get_InteractableType", Interactable>();
    const auto actual = TryCall<"get_ID", int64_t>();
    return type && type.value == Interactable::Star && actual && actual.value == id;
  }
};
inline NavigationCamera* Camera(NavigationManager* manager)
{ return manager->Get<"NavigationCamera", NavigationCamera*>(); }
inline il2cpp::Method ProcessTapMethod()
{
  const auto candidate =
      NavigationManager::Class::Method<void(unity::Vector2, Reference*, Reference*, Poi*)>("ProcessInputTap");
  return candidate
                 && il2cpp::detail::ValueClassMatches(
                     candidate.Info()->parameters[0],
                     il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Vector2">::Raw(), sizeof(unity::Vector2))
             ? candidate
             : il2cpp::Method{};
}
inline il2cpp::CallResult<unity::Vector2> Touch(Tap* tap)
{
  const auto method = Tap::Class::Method<unity::Vector2()>("touchLocation");
  if (!method
      || !il2cpp::detail::ValueClassMatches(method.Info()->return_type,
                                            il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Vector2">::Raw(),
                                            sizeof(unity::Vector2)))
    return {};
  const auto box = il2cpp::detail::TryInvokeBoxed(method.Info(), tap, nullptr);
  if (!box || !box.value
      || !il2cpp::detail::ValueClassMatches(&box.value->klass->byval_arg,
                                            il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Vector2">::Raw(),
                                            sizeof(unity::Vector2)))
    return {};
  const auto value = il2cpp::detail::FromBoxed<unity::Vector2>(box.value);
  return Finite(value) ? il2cpp::CallResult<unity::Vector2>{true, value} : il2cpp::CallResult<unity::Vector2>{};
}
inline il2cpp::CallResult<unity::Vector3> TransformPoint(NavigationTransitionAnimator* animator, unity::Vector3 point,
                                                         bool to_world)
{
  static const auto* world = il2cpp::detail::MethodCall<unity::Vector3(unity::Vector3, int32_t)>::Resolve(
      NavigationTransitionAnimator::Class::Raw(), "DepthToWorldPosition", false);
  const auto* method =
      to_world
          ? world
          : NavigationTransitionAnimator::Class::Method<unity::Vector3(unity::Vector3, int32_t)>("WorldToDepth").Info();
  if (!method || !il2cpp::detail::ValueClassMatches(method->parameters[0], unity::Vector3Class(), sizeof(point))
      || !il2cpp::detail::ExactScalarMatches(method->parameters[1], IL2CPP_TYPE_I4))
    return {};
  int32_t    depth = 1;
  void*      args[]{&point, &depth};
  const auto observed = unity::TryReadVector3(method, animator, args);
  return observed && Finite(observed.value) ? observed : il2cpp::CallResult<unity::Vector3>{};
}
inline il2cpp::CallResult<unity::Vector3> MousePoint(unity::Camera* camera, unity::Vector3 screen)
{
  const auto method =
      MathUtils::Class::StaticMethod<unity::Vector3(unity::Camera*, unity::Vector3)>("GetMouseWorldPos");
  if (!method || !il2cpp::detail::ValueClassMatches(method.Info()->parameters[1], unity::Vector3Class(), sizeof(screen))
      || !il2cpp::detail::ValueClassMatches(method.Info()->return_type, unity::Vector3Class(), sizeof(screen)))
    return {};
  void*      args[]{camera, &screen};
  const auto box = il2cpp::detail::TryInvokeBoxed(method.Info(), nullptr, args);
  if (!box || !box.value || box.value->klass != unity::Vector3Class())
    return {};
  const auto value = il2cpp::detail::FromBoxed<unity::Vector3>(box.value);
  return Finite(value) ? il2cpp::CallResult<unity::Vector3>{true, value} : il2cpp::CallResult<unity::Vector3>{};
}

// Read protobuf backing arrays without per-node boxing. Retain both container and actual array during the scan.
template <class T> struct Values {
  il2cpp::GcHandle   owner, array;
  std::span<const T> data;
};
template <class T> std::optional<Values<T>> ReadValues(Reference* repeated, int32_t expected)
{
  if (!repeated || expected < 0)
    return std::nullopt;
  auto* array_field = il2cpp_class_get_field_from_name(repeated->klass, "array");
  auto* count_field = il2cpp_class_get_field_from_name(repeated->klass, "count");
  if (!array_field || !count_field || !array_field->type || array_field->type->byref
      || array_field->type->type != IL2CPP_TYPE_SZARRAY || (array_field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || !il2cpp::detail::ExactScalarMatches(count_field->type, IL2CPP_TYPE_I4)
      || (count_field->type->attrs & FIELD_ATTRIBUTE_STATIC))
    return std::nullopt;
  Values<T> result;
  result.owner = il2cpp::GcHandle::Strong(repeated);
  if (!result.owner)
    return std::nullopt;
  const auto count = il2cpp::detail::ReadField<int32_t>(count_field, repeated);
  auto*      array = il2cpp::detail::ReadField<Il2CppArray*>(array_field, repeated);
  result.array     = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(array));
  if (!result.array || count != expected || !array || array->max_length < static_cast<size_t>(count)
      || !il2cpp_class_get_type(array->klass) || il2cpp_class_get_type(array->klass)->type != IL2CPP_TYPE_SZARRAY
      || il2cpp_class_get_element_class(array->klass) != il2cpp::PrimitiveClass<T>())
    return std::nullopt;
  result.data = {reinterpret_cast<const T*>(reinterpret_cast<const char*>(array) + kIl2CppSizeOfArray),
                 static_cast<size_t>(count)};
  return result;
}
struct PinReader {
  std::vector<il2cpp::GcHandle> roots;
  bool                          Retain(Il2CppObject* object)
  {
    if (!object)
      return true;
    auto root = il2cpp::GcHandle::Strong(object);
    if (!root)
      return false;
    roots.push_back(std::move(root));
    return true;
  }
  bool Field(Il2CppObject* object, const char* name, Il2CppObject*& out)
  {
    auto* field = object
                      ? il2cpp::detail::ResolveField(object->klass, name, il2cpp::detail::SlotOf<Reference*>(), false)
                      : nullptr;
    out         = field ? il2cpp::detail::ReadField<Reference*>(field, object) : nullptr;
    return field && Retain(out);
  }
  bool Property(Il2CppObject* object, const char* name, Il2CppObject*& out)
  {
    const auto* method =
        object ? il2cpp::detail::MethodCall<Reference*()>::Resolve(object->klass, name, false) : nullptr;
    const auto read = il2cpp::detail::TryInvoke<Reference*>(method, object);
    out             = read ? read.value : nullptr;
    return read && Retain(out);
  }
  bool Dictionary(Il2CppObject* object, std::vector<Il2CppObject*>& values)
  {
    if (!object)
      return true;
    const auto* count_getter = il2cpp::detail::MethodCall<int32_t()>::Resolve(object->klass, "get_Count", false);
    const auto* contains     = il2cpp::detail::MethodCall<bool(int32_t)>::Resolve(object->klass, "ContainsKey", false);
    const auto* item = il2cpp::detail::MethodCall<Reference*(int32_t)>::Resolve(object->klass, "get_Item", false);
    if (!galaxy_selection::PinCountGetter(count_getter) || !contains || !item
        || !il2cpp::detail::ExactScalarMatches(contains->return_type, IL2CPP_TYPE_BOOLEAN)
        || !il2cpp::detail::ExactScalarMatches(contains->parameters[0], IL2CPP_TYPE_I4)
        || !il2cpp::detail::ExactScalarMatches(item->parameters[0], IL2CPP_TYPE_I4))
      return false;
    const auto count = il2cpp::detail::TryInvoke<int32_t>(count_getter, object);
    if (!count || count.value < 0)
      return false;
    int32_t observed = 0;
    // This is the retained InteractableType key catalog, not a limit on the game's collection size.
    // An added/unknown key makes coverage incomplete and therefore cannot authorize retirement.
    for (int32_t key = static_cast<int32_t>(Interactable::NonInteractable);
         key <= static_cast<int32_t>(Interactable::Outpost); ++key) {
      const auto exists = il2cpp::detail::TryInvoke<bool>(contains, object, key);
      if (!exists)
        return false;
      if (!exists.value)
        continue;
      const auto value = il2cpp::detail::TryInvoke<Reference*>(item, object, key);
      if (!value || !Retain(value.value))
        return false;
      ++observed;
      if (value.value)
        values.push_back(value.value);
    }
    const auto after = il2cpp::detail::TryInvoke<int32_t>(count_getter, object);
    return after && after.value == count.value && observed == count.value;
  }
  bool List(Il2CppObject* object, std::vector<Il2CppObject*>& pins)
  {
    if (!object)
      return true;
    auto*       list         = reinterpret_cast<il2cpp::Object*>(object);
    const auto* count_getter = il2cpp::detail::MethodCall<int32_t()>::Resolve(list->klass, "get_Count", false);
    const auto* item_getter  = il2cpp::detail::MethodCall<Reference*(int32_t)>::Resolve(list->klass, "get_Item", false);
    if (!galaxy_selection::PinCountGetter(count_getter) || !galaxy_selection::PinItemGetter(item_getter))
      return false;
    const auto count = il2cpp::detail::TryInvoke<int32_t>(count_getter, list);
    if (!count || count.value < 0)
      return false;
    for (int32_t i = 0; i < count.value; ++i) {
      const auto item = il2cpp::detail::TryInvoke<Reference*>(item_getter, list, i);
      if (!item)
        return false;
      if (!Retain(item.value))
        return false;
      if (item.value)
        pins.push_back(item.value);
    }
    return true;
  }
};
} // namespace galaxy_binding
