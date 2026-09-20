#pragma once

// Build a bounded set of bare POIs at click time;
// leave galaxy entity filtering, labels, meshes and background updates native.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include "patches/galaxy_selection_pins.h"

namespace galaxy_selection
{
struct Vec3 { float x, y, z; };
struct Node { Il2CppObject* galaxy; int index; int padding = 0; };
struct Proxy {
  int64_t id; Il2CppGCHandle object, poi;
  uint64_t last_click = 0;
  std::chrono::steady_clock::time_point last_used{};
};
inline uint64_t generation = 0;
inline std::vector<Proxy> proxies;
inline Il2CppGCHandle galaxy_handle = nullptr;
inline bool active = false;

inline Il2CppObject* Invoke(void* object, const MethodInfo* method, void** args = nullptr)
{
  if (!method) return nullptr;
  Il2CppException* error = nullptr;
  auto* result = il2cpp_runtime_invoke(method, object, args, &error);
  return error ? nullptr : result;
}
inline Il2CppObject* Call(Il2CppObject* object, const char* name, void** args = nullptr, int count = 0)
{
  return object ? Invoke(object, il2cpp_class_get_method_from_name(object->klass, name, count), args) : nullptr;
}
inline Il2CppObject* Field(Il2CppObject* object, const char* name)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  Il2CppObject* result = nullptr;
  if (field && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      && (field->type->type == IL2CPP_TYPE_CLASS || field->type->type == IL2CPP_TYPE_OBJECT
          || (field->type->type == IL2CPP_TYPE_GENERICINST
              && !il2cpp_class_is_valuetype(il2cpp_class_from_type(field->type)))))
    il2cpp_field_get_value(object, field, &result);
  return result;
}
inline bool Alive(Il2CppObject* object)
{
  static auto cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto* method = method_contract::Resolve(cls.get_cls(), "op_Implicit", true, "System.Boolean",
                                                {"UnityEngine.Object"});
  void* args[]{object};
  auto* result = object ? Invoke(nullptr, method, args) : nullptr;
  return result && *static_cast<bool*>(il2cpp_object_unbox(result));
}
inline void SetActive(Il2CppObject* object, bool value)
{
  if (Alive(object)) { void* args[]{&value}; Call(object, "SetActive", args, 1); }
}
inline void Deactivate()
{
  if (!active) return;
  for (const auto& proxy : proxies) SetActive(il2cpp_gchandle_get_target(proxy.object), false);
  active = false;
}
inline void Destroy(Il2CppObject* object)
{
  static auto cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto* method = method_contract::Resolve(cls.get_cls(), "Destroy", true, "System.Void", {"UnityEngine.Object"});
  if (Alive(object)) { void* args[]{object}; Invoke(nullptr, method, args); }
}
inline void Reset()
{
  Deactivate();
  static auto cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto* destroy = method_contract::Resolve(cls.get_cls(), "Destroy", true, "System.Void", {"UnityEngine.Object"});
  for (const auto& proxy : proxies) {
    auto* object = il2cpp_gchandle_get_target(proxy.object);
    if (Alive(object)) { void* args[]{object}; Invoke(nullptr, destroy, args); }
    il2cpp_gchandle_free(proxy.object);
    il2cpp_gchandle_free(proxy.poi);
  }
  proxies.clear();
  if (galaxy_handle) il2cpp_gchandle_free(galaxy_handle);
  galaxy_handle = nullptr;
}

