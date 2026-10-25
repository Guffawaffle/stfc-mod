// Missions: the [ui] hud_* settings, which force mission HUD buttons always shown or never shown, and
// daily_bulk_claim_factions and daily_bulk_claim_toggle_default_on, which limit the missions screen's Claim All to
// the chosen factions' dailies, show only their icons next to the toggle, and can switch the toggle on.

#include "config.h"
#include "features/features.h"
#include "mission_hud_policy.h"
#include "text.h"

#include "game/hud.h"
#include "game/missions.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/core.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
// --- Mission HUD buttons ---

using ButtonGetter = unity::Component* (*)(MissionsHudViewController*);

template <il2cpp::Name Field> unity::Component* HudButton(MissionsHudViewController* controller)
{ return controller->ButtonField<Field>(); }

struct ButtonSetting {
  MissionHudVisibility Config::Ui::* visibility;
  ButtonGetter                       button;
};

constexpr ButtonSetting kHudButtons[] = {
    {&Config::Ui::hud_q_trials, HudButton<"_challengesButton">},
    {&Config::Ui::hud_field_training, HudButton<"_achievementsButton">},
    {&Config::Ui::hud_outposts, HudButton<"_outpostsButton">},
    {&Config::Ui::hud_missions, HudButton<"_missionsButton">},
};

struct HudInstance {
  il2cpp::GcHandle         controller, missions;
  mission_hud::Restoration restoration;
};
struct HudStateData {
  std::vector<HudInstance>                      instances;
  mission_hud::DeferredOwners<il2cpp::GcHandle> deferred;
  bool                                          installed = false, refreshing = false, applying = false, warned = false;
};
HudStateData& HudState()
{
  // Managed weak handles must never be released from native static teardown.
  static auto* state = new HudStateData;
  return *state;
}
void HudReportFailure() noexcept
{
  try {
    if (!std::exchange(HudState().warned, true))
      spdlog::warn("[MissionHud] native visibility update unavailable; keeping unconfirmed restoration pending");
  } catch (...) {
  }
}
void HudDefer(MissionsHudViewController* controller)
{ HudState().deferred.Remember(controller); }
unity::GameObject* HudButtonObject(MissionsHudViewController* controller, const ButtonSetting& definition)
{
  auto* component = definition.button(controller);
  if (!unity::Alive(component))
    return nullptr;
  const auto observed = component->TryCall<"get_gameObject", unity::GameObject*>();
  return observed && unity::Alive(observed.value) ? observed.value : nullptr;
}
HudInstance* HudFind(MissionsHudViewController* controller)
{
  auto&      instances = HudState().instances;
  const auto found =
      std::ranges::find_if(instances, [&](const auto& row) { return row.controller.Get() == controller; });
  return found == instances.end() ? nullptr : &*found;
}
HudInstance* HudTrack(MissionsHudViewController* controller, unity::GameObject* missions)
{
  auto& state = HudState();
  if (state.refreshing) {
    // Refresh iterates an owned snapshot. Native callbacks may replace views or
    // buttons, but their registry admission waits until that operation returns.
    auto* known = HudFind(controller);
    if (known && known->missions.Get() == missions)
      return known;
    HudDefer(controller);
    return nullptr;
  }
  std::erase_if(state.instances, [](const auto& row) { return !unity::Alive(row.controller.Get()); });
  if (auto* known = HudFind(controller)) {
    if (known->missions.Get() != missions) {
      auto weak = il2cpp::GcHandle::Weak(missions);
      if (!weak)
        return nullptr;
      known->missions = std::move(weak);
      known->restoration.Reset(); // The old baseline cannot describe a replacement button.
    }
    return known;
  }
  auto owner  = il2cpp::GcHandle::Weak(controller);
  auto button = il2cpp::GcHandle::Weak(missions);
  if (!owner || !button)
    return nullptr;
  state.instances.push_back({std::move(owner), std::move(button), {}});
  return &state.instances.back();
}
std::array<std::optional<bool>, std::size(kHudButtons)> HudModes()
{
  std::array<std::optional<bool>, std::size(kHudButtons)> modes;
  // Copy values before managed callbacks, which may replace Config on reload.
  const auto& ui = Config::Get().ui;
  for (size_t i = 0; i < modes.size(); ++i)
    if (const auto value = ui.*kHudButtons[i].visibility; value != MissionHudVisibility::kAuto)
      modes[i] = value == MissionHudVisibility::kAlways;
  return modes;
}
struct NativeHudButton {
  const il2cpp::GcHandle& owner;
  HudInstance&            instance;
  const ButtonSetting&    definition;
  il2cpp::GcHandle        button;

