// Streamer Mode: separate Player ID view and HUD resource balances.
#include "game/streamer_account.h"
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"

#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace
{
using streamer_mode::SurfaceCover;
using streamer_mode::game::GenericHudBalance;
using streamer_mode::game::HudResourceBalance;
using streamer_mode::game::PlayerIdDisplay;
using streamer_mode::game::ResourceAmount;
using streamer_mode::game::ResourceDropdown;
using streamer_mode::game::ResourceDropdownList;
using streamer_mode::game::ShopShowcaseBalance;

struct StreamerAccountTarget {
  unity::GameObject* object   = nullptr;
  bool               observed = true;
};

using StreamerAccountTargets        = std::array<StreamerAccountTarget, 5>;
using CollectStreamerAccountTargets = StreamerAccountTargets (*)(il2cpp::Object*);

struct StreamerAccountRow {
  il2cpp::GcHandle                           owner;
  CollectStreamerAccountTargets              collect = nullptr;
  std::map<unity::GameObject*, SurfaceCover> covers;
  bool                                       complete = true;
};

struct StreamerAccountViews {
  std::vector<std::shared_ptr<StreamerAccountRow>> rows;
  Throttle                                         review{std::chrono::milliseconds(250)};
  bool                                             installed = false;
  bool                                             hidden    = false;
  bool                                             reported  = false;
  bool                                             untracked = false;
  bool                                             pending   = false;
};

StreamerAccountViews& StreamerIdViews()
{
  static auto* views = new StreamerAccountViews;
  return *views;
}
StreamerAccountViews& StreamerBalanceViews()
{
  static auto* views = new StreamerAccountViews;
  return *views;
}

StreamerAccountTarget ObserveStreamerAccountObject(unity::GameObject* object)
{
  const auto alive = unity::TryAlive(object);
  return {alive.value ? object : nullptr, alive.succeeded};
}

StreamerAccountTarget ObserveStreamerAccountTarget(il2cpp::Object* component)
{
  if (!component)
    return {};
  auto root = il2cpp::GcHandle::Strong(component);
  if (!root)
    return {nullptr, false};
  const auto alive = unity::TryAlive(root.Get());
  if (!alive || !alive.value)
    return {nullptr, alive.succeeded};
  const auto object = component->TryCall<"get_gameObject", unity::GameObject*>();
  return object && object.value ? ObserveStreamerAccountObject(object.value) : StreamerAccountTarget{nullptr, false};
}

std::array<StreamerAccountTarget, 2> ObserveStreamerResourceAmount(ResourceAmount* resource)
{
  const auto alive = unity::TryAlive(resource);
  if (!alive || !alive.value)
    return {StreamerAccountTarget{nullptr, alive.succeeded}, StreamerAccountTarget{nullptr, alive.succeeded}};
  // The native ResourceWidget can display its amount through either of these two optional renderers.
  return {ObserveStreamerAccountTarget(resource->Quantity()), ObserveStreamerAccountTarget(resource->CostText())};
}

std::array<StreamerAccountTarget, 2> ObserveStreamerResourceDropdown(ResourceDropdown* dropdown)
{
  const auto dropdown_alive = unity::TryAlive(dropdown);
  if (!dropdown_alive || !dropdown_alive.value)
    return {StreamerAccountTarget{nullptr, dropdown_alive.succeeded},
            StreamerAccountTarget{nullptr, dropdown_alive.succeeded}};
  auto*      list       = dropdown->List();
  const auto list_alive = unity::TryAlive(list);
  if (!list_alive || !list_alive.value)
    return {StreamerAccountTarget{nullptr, list_alive.succeeded}, StreamerAccountTarget{nullptr, list_alive.succeeded}};
  return {ObserveStreamerAccountTarget(list->Canvas()), ObserveStreamerAccountTarget(list->Content())};
}

StreamerAccountTargets CollectStreamerIdTargets(il2cpp::Object* owner)
{ return {ObserveStreamerAccountTarget(static_cast<PlayerIdDisplay*>(owner)->Label())}; }

StreamerAccountTargets CollectStreamerHudTargets(il2cpp::Object* owner)
{
  const auto [quantity, cost] = ObserveStreamerResourceAmount(static_cast<HudResourceBalance*>(owner)->Resource());
  return {quantity, cost};
}

StreamerAccountTargets CollectStreamerGenericHudTargets(il2cpp::Object* owner)
{
  auto* widget                 = static_cast<GenericHudBalance*>(owner);
  const auto [quantity, cost]  = ObserveStreamerResourceAmount(widget->HardResource());
  const auto [canvas, content] = ObserveStreamerResourceDropdown(widget->Dropdown());
  return {quantity, cost, ObserveStreamerAccountTarget(widget->SoftResources()), canvas, content};
}

StreamerAccountTargets CollectStreamerShopTargets(il2cpp::Object* owner)
{ return {ObserveStreamerAccountObject(static_cast<ShopShowcaseBalance*>(owner)->Resources())}; }

bool RestoreStreamerAccountRow(StreamerAccountRow& row)
{
  for (auto it = row.covers.begin(); it != row.covers.end();) {
    if (it->second.Restore())
      it = row.covers.erase(it);
    else
      ++it;
  }
  return row.covers.empty();
}

bool ApplyStreamerAccountRow(StreamerAccountRow& row, bool hidden)
{
  auto* observed_owner = row.owner.Get();
  if (!hidden || !observed_owner)
    return RestoreStreamerAccountRow(row);
  auto       owner = il2cpp::GcHandle::Strong(observed_owner);
  const auto alive = owner ? unity::TryAlive(owner.Get()) : il2cpp::CallResult<bool>{};
  if (!alive)
    return false;
  if (!alive.value) {
    row.owner.Reset();
    return RestoreStreamerAccountRow(row);
  }

  const auto targets = row.collect(owner.Get<il2cpp::Object>());
  bool       applied = std::ranges::all_of(targets, &StreamerAccountTarget::observed);
  for (const auto& target : targets)
    if (target.observed && target.object)
      applied &= row.covers[target.object].Set(target.object, true);
  if (!applied)
    return false;
  for (auto it = row.covers.begin(); it != row.covers.end();) {
    if (std::ranges::contains(targets, it->first, &StreamerAccountTarget::object)) {
      ++it;
    } else if (it->second.Restore()) {
      it = row.covers.erase(it);
    } else {
      applied = false;
      ++it;
    }
  }
  return applied;
}

bool StreamerAccountDegraded(const StreamerAccountViews& views)
{ return views.pending || (views.hidden && views.untracked); }

void ReportStreamerAccountCoverage(StreamerAccountViews& views)
{
  views.pending = std::ranges::any_of(views.rows, [](const auto& row) { return !row->complete; });
  if (StreamerAccountDegraded(views) && !std::exchange(views.reported, true))
    spdlog::warn("[StreamerAccountBalances] could not verify all presentation masks");
}

void UpdateStreamerAccountRow(StreamerAccountViews& views, il2cpp::Object* owner, CollectStreamerAccountTargets collect,
                              bool hidden)
{
  std::erase_if(views.rows, [](const auto& row) { return !row->owner.Get() && RestoreStreamerAccountRow(*row); });
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  const auto row   = found == views.rows.end() ? std::make_shared<StreamerAccountRow>() : *found;
  if (found == views.rows.end()) {
    row->owner   = il2cpp::GcHandle::Weak(owner);
    row->collect = collect;
    if (!row->owner) {
      views.untracked = true;
      ReportStreamerAccountCoverage(views);
      return;
    }
    views.rows.push_back(row);
  }
  row->complete = ApplyStreamerAccountRow(*row, hidden);
  ReportStreamerAccountCoverage(views);
}

void ReleaseStreamerAccountRow(StreamerAccountViews& views, il2cpp::Object* owner)
{
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  if (found == views.rows.end())
    return;
  const auto row = *found;
  row->owner.Reset();
  if (RestoreStreamerAccountRow(*row)) {
    views.rows.erase(found);
  } else {
    row->complete = false;
  }
  ReportStreamerAccountCoverage(views);
}

void PlayerIdDisplay_SetPlayerIdToText_Hook(auto original, PlayerIdDisplay* _this, bool enabled)
{
  UpdateStreamerAccountRow(StreamerIdViews(), _this, CollectStreamerIdTargets, streamer_mode::HideAccount());
  original(_this, enabled);
}
void PlayerIdDisplay_OnDisable_Hook(auto original, PlayerIdDisplay* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerIdViews(), _this);
}

