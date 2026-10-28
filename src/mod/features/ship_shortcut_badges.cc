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

#include <algorithm>
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
  il2cpp::GcHandle widget, label;
  int64_t          fleet      = 0;
  int32_t          index      = -1;
  uint64_t         generation = 0;
  std::string      applied;
  bool             may_override = false;
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
  auto       bar_root = il2cpp::GcHandle::Strong(unity::Newest<FleetBarViewController>());
  auto*      bar      = bar_root.Get<FleetBarViewController>();
  const auto context_observation =
      bar ? bar->TryCall<"get_CanvasContext", FleetBarContext*>() : il2cpp::CallResult<FleetBarContext*>{};
  auto       context_root = il2cpp::GcHandle::Strong(context_observation ? context_observation.value : nullptr);
  auto*      context      = context_root.Get<FleetBarContext>();
  const auto array_observation =
      context ? context->TryCall<"get_FleetData", il2cpp::ArrayObject*>() : il2cpp::CallResult<il2cpp::ArrayObject*>{};
  auto       array_root = il2cpp::GcHandle::Strong(array_observation ? array_observation.value : nullptr);
  const auto fleets     = array_root ? il2cpp::TryReferenceArray<FleetPlayerData*>(array_root.Get())
                                     : il2cpp::CallResult<il2cpp::Array<FleetPlayerData*>*>{};
  if (!fleets || !fleets.value)
    return {};
  std::array<int32_t, kShipBadgeActions.size()>          indices;
  std::array<il2cpp::GcHandle, kShipBadgeActions.size()> fleet_roots;
  indices.fill(-1);
  const auto count = std::min(fleets.value->size(), indices.size());
  for (size_t position = 0; position < count; ++position) {
    auto* observed = (*fleets.value)[position];
    if (!observed)
      continue;
    fleet_roots[position] = il2cpp::GcHandle::Strong(observed);
    if (!fleet_roots[position])
      return {};
  }
  for (size_t position = 0; position < count; ++position) {
    auto* fleet = fleet_roots[position].Get<FleetPlayerData>();
    if (!fleet)
      continue;
    const auto native_index = fleet->TryCall<"get_Index", int32_t>();
    if (!native_index)
      return {};
    indices[position] = native_index.value;
  }
  for (size_t position = 0; position < count; ++position)
    if ((*fleets.value)[position] != fleet_roots[position].Get())
      return {};
  const auto current_context = bar->TryCall<"get_CanvasContext", FleetBarContext*>();
  const auto current_array   = context->TryCall<"get_FleetData", il2cpp::ArrayObject*>();
  if (!current_context || current_context.value != context || !current_array || current_array.value != array_root.Get()
      || unity::Newest<FleetBarViewController>() != bar)
    return {};
  const auto action = ShipShortcutHintAction(kShipBadgeActions[static_cast<size_t>(index)], count,
                                             [&indices](size_t position) { return indices[position]; });
  return action == Action::kCount ? std::string{} : Shortcut::Hint(action);
}