// Read known native owners before retiring any immutable POI. Missing owner
// bindings fail closed. This is click-time work only, with no scene-wide search.
struct PinReader {
  bool Reference(const Il2CppType* type)
  {
    if (!type || type->byref) return false;
    if (type->type == IL2CPP_TYPE_CLASS || type->type == IL2CPP_TYPE_OBJECT
        || type->type == IL2CPP_TYPE_SZARRAY || type->type == IL2CPP_TYPE_ARRAY) return true;
    auto* cls = type->type == IL2CPP_TYPE_GENERICINST ? il2cpp_class_from_type(type) : nullptr;
    return cls && !il2cpp_class_is_valuetype(cls);
  }
  bool Field(Il2CppObject* object, const char* name, Il2CppObject*& result)
  {
    result = nullptr;
    auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
    if (!field || (field->type->attrs & FIELD_ATTRIBUTE_STATIC)) return false;
    if (!Reference(field->type)) return false;
    il2cpp_field_get_value(object, field, &result);
    return true;
  }
  bool Read(Il2CppObject* object, const MethodInfo* method, void** args, Il2CppObject*& result)
  {
    result = nullptr;
    if (!object || !method || (method->flags & METHOD_ATTRIBUTE_STATIC)) return false;
    Il2CppException* error = nullptr;
    result = il2cpp_runtime_invoke(method, object, args, &error);
    return error == nullptr;
  }
  bool Property(Il2CppObject* object, const char* name, Il2CppObject*& result)
  {
    const auto* method = object ? il2cpp_class_get_method_from_name(object->klass, name, 0) : nullptr;
    return method && Reference(method->return_type) && Read(object, method, nullptr, result);
  }
  bool List(Il2CppObject* items, std::vector<Il2CppObject*>& pins)
  {
    if (!items) return true;
    const auto* count_method = method_contract::Resolve(items->klass, "get_Count", false, "System.Int32", {});
    const auto* item = il2cpp_class_get_method_from_name(items->klass, "get_Item", 1);
    Il2CppObject* boxed = nullptr;
    if (!item || !Reference(item->return_type)
        || !method_contract::Type(item->parameters[0].parameter_type, "System.Int32")
        || !Read(items, count_method, nullptr, boxed) || !boxed) return false;
    const int count = *static_cast<int*>(il2cpp_object_unbox(boxed));
    if (count < 0 || count > 4096) return false;
    for (int i = 0; i < count; ++i) {
      void* args[]{&i};
      Il2CppObject* poi = nullptr;
      if (!Read(items, item, args, poi)) return false;
      if (poi) pins.push_back(poi);
    }
    return true;
  }
};
inline bool CollectPins(Il2CppObject* manager, std::vector<Il2CppObject*>& pins)
{
  PinReader reader;
  return CollectOwnerPins(manager, pins, reader);
}
inline bool MakeRoom(const std::vector<Il2CppObject*>& pins, bool bindings_ok)
{
  if (proxies.size() < 64) return true;
  if (!bindings_ok) return false;
  const auto now = std::chrono::steady_clock::now();
  auto victim = proxies.end();
  for (auto it = proxies.begin(); it != proxies.end(); ++it) {
    // Protect current/recent clicks as well as UI owners, including transitions.
    if (generation - it->last_click <= 2 || now - it->last_used < std::chrono::seconds(2)) continue;
    auto* poi = il2cpp_gchandle_get_target(it->poi);
    if (std::find(pins.begin(), pins.end(), poi) != pins.end()) continue;
    if (victim == proxies.end() || it->last_click < victim->last_click) victim = it;
  }
  if (victim == proxies.end()) return false;
  // Destroy, never change ID: a stale managed reference cannot become another
  // system. Unity destruction is deferred until after the current update.
  Destroy(il2cpp_gchandle_get_target(victim->object));
  il2cpp_gchandle_free(victim->object);
  il2cpp_gchandle_free(victim->poi);
  proxies.erase(victim);
  return true;
}

// Read existing protobuf arrays without allocating a boxed value for every node.
// Validate both the count field and actual array element type before reading.
template<class T> inline const T* Values(Il2CppObject* repeated, const char* type, int expected)
{
  if (!repeated) return nullptr;
  auto* array_field = il2cpp_class_get_field_from_name(repeated->klass, "array");
  auto* count_field = il2cpp_class_get_field_from_name(repeated->klass, "count");
  if (!array_field || !count_field || array_field->type->type != IL2CPP_TYPE_SZARRAY
      || !method_contract::Type(count_field->type, "System.Int32")) return nullptr;
  Il2CppArray* array = nullptr;
  int count = 0;
  il2cpp_field_get_value(repeated, array_field, &array);
  il2cpp_field_get_value(repeated, count_field, &count);
  if (!array || count != expected || array->max_length < static_cast<unsigned>(count)
      || !method_contract::Type(il2cpp_class_get_type(il2cpp_class_get_element_class(array->klass)), type)) return nullptr;
  return reinterpret_cast<const T*>(reinterpret_cast<const char*>(array) + kIl2CppSizeOfArray);
}
inline bool Vector(void* object, const MethodInfo* method, void** args, Vec3& out)
{
  auto* result = Invoke(object, method, args);
  if (!result || !method_contract::Type(il2cpp_class_get_type(result->klass), "UnityEngine.Vector3")) return false;
  out = *static_cast<Vec3*>(il2cpp_object_unbox(result));
  return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}