  unity::GameObject* Current()
  {
    auto* controller = owner.Get<MissionsHudViewController>();
    auto* object     = button.Get<unity::GameObject>();
    if (!unity::Alive(controller) || instance.controller.Get() != controller || !unity::Alive(object))
      return nullptr;
    if (HudButtonObject(controller, kHudButtons[3]) != instance.missions.Get()) {
      HudDefer(controller);
      return nullptr;
    }
    auto* current = HudButtonObject(controller, definition);
    return current == object && unity::Alive(controller) && instance.controller.Get() == controller
                   && unity::Alive(object)
               ? object
               : nullptr;
  }
  std::optional<bool> Observe()
  {
    auto*      object = Current();
    const auto value  = object ? object->TryCall<"get_activeSelf", bool>() : il2cpp::CallResult<bool>{};
    return value ? std::optional{value.value} : std::nullopt;
  }
  bool Write(bool visible)
  {
    auto* object = Current();
    return object && object->TryCall<"SetActive", void(bool)>(visible).succeeded;
  }
};
void HudDrainDeferred() noexcept;

// The game decides visibility in each hooked path; the hud_* settings are applied on top every time it does. A
// missing path would let the game's choice show through, so the paths install as one set: all of them or none.
void ApplyHudVisibility(MissionsHudViewController* controller) noexcept
{
  try {
    auto& state = HudState();
    if (!state.installed)
      return;
    mission_hud::ReentryGuard applying(state.applying);
    if (!applying) {
      HudDefer(controller);
      return;
    }
    auto owner = il2cpp::GcHandle::Strong(controller);
    if (!owner || !unity::Alive(owner.Get()))
      return;
    auto* missions = HudButtonObject(controller, kHudButtons[3]);
    auto* instance = missions ? HudTrack(controller, missions) : nullptr;
    if (!instance)
      return;
    const auto modes = HudModes();
    for (size_t i = 0; i < modes.size(); ++i) {
      NativeHudButton button{owner, *instance, kHudButtons[i],
                             il2cpp::GcHandle::Strong(HudButtonObject(controller, kHudButtons[i]))};
      if (i == 3) {
        mission_hud::ApplyVisibility(instance->restoration, modes[i], button);
      } else if (modes[i]) {
        const auto current = button.Observe();
        if (current && *current != *modes[i])
          (void)button.Write(*modes[i]);
      }
    }
  } catch (...) {
    HudReportFailure();
  }
  HudDrainDeferred();
}
void HudDrainDeferred() noexcept
{
  try {
    auto& state = HudState();
    state.deferred.Drain(state.refreshing || state.applying, [](auto* deferred) {
      auto owner = il2cpp::GcHandle::Strong(deferred);
      if (auto* controller = owner.Get<MissionsHudViewController>(); unity::Alive(controller))
        ApplyHudVisibility(controller);
    });
  } catch (...) {
    HudReportFailure();
  }
}
void MissionHudFrame()
{
  try {
    HudDrainDeferred(); // No owner scan or managed observation while idle.
  } catch (...) {
    HudReportFailure();
  }
}

void MissionsHudViewController_OnEnable_Hook(auto original, MissionsHudViewController* _this)
{
  original(_this);
  ApplyHudVisibility(_this);
}
void MissionsHudViewController_SetupAchievementsButton_Hook(auto original, MissionsHudViewController* _this)
{
  original(_this);
  ApplyHudVisibility(_this);
}
void MissionsHudViewController_SetupChallengesButton_Hook(auto original, MissionsHudViewController* _this, bool value)
{
  original(_this, value);
  ApplyHudVisibility(_this);
}
void MissionsHudViewController_SetupOutpostsButton_Hook(auto original, MissionsHudViewController* _this, bool value)
{
  original(_this, value);
  ApplyHudVisibility(_this);
}
void MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook(auto original, MissionsHudViewController* _this)
{
  original(_this);
  ApplyHudVisibility(_this);
}

struct HudRefreshController {
  il2cpp::GcHandle owner, missions;

