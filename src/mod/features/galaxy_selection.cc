// [graphics] galaxy_extended_selection: click minor systems at Far zoom using native hit testing, without
// enabling full entities/labels. Temporary immutable POIs are active only during the original tap dispatch.
#include "config.h"
#include "features/features.h"
#include "features/galaxy.h"
#include "galaxy_selection_pins.h"
#include "galaxy_selection_policy.h"
#include "game/galaxy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <il2cpp/hook.h>
#include <limits>
#include <optional>
#include <spdlog/spdlog.h>
#include <thread>
#include <unity/core.h>
#include <vector>

namespace
{
namespace gs = galaxy_selection;
namespace gb = galaxy_binding;
struct GalaxyProxy {
  int64_t                               id = 0;
  il2cpp::GcHandle                      object, poi, galaxy;
  bool                                  ready      = false;
  uint64_t                              last_click = 0;
  std::chrono::steady_clock::time_point last_used{};
};
struct GalaxySelectionState {
  bool installed = false, active = false, preparing = false, cleaning = false, dispatching = false, checking = false;
  uint64_t                 generation = 0;
  std::thread::id          thread;
  std::vector<GalaxyProxy> proxies;
};
GalaxySelectionState& SelectionState()
{
  static auto* state = new GalaxySelectionState;
  return *state;
}
thread_local gs::TapWatch* galaxy_tap_watch = nullptr;
struct GalaxyClickSource {
  il2cpp::GcHandle manager, navigation, world, galaxy, camera, animator;

