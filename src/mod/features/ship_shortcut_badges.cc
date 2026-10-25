// [graphics] ship_hotkey_badges: local-player system fleet labels show their
// current mod shortcut. Native mode and disabling the option restore native text.
#include "features/ship_shortcut_badges.h"
#include "config.h"
#include "features/features.h"
#include "features/ship_shortcut_hint.h"
#include "shortcuts.h"

#include "game/fleet.h"
#include "game/navigation.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/core.h>
#include <unity/tracker.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr std::array kShipBadgeActions{Action::kSelectShip1, Action::kSelectShip2, Action::kSelectShip3,
                                       Action::kSelectShip4, Action::kSelectShip5, Action::kSelectShip6,
                                       Action::kSelectShip7, Action::kSelectShip8};
struct ShipBadge {
  il2cpp::GcHandle widget;
  int64_t          fleet      = 0;
  int32_t          index      = -1;
  uint64_t         generation = 0;
  std::string      applied;
};
struct ShipBadgeState {
  std::unordered_map<NavigationFleetWidget*, ShipBadge> badges;
  uint64_t                                              generation = 0;
  bool                                                  installed  = false;
};
ShipBadgeState& ShipBadges()
{
  // Weak records have process lifetime; no GC release occurs after runtime teardown.
  static auto* state = new ShipBadgeState;
  return *state;
}

std::string ShipBadgeHint(int32_t index)
{
  const auto& config = Config::Get();
  if (!config.graphics.ship_hotkey_badges || !config.control.hotkeys_enabled || config.control.use_scopely_hotkeys
      || !config.patches.hotkeyhooks || index < 0 || static_cast<size_t>(index) >= kShipBadgeActions.size())
    return {};
  auto*      bar     = unity::Newest<FleetBarViewController>();
  auto*      context = bar ? bar->CanvasContext() : nullptr;
  const auto fleets  = il2cpp::Items(context ? context->FleetData() : nullptr);
  const auto action =
      ShipShortcutHintAction(kShipBadgeActions[static_cast<size_t>(index)], fleets.size(), [fleets](size_t position) {
        auto* fleet = fleets[position];
        return fleet ? fleet->Index() : -1;
      });
  return action == Action::kCount ? std::string{} : Shortcut::Hint(action);
}

void UpdateShipBadge(NavigationFleetWidget* key)
{
  auto& state = ShipBadges();
  auto  found = state.badges.find(key);
  if (found == state.badges.end())
    return;
  auto  source = il2cpp::GcHandle::Strong(found->second.widget.Get());
  auto* widget = source.Get<NavigationFleetWidget>();
  if (!widget || !unity::Alive(widget)) {
    state.badges.erase(found);
    return;
  }
  const auto generation = found->second.generation;
  const auto fleet_id   = found->second.fleet;
  const auto hint       = ShipBadgeHint(found->second.index);
  if (hint == found->second.applied)
    return;
  const auto context     = widget->TryCall<"get_Context", FleetDeployedData*>();
  auto       fleet_root  = il2cpp::GcHandle::Strong(context ? context.value : nullptr);
  auto*      fleet       = fleet_root.Get<FleetDeployedData>();
  const auto observed_id = fleet ? fleet->TryId() : il2cpp::CallResult<int64_t>{};
  if (!observed_id || observed_id.value != fleet_id)
    return;
  const auto label      = widget->TryFleetLevel();
  auto       label_root = il2cpp::GcHandle::Strong(label ? label.value : nullptr);
  auto*      localizer  = label_root.Get<TextLocalizer>();
  found                 = state.badges.find(key);
  if (!localizer || !unity::Alive(localizer) || found == state.badges.end() || found->second.generation != generation)
    return;
  const auto written = hint.empty() ? localizer->TryCall<"ClearTextOverride", void>()
                                    : localizer->TryCall<"OverrideLocalizedText", void>(il2cpp::NewString(hint));
  found              = state.badges.find(key);
  if (written && found != state.badges.end() && found->second.generation == generation)
    found->second.applied = hint;
}

void ShipBadgeFrame()
{
  auto& state = ShipBadges();
  if (state.badges.empty())
    return;
  static auto next = std::chrono::steady_clock::time_point{};
  const auto  now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::milliseconds(250);
  // Native text writes can reenter widget lifecycle callbacks. Snapshot keys,
  // then reacquire each weak record instead of holding map iterators across calls.
  std::vector<NavigationFleetWidget*> keys;
  keys.reserve(state.badges.size());
  for (const auto& [key, badge] : state.badges)
    keys.push_back(key);
  for (auto* key : keys)
    UpdateShipBadge(key);
}

bool InstallShipShortcutBadges()
{
  if (!NavigationFleetWidget::Class::HasField<"_fleetLevel", TextLocalizer*>()
      || !FleetDeployedData::Class::Method<bool()>("get_IsLocalPlayer")
      || !FleetDeployedData::Class::Method<int64_t()>("get_ID")
      || !FleetsManager::Class::Method<int32_t(int64_t)>("GetPlayerFleetIndex")
      || !TextLocalizer::Class::Method<void(Il2CppString*)>("OverrideLocalizedText")
      || !TextLocalizer::Class::Method<void()>("ClearTextOverride"))
    return false;
  ShipBadges().installed = EnsureNavigationFleetLifecycle();
  return ShipBadges().installed;
}
} // namespace

namespace ship_shortcut_badges
{
void Release(NavigationFleetWidget* widget)
{
  auto& state = ShipBadges();
  auto  found = state.badges.find(widget);
  if (found == state.badges.end())
    return;
  const bool overridden = !found->second.applied.empty();
  auto       source     = il2cpp::GcHandle::Strong(found->second.widget.Get());
  state.badges.erase(found);
  auto* owner = source.Get<NavigationFleetWidget>();
  if (!overridden || !owner || !unity::Alive(owner))
    return;
  const auto label = owner->TryFleetLevel();
  auto       root  = il2cpp::GcHandle::Strong(label ? label.value : nullptr);
  if (auto* localizer = root.Get<TextLocalizer>(); unity::Alive(localizer))
    (void)localizer->TryCall<"ClearTextOverride", void>();
}

void Bind(NavigationFleetWidget* widget)
{
  auto& state = ShipBadges();
  if (!state.installed)
    return;
  Release(widget);
  const auto context    = widget->TryCall<"get_Context", FleetDeployedData*>();
  auto       fleet_root = il2cpp::GcHandle::Strong(context ? context.value : nullptr);
  auto*      fleet      = fleet_root.Get<FleetDeployedData>();
  const auto local      = fleet ? fleet->TryIsLocalPlayer() : il2cpp::CallResult<bool>{};
  const auto id         = fleet ? fleet->TryId() : il2cpp::CallResult<int64_t>{};
  auto*      manager    = id ? FleetsManager::Instance() : nullptr;
  const auto index      = manager ? manager->TryPlayerFleetIndex(id.value) : il2cpp::CallResult<int32_t>{};
  if (!local || !local.value || !id || id.value <= 0 || !index || index.value < 0
      || static_cast<size_t>(index.value) >= kShipBadgeActions.size())
    return;
  auto weak = il2cpp::GcHandle::Weak(widget);
  if (!weak)
    return;
  state.badges.insert_or_assign(widget, ShipBadge{std::move(weak), id.value, index.value, ++state.generation, {}});
  UpdateShipBadge(widget);
}
} // namespace ship_shortcut_badges

const Feature kShipShortcutBadges{"ShipShortcutBadges", &Config::Patches::zoomhooks, InstallShipShortcutBadges,
                                  ShipBadgeFrame};