inline bool Visible(Il2CppObject* world, const MethodInfo* method, Node& node)
{
  void* args[]{&node};
  auto* result = Invoke(world, method, args);
  return result && *static_cast<bool*>(il2cpp_object_unbox(result));
}
inline void Prepare(Il2CppObject* manager, float x, float y)
{
  Deactivate(); // Previous click candidates must never shadow a native POI.
  std::erase_if(proxies, [](const Proxy& p) {
    if (Alive(il2cpp_gchandle_get_target(p.object)) && Alive(il2cpp_gchandle_get_target(p.poi))) return false;
    Destroy(il2cpp_gchandle_get_target(p.object));
    il2cpp_gchandle_free(p.object); il2cpp_gchandle_free(p.poi);
    return true;
  });
  auto world_cls = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "GameWorldManager");
  auto math_cls = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils");
  auto poi_cls = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "POI");
  auto go_cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  if (!world_cls.isValidHelper() || !math_cls.isValidHelper() || !poi_cls.isValidHelper() || !go_cls.isValidHelper()) {
    spdlog::warn("[GalaxySelection] Class bindings unavailable"); return;
  }
  auto* world = Invoke(nullptr, il2cpp_class_get_method_from_name(il2cpp_class_get_parent(world_cls.get_cls()), "get_Instance", 0));
  auto* galaxy = Call(world, "get_ViewingGalaxy");
  auto* camera = Call(Call(manager, "get_NavigationCamera"), "get_SceneCamera");
  auto* animator = Field(manager, "_transitionAnimator");
  auto* count_value = Call(galaxy, "get_NodesCount");
  if (!galaxy || !camera || !animator || !count_value) {
    spdlog::warn("[GalaxySelection] Scene bindings galaxy={} camera={} animator={} count={}",
                 galaxy != nullptr, camera != nullptr, animator != nullptr, count_value != nullptr); return;
  }
  if (!galaxy_handle || il2cpp_gchandle_get_target(galaxy_handle) != galaxy) {
    Reset();
    galaxy_handle = il2cpp_gchandle_new_weakref(galaxy, false);
  }
  const int count = *static_cast<int*>(il2cpp_object_unbox(count_value));
  if (count <= 0 || count > 100000) return;
  const auto* ids = Values<int64_t>(Call(galaxy, "get_NodeIds"), "System.Int64", count);
  const auto* xs = Values<int>(Call(galaxy, "get_XCoords"), "System.Int32", count);
  const auto* ys = Values<int>(Call(galaxy, "get_YCoords"), "System.Int32", count);
  auto* mouse = method_contract::Resolve(math_cls.get_cls(), "GetMouseWorldPos", true, "UnityEngine.Vector3",
                                        {"UnityEngine.Camera", "UnityEngine.Vector3"});
  auto* to_depth = method_contract::Resolve(animator->klass, "WorldToDepth", false, "UnityEngine.Vector3",
                                           {"UnityEngine.Vector3", "Digit.PrimeServer.Models.NodeDepth"});
  auto* to_world = method_contract::Resolve(animator->klass, "DepthToWorldPosition", false, "UnityEngine.Vector3",
                                           {"UnityEngine.Vector3", "Digit.PrimeServer.Models.NodeDepth"});
  auto* native_get = method_contract::Resolve(poi_cls.get_cls(), "GetOfType", true, "Digit.Prime.Navigation.POI",
                                             {"System.Int64", "Digit.Prime.Navigation.InteractableType"});
  auto* minor = world_cls.GetMethodInfo("IsMinorNode", 1);
  if (!ids || !xs || !ys || !mouse || !to_depth || !to_world || !native_get || !minor) {
    spdlog::warn("[GalaxySelection] Data/coordinate bindings unavailable");
    return;
  }
  int depth = 1, star = 2;
  float min_x = std::numeric_limits<float>::max(), min_z = min_x, max_x = -min_x, max_z = -min_x;
  // Broad click-local prefilter only. Native POI interaction spheres still make
  // the actual hit decision; no forced POI and no enlarged native hit radius.
  for (float dx : {-64.f, 64.f}) for (float dy : {-64.f, 64.f}) {
    Vec3 screen{x + dx, y + dy, 0}, world_pos{}, local{};
    void* mouse_args[]{camera, &screen};
    void* depth_args[]{&world_pos, &depth};
    if (!Vector(nullptr, mouse, mouse_args, world_pos) || !Vector(animator, to_depth, depth_args, local)) return;
    min_x = std::min(min_x, local.x); max_x = std::max(max_x, local.x);
    min_z = std::min(min_z, local.z); max_z = std::max(max_z, local.z);
  }
  ++generation;
  std::vector<Il2CppObject*> pins;
  const bool pins_ok = CollectPins(manager, pins);
  unsigned candidates = 0;
  for (int i = 0; i < count; ++i) {
    if (xs[i] < min_x || xs[i] > max_x || ys[i] < min_z || ys[i] > max_z) continue;
    Node node{galaxy, i};
    // IsNodeViewable means entering the system view: its first native check
    // rejects unvisited nodes. It is not a galaxy selection eligibility rule.
    // Keep the same minor-node classification as native far-tier culling.
    if (!Visible(world, minor, node)) continue;
    int64_t id = ids[i];
    void* get_args[]{&id, &star};
    if (Alive(Invoke(nullptr, native_get, get_args))) continue;
    if (++candidates > 16) { Deactivate(); break; } // Bound candidates; never instantiate a map of entities.
    auto it = std::find_if(proxies.begin(), proxies.end(), [id](const Proxy& p) { return p.id == id; });
    if (it == proxies.end()) {
      if (!MakeRoom(pins, pins_ok)) {
        spdlog::warn("[GalaxySelection] retirement blocked pool={} pins={} bindings={}", proxies.size(), pins.size(), pins_ok);
        Deactivate(); break;
      }
      auto* object = il2cpp_object_new(go_cls.get_cls());
      auto* ctor = method_contract::Resolve(go_cls.get_cls(), ".ctor", false, "System.Void", {"System.String"});
      void* ctor_args[]{il2cpp_string_new("GalaxySelectionTarget")};
      Invoke(object, ctor, ctor_args);
      if (!Alive(object)) break;
      SetActive(object, false);
      auto* add = method_contract::Resolve(go_cls.get_cls(), "AddComponent", false, "UnityEngine.Component", {"System.Type"});
      void* add_args[]{poi_cls.GetType()};
      auto* poi = Invoke(object, add, add_args);
      if (!Alive(poi)) { Destroy(object); break; }
      void* id_args[]{&id}; Call(poi, "set_ID", id_args, 1);
      void* type_args[]{&star}; Call(poi, "set_InteractableType", type_args, 1);
      auto object_handle = il2cpp_gchandle_new_weakref(object, false);
      auto poi_handle = il2cpp_gchandle_new_weakref(poi, false);
      if (!object_handle || !poi_handle) {
        if (object_handle) il2cpp_gchandle_free(object_handle);
        if (poi_handle) il2cpp_gchandle_free(poi_handle);
        Destroy(object); break;
      }
      proxies.push_back({id, object_handle, poi_handle});
      it = std::prev(proxies.end());
    }
    it->last_click = generation;
    it->last_used = std::chrono::steady_clock::now();
    auto* object = il2cpp_gchandle_get_target(it->object);
    if (!Alive(object)) continue;
    Vec3 local{static_cast<float>(xs[i]), 0, static_cast<float>(ys[i])}, world_pos{};
    void* world_args[]{&local, &depth};
    if (!Vector(animator, to_world, world_args, world_pos)) continue;
    void* position_args[]{&world_pos}; Call(Call(object, "get_transform"), "set_position", position_args, 1);
    SetActive(object, true);
    active = true;

  }

}
} // namespace galaxy_selection