  bool Current()
  {
    auto& state = SelectionState();
    if (state.checking || !manager || !navigation || !world || !galaxy || !camera || !animator)
      return false;
    state.checking = true;
    struct Done {
      bool& flag;
      ~Done()
      { flag = false; }
    } done{state.checking};
    const auto current_world      = gb::World::Class::TryCall<"get_Instance", gb::World*>();
    const auto current_galaxy     = world.Get<gb::World>()->TryViewing();
    const auto current_navigation = gb::TryCamera(manager.Get<NavigationManager>());
    const auto current_camera     = navigation.Get<gb::NavigationCamera>()->TryScene();
    const auto current_animator   = manager.Get<NavigationManager>()->TryTransitionAnimator();
    const auto current_zoom       = navigation.Get<gb::NavigationCamera>()->TryZoom();
    auto zoom = current_zoom && current_zoom.value ? il2cpp::GcHandle::Strong(current_zoom.value) : il2cpp::GcHandle{};
    const auto depth = zoom ? zoom.Get<NavigationZoom>()->TryDepth() : il2cpp::CallResult<NodeDepth>{};
    const auto level = zoom ? zoom.Get<NavigationZoom>()->TryZoomLevel() : il2cpp::CallResult<ZoomLevels>{};
    return current_world && current_world.value == world.Get<gb::World>() && current_galaxy
           && current_galaxy.value == galaxy.Get<gb::OptimisedGalaxy>() && current_navigation
           && current_navigation.value == navigation.Get<gb::NavigationCamera>() && current_camera
           && current_camera.value == camera.Get<unity::Camera>() && current_animator
           && current_animator.value == animator.Get<NavigationTransitionAnimator>() && depth
           && depth.value == NodeDepth::Galaxy && level && level.value == ZoomLevels::Far
           && Config::Get().graphics.galaxy_extended_selection;
  }
};
thread_local GalaxyClickSource* galaxy_click_source = nullptr;
bool                            GalaxySelectionThread()
{ return SelectionState().installed && SelectionState().thread == std::this_thread::get_id(); }
bool DeactivateGalaxyProxies()
{
  auto& state = SelectionState();
  if (!state.active)
    return true;
  if (state.cleaning)
    return false;
  state.cleaning = true;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{state.cleaning};
  // Pool-owned strong handles retain every receiver. No new GC-handle allocation is needed for cleanup;
  // the cleaning guard prevents synchronous callbacks from mutating the pool.
  std::vector<Il2CppObject*> objects;
  for (const auto& proxy : state.proxies)
    objects.push_back(proxy.object.Get());
  struct Host {
    std::optional<bool> Alive(Il2CppObject* object)
    {
      const auto observed = unity::TryAlive(object);
      return observed ? std::optional(observed.value) : std::nullopt;
    }
    std::optional<bool> Active(Il2CppObject* object)
    {
      const auto observed = reinterpret_cast<unity::GameObject*>(object)->TryCall<"get_activeSelf", bool>();
      return observed ? std::optional(observed.value) : std::nullopt;
    }
    void Disable(Il2CppObject* object)
    { (void)reinterpret_cast<unity::GameObject*>(object)->TryCall<"SetActive", void(bool)>(false); }
  } host;
  state.active = !gs::Deactivate(objects, host);
  return !state.active;
}
void GalaxySelectionFrame()
{
  auto& state = SelectionState();
  if (GalaxySelectionThread() && state.active && !state.dispatching && !state.preparing && !state.cleaning
      && !state.checking)
    (void)DeactivateGalaxyProxies();
}

bool DestroyGalaxyProxy(GalaxyProxy& proxy)
{
  const auto alive = unity::TryAlive(proxy.object.Get());
  if (!alive)
    return false;
  if (alive.value) {
    (void)proxy.object.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(false);
    const auto active = proxy.object.Get<unity::GameObject>()->TryCall<"get_activeSelf", bool>();
    if (!active || active.value)
      return false;
    if (!unity::UnityObject::Class::TryCall<"Destroy", void(unity::UnityObject*)>(
            reinterpret_cast<unity::UnityObject*>(proxy.object.Get())))
      return false;
  }
  return true;
}
bool MakeGalaxyRoom(gb::PinReader& reader, const std::vector<Il2CppObject*>& pins, bool known)
{
  (void)reader; // Its roots must outlive the decision and native destruction.
  auto& state = SelectionState();
  if (state.proxies.size() < gs::MaximumProxies)
    return true;
  const auto now    = std::chrono::steady_clock::now();
  size_t     victim = state.proxies.size();
  for (size_t i = 0; i < state.proxies.size(); ++i) {
    const auto& proxy  = state.proxies[i];
    const bool  pinned = std::ranges::find(pins, proxy.poi.Get()) != pins.end();
    if (gs::CanRetire(state.generation, proxy.last_click, now - proxy.last_used, known, pinned)
        && (victim == state.proxies.size() || proxy.last_click < state.proxies[victim].last_click))
      victim = i;
  }
  if (victim == state.proxies.size() || !DestroyGalaxyProxy(state.proxies[victim]))
    return false;
  state.proxies.erase(state.proxies.begin() + static_cast<ptrdiff_t>(victim));
  return true;
}
std::optional<std::vector<int32_t>> GalaxyClickCandidates(gb::World* world, gb::OptimisedGalaxy* galaxy,
                                                          std::span<const int64_t> ids, std::span<const int32_t> xs,
                                                          std::span<const int32_t> ys, float min_x, float max_x,
                                                          float min_z, float max_z)
{
  if (!world || !galaxy || ids.size() != xs.size() || ids.size() != ys.size()
      || ids.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
    return {};
  std::vector<int32_t> candidates;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (xs[i] < min_x || xs[i] > max_x || ys[i] < min_z || ys[i] > max_z)
      continue;
    const auto minor = world->Minor({galaxy, static_cast<int32_t>(i), 0});
    if (!minor)
      return {};
    if (!minor.value)
      continue;
    const auto existing = gb::Poi::TryExisting(ids[i]);
    if (!existing)
      return {};
    auto root = il2cpp::GcHandle::Strong(existing.value);
    if (existing.value && !root)
      return {};
    const auto alive = unity::TryAlive(root.Get());
    if (!alive)
      return {};
    if (alive.value)
      continue;
    candidates.push_back(static_cast<int32_t>(i));
    if (candidates.size() > gs::MaximumClickCandidates)
      return {}; // All-or-native fallback, never a truncated candidate list.
  }
  return candidates;
}

bool PrepareGalaxyProxies(NavigationManager* manager, gb::NavigationCamera* navigation, unity::Vector2 position,
                          GalaxyClickSource& source)
{
  auto& state = SelectionState();
  if (state.preparing)
    return false;
  state.preparing = true;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{state.preparing};
  if (!DeactivateGalaxyProxies())
    return false;
  for (size_t i = 0; i < state.proxies.size();) {
    const auto alive     = unity::TryAlive(state.proxies[i].object.Get());
    const auto poi_alive = unity::TryAlive(state.proxies[i].poi.Get());
    if (alive && poi_alive && (!state.proxies[i].ready || !alive.value || !poi_alive.value)
        && DestroyGalaxyProxy(state.proxies[i]))
      state.proxies.erase(state.proxies.begin() + static_cast<ptrdiff_t>(i));
    else
      ++i;
  }
  source.manager    = il2cpp::GcHandle::Strong(manager);
  source.navigation = il2cpp::GcHandle::Strong(navigation);
  if (!source.manager || !source.navigation)
    return false;
  const auto world_read  = gb::World::Class::TryCall<"get_Instance", gb::World*>();
  source.world           = world_read ? il2cpp::GcHandle::Strong(world_read.value) : il2cpp::GcHandle{};
  auto*      world       = source.world.Get<gb::World>();
  const auto galaxy_read = world ? world->TryViewing() : il2cpp::CallResult<gb::OptimisedGalaxy*>{};
  source.galaxy          = galaxy_read ? il2cpp::GcHandle::Strong(galaxy_read.value) : il2cpp::GcHandle{};
  if (!source.galaxy)
    return false;
  const auto camera_read = source.navigation.Get<gb::NavigationCamera>()->TryScene();
  source.camera          = camera_read ? il2cpp::GcHandle::Strong(camera_read.value) : il2cpp::GcHandle{};
  if (!source.camera)
    return false;
  const auto animator_read = source.manager.Get<NavigationManager>()->TryTransitionAnimator();
  source.animator          = animator_read ? il2cpp::GcHandle::Strong(animator_read.value) : il2cpp::GcHandle{};
  auto* galaxy             = source.galaxy.Get<gb::OptimisedGalaxy>();
  auto* camera             = source.camera.Get<unity::Camera>();
  auto* animator           = source.animator.Get<NavigationTransitionAnimator>();
  if (!source.manager || !source.navigation || !world || !galaxy || !camera || !animator)
    return false;
  const auto count = galaxy->TryNodesCount();
  if (!count || count.value <= 0)
    return false;
  const auto ids_read = galaxy->TryNodeIds();
  if (!ids_read)
    return false;
  auto ids = gb::ReadValues<int64_t>(ids_read.value, count.value);
  if (!ids)
    return false;
  const auto xs_read = galaxy->TryXCoords();
  if (!xs_read)
    return false;
  auto xs = gb::ReadValues<int32_t>(xs_read.value, count.value);
  if (!xs)
    return false;
  const auto ys_read = galaxy->TryYCoords();
  if (!ys_read)
    return false;
  auto ys = gb::ReadValues<int32_t>(ys_read.value, count.value);
  if (!ys)
    return false;
  float min_x = std::numeric_limits<float>::max(), min_z = min_x, max_x = -min_x, max_z = -min_x;
  for (float dx : {-64.f, 64.f})
    for (float dy : {-64.f, 64.f}) {
      const auto mouse = gb::MousePoint(camera, {position.x + dx, position.y + dy, 0});
      const auto local =
          mouse ? gb::TransformPoint(animator, mouse.value, false) : il2cpp::CallResult<unity::Vector3>{};
      if (!local)
        return false;
      min_x = std::min(min_x, local.value.x);
      max_x = std::max(max_x, local.value.x);
      min_z = std::min(min_z, local.value.z);
      max_z = std::max(max_z, local.value.z);
    }
  ++state.generation;
  gb::PinReader              reader;
  std::vector<Il2CppObject*> pins;
  const bool                 pins_known = gs::CollectOwnerPins(static_cast<Il2CppObject*>(manager), pins, reader);
  // Complete candidate qualification before activating any proxy. Too dense means native fallback, never a
  // truncated candidate list or a forced target. No cap/truncation of source Galaxy nodes is applied.
  const auto candidates =
      GalaxyClickCandidates(world, galaxy, ids->data, xs->data, ys->data, min_x, max_x, min_z, max_z);
  if (!candidates)
    return false;
  // A callback may replace the viewed Galaxy/camera while resolving pins; refuse that click rather than
  // publishing targets based on mixed source contexts.
  if (!source.Current())
    return false;
  state.proxies.reserve(gs::MaximumProxies);
  for (int32_t index : *candidates) {
    const auto id = ids->data[index];
    if (id <= 0)
      return false;
    auto found = std::ranges::find_if(
        state.proxies, [&](const auto& proxy) { return proxy.id == id && proxy.galaxy.Get() == galaxy; });
    if (found == state.proxies.end()) {
      if (!MakeGalaxyRoom(reader, pins, pins_known))
        return false;
      auto* created     = unity::GameObject::Create("GalaxySelectionTarget");
      auto  object_root = il2cpp::GcHandle::Strong(created);
      if (!object_root) {
        if (created)
          unity::Destroy(created);
        return false;
      }
      // Take custody before any fallible native mutation. Partial construction remains in the bounded pool
      // until cleanup can observe an inactive object and safely destroy it on the next preparation.
      state.proxies.push_back({id, std::move(object_root)});
      found        = std::prev(state.proxies.end());
      state.active = true;
      auto* object = found->object.Get<unity::GameObject>();
      if (!object->TryCall<"SetActive", void(bool)>(false))
        return false;
      found->poi = il2cpp::GcHandle::Strong(object->AddComponent<gb::Poi>());
      auto* poi  = found->poi.Get<gb::Poi>();
      if (!poi || !poi->Initialize(id))
        return false;
      found->galaxy = il2cpp::GcHandle::Weak(galaxy);
      if (!found->galaxy)
        return false;
      found->ready = true;
    }
    // Objects never change IDs. Existing preview references cannot silently become a different system.
    const auto world_point = gb::TransformPoint(
        animator, {static_cast<float>(xs->data[index]), 0, static_cast<float>(ys->data[index])}, true);
    if (!world_point || !source.Current())
      return false;
    auto object_root = il2cpp::GcHandle::Strong(found->object.Get());
    if (!object_root)
      return false;
    auto transform_root = il2cpp::GcHandle::Strong(object_root.Get<unity::GameObject>()->transform());
    if (!transform_root
        || !transform_root.Get<unity::Transform>()->TryCall<"set_position", void(unity::Vector3)>(world_point.value))
      return false;
    found->last_click = state.generation;
    found->last_used  = std::chrono::steady_clock::now();
    state.active      = true; // Even an enable that throws after mutation must be covered by outer cleanup.
    if (!object_root.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(true))
      return false;
  }
  return true;
}
void NavigationManager_ProcessInputTap_GalaxyHook(auto original, NavigationManager* manager, unity::Vector2 position,
                                                  gb::Reference* empty, gb::Reference* multiple, gb::Poi* forced)
{
  auto& state = SelectionState();
  if (GalaxySelectionThread()) {
    if (state.preparing || state.cleaning || state.checking)
      return;
    const bool matching =
        state.dispatching && galaxy_tap_watch && galaxy_tap_watch->Matches(manager, position.x, position.y);
    if (state.active && (!matching || !galaxy_click_source || !galaxy_click_source->Current())) {
      if (!DeactivateGalaxyProxies()) {
        if (galaxy_tap_watch)
          galaxy_tap_watch->blocked = true;
        return;
      }
    }
    if (galaxy_tap_watch && galaxy_tap_watch->Matches(manager, position.x, position.y))
      galaxy_tap_watch->dispatched = true;
  }
  original(manager, position, empty, multiple, forced);
}
void DispatchGalaxyTap(auto original, NavigationManager* manager, gb::Tap* tap, unity::Vector2 point, bool prepared,
                       GalaxyClickSource* source)
{
  auto&        state = SelectionState();
  gs::TapWatch watch{manager, point.x, point.y};
  auto*        previous        = std::exchange(galaxy_tap_watch, &watch);
  auto*        previous_source = std::exchange(galaxy_click_source, source);
  state.dispatching            = true;
  struct Dispatch {
    gs::TapWatch*      previous;
    GalaxyClickSource* source;
    ~Dispatch()
    {
      SelectionState().dispatching = false;
      galaxy_tap_watch             = previous;
      galaxy_click_source          = source;
    }
  } dispatch{previous, previous_source};
  original(manager, tap);
  if (prepared && !watch.dispatched && !watch.blocked && source && source->Current())
    (void)il2cpp::detail::TryInvoke<void>(gb::SingleTapMethod().Info(), manager, point, static_cast<gb::Poi*>(nullptr));
}
void NavigationManager_OnTapRecognised_GalaxyHook(auto original, NavigationManager* manager, gb::Tap* tap)
{
  auto& state = SelectionState();
  if (!GalaxySelectionThread())
    return original(manager, tap);
  if (state.preparing || state.cleaning || state.dispatching || state.checking)
    return;
  if (!DeactivateGalaxyProxies())
    return;
  if (!Config::Get().graphics.galaxy_extended_selection || !tap)
    return original(manager, tap);
  auto       manager_root    = il2cpp::GcHandle::Strong(manager);
  auto       tap_root        = il2cpp::GcHandle::Strong(tap);
  const auto navigation_read = gb::TryCamera(manager);
  auto       navigation_root = navigation_read ? il2cpp::GcHandle::Strong(navigation_read.value) : il2cpp::GcHandle{};
  auto*      navigation      = navigation_root.Get<gb::NavigationCamera>();
  const auto zoom_read       = navigation ? navigation->TryZoom() : il2cpp::CallResult<NavigationZoom*>{};
  auto       zoom_root       = zoom_read ? il2cpp::GcHandle::Strong(zoom_read.value) : il2cpp::GcHandle{};
  auto*      zoom            = zoom_root.Get<NavigationZoom>();
  const auto point           = gb::Touch(tap);
  const auto depth           = zoom ? zoom->TryDepth() : il2cpp::CallResult<NodeDepth>{};
  const auto level           = zoom ? zoom->TryZoomLevel() : il2cpp::CallResult<ZoomLevels>{};
  if (!manager_root || !tap_root || !navigation || !depth || depth.value != NodeDepth::Galaxy || !level
      || level.value != ZoomLevels::Far || !point)
    return original(manager, tap);
  GalaxyClickSource source;
  struct Cleanup {
    ~Cleanup()
    { DeactivateGalaxyProxies(); }
  } cleanup;
  bool prepared = PrepareGalaxyProxies(manager, navigation, point.value, source);
  if (prepared && !source.Current())
    prepared = false;
  if (!prepared && !DeactivateGalaxyProxies())
    return;
  DispatchGalaxyTap(original, manager, tap, point.value, prepared, &source);
}
bool InstallGalaxySelectionWithEntries(auto resolve_entry)
{
  const auto recognised = NavigationManager::Class::Method<void(gb::Tap*)>("OnTapRecognised");
  const auto process    = gb::ProcessTapMethod();
  const auto single     = gb::SingleTapMethod();
  if (!gb::NodeLayout() || !gb::World::MinorMethod() || !recognised || !process || !single)
    return false;
  if (recognised.Info()->methodPointer == process.Info()->methodPointer
      || recognised.Info()->methodPointer == single.Info()->methodPointer
      || process.Info()->methodPointer == single.Info()->methodPointer) {
    spdlog::warn(
        "[GalaxySelection] incompatible tap callbacks or fallback share a native entry; using native selection");
    return false;
  }
  // On ARM64, distinct small thunks can resolve to the same branch body.
  // Admit and stage this exact snapshot, including the directly invoked
  // fallback's resolved body, rather than re-resolving after the alias check.
  const std::array<void*, 3> entries{resolve_entry(recognised), resolve_entry(process), resolve_entry(single)};
  if (std::ranges::find(entries, nullptr) != entries.end())
    return false;
  if (entries[0] == entries[1] || entries[0] == entries[2] || entries[1] == entries[2]) {
    spdlog::warn(
        "[GalaxySelection] incompatible tap callbacks or fallback resolve to one body; using native selection");
    return false;
  }
  il2cpp::HookSet hooks("GalaxySelection");
  IL2CPP_STAGE_AT(hooks, entries[0], NavigationManager_OnTapRecognised_GalaxyHook);
  IL2CPP_STAGE_AT(hooks, entries[1], NavigationManager_ProcessInputTap_GalaxyHook);
  if (!hooks.Commit())
    return false;
  SelectionState().thread    = std::this_thread::get_id();
  SelectionState().installed = true;
  return true;
}
bool InstallGalaxySelection()
{
  return InstallGalaxySelectionWithEntries([](const auto& method) { return il2cpp::CheckedEntry(method); });
}
} // namespace
bool galaxy_controls::SelectionAvailable()
{ return SelectionState().installed; }
const Feature kGalaxySelection{"GalaxySelection", &Config::Patches::zoomhooks, InstallGalaxySelection,
                               GalaxySelectionFrame};
