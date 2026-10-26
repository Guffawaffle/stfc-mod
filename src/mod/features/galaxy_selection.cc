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
  bool                     installed = false, active = false, preparing = false, cleaning = false, dispatching = false;
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
bool                       GalaxySelectionThread()
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
  if (GalaxySelectionThread() && state.active && !state.dispatching && !state.preparing && !state.cleaning)
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
bool PrepareGalaxyProxies(NavigationManager* manager, gb::NavigationCamera* navigation, unity::Vector2 position)
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
  auto  manager_root    = il2cpp::GcHandle::Strong(manager);
  auto  navigation_root = il2cpp::GcHandle::Strong(navigation);
  auto  world_root      = il2cpp::GcHandle::Strong(gb::World::Instance());
  auto* world           = world_root.Get<gb::World>();
  auto  galaxy_root     = il2cpp::GcHandle::Strong(world ? world->Viewing() : nullptr);
  auto  camera_root     = il2cpp::GcHandle::Strong(navigation->Scene());
  auto  animator_root   = il2cpp::GcHandle::Strong(manager->TryTransitionAnimator().value);
  auto* galaxy          = galaxy_root.Get<gb::OptimisedGalaxy>();
  auto* camera          = camera_root.Get<unity::Camera>();
  auto* animator        = animator_root.Get<NavigationTransitionAnimator>();
  if (!manager_root || !navigation_root || !world || !galaxy || !camera || !animator)
    return false;
  const auto count = galaxy->TryCall<"get_NodesCount", int32_t>();
  if (!count || count.value <= 0)
    return false;
  auto ids = gb::ReadValues<int64_t>(galaxy->Get<"NodeIds", gb::Reference*>(), count.value);
  auto xs  = gb::ReadValues<int32_t>(galaxy->Get<"XCoords", gb::Reference*>(), count.value);
  auto ys  = gb::ReadValues<int32_t>(galaxy->Get<"YCoords", gb::Reference*>(), count.value);
  if (!ids || !xs || !ys)
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
  std::vector<int32_t> candidates;
  for (int32_t i = 0; i < count.value; ++i) {
    if (xs->data[i] < min_x || xs->data[i] > max_x || ys->data[i] < min_z || ys->data[i] > max_z)
      continue;
    const auto minor = world->Minor({galaxy, i, 0});
    if (!minor)
      return false;
    if (!minor.value)
      continue;
    auto       existing = il2cpp::GcHandle::Strong(gb::Poi::Existing(ids->data[i]));
    const auto alive    = unity::TryAlive(existing.Get());
    if (!alive)
      return false;
    if (alive.value)
      continue;
    candidates.push_back(i);
    if (candidates.size() > gs::MaximumClickCandidates)
      return false;
  }
  // A callback may replace the viewed Galaxy/camera while resolving pins; refuse that click rather than
  // publishing targets based on mixed source contexts.
  if (world->Viewing() != galaxy || gb::Camera(manager) != navigation)
    return false;
  state.proxies.reserve(gs::MaximumProxies);
  for (int32_t index : candidates) {
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
    if (!world_point)
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
    if (state.preparing || state.cleaning)
      return;
    if (state.active
        && (!state.dispatching || !galaxy_tap_watch || !galaxy_tap_watch->Matches(manager, position.x, position.y))) {
      if (!DeactivateGalaxyProxies())
        return;
    }
    if (galaxy_tap_watch && galaxy_tap_watch->Matches(manager, position.x, position.y))
      galaxy_tap_watch->dispatched = true;
  }
  original(manager, position, empty, multiple, forced);
}
void NavigationManager_OnTapRecognised_GalaxyHook(auto original, NavigationManager* manager, gb::Tap* tap)
{
  auto& state = SelectionState();
  if (!GalaxySelectionThread())
    return original(manager, tap);
  if (state.preparing || state.cleaning || state.dispatching)
    return;
  if (!DeactivateGalaxyProxies())
    return;
  if (!Config::Get().graphics.galaxy_extended_selection || !tap)
    return original(manager, tap);
  auto       manager_root    = il2cpp::GcHandle::Strong(manager);
  auto       tap_root        = il2cpp::GcHandle::Strong(tap);
  auto       navigation_root = il2cpp::GcHandle::Strong(gb::Camera(manager));
  auto*      navigation      = navigation_root.Get<gb::NavigationCamera>();
  auto       zoom_root       = il2cpp::GcHandle::Strong(navigation ? navigation->Zoom() : nullptr);
  auto*      zoom            = zoom_root.Get<NavigationZoom>();
  const auto point           = gb::Touch(tap);
  if (!manager_root || !tap_root || !navigation || !zoom || zoom->depth() != NodeDepth::Galaxy
      || zoom->zoomLevel() != ZoomLevels::Far || !point)
    return original(manager, tap);
  struct Cleanup {
    ~Cleanup()
    { DeactivateGalaxyProxies(); }
  } cleanup;
  const bool prepared = PrepareGalaxyProxies(manager, navigation, point.value);
  if (!prepared && !DeactivateGalaxyProxies())
    return;
  gs::TapWatch watch{manager, point.value.x, point.value.y};
  auto*        previous = std::exchange(galaxy_tap_watch, &watch);
  state.dispatching     = true;
  struct Dispatch {
    gs::TapWatch* previous;
    ~Dispatch()
    {
      SelectionState().dispatching = false;
      galaxy_tap_watch             = previous;
    }
  } dispatch{previous};
  original(manager, tap);
  if (prepared && !watch.dispatched)
    (void)manager->TryCall<"OnSingleTap", void(unity::Vector2, gb::Poi*)>(point.value, nullptr);
}
bool InstallGalaxySelection()
{
  if (!gb::NodeLayout() || !gb::World::MinorMethod() || !gb::ProcessTapMethod()
      || !NavigationManager::Class::Method<void(unity::Vector2, gb::Poi*)>("OnSingleTap"))
    return false;
  il2cpp::HookSet hooks("GalaxySelection");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationManager::Class::Method<void(gb::Tap*)>("OnTapRecognised")),
                  NavigationManager_OnTapRecognised_GalaxyHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(gb::ProcessTapMethod()), NavigationManager_ProcessInputTap_GalaxyHook);
  if (!hooks.Commit())
    return false;
  SelectionState().thread    = std::this_thread::get_id();
  SelectionState().installed = true;
  return true;
}
} // namespace
bool galaxy_controls::SelectionAvailable()
{ return SelectionState().installed; }
const Feature kGalaxySelection{"GalaxySelection", &Config::Patches::zoomhooks, InstallGalaxySelection,
                               GalaxySelectionFrame};