void UpdateShipBadge(NavigationFleetWidget* key)
{
  auto& state = ShipBadges();
  auto  found = state.badges.find(key);
  if (found == state.badges.end())
    return;
  const auto generation = found->second.generation;
  const auto fleet_id   = found->second.fleet;
  const auto index      = found->second.index;
  auto       source     = il2cpp::GcHandle::Strong(found->second.widget.Get());
  auto*      widget     = source.Get<NavigationFleetWidget>();
  if (!widget || !unity::Alive(widget)) {
    found = state.badges.find(key);
    if (found != state.badges.end() && found->second.generation == generation)
      state.badges.erase(found);
    return;
  }
  if (fleet_id <= 0)
    return;
  const auto hint = ShipBadgeHint(index);
  found           = state.badges.find(key);
  if (found == state.badges.end() || found->second.generation != generation || hint == found->second.applied)
    return;
  const auto context     = widget->TryCall<"get_Context", FleetDeployedData*>();
  auto       fleet_root  = il2cpp::GcHandle::Strong(context ? context.value : nullptr);
  auto*      fleet       = fleet_root.Get<FleetDeployedData>();
  const auto observed_id = fleet ? fleet->TryId() : il2cpp::CallResult<int64_t>{};
  found                  = state.badges.find(key);
  if (!observed_id || observed_id.value != fleet_id || found == state.badges.end()
      || found->second.generation != generation)
    return;
  const auto label       = widget->TryFleetLevel();
  auto       label_root  = il2cpp::GcHandle::Strong(label ? label.value : nullptr);
  auto*      localizer   = label_root.Get<TextLocalizer>();
  const bool label_alive = localizer && unity::Alive(localizer);
  found                  = state.badges.find(key);
  if (!label_alive || found == state.badges.end() || found->second.generation != generation)
    return;
  auto label_weak = il2cpp::GcHandle::Weak(localizer);
  if (!label_weak)
    return;
  found->second.label = std::move(label_weak);
  if (!hint.empty())
    found->second.may_override = true;
  const auto written = hint.empty() ? localizer->TryCall<"ClearTextOverride", void>()
                                    : localizer->TryCall<"OverrideLocalizedText", void>(il2cpp::NewString(hint));
  found              = state.badges.find(key);
  if (written && found != state.badges.end() && found->second.generation == generation) {
    found->second.applied      = hint;
    found->second.may_override = !hint.empty();
  }
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
  const bool overridden = found->second.may_override || !found->second.applied.empty();
  auto       source     = il2cpp::GcHandle::Strong(found->second.label.Get());
  state.badges.erase(found);
  if (auto* localizer = source.Get<TextLocalizer>(); overridden && localizer && unity::Alive(localizer))
    (void)localizer->TryCall<"ClearTextOverride", void>();
}

void Bind(NavigationFleetWidget* widget)
{
  auto& state = ShipBadges();
  if (!state.installed)
    return;
  Release(widget);
  // Reserve this bind before managed reads. A nested bind/release retires its
  // generation, so this older operation cannot replace the new record.
  if (state.badges.contains(widget))
    return;
  auto owner = il2cpp::GcHandle::Strong(widget);
  auto weak  = owner ? il2cpp::GcHandle::Weak(widget) : il2cpp::GcHandle{};
  if (!weak)
    return;
  const auto generation = ++state.generation;
  state.badges.insert_or_assign(widget, ShipBadge{std::move(weak), {}, 0, -1, generation, {}});
  const auto current = [&] {
    const auto found = state.badges.find(widget);
    return found != state.badges.end() && found->second.generation == generation;
  };
  const auto context      = widget->TryCall<"get_Context", FleetDeployedData*>();
  auto       fleet_root   = il2cpp::GcHandle::Strong(context ? context.value : nullptr);
  auto*      fleet        = fleet_root.Get<FleetDeployedData>();
  const auto local        = fleet ? fleet->TryIsLocalPlayer() : il2cpp::CallResult<bool>{};
  const auto id           = fleet ? fleet->TryId() : il2cpp::CallResult<int64_t>{};
  auto       manager_root = il2cpp::GcHandle::Strong(id ? FleetsManager::Instance() : nullptr);
  auto*      manager      = manager_root.Get<FleetsManager>();
  const auto index        = manager ? manager->TryPlayerFleetIndex(id.value) : il2cpp::CallResult<int32_t>{};
  if (!current())
    return;
  if (!local || !local.value || !id || id.value <= 0 || !index || index.value < 0
      || static_cast<size_t>(index.value) >= kShipBadgeActions.size()) {
    state.badges.erase(widget);
    return;
  }
  auto& badge = state.badges.at(widget);
  badge.fleet = id.value;
  badge.index = index.value;
  UpdateShipBadge(widget);
}
} // namespace ship_shortcut_badges

const Feature kShipShortcutBadges{"ShipShortcutBadges", &Config::Patches::zoomhooks, InstallShipShortcutBadges,
                                  ShipBadgeFrame};