void HudResourceBalance_SetWidgetData_Hook(auto original, HudResourceBalance* _this)
{
  original(_this);
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, CollectStreamerHudTargets, streamer_mode::HideBalances());
}
void HudResourceBalance_OnAboutToReleaseContext_Hook(auto original, HudResourceBalance* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerBalanceViews(), _this);
}

void GenericHudBalance_SetWidgetData_Hook(auto original, GenericHudBalance* _this)
{
  original(_this);
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, CollectStreamerGenericHudTargets,
                           streamer_mode::HideBalances());
}
void GenericHudBalance_OnAboutToReleaseContext_Hook(auto original, GenericHudBalance* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerBalanceViews(), _this);
}

void ShopShowcaseBalance_OnDidBindCanvasContext_Hook(auto original, ShopShowcaseBalance* _this)
{
  original(_this);
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, CollectStreamerShopTargets, streamer_mode::HideBalances());
}
void ShopShowcaseBalance_OnAboutToReleaseCanvasContext_Hook(auto original, ShopShowcaseBalance* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerBalanceViews(), _this);
}

template <class View, class Signature> auto StreamerAccountEntry(const char* method)
{
  const auto binding = View::Class::template Method<Signature>(method);
  return binding && binding.Info()->klass == View::Class::Raw() ? il2cpp::CheckedEntry(binding) : nullptr;
}

