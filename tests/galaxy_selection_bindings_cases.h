#pragma once

// The production Galaxy selection adapter, in the existing controlled IL2CPP
// runtime. Projection/Unity hit testing remain native qualification; these cases
// exercise candidate enumeration, dispatch correlation, ownership and cleanup.
#include "../src/mod/features/galaxy_selection.cc"

namespace
{
struct GalaxySelectionObject {
  Il2CppObject                 header;
  bool                         alive = true, active = true;
  Il2CppObject*                poi       = nullptr;
  Il2CppObject*                transform = nullptr;
  int64_t                      id        = 0;
  galaxy_binding::Interactable type      = galaxy_binding::Interactable::NonInteractable;
  unity::Vector3               position{};
};
struct GalaxySelectionZoom {
  Il2CppObject header;
  NodeDepth    depth = NodeDepth::Galaxy;
  ZoomLevels   level = ZoomLevels::Far;
};
struct GalaxySelectionManager {
  Il2CppObject  header;
  Il2CppObject* animator;
};
template <class T> struct GalaxySelectionVector {
  Il2CppObject        header{};
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 5;
  T                   values[5]{};
};
struct GalaxySelectionRepeated {
  Il2CppObject header{};
  Il2CppArray* array = nullptr;
  int32_t      count = 2;
};
struct GalaxySelectionData {
  GalaxySelectionVector<int64_t> ids;
  GalaxySelectionVector<int32_t> xs, ys;
  GalaxySelectionRepeated        node_ids, x_coords, y_coords;
};
Il2CppObject  selection_world{}, selection_galaxy{}, selection_navigation{}, selection_scene{}, selection_other_scene{};
Il2CppObject* selection_current_world = &selection_world;
std::unordered_map<Il2CppObject*, Il2CppObject*> selection_galaxies, selection_navigations, selection_scenes,
    selection_zooms;
std::unordered_map<Il2CppObject*, GalaxySelectionData*> selection_data;
std::unordered_map<Il2CppObject*, float>                selection_projection_offsets;
FakeClass*                                              selection_poi_class       = nullptr;
FakeClass*                                              selection_transform_class = nullptr;
const MethodInfo*                                       selection_touch_method    = nullptr;
const MethodInfo*                                       selection_mouse_method    = nullptr;
const MethodInfo*                                       selection_depth_method    = nullptr;
struct GalaxySelectionTapBox {
  Il2CppObject   header{};
  unity::Vector2 value{12, 24};
} selection_tap_box;
GalaxySelectionZoom   selection_zoom{};
GalaxySelectionObject selection_existing{};
bool                  selection_existing_throws = false, selection_minor_throws = false;
bool                  selection_alive_throws = false, selection_active_throws = false;
bool selection_disable_before = false, selection_disable_after = false, selection_destroy_throws = false;
bool selection_scene_replaced = false;
std::function<void()> selection_disable_reentry;
int     selection_minor_reads = 0, selection_existing_reads = 0, selection_single_taps = 0, selection_destroys = 0;
int32_t selection_last_minor = -1;
std::deque<GalaxySelectionObject> selection_objects;
Il2CppObject*                     selection_existing_result = nullptr;
Il2CppObject*                     SelectionWorld(const MethodInfo*)
{ return selection_current_world; }
Il2CppObject* SelectionGalaxy(Il2CppObject* world, const MethodInfo*)
{ return selection_galaxies.at(world); }
Il2CppObject* SelectionNavigation(Il2CppObject* manager, const MethodInfo*)
{ return selection_navigations.at(manager); }
Il2CppObject* SelectionScene(Il2CppObject* camera, const MethodInfo*)
{ return selection_scene_replaced ? &selection_other_scene : selection_scenes.at(camera); }
Il2CppObject* SelectionZoom(Il2CppObject* camera, const MethodInfo*)
{ return selection_zooms.at(camera); }
bool SelectionMinor(Il2CppObject*, galaxy_binding::Node* node, const MethodInfo*)
{
  ++selection_minor_reads;
  selection_last_minor = node->index;
  if (selection_minor_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return true;
}
Il2CppObject* SelectionExisting(int64_t, galaxy_binding::Interactable type, const MethodInfo*)
{
  Require(type == galaxy_binding::Interactable::Star, "only native Star POIs are queried");
  ++selection_existing_reads;
  if (selection_existing_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return selection_existing_result;
}
void SelectionSingle(NavigationManager*, unity::Vector2 point, galaxy_binding::Poi* forced, const MethodInfo*)
{
  Require(point.x == 12 && point.y == 24 && !forced, "fallback keeps original click and never forces a POI");
  ++selection_single_taps;
}
void SelectionRecognisedStub(NavigationManager*, galaxy_binding::Tap*, const MethodInfo*) {}
void SelectionProcessStub(NavigationManager*, unity::Vector2, galaxy_binding::Reference*, galaxy_binding::Reference*,
                          galaxy_binding::Poi*, const MethodInfo*)
{
}
bool SelectionAlive(Il2CppObject* object, const MethodInfo*)
{
  if (selection_alive_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<GalaxySelectionObject*>(object)->alive;
}
bool SelectionActive(GalaxySelectionObject* object, const MethodInfo*)
{
  if (selection_active_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return object->active;
}
void SelectionSetActive(GalaxySelectionObject* object, bool active, const MethodInfo*)
{
  if (selection_disable_before)
    throw Il2CppExceptionWrapper(&thrown);
  if (selection_disable_reentry)
    selection_disable_reentry();
  object->active = active;
  if (selection_disable_after)
    throw Il2CppExceptionWrapper(&thrown);
}
void SelectionDestroy(Il2CppObject* object, const MethodInfo*)
{
  if (selection_destroy_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++selection_destroys;
  reinterpret_cast<GalaxySelectionObject*>(object)->alive = false;
}
int32_t SelectionNodesCount(Il2CppObject*, const MethodInfo*)
{ return 1; }
Il2CppObject* SelectionNodeIds(Il2CppObject* galaxy, const MethodInfo*)
{ return &selection_data.at(galaxy)->node_ids.header; }
Il2CppObject* SelectionXCoords(Il2CppObject* galaxy, const MethodInfo*)
{ return &selection_data.at(galaxy)->x_coords.header; }
Il2CppObject* SelectionYCoords(Il2CppObject* galaxy, const MethodInfo*)
{ return &selection_data.at(galaxy)->y_coords.header; }
Il2CppObject* SelectionAllocate(const Il2CppClass* klass, bool& handled)
{
  handled = klass == &game_object_class.klass;
  if (!handled)
    return nullptr;
  auto& object        = selection_objects.emplace_back();
  object.header.klass = &game_object_class.klass;
  return &object.header;
}
void SelectionConstruct(Il2CppObject*, Il2CppString* name, const MethodInfo*)
{ Require(il2cpp::Equals(name, "GalaxySelectionTarget"), "full preparation creates its own immutable target"); }
Il2CppObject* SelectionAddComponent(GalaxySelectionObject* object, Il2CppReflectionType*, const MethodInfo*)
{
  Require(!object->active && !object->poi, "POI construction starts under an inactive owned GameObject");
  auto& poi        = selection_objects.emplace_back();
  poi.header.klass = &selection_poi_class->klass;
  object->poi      = &poi.header;
  return object->poi;
}
void SelectionSetId(GalaxySelectionObject* poi, int64_t id, const MethodInfo*)
{ poi->id = id; }
int64_t SelectionId(GalaxySelectionObject* poi, const MethodInfo*)
{ return poi->id; }
void SelectionSetType(GalaxySelectionObject* poi, galaxy_binding::Interactable type, const MethodInfo*)
{ poi->type = type; }
galaxy_binding::Interactable SelectionType(GalaxySelectionObject* poi, const MethodInfo*)
{ return poi->type; }
Il2CppObject* SelectionTransform(GalaxySelectionObject* object, const MethodInfo*)
{
  if (!object->transform) {
    auto& transform        = selection_objects.emplace_back();
    transform.header.klass = &selection_transform_class->klass;
    object->transform      = &transform.header;
  }
  return object->transform;
}
void SelectionPosition(GalaxySelectionObject* transform, unity::Vector3 value, const MethodInfo*)
{ transform->position = value; }
Il2CppObject* SelectionInvoke(const MethodInfo* method, void* receiver, void** args, Il2CppException**, bool& handled)
{
  handled = method == selection_touch_method || method == selection_mouse_method || method == warp_world_method
            || method == selection_depth_method;
  if (!handled)
    return nullptr;
  if (method == selection_touch_method)
    return &selection_tap_box.header;
  const auto point = *static_cast<unity::Vector3*>(args[method == selection_mouse_method ? 1 : 0]);
  if (method == selection_mouse_method) {
    const auto offset     = selection_projection_offsets.at(static_cast<Il2CppObject*>(args[0]));
    warp_vector_box.value = {point.x + offset, 0, point.y + offset};
  } else {
    Require(*static_cast<int32_t*>(args[1]) == 1, "full preparation projects only Galaxy depth");
    const auto offset     = selection_projection_offsets.at(static_cast<Il2CppObject*>(receiver));
    const auto shift      = method == selection_depth_method ? offset : -offset;
    warp_vector_box.value = {point.x + shift, point.y, point.z + shift};
  }
  return &warp_vector_box.header;
}

void GalaxySelectionNativeCases(std::string_view mode)
{
  auto& world  = Define("Digit.Prime.Navigation", "GameWorldManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& galaxy = Define("Digit.PrimeServer.Models", "OptimisedGalaxy", IL2CPP_TYPE_CLASS, &object_class);
  auto& node =
      Define("Digit.PrimeServer.Models", "GalaxyNode", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(galaxy_binding::Node));
  AddField(node, "_galaxy", galaxy, 0);
  AddField(node, "_index", int_class, offsetof(galaxy_binding::Node, index));
  auto& camera         = Define("Digit.Prime.Navigation", "NavigationCamera", IL2CPP_TYPE_CLASS, &object_class);
  auto& scene          = Define("UnityEngine", "Camera", IL2CPP_TYPE_CLASS, &object_class);
  auto& zoom           = Define("Digit.Prime.Navigation", "NavigationZoom", IL2CPP_TYPE_CLASS, &object_class);
  auto& poi            = Define("Digit.Prime.Navigation", "POI", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& tap            = Define("", "TKTapRecognizer", IL2CPP_TYPE_CLASS, &object_class);
  auto& vector2        = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& interactable   = Define("Digit.Prime.Navigation", "InteractableType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  interactable.is_enum = true;
  AddField(interactable, "value__", int_class, sizeof(Il2CppObject));
  AddField(zoom, "_depth", int_class, offsetof(GalaxySelectionZoom, depth));
  AddField(zoom, "_zoomLevel", int_class, offsetof(GalaxySelectionZoom, level));
  AddField(warp_navigation_class, "_transitionAnimator", warp_animator_class, sizeof(Il2CppObject));
  AddMethod(world, "get_Instance", reinterpret_cast<void*>(&SelectionWorld), T(world), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(world, "get_ViewingGalaxy", reinterpret_cast<void*>(&SelectionGalaxy), T(galaxy), {});
  AddMethod(world, "IsMinorNode", reinterpret_cast<void*>(&SelectionMinor), T(bool_class), {Ref(node)});
  AddMethod(warp_navigation_class, "get_NavigationCamera", reinterpret_cast<void*>(&SelectionNavigation), T(camera),
            {});
  AddMethod(camera, "get_SceneCamera", reinterpret_cast<void*>(&SelectionScene), T(scene), {});
  AddMethod(camera, "get_NavZoomCamera", reinterpret_cast<void*>(&SelectionZoom), T(zoom), {});
  AddMethod(poi, "GetOfType", reinterpret_cast<void*>(&SelectionExisting), T(poi), {T(long_class), T(interactable)},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto& single     = AddMethod(warp_navigation_class, "OnSingleTap", reinterpret_cast<void*>(&SelectionSingle),
                               T(void_class), {T(vector2), T(poi)});
  auto& recognised = AddMethod(warp_navigation_class, "OnTapRecognised",
                               reinterpret_cast<void*>(&SelectionRecognisedStub), T(void_class), {T(tap)});
  auto& process    = AddMethod(warp_navigation_class, "ProcessInputTap", reinterpret_cast<void*>(&SelectionProcessStub),
                               T(void_class), {T(vector2), T(object_class), T(object_class), T(poi)});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&SelectionActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&SelectionSetActive), T(void_class),
            {T(bool_class)});
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&SelectionDestroy), T(void_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  for (auto* method : unity_object_class.methods)
    if (std::string_view(method->name) == "op_Implicit")
      const_cast<MethodInfo*>(method)->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&SelectionAlive);
  selection_world.klass      = &world.klass;
  selection_galaxy.klass     = &galaxy.klass;
  selection_navigation.klass = &camera.klass;
  selection_scene.klass = selection_other_scene.klass = &scene.klass;
  selection_zoom.header.klass                         = &zoom.klass;
  selection_existing.header.klass                     = &poi.klass;
  GalaxySelectionManager manager{{&warp_navigation_class.klass}, &warp_animator};
  selection_galaxies[&selection_world]    = &selection_galaxy;
  selection_navigations[&manager.header]  = &selection_navigation;
  selection_scenes[&selection_navigation] = &selection_scene;
  selection_zooms[&selection_navigation]  = &selection_zoom.header;
  auto*      native_manager               = reinterpret_cast<NavigationManager*>(&manager);
  const auto initial_roots                = warp_roots.size();
  auto&      state                        = SelectionState();
  state.proxies.clear();
  state.active = state.preparing = state.cleaning = state.dispatching = state.checking = false;
  state.generation                                                                     = 100;
  if (mode == "galaxy-selection-alias") {
    process.methodPointer = recognised.methodPointer;
    Require(!InstallGalaxySelection() && !state.installed && !il2cpp::HookStateIndeterminate(),
            "incompatible aliased hook bodies decline before any transaction");
    return;
  }
  if (mode == "galaxy-selection-fallback-alias") {
    single.methodPointer = process.methodPointer;
    Require(!InstallGalaxySelection() && !state.installed && !il2cpp::HookStateIndeterminate(),
            "direct fallback cannot share the ProcessInputTap detour's incompatible entry");
    single.methodPointer = recognised.methodPointer;
    Require(!InstallGalaxySelection() && !state.installed && !il2cpp::HookStateIndeterminate(),
            "direct fallback cannot share the recognised-tap detour's incompatible entry");
    return;
  }
  if (mode == "galaxy-selection-resolved-alias") {
    for (const auto [first, second] :
         {std::pair{&recognised, &process}, std::pair{&recognised, &single}, std::pair{&process, &single}}) {
      int        resolutions    = 0;
      const auto resolved_alias = [&](const il2cpp::Method& method) {
        ++resolutions;
        return method.Info() == second ? reinterpret_cast<void*>(first->methodPointer) : method.Pointer();
      };
      Require(!InstallGalaxySelectionWithEntries(resolved_alias) && resolutions == 3 && !state.installed
                  && !il2cpp::HookStateIndeterminate(),
              "distinct raw thunk entries resolving to one body must decline before a hook transaction");
    }
    return;
  }
  Require(mode == "galaxy-selection-native", "unknown Galaxy selection fixture mode");
  state.installed = true; // Controlled host admission, not a native hook-install claim.
  state.thread    = std::this_thread::get_id();
  Config::Get().graphics.galaxy_extended_selection = true;
  GalaxyClickSource source;
  source.manager    = il2cpp::GcHandle::Strong(&manager.header);
  source.navigation = il2cpp::GcHandle::Strong(&selection_navigation);
  source.world      = il2cpp::GcHandle::Strong(&selection_world);
  source.galaxy     = il2cpp::GcHandle::Strong(&selection_galaxy);
  source.camera     = il2cpp::GcHandle::Strong(&selection_scene);
  source.animator   = il2cpp::GcHandle::Strong(&warp_animator);
  Require(source.Current(), "qualified prepared click recognizes the same current world, camera, zoom and animator");
  selection_scene_replaced = true;
  Require(!source.Current(), "replacement scene camera invalidates prepared click before native publication");
  selection_scene_replaced = false;
  std::vector<int64_t> ids(100005, 99);
  std::vector<int32_t> xs(ids.size(), 1000), ys(ids.size(), 1000);
  xs.back() = ys.back() = 0;
  auto candidates = GalaxyClickCandidates(reinterpret_cast<galaxy_binding::World*>(&selection_world),
                                          reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&selection_galaxy), ids,
                                          xs, ys, -1, 1, -1, 1);
  Require(candidates && *candidates == std::vector<int32_t>{100004} && selection_last_minor == 100004,
          "actual source scan reaches beyond the frozen 100000-node ceiling without truncation");
  ids.resize(16);
  xs.assign(16, 0);
  ys.assign(16, 0);
  candidates = GalaxyClickCandidates(reinterpret_cast<galaxy_binding::World*>(&selection_world),
                                     reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&selection_galaxy), ids, xs, ys,
                                     -1, 1, -1, 1);
  Require(candidates && candidates->size() == 16, "all sixteen click-local candidates are admitted");
  ids.push_back(100);
  xs.push_back(0);
  ys.push_back(0);
  candidates = GalaxyClickCandidates(reinterpret_cast<galaxy_binding::World*>(&selection_world),
                                     reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&selection_galaxy), ids, xs, ys,
                                     -1, 1, -1, 1);
  Require(!candidates && state.proxies.empty(), "seventeenth candidate falls back without partial proxy activation");
  ids.resize(1);
  xs.resize(1);
  ys.resize(1);
  selection_existing_throws = true;
  candidates = GalaxyClickCandidates(reinterpret_cast<galaxy_binding::World*>(&selection_world),
                                     reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&selection_galaxy), ids, xs, ys,
                                     -1, 1, -1, 1);
  Require(!candidates, "failed native POI lookup cannot authorize a duplicate proxy as observed null");
  selection_existing_throws = false;
  selection_existing_result = &selection_existing.header;
  candidates = GalaxyClickCandidates(reinterpret_cast<galaxy_binding::World*>(&selection_world),
                                     reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&selection_galaxy), ids, xs, ys,
                                     -1, 1, -1, 1);
  Require(candidates && candidates->empty(), "an existing native POI keeps priority over synthetic hit-test targets");
  selection_existing_result = nullptr;

  int                  originals = 0, processed = 0;
  const unity::Vector2 point{12, 24};
  const auto           process_click = [&](unity::Vector2 position) {
    NavigationManager_ProcessInputTap_GalaxyHook([&](auto*, auto, auto*, auto*, auto*) { ++processed; }, native_manager,
                                                 position, nullptr, nullptr, nullptr);
  };
  DispatchGalaxyTap([&](auto*, auto*) { ++originals; }, native_manager, nullptr, point, true, &source);
  Require(originals == 1 && selection_single_taps == 1, "missing native dispatch gets exactly one unforced fallback");
  DispatchGalaxyTap(
      [&](auto*, auto*) {
        ++originals;
        process_click(point);
      },
      native_manager, nullptr, point, true, &source);
  Require(originals == 2 && processed == 1 && selection_single_taps == 1,
          "matching native dispatch preserves original once without a duplicate fallback");
  DispatchGalaxyTap(
      [&](auto*, auto*) {
        ++originals;
        process_click({13, 24});
      },
      native_manager, nullptr, point, true, &source);
  Require(originals == 3 && processed == 2 && selection_single_taps == 2,
          "another position does not falsely acknowledge this tap");
  DispatchGalaxyTap([&](auto*, auto*) { ++originals; }, native_manager, nullptr, point, false, nullptr);
  Require(originals == 4 && selection_single_taps == 2,
          "failed candidate preparation retains only the native original");
  selection_scene_replaced = true;
  DispatchGalaxyTap([&](auto*, auto*) { ++originals; }, native_manager, nullptr, point, true, &source);
  Require(selection_single_taps == 2, "replaced click source cannot trigger a stale fallback");
  selection_scene_replaced = false;

  const auto add_proxy = [&](int64_t id) {
    auto& object              = selection_objects.emplace_back();
    object.header.klass       = &game_object_class.klass;
    auto& point_object        = selection_objects.emplace_back();
    point_object.header.klass = &poi.klass;
    state.proxies.push_back({id,
                             il2cpp::GcHandle::Strong(&object.header),
                             il2cpp::GcHandle::Strong(&point_object.header),
                             {},
                             true,
                             1,
                             std::chrono::steady_clock::now() - std::chrono::seconds(5)});
  };
  add_proxy(1);
  state.active             = true;
  selection_disable_before = true;
  const auto held_roots    = warp_roots.size();
  DispatchGalaxyTap(
      [&](auto*, auto*) {
        ++originals;
        process_click({13, 24});
      },
      native_manager, nullptr, point, true, &source);
  Require(processed == 2 && selection_single_taps == 2 && state.active && warp_roots.size() == held_roots,
          "unconfirmed cleanup retains proxy custody and blocks both native hit testing and outer fallback");
  selection_disable_before  = false;
  selection_disable_after   = true;
  selection_disable_reentry = [&] { process_click(point); };
  Require(DeactivateGalaxyProxies() && !state.active,
          "confirmed inactive readback completes cleanup even when SetActive threw after mutation");
  Require(processed == 2 && state.proxies.size() == 1,
          "reentrant cleanup callback cannot dispatch native hit testing or mutate the proxy pool");
  selection_disable_reentry = {};
  selection_disable_after   = false;
  state.active              = true;
  selection_alive_throws    = true;
  Require(!DeactivateGalaxyProxies() && state.active && warp_roots.size() == held_roots,
          "unavailable native liveness retains cleanup custody instead of proving destruction");
  selection_alive_throws  = false;
  selection_active_throws = true;
  Require(!DeactivateGalaxyProxies() && state.active,
          "unavailable inactive readback cannot discharge cleanup responsibility");
  selection_active_throws = false;
  Require(DeactivateGalaxyProxies() && !state.active, "later qualified readback completes retained cleanup");
  state.proxies.clear();
  for (int i = 0; i < 64; ++i)
    add_proxy(i + 1);
  galaxy_binding::PinReader  reader;
  std::vector<Il2CppObject*> pins;
  for (const auto& proxy : state.proxies)
    pins.push_back(proxy.poi.Get());
  Require(!MakeGalaxyRoom(reader, {}, false) && !MakeGalaxyRoom(reader, pins, true) && selection_destroys == 0,
          "unknown owner coverage and explicitly pinned POIs cannot be retired at pool pressure");
  selection_destroy_throws = true;
  Require(!MakeGalaxyRoom(reader, {}, true) && state.proxies.size() == 64,
          "failed native destruction retains the complete bounded pool and its roots");
  selection_destroy_throws = false;
  Require(MakeGalaxyRoom(reader, {}, true) && state.proxies.size() == 63 && selection_destroys == 1,
          "qualified old unpinned target retires by destruction, never ID reassignment");
  state.proxies.clear();

  // Drive the enabled production tap/preparation path across a complete source replacement. These native
  // substitutes qualify source ownership and lifetime, not Unity projection accuracy or actual POI physics.
  {
    auto& transform           = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &unity_object_class);
    selection_poi_class       = &poi;
    selection_transform_class = &transform;
    AddMethod(game_object_class, ".ctor", reinterpret_cast<void*>(&SelectionConstruct), T(void_class),
              {T(string_class)});
    AddMethod(game_object_class, "AddComponent", reinterpret_cast<void*>(&SelectionAddComponent), T(poi),
              {T(type_class)});
    auto& get_transform =
        AddMethod(game_object_class, "get_transform", reinterpret_cast<void*>(&SelectionTransform), T(transform), {});
    AddProperty(game_object_class, "transform", &get_transform);
    AddMethod(transform, "set_position", reinterpret_cast<void*>(&SelectionPosition), T(void_class), {T(vec3_class)});
    AddMethod(poi, "set_ID", reinterpret_cast<void*>(&SelectionSetId), T(void_class), {T(long_class)});
    AddMethod(poi, "get_ID", reinterpret_cast<void*>(&SelectionId), T(long_class), {});
    AddMethod(poi, "set_InteractableType", reinterpret_cast<void*>(&SelectionSetType), T(void_class),
              {T(interactable)});
    AddMethod(poi, "get_InteractableType", reinterpret_cast<void*>(&SelectionType), T(interactable), {});
    auto& ids_vector    = Define("System", "GalaxySelectionInt64[]", IL2CPP_TYPE_SZARRAY, &array_class);
    auto& coords_vector = Define("System", "GalaxySelectionInt32[]", IL2CPP_TYPE_SZARRAY, &array_class);
    ids_vector.klass.byval_arg.data.type    = T(long_class);
    coords_vector.klass.byval_arg.data.type = T(int_class);
    auto& ids_repeated                      = Define("Game", "GalaxySelectionIds", IL2CPP_TYPE_CLASS, &object_class);
    auto& coords_repeated                   = Define("Game", "GalaxySelectionCoords", IL2CPP_TYPE_CLASS, &object_class);
    AddField(ids_repeated, "array", ids_vector, offsetof(GalaxySelectionRepeated, array));
    AddField(ids_repeated, "count", int_class, offsetof(GalaxySelectionRepeated, count));
    AddField(coords_repeated, "array", coords_vector, offsetof(GalaxySelectionRepeated, array));
    AddField(coords_repeated, "count", int_class, offsetof(GalaxySelectionRepeated, count));
    AddMethod(galaxy, "get_NodesCount", reinterpret_cast<void*>(&SelectionNodesCount), T(int_class), {});
    AddMethod(galaxy, "get_NodeIds", reinterpret_cast<void*>(&SelectionNodeIds), T(ids_repeated), {});
    AddMethod(galaxy, "get_XCoords", reinterpret_cast<void*>(&SelectionXCoords), T(coords_repeated), {});
    AddMethod(galaxy, "get_YCoords", reinterpret_cast<void*>(&SelectionYCoords), T(coords_repeated), {});
    auto& math             = Define("Digit.Client.Core", "MathUtils", IL2CPP_TYPE_CLASS, &object_class);
    selection_mouse_method = &AddMethod(math, "GetMouseWorldPos", reinterpret_cast<void*>(&Never), T(vec3_class),
                                        {T(scene), T(vec3_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
    selection_depth_method = &AddMethod(warp_animator_class, "DepthToWorldPosition", reinterpret_cast<void*>(&Never),
                                        T(vec3_class), {T(vec3_class), T(int_class)});
    selection_touch_method = &AddMethod(tap, "touchLocation", reinterpret_cast<void*>(&Never), T(vector2), {});
    for (const auto* method : {selection_mouse_method, selection_depth_method, selection_touch_method})
      const_cast<MethodInfo*>(method)->invoker_method = &WarpInvokerSentinel;
    selection_tap_box.header.klass         = &vector2.klass;
    const auto          previous_allocator = std::exchange(tow_object_new_override, &SelectionAllocate);
    const auto          previous_invoker   = std::exchange(tow_runtime_invoke_override, &SelectionInvoke);
    const auto          previous_box       = std::exchange(extra_value_box, &selection_tap_box.header);
    const auto          previous_payload   = std::exchange(extra_value_payload, &selection_tap_box.value);
    GalaxySelectionData first_data, second_data;
    const auto          configure_data = [&](GalaxySelectionData& data, Il2CppObject* owner, int32_t x, int32_t y,
                                             int32_t retained_count) {
      data.ids.header.klass = &ids_vector.klass;
      data.xs.header.klass = data.ys.header.klass = &coords_vector.klass;
      data.ids.length = data.xs.length = data.ys.length = static_cast<il2cpp_array_size_t>(retained_count);
      data.ids.values[0]                                = 99;
      data.xs.values[0]                                 = x;
      data.ys.values[0]                                 = y;
      // The transferred Galaxy can retain an arbitrary logical protobuf suffix beyond its native NodesCount.
      // Every extra entry has a distinct immutable identity and lies well outside this click's coordinates.
      for (int32_t i = 1; i < retained_count; ++i) {
        data.ids.values[i] = 99 + i;
        data.xs.values[i] = data.ys.values[i] = 10000 + i;
      }
      data.node_ids         = {{&ids_repeated.klass}, reinterpret_cast<Il2CppArray*>(&data.ids), retained_count};
      data.x_coords         = {{&coords_repeated.klass}, reinterpret_cast<Il2CppArray*>(&data.xs), retained_count};
      data.y_coords         = {{&coords_repeated.klass}, reinterpret_cast<Il2CppArray*>(&data.ys), retained_count};
      selection_data[owner] = &data;
    };
    configure_data(first_data, &selection_galaxy, 12, 24, 2);
    selection_projection_offsets[&selection_scene] = selection_projection_offsets[&warp_animator] = 0;
    Il2CppObject first_tap{&tap.klass};
    const auto   roots_before_clicks     = warp_roots.size();
    const auto   fallbacks_before_clicks = selection_single_taps;
    int          full_dispatches         = 0;
    const auto full_click = [&](GalaxySelectionManager& owner, Il2CppObject& recognition, Il2CppObject* current_galaxy,
                                size_t expected_pool, unity::Vector3 expected_position) {
      auto* entered = reinterpret_cast<NavigationManager*>(&owner);
      {
        auto&      data = *selection_data.at(current_galaxy);
        const auto ids_prefix =
            galaxy_binding::ReadValues<int64_t>(reinterpret_cast<galaxy_binding::Reference*>(&data.node_ids), 1);
        const auto xs_prefix =
            galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&data.x_coords), 1);
        const auto ys_prefix =
            galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&data.y_coords), 1);
        Require(ids_prefix && xs_prefix && ys_prefix && ids_prefix->data.size() == 1 && xs_prefix->data.size() == 1
                    && ys_prefix->data.size() == 1 && ids_prefix->data[0] == 99
                    && xs_prefix->data[0] == data.xs.values[0] && ys_prefix->data[0] == data.ys.values[0],
                "longer logical protobuf arrays expose exactly the native NodesCount prefix, never their suffix");
      }
      NavigationManager_OnTapRecognised_GalaxyHook(
          [&](NavigationManager* received, galaxy_binding::Tap* received_tap) {
            Require(received == entered && received_tap == reinterpret_cast<galaxy_binding::Tap*>(&recognition)
                        && state.active && state.dispatching && galaxy_click_source && galaxy_click_source->Current()
                        && galaxy_click_source->world.Get() == selection_current_world
                        && galaxy_click_source->galaxy.Get() == current_galaxy && state.proxies.size() == expected_pool,
                    "enabled original dispatch observes the current complete prepared source and owned pool");
            size_t active = 0;
            for (const auto& proxy : state.proxies) {
              auto* object = reinterpret_cast<GalaxySelectionObject*>(proxy.object.Get());
              if (!object->active)
                continue;
              ++active;
              const auto* prepared_poi = reinterpret_cast<GalaxySelectionObject*>(proxy.poi.Get());
              const auto* placed       = reinterpret_cast<GalaxySelectionObject*>(object->transform);
              Require(proxy.ready && proxy.id == 99 && proxy.galaxy.Get() == current_galaxy && prepared_poi->id == 99
                          && prepared_poi->type == galaxy_binding::Interactable::Star && placed
                          && placed->position.x == expected_position.x && placed->position.y == expected_position.y
                          && placed->position.z == expected_position.z,
                      "only a newly qualified current-Galaxy proxy carries the native ID and current projection");
            }
            Require(active == 1, "a prior Galaxy's immutable target stays inactive during current dispatch");
            NavigationManager_ProcessInputTap_GalaxyHook(
                [&](auto* actual, auto position, auto*, auto*, auto* forced) {
                  Require(actual == entered && position.x == 12 && position.y == 24 && !forced && state.active,
                          "full recognised tap reaches native processing once without a forced target");
                  ++full_dispatches;
                },
                received, point, nullptr, nullptr, nullptr);
          },
          entered, reinterpret_cast<galaxy_binding::Tap*>(&recognition));
      Require(!state.active && !state.dispatching && !galaxy_tap_watch && !galaxy_click_source
                  && std::ranges::all_of(state.proxies,
                                         [](const auto& proxy) {
                                           return !reinterpret_cast<GalaxySelectionObject*>(proxy.object.Get())->active;
                                         })
                  && warp_roots.size() == roots_before_clicks + expected_pool * 3,
              "full tap deactivates every target and releases all click/array/projection roots, retaining only pool "
              "custody");
    };
    full_click(manager, first_tap, &selection_galaxy, 1, {12, 0, 24});
    auto*        first_proxy = state.proxies[0].object.Get();
    Il2CppObject second_world{&world.klass}, second_galaxy{&galaxy.klass}, second_navigation{&camera.klass},
        second_scene{&scene.klass}, second_animator{&warp_animator_class.klass}, second_tap{&tap.klass};
    GalaxySelectionZoom    second_zoom{{&zoom.klass}};
    GalaxySelectionManager second_manager{{&warp_navigation_class.klass}, &second_animator};
    selection_current_world                       = &second_world;
    selection_galaxies[&second_world]             = &second_galaxy;
    selection_navigations[&second_manager.header] = &second_navigation;
    selection_scenes[&second_navigation]          = &second_scene;
    selection_zooms[&second_navigation]           = &second_zoom.header;
    selection_projection_offsets[&second_scene] = selection_projection_offsets[&second_animator] = 1000;
    configure_data(second_data, &second_galaxy, 18, 30, 5);
    Require(
        Config::Get().graphics.galaxy_extended_selection && !source.Current(),
        "replacing World/Galaxy/navigation/scene/zoom/animator invalidates the prior source with selection still on");
    full_click(second_manager, second_tap, &second_galaxy, 2, {1018, 0, 1030});
    Require(full_dispatches == 2 && selection_single_taps == fallbacks_before_clicks
                && state.proxies[0].object.Get() == first_proxy && state.proxies[0].galaxy.Get() == &selection_galaxy
                && state.proxies[1].object.Get() != first_proxy && state.proxies[1].galaxy.Get() == &second_galaxy,
            "two enabled full taps preserve distinct immutable targets for identical IDs in replacement Galaxies");
    for (auto& proxy : state.proxies)
      Require(DestroyGalaxyProxy(proxy), "inactive full-path targets can be destroyed before releasing their custody");
    state.proxies.clear();
    Require(warp_roots.size() == roots_before_clicks, "full source replacement and teardown leak no GC handles");
    selection_current_world = &selection_world;
    selection_data.clear();
    selection_projection_offsets.clear();
    tow_object_new_override     = previous_allocator;
    tow_runtime_invoke_override = previous_invoker;
    extra_value_box             = previous_box;
    extra_value_payload         = previous_payload;
  }
  Config::Get().graphics.galaxy_extended_selection = false;
  NavigationManager_OnTapRecognised_GalaxyHook([&](auto*, auto*) { ++originals; }, native_manager, nullptr);
  Require(originals == 7, "disabled feature preserves native recognised-tap original");
  std::thread wrong_thread([&] {
    NavigationManager_OnTapRecognised_GalaxyHook([&](auto*, auto*) { ++originals; }, native_manager, nullptr);
  });
  wrong_thread.join();
  Require(originals == 8, "another thread preserves native original without acquiring or mutating proxies");
  source = {};
  Require(warp_roots.size() == initial_roots && !galaxy_tap_watch && !galaxy_click_source && !state.dispatching,
          "all click-local sources, proxies and correlation pointers retire at fixture teardown");
  state.installed = false;
  std::cout << "Galaxy selection production candidate, dispatch, cleanup and immutable-pool adapters passed\n";
}
} // namespace
