// Streamer Mode's commander identity and location display masks.
#include "game/streamer_navigation_privacy.h"
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"
#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
namespace snb = streamer_navigation_binding;

using NavigationPrivacyCollector = bool (*)(il2cpp::Object*, snb::Targets&);
struct NavigationPrivacyView {
  il2cpp::GcHandle                                          owner;
  std::string_view                                          name;
  NavigationPrivacyCollector                                collect = nullptr;
  std::map<unity::GameObject*, streamer_mode::SurfaceCover> covers;
  bool                                                      applying = false, incomplete = false;
};

struct NavigationPrivacyLane {
  bool                                                              installed = false, hidden = false;
  bool                                                              untracked_view = false;
  std::set<std::string_view>                                        warned_surfaces;
  std::map<il2cpp::Object*, std::shared_ptr<NavigationPrivacyView>> views;
};

struct NavigationPrivacyState {
  std::thread::id       thread;
  NavigationPrivacyLane identity, locations;
};

NavigationPrivacyState& NavigationPrivacy()
{
  static auto* state = new NavigationPrivacyState;
  return *state;
}

bool NavigationPrivacyThread()
{ return std::this_thread::get_id() == NavigationPrivacy().thread; }

void NavigationPrivacyFailure(NavigationPrivacyLane& lane, std::string_view view, std::string_view stage)
{
  if (lane.warned_surfaces.insert(view).second)
    spdlog::warn("[StreamerNavigationPrivacy] {} presentation incomplete for {} ({})",
                 &lane == &NavigationPrivacy().identity ? "Commander identity" : "Location", view, stage);
}

bool RestoreNavigationCovers(NavigationPrivacyView& view)
{
  for (auto it = view.covers.begin(); it != view.covers.end();) {
    if (it->second.Restore())
      it = view.covers.erase(it);
    else
      ++it;
  }
  return view.covers.empty();
}

void ApplyNavigationPrivacy(const std::shared_ptr<NavigationPrivacyView>& view, NavigationPrivacyLane& lane,
                            bool hidden)
{
  if (std::exchange(view->applying, true))
    return;
  struct Finish {
    bool& applying;
    ~Finish()
    { applying = false; }
  } finish{view->applying};

  auto record = [&](bool complete, std::string_view stage) {
    view->incomplete = !complete;
    if (!complete)
      NavigationPrivacyFailure(lane, view->name, stage);
  };
  auto* observed = view->owner.Get();
  if (!hidden || !observed) {
    record(RestoreNavigationCovers(*view), "restore");
    return;
  }

  auto owner = il2cpp::GcHandle::Strong(observed);
  if (!owner) {
    record(false, "view lifetime");
    return;
  }
  const auto alive = unity::TryAlive(owner.Get());
  if (alive && !alive.value) {
    record(RestoreNavigationCovers(*view), "restore");
    return;
  }
  if (!alive) {
    record(false, "view lifetime");
    return;
  }

  snb::Targets targets;
  const bool   collected = view->collect(owner.Get<il2cpp::Object>(), targets);
  bool         updated   = true;
  if (collected)
    for (auto it = view->covers.begin(); it != view->covers.end();) {
      const bool current = std::ranges::any_of(targets, [&](const auto& root) { return root.Get() == it->first; });
      if (current) {
        ++it;
      } else if (it->second.Restore()) {
        it = view->covers.erase(it);
      } else {
        updated = false;
        ++it;
      }
    }
  for (const auto& root : targets) {
    auto* target = root.Get<unity::GameObject>();
    if (!view->covers[target].Set(target, true))
      updated = false;
  }
  record(collected && updated, collected ? "apply or restore cover" : "resolve renderer");
}

template <typename View> void ObserveNavigationPrivacy(View* owner, NavigationPrivacyLane& lane, bool hidden)
{
  if (!lane.installed || !owner->template Is<View>())
    return;
  const auto found = lane.views.find(owner);
  if (found != lane.views.end() && found->second->owner.Get() == owner) {
    ApplyNavigationPrivacy(found->second, lane, hidden);
    return;
  }
  if (found != lane.views.end()) {
    if (!RestoreNavigationCovers(*found->second)) {
      found->second->incomplete = true;
      NavigationPrivacyFailure(lane, found->second->name, "restore replaced view");
      return;
    }
    lane.views.erase(found);
  }
  auto root = il2cpp::GcHandle::Weak(owner);
  if (!root) {
    if (hidden) {
      lane.untracked_view = true;
      NavigationPrivacyFailure(lane, View::surface_name, "retain view");
    }
    return;
  }
  auto view     = std::make_shared<NavigationPrivacyView>();
  view->owner   = std::move(root);
  view->name    = View::surface_name;
  view->collect = [](il2cpp::Object* object, snb::Targets& targets) {
    return static_cast<View*>(object)->Collect(targets);
  };
  lane.views.emplace(owner, view);
  ApplyNavigationPrivacy(view, lane, hidden);
}