bool InstallStreamerIdDisplay()
{
  if (!PlayerIdDisplay::Class::HasField<"_textLocalizer", TextLocalizer*>())
    return false;
  auto* set     = StreamerAccountEntry<PlayerIdDisplay, void(bool)>("SetPlayerIdToText");
  auto* disable = StreamerAccountEntry<PlayerIdDisplay, void()>("OnDisable");
  if (!set || !disable || set == disable)
    return false;
  il2cpp::HookSet hooks("StreamerIdDisplay");
  IL2CPP_STAGE_AT(hooks, set, PlayerIdDisplay_SetPlayerIdToText_Hook);
  IL2CPP_STAGE_AT(hooks, disable, PlayerIdDisplay_OnDisable_Hook);
  return hooks.Commit();
}

bool InstallStreamerBalances()
{
  if (!ResourceAmount::Class::HasField<"_quantityLocalizer", TextLocalizer*>()
      || !ResourceAmount::Class::HasField<"_costTextRenderer", unity::TMP_Text*>()
      || !HudResourceBalance::Class::HasField<"_resourceWidget", ResourceAmount*>()
      || !GenericHudBalance::Class::HasField<"_hardResource", ResourceAmount*>()
      || !GenericHudBalance::Class::HasField<"_softResources", il2cpp::Object*>()
      || !GenericHudBalance::Class::HasField<"_resourcesListDropdown", ResourceDropdown*>()
      || !ResourceDropdown::Class::HasField<"_dropdown", ResourceDropdownList*>()
      || !ResourceDropdownList::Class::HasField<"_dropdownCanvas", unity::Canvas*>()
      || !ResourceDropdownList::Class::HasField<"_content", unity::RectTransform*>())
    return false;
  auto*            hud       = StreamerAccountEntry<HudResourceBalance, void()>("SetWidgetData");
  auto*            hud_clear = StreamerAccountEntry<HudResourceBalance, void()>("OnAboutToReleaseContext");
  auto*            generic   = StreamerAccountEntry<GenericHudBalance, void()>("SetWidgetData");
  auto*            clear     = StreamerAccountEntry<GenericHudBalance, void()>("OnAboutToReleaseContext");
  const std::array entries{hud, hud_clear, generic, clear};
  for (auto it = entries.begin(); it != entries.end(); ++it)
    if (!*it || std::find(entries.begin(), it, *it) != it)
      return false;
  il2cpp::HookSet hooks("StreamerBalances");
  IL2CPP_STAGE_AT(hooks, hud, HudResourceBalance_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, hud_clear, HudResourceBalance_OnAboutToReleaseContext_Hook);
  IL2CPP_STAGE_AT(hooks, generic, GenericHudBalance_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, clear, GenericHudBalance_OnAboutToReleaseContext_Hook);
  return hooks.Commit();
}

bool InstallStreamerShopBalance()
{
  if (!ShopShowcaseBalance::Class::HasField<"_resourcesContainer", unity::GameObject*>())
    return false;
  auto* bind    = StreamerAccountEntry<ShopShowcaseBalance, void()>("OnDidBindCanvasContext");
  auto* release = StreamerAccountEntry<ShopShowcaseBalance, void()>("OnAboutToReleaseCanvasContext");
  if (!bind || !release || bind == release)
    return false;
  il2cpp::HookSet hooks("StreamerShopBalance");
  IL2CPP_STAGE_AT(hooks, bind, ShopShowcaseBalance_OnDidBindCanvasContext_Hook);
  IL2CPP_STAGE_AT(hooks, release, ShopShowcaseBalance_OnAboutToReleaseCanvasContext_Hook);
  return hooks.Commit();
}

bool InstallStreamerAccountBalances()
{
  if (!unity::CanvasGroup::Available())
    return false;
  StreamerIdViews().installed      = InstallStreamerIdDisplay();
  StreamerBalanceViews().installed = InstallStreamerBalances();
  if (StreamerBalanceViews().installed)
    InstallStreamerShopBalance();
  return StreamerIdViews().installed || StreamerBalanceViews().installed;
}

void RefreshStreamerAccountViews(StreamerAccountViews& views, bool hidden)
{
  if (!views.installed)
    return;
  const bool changed = std::exchange(views.hidden, hidden) != hidden;
  if (!changed && (!views.pending || !views.review.Due()))
    return;
  const auto rows = views.rows;
  for (const auto& row : rows)
    if (changed || !row->complete)
      row->complete = ApplyStreamerAccountRow(*row, hidden);
  std::erase_if(views.rows, [](const auto& row) { return !row->owner.Get() && row->complete; });
  ReportStreamerAccountCoverage(views);
}

void StreamerAccountBalancesFrame()
{
  RefreshStreamerAccountViews(StreamerIdViews(), streamer_mode::HideAccount());
  RefreshStreamerAccountViews(StreamerBalanceViews(), streamer_mode::HideBalances());
}
} // namespace

bool streamer_mode::BalancesAvailable()
{ return StreamerBalanceViews().installed && FrameDeliveryAvailable(); }

bool streamer_mode::BalancesDegraded()
{ return StreamerAccountDegraded(StreamerBalanceViews()); }

const Feature kStreamerAccountBalances{"StreamerAccountBalances", &Config::Patches::streamermodehooks,
                                       InstallStreamerAccountBalances, StreamerAccountBalancesFrame};
