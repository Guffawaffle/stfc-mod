// Streamer Mode: mask the local Player Profile's name and alliance with streamer_hide_account.
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"
#include "game/player_profile.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace
{
struct StreamerSelfProfileView {
  il2cpp::GcHandle             owner;
  streamer_mode::SurfaceCovers covers;
  bool                         showing  = true;
  bool                         complete = true;
};

struct StreamerSelfProfileState {
  std::vector<std::shared_ptr<StreamerSelfProfileView>> views;
  Throttle                                              retry{std::chrono::milliseconds(250)};
  std::thread::id                                       game_thread;
  bool                                                  installed = false;
  bool                                                  hidden    = false;
  bool                                                  untracked = false;
  bool                                                  warned    = false;
};

StreamerSelfProfileState& StreamerSelfProfile()
{
  static auto* state = new StreamerSelfProfileState;
  return *state;
}

il2cpp::GcHandle StreamerProfileTarget(il2cpp::Object* component)
{
  const auto source = il2cpp::GcHandle::Strong(component);
  if (!unity::Alive(source.Get()))
    return {};
  const auto object = source.Get<il2cpp::Object>()->TryCall<"get_gameObject", unity::GameObject*>();
  return object && unity::Alive(object.value) ? il2cpp::GcHandle::Strong(object.value) : il2cpp::GcHandle{};
}

bool CoverSelfProfile(StreamerSelfProfileView& view, PlayerProfileViewController* owner)
{
  const auto info     = il2cpp::GcHandle::Strong(owner->playerInfoWidget());
  const auto alliance = il2cpp::GcHandle::Strong(owner->profileAllianceWidget());
  auto*      profile  = unity::Alive(info.Get()) ? info.Get<UserProfileWidget>() : nullptr;
  auto*      area     = unity::Alive(alliance.Get()) ? alliance.Get<PlayerProfileAllianceAreaWidget>() : nullptr;
  // TextLocalizer and ImageSelector resolve their graphics on the same authored GameObjects.
  const std::array targets{StreamerProfileTarget(profile ? profile->nameLabel() : nullptr),
                           StreamerProfileTarget(area ? area->allianceName() : nullptr),
                           StreamerProfileTarget(area ? area->allianceTag() : nullptr),
                           StreamerProfileTarget(area ? area->allianceIcon() : nullptr)};
  const std::array objects{targets[0].Get<unity::GameObject>(), targets[1].Get<unity::GameObject>(),
                           targets[2].Get<unity::GameObject>(), targets[3].Get<unity::GameObject>()};
  const bool       covered = view.covers.Apply(objects);
  return covered && std::ranges::all_of(objects, [](auto* object) { return object != nullptr; });
}

bool ApplyStreamerSelfProfile(StreamerSelfProfileView& view, bool hide)
{
  if (!hide || !view.showing)
    return view.covers.Apply();

  auto* observed = view.owner.Get();
  auto  owner    = il2cpp::GcHandle::Strong(observed);
  if (observed && !owner)
    return false;
  const auto alive = unity::TryAlive(owner.Get());
  if (!alive)
    return false;
  if (!alive.value) {
    view.showing = false;
    return view.covers.Apply();
  }

  auto*      profile = owner.Get<PlayerProfileViewController>();
  const auto local   = profile->TryIsLocalProfile();
  if (local && local.value)
    return CoverSelfProfile(view, profile);
  // A reused controller may now hold another player's profile or have no proven local owner.
  const bool restored = view.covers.Apply();
  return local.succeeded && restored;
}

void RefreshStreamerSelfProfileView(StreamerSelfProfileView& view, bool hide)
{
  view.complete = ApplyStreamerSelfProfile(view, hide);
  if (!view.complete && !std::exchange(StreamerSelfProfile().warned, true))
    spdlog::warn("[StreamerSelfProfile] could not verify Player Profile privacy");
}

void PruneStreamerSelfProfiles(StreamerSelfProfileState& state)
{
  std::erase_if(state.views, [](const auto& view) { return !view->showing && view->complete; });
}

auto FindStreamerSelfProfile(StreamerSelfProfileState& state, PlayerProfileViewController* owner)
{
  return std::ranges::find_if(state.views, [owner](const auto& view) { return view->owner.Get() == owner; });
}

void PlayerProfileViewController_AboutToShow_Hook(auto original, PlayerProfileViewController* _this)
{
  original(_this);
  auto& state = StreamerSelfProfile();
  if (state.game_thread != std::this_thread::get_id())
    return;

  const auto found = FindStreamerSelfProfile(state, _this);
  const auto view  = found != state.views.end() ? *found : std::make_shared<StreamerSelfProfileView>();
  if (found == state.views.end()) {
    view->owner = il2cpp::GcHandle::Weak(_this);
    if (!view->owner) {
      state.untracked = true;
      if (streamer_mode::HideAccount() && !std::exchange(state.warned, true))
        spdlog::warn("[StreamerSelfProfile] could not track the Player Profile view");
      return;
    }
    state.views.push_back(view);
  }
  view->showing = true;
  RefreshStreamerSelfProfileView(*view, streamer_mode::HideAccount());
}

void PlayerProfileViewController_DidHideCanvasEventHandler_Hook(auto original, PlayerProfileViewController* _this,
                                                                IVisibilityHandler* visibility, il2cpp::Object* context)
{
  auto& state = StreamerSelfProfile();
  if (state.game_thread == std::this_thread::get_id()) {
    const auto found = FindStreamerSelfProfile(state, _this);
    if (found != state.views.end()) {
      const auto view = *found;
      view->showing   = false;
      RefreshStreamerSelfProfileView(*view, false);
      PruneStreamerSelfProfiles(state);
    }
  }
  original(_this, visibility, context);
}

void StreamerSelfProfileFrame()
{
  auto&      state   = StreamerSelfProfile();
  const bool hide    = streamer_mode::HideAccount();
  const bool changed = std::exchange(state.hidden, hide) != hide;
  if (state.views.empty() || (!changed && !state.retry.Due()))
    return;

  const auto views = state.views;
  for (const auto& view : views) {
    if (!view->owner.Get())
      view->showing = false;
    if (changed || !view->complete || !view->showing)
      RefreshStreamerSelfProfileView(*view, hide);
  }
  PruneStreamerSelfProfiles(state);
}

bool InstallStreamerSelfProfile()
{
  using Controller = PlayerProfileViewController::Class;
  using Alliance   = PlayerProfileAllianceAreaWidget::Class;
  if (!unity::CanvasGroup::AlphaAvailable() || !Controller::HasField<"_userProfile", UserProfile*>()
      || !UserProfile::Class::Method<bool()>("get_IsLocalPlayer")
      || !Controller::HasField<"_playerInfoWidget", UserProfileWidget*>()
      || !Controller::HasField<"_profileAllianceWidget", PlayerProfileAllianceAreaWidget*>()
      || !UserProfileWidget::Class::HasField<"_name", TextLocalizer*>()
      || !Alliance::HasField<"_allianceName", TextLocalizer*>() || !Alliance::HasField<"_allianceTag", TextLocalizer*>()
      || !Alliance::HasField<"_allianceIcon", unity::Component*>())
    return false;

  const auto show = Controller::Method<void()>("AboutToShow");
  const auto hide = Controller::Method<void(IVisibilityHandler*, il2cpp::Object*)>("DidHideCanvasEventHandler");
  if (!show || !hide || show.Info()->klass != Controller::Raw() || hide.Info()->klass != Controller::Raw())
    return false;
  auto* show_entry = il2cpp::CheckedEntry(show);
  auto* hide_entry = il2cpp::CheckedEntry(hide);
  if (!show_entry || !hide_entry || show_entry == hide_entry)
    return false;

  il2cpp::HookSet hooks("StreamerSelfProfile");
  IL2CPP_STAGE_AT(hooks, show_entry, PlayerProfileViewController_AboutToShow_Hook);
  IL2CPP_STAGE_AT(hooks, hide_entry, PlayerProfileViewController_DidHideCanvasEventHandler_Hook);
  auto& state       = StreamerSelfProfile();
  state.game_thread = std::this_thread::get_id();
  state.installed   = hooks.Commit();
  return state.installed;
}
} // namespace

namespace streamer_mode
{
bool SelfProfileAvailable()
{ return StreamerSelfProfile().installed && FrameDeliveryAvailable(); }

bool SelfProfileDegraded()
{
  const auto& state = StreamerSelfProfile();
  return state.installed && HideAccount()
         && (state.untracked || std::ranges::any_of(state.views, [](const auto& view) { return !view->complete; }));
}
} // namespace streamer_mode

const Feature kStreamerSelfProfile{"StreamerSelfProfile", &Config::Patches::streamermodehooks,
                                   InstallStreamerSelfProfile, StreamerSelfProfileFrame};