  bool Ready()
  {
    auto* controller = owner.Get<MissionsHudViewController>();
    if (!unity::Alive(controller) || !controller->ActiveForRefresh())
      return false;
    auto* known   = HudFind(controller);
    auto* current = HudButtonObject(controller, kHudButtons[3]);
    if (!known || !unity::Alive(missions.Get()) || current != missions.Get() || known->missions.Get() != current) {
      HudDefer(controller);
      return false;
    }
    return unity::Alive(controller) && controller->ActiveForRefresh();
  }
  bool Achievements()
  { return owner.Get<MissionsHudViewController>()->TryRefreshAchievements(); }
  bool Outposts()
  { return owner.Get<MissionsHudViewController>()->TryRefreshOutposts(); }
  void Apply()
  { ApplyHudVisibility(owner.Get<MissionsHudViewController>()); }
};
void RefreshHudOwners() noexcept
{
  try {
    auto& state = HudState();
    if (!state.installed || state.applying || state.deferred.Draining())
      return;
    {
      mission_hud::ReentryGuard refreshing(state.refreshing);
      if (!refreshing)
        return;
      std::vector<HudRefreshController> snapshot;
      snapshot.reserve(state.instances.size());
      for (const auto& instance : state.instances) {
        auto owner    = il2cpp::GcHandle::Strong(instance.controller.Get());
        auto missions = il2cpp::GcHandle::Strong(instance.missions.Get());
        if (owner && missions)
          snapshot.push_back({std::move(owner), std::move(missions)});
      }
      for (auto& controller : snapshot)
        mission_hud::RefreshController(controller);
    }
    HudDrainDeferred();
  } catch (...) {
    HudReportFailure();
  }
}

// --- Daily faction claims ---

using DailyGoalList = il2cpp::List<DailyGoal*>;

struct Faction {
  const char* name; // as in daily_bulk_claim_factions
  const char* icon; // the GameObject next to the Claim All toggle
};

constexpr Faction kFactions[] = {
    {"federation", "Fed_Icon"},
    {"romulan", "Rom_Icon"},
    {"klingon", "Klg_Icon"},
};

bool Claimed(std::string_view faction)
{
  const auto& factions = Config::Get().ui.daily_bulk_claim_factions;
  return factions.empty() || std::ranges::find(factions, faction) != factions.end();
}

void ShowClaimedFactionIcons(MissionViewController* controller)
{
  auto* toggle    = controller->claimAllToggle();
  auto* transform = toggle ? toggle->transform() : nullptr;
  auto* parent    = transform ? transform->parent() : nullptr;
  if (!parent)
    return;
  for (const auto& faction : kFactions) {
    auto* icon   = parent->FindNamed(faction.icon);
    auto* object = icon ? icon->gameObject() : nullptr;
    if (object)
      object->SetActive(Claimed(faction.name));
    else
      spdlog::debug("[DailyClaimFilter] no {} icon next to Claim All", faction.icon);
  }
}

// The faction whose points a daily rewards, lower-cased ("federation"), or empty for other dailies.
std::string FactionOf(DailyGoal* daily)
{
  constexpr std::string_view kPrefix = "Resource_FactionPoint_";
  for (auto* item : il2cpp::Items(daily ? daily->factionPointRewards() : nullptr)) {
    auto* spec = item ? item->resourceSpec() : nullptr;
    auto* id   = spec ? spec->resourceId() : nullptr;
    if (auto resource = id ? il2cpp::ToUtf8(id) : std::string(); resource.starts_with(kPrefix))
      return AsciiStrToLower(resource.substr(kPrefix.size()));
  }
  return {};
}

DailyGoalList* MissionViewController_GetClaimableDailiesList_Hook(auto original, MissionViewController* _this)
{
  auto* list = original(_this);
  ShowClaimedFactionIcons(_this);
  if (!list || Config::Get().ui.daily_bulk_claim_factions.empty())
    return list;

  // Only the three main factions' dailies are filtered; anything else is always claimed.
  const auto total = list->Count();
  for (int32_t i = total - 1; i >= 0; --i) {
    const auto faction = FactionOf((*list)[i]);
    const bool main    = std::ranges::any_of(kFactions, [&](const Faction& f) { return faction == f.name; });
    if (main && !Claimed(faction))
      list->RemoveAt(i);
  }
  spdlog::debug("[DailyClaimFilter] claiming {} of {} dailies", list->Count(), total);
  return list;
}

// daily_bulk_claim_toggle_default_on flips the toggle once per session. The toggle is not interactable straight
// away, so the frame tick keeps trying while the missions screen is open, for at most kMaxToggleAttempts frames.
constexpr int kMaxToggleAttempts = 300;

