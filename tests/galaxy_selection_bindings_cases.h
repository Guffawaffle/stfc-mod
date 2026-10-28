#pragma once

// The production Galaxy selection adapter, in the existing controlled IL2CPP
// runtime. Projection/Unity hit testing remain native qualification; these cases
// exercise candidate enumeration, dispatch correlation, ownership and cleanup.
#include "../src/mod/features/galaxy_selection.cc"

namespace
{
struct GalaxySelectionObject {
  Il2CppObject header;
  bool         alive = true, active = true;
};
struct GalaxySelectionZoom {
  Il2CppObject header;
  NodeDepth    depth = NodeDepth::Galaxy;
  ZoomLevels   level = ZoomLevels::Far;
};
Il2CppObject selection_world{}, selection_galaxy{}, selection_navigation{}, selection_scene{}, selection_other_scene{};
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
{ return &selection_world; }
Il2CppObject* SelectionGalaxy(Il2CppObject*, const MethodInfo*)
{ return &selection_galaxy; }
Il2CppObject* SelectionNavigation(Il2CppObject*, const MethodInfo*)
{ return &selection_navigation; }
Il2CppObject* SelectionScene(Il2CppObject*, const MethodInfo*)
{ return selection_scene_replaced ? &selection_other_scene : &selection_scene; }
Il2CppObject* SelectionZoom(Il2CppObject*, const MethodInfo*)
{ return &selection_zoom.header; }
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
  struct Manager {
    Il2CppObject  header;
    Il2CppObject* animator;
  } manager{{&warp_navigation_class.klass}, &warp_animator};
  auto*      native_manager = reinterpret_cast<NavigationManager*>(&manager);
  const auto initial_roots  = warp_roots.size();
  auto&      state          = SelectionState();
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