template <typename View> void ObserveStreamerIdentity(View* owner)
{
  if (NavigationPrivacyThread())
    ObserveNavigationPrivacy(owner, NavigationPrivacy().identity, streamer_mode::HideIdentity());
}

template <typename View> void ObserveStreamerLocation(View* owner)
{
  if (NavigationPrivacyThread())
    ObserveNavigationPrivacy(owner, NavigationPrivacy().locations, streamer_mode::HideLocations());
}

void UserProfileWidget_SetWidgetData_StreamerHook(auto original, snb::UserProfileWidget* _this)
{
  original(_this);
  ObserveStreamerIdentity(_this);
}
void PlayerProfileAllianceAreaWidget_SetWidgetData_StreamerHook(auto                                  original,
                                                                snb::PlayerProfileAllianceAreaWidget* _this)
{
  original(_this);
  ObserveStreamerIdentity(_this);
}
void NavigationFleetWidget_SetWidgetData_StreamerHook(auto original, snb::NavigationFleetWidget* _this)
{
  original(_this);
  ObserveStreamerIdentity(_this);
}
void NavigationStarbaseWidget_SetWidgetData_StreamerHook(auto original, snb::NavigationStarbaseWidget* _this)
{
  original(_this);
  ObserveStreamerIdentity(_this);
}
void NavigationGalaxyFleetWidget_SetWidgetData_StreamerHook(auto original, snb::NavigationGalaxyFleetWidget* _this)
{
  original(_this);
  ObserveStreamerIdentity(_this);
}
void NavigationStarEntityWidget_SetWidgetData_StreamerHook(auto original, snb::NavigationStarEntityWidget* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void FactionInformation_OnEnable_StreamerHook(auto original, snb::FactionInformation* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void HUDHomeLocatorAndFleetCommanderViewController_AboutToShow_StreamerHook(
    auto original, snb::HUDHomeLocatorAndFleetCommanderViewController* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void ViewLocationWidget_OnDidBindContext_StreamerHook(auto original, snb::ViewLocationWidget* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void BookmarkWidget_OnDidBindContext_StreamerHook(auto original, snb::BookmarkWidget* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void BookmarkListWidget_SetWidgetData_StreamerHook(auto original, snb::BookmarkListWidget* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}
void BookmarkDetailsViewController_OnDidBindCanvasContext_StreamerHook(auto                                original,
                                                                       snb::BookmarkDetailsViewController* _this)
{
  original(_this);
  ObserveStreamerLocation(_this);
}

template <typename View> void* NavigationPrivacyEntry(const char* name)
{
  if (!View::Available())
    return nullptr;
  const auto method = View::Class::template Method<void()>(name);
  return method && method.Info()->klass == View::Class::Raw() ? il2cpp::CheckedEntry(method) : nullptr;
}

bool NavigationPrivacyUniqueEntries(std::span<void* const> entries)
{
  for (size_t index = 0; index < entries.size(); ++index)
    if (entries[index] && std::ranges::contains(entries.first(index), entries[index]))
      return false;
  return true;
}

bool InstallStreamerIdentity()
{
  const std::array entries{NavigationPrivacyEntry<snb::UserProfileWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::PlayerProfileAllianceAreaWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::NavigationFleetWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::NavigationStarbaseWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::NavigationGalaxyFleetWidget>("SetWidgetData")};
  if (!NavigationPrivacyUniqueEntries(entries))
    return false;

  il2cpp::HookSet hooks("StreamerIdentity");
  IL2CPP_STAGE_AT(hooks, entries[0], UserProfileWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[1], PlayerProfileAllianceAreaWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[2], NavigationFleetWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[3], NavigationStarbaseWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[4], NavigationGalaxyFleetWidget_SetWidgetData_StreamerHook);
  return hooks.Commit();
}

bool InstallStreamerLocations()
{
  const std::array entries{NavigationPrivacyEntry<snb::NavigationStarEntityWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::FactionInformation>("OnEnable"),
                           NavigationPrivacyEntry<snb::HUDHomeLocatorAndFleetCommanderViewController>("AboutToShow"),
                           NavigationPrivacyEntry<snb::ViewLocationWidget>("OnDidBindContext"),
                           NavigationPrivacyEntry<snb::BookmarkWidget>("OnDidBindContext"),
                           NavigationPrivacyEntry<snb::BookmarkListWidget>("SetWidgetData"),
                           NavigationPrivacyEntry<snb::BookmarkDetailsViewController>("OnDidBindCanvasContext")};
  if (!NavigationPrivacyUniqueEntries(entries))
    return false;

  il2cpp::HookSet hooks("StreamerLocations");
  IL2CPP_STAGE_AT(hooks, entries[0], NavigationStarEntityWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[1], FactionInformation_OnEnable_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[2], HUDHomeLocatorAndFleetCommanderViewController_AboutToShow_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[3], ViewLocationWidget_OnDidBindContext_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[4], BookmarkWidget_OnDidBindContext_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[5], BookmarkListWidget_SetWidgetData_StreamerHook);
  IL2CPP_STAGE_AT(hooks, entries[6], BookmarkDetailsViewController_OnDidBindCanvasContext_StreamerHook);
  return hooks.Commit();
}

bool InstallStreamerNavigationPrivacy()
{
  if (!unity::CanvasGroup::Available())
    return false;
  auto& state               = NavigationPrivacy();
  state.thread              = std::this_thread::get_id();
  state.identity.installed  = InstallStreamerIdentity();
  state.locations.installed = InstallStreamerLocations();
  return state.identity.installed || state.locations.installed;
}

void RefreshNavigationPrivacy(NavigationPrivacyLane& lane, bool hidden, bool prune)
{
  if (!lane.installed)
    return;
  const bool changed = std::exchange(lane.hidden, hidden) != hidden;
  if (changed && !hidden)
    lane.untracked_view = false;
  if (!changed && !prune)
    return;
  std::vector<std::shared_ptr<NavigationPrivacyView>> views;
  for (auto it = lane.views.begin(); it != lane.views.end();) {
    if (!it->second->owner.Get() && it->second->covers.empty()) {
      it = lane.views.erase(it);
    } else {
      if (changed || it->second->incomplete || !it->second->owner.Get())
        views.push_back(it->second);
      ++it;
    }
  }
  for (const auto& view : views)
    ApplyNavigationPrivacy(view, lane, hidden);
}

void StreamerNavigationPrivacyFrame()
{
  if (!NavigationPrivacyThread())
    return;
  static Throttle cleanup(std::chrono::seconds(5));
  const bool      prune = cleanup.Due();
  auto&           state = NavigationPrivacy();
  RefreshNavigationPrivacy(state.identity, streamer_mode::HideIdentity(), prune);
  RefreshNavigationPrivacy(state.locations, streamer_mode::HideLocations(), prune);
}

bool NavigationPrivacyAvailable(const NavigationPrivacyLane& lane)
{ return NavigationPrivacyThread() && lane.installed && FrameDeliveryAvailable(); }

bool NavigationPrivacyDegraded(const NavigationPrivacyLane& lane, bool enabled)
{
  return enabled && NavigationPrivacyAvailable(lane)
         && (lane.untracked_view || std::ranges::any_of(lane.views, [](const auto& entry) {
               return entry.second->incomplete && entry.second->owner.Get();
             }));
}
} // namespace

bool streamer_mode::IdentityAvailable()
{ return NavigationPrivacyAvailable(NavigationPrivacy().identity); }

bool streamer_mode::LocationsAvailable()
{ return NavigationPrivacyAvailable(NavigationPrivacy().locations); }

bool streamer_mode::IdentityDegraded()
{ return NavigationPrivacyDegraded(NavigationPrivacy().identity, streamer_mode::HideIdentity()); }

bool streamer_mode::LocationsDegraded()
{ return NavigationPrivacyDegraded(NavigationPrivacy().locations, streamer_mode::HideLocations()); }

const Feature kStreamerNavigationPrivacy{"StreamerNavigationPrivacy", &Config::Patches::streamermodehooks,
                                         InstallStreamerNavigationPrivacy, StreamerNavigationPrivacyFrame};