// Weak, and checked before use: the screen can be destroyed without AboutToHide. Never destroyed: freeing a GC
// handle in a static destructor can run after the runtime has shut down.
auto& shown_missions_screen = *new il2cpp::GcHandle;
bool  toggle_done           = false;
int   toggle_attempts       = 0;

void MissionViewController_AboutToShow_Hook(auto original, MissionViewController* _this)
{
  original(_this);
  ShowClaimedFactionIcons(_this);
  shown_missions_screen = il2cpp::GcHandle::Weak(_this);
}

void MissionViewController_AboutToHide_Hook(auto original, MissionViewController* _this)
{
  if (shown_missions_screen.Get() == _this)
    shown_missions_screen.Reset();
  original(_this);
}

void DailyClaimFilterFrame()
{
  if (toggle_done || !shown_missions_screen || !Config::Get().ui.daily_bulk_claim_toggle_default_on)
    return;
  auto* controller = shown_missions_screen.Get<MissionViewController>();
  auto* toggle     = unity::Alive(controller) ? controller->claimAllToggle() : nullptr;
  if (!toggle)
    return;
  if (!toggle->isOn()) {
    if (++toggle_attempts > kMaxToggleAttempts) {
      spdlog::warn("[DailyClaimFilter] the Claim All toggle never became usable; leaving it off");
      toggle_done = true;
      return;
    }
    toggle->SimulateClick();
  }
  toggle_done = toggle->isOn();
}

bool InstallMissionHud()
{
  using Controller = MissionsHudViewController::Class;
  auto& state      = HudState();
  if (!Controller::HasField<"_challengesButton", unity::Component*>()
      || !Controller::HasField<"_achievementsButton", unity::Component*>()
      || !Controller::HasField<"_outpostsButton", unity::Component*>()
      || !Controller::HasField<"_missionsButton", unity::Component*>()
      || !unity::Component::Class::Method<unity::GameObject*()>("get_gameObject")
      || !unity::GameObject::Class::HasProperty<"activeSelf", bool>()
      || !unity::GameObject::Class::Method<void(bool)>("SetActive")
      || !Controller::Method<bool()>("get_isActiveAndEnabled")) {
    spdlog::warn("[MissionHud] checked visibility/refresh bindings unavailable; keeping native visibility");
    return false;
  }
  il2cpp::HookSet hooks("MissionHud");
  IL2CPP_STAGE(hooks, Controller::Method<void()>("OnEnable"), MissionsHudViewController_OnEnable_Hook);
  IL2CPP_STAGE(hooks, Controller::Method<void()>("SetupAchievementsButton"),
               MissionsHudViewController_SetupAchievementsButton_Hook);
  IL2CPP_STAGE(hooks, Controller::Method<void(bool)>("SetupChallengesButton"),
               MissionsHudViewController_SetupChallengesButton_Hook);
  IL2CPP_STAGE(hooks, Controller::Method<void(bool)>("SetupOutpostsButton"),
               MissionsHudViewController_SetupOutpostsButton_Hook);
  IL2CPP_STAGE(hooks, Controller::Method<void()>("HandleOutpostsAndChallengesHUD"),
               MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook);
  state.installed = hooks.Commit();
  return state.installed;
}

bool InstallDailyClaimFilter()
{
  using Controller = MissionViewController::Class;
  il2cpp::HookSet hooks("DailyClaimFilter");
  IL2CPP_STAGE(hooks, Controller::Method("GetClaimableDailiesList", 0),
               MissionViewController_GetClaimableDailiesList_Hook);
  IL2CPP_STAGE(hooks, Controller::Method("AboutToShow", 0), MissionViewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, Controller::Method("AboutToHide", 0), MissionViewController_AboutToHide_Hook);
  return hooks.Commit();
}
} // namespace

namespace mission_hud
{
bool Available() noexcept
{
  try {
    return HudState().installed;
  } catch (...) {
    return false;
  }
}
bool CanChange() noexcept
{
  try {
    const auto& state = HudState();
    return state.installed && !state.refreshing && !state.applying && !state.deferred.Draining();
  } catch (...) {
    return false;
  }
}
void Refresh() noexcept
{ RefreshHudOwners(); }
} // namespace mission_hud

const Feature kMissionHud{"MissionHud", &Config::Patches::missionhudtweakshooks, InstallMissionHud, MissionHudFrame};
const Feature kDailyClaimFilter{"DailyClaimFilter", &Config::Patches::dailyfactionbulkclaimhooks,
                                InstallDailyClaimFilter, DailyClaimFilterFrame};
