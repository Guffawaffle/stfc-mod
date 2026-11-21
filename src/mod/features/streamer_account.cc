// Streamer Mode: separate Player ID view and HUD resource balances.
#include "game/streamer_account.h"
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"

#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <memory>
#include <spdlog/spdlog.h>
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

struct StreamerAccountRow {
  il2cpp::GcHandle                owner;
  std::array<il2cpp::GcHandle, 4> targets;
  std::array<SurfaceCover, 4>     covers;
  bool                            observed = true;
};

struct StreamerAccountViews {
  std::vector<std::shared_ptr<StreamerAccountRow>> rows;
  bool                                             installed = false;
  bool                                             healthy   = true;
  bool                                             hidden    = false;
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

StreamerAccountTarget ObserveStreamerAccountTarget(il2cpp::Object* component, bool required = true)
{
  if (!component)
    return {nullptr, !required};
  auto root = il2cpp::GcHandle::Strong(component);
  if (!root)
    return {nullptr, false};
  const auto object = component->TryCall<"get_gameObject", unity::GameObject*>();
  return {object.value, object.succeeded && object.value != nullptr};
}

bool ApplyStreamerAccountRow(const std::shared_ptr<StreamerAccountRow>& row, bool hidden)
{
  bool applied = !hidden || row->observed;
  if (!hidden) {
    for (auto& cover : row->covers)
      applied &= cover.Restore();
    return applied;
  }
  for (size_t index = 0; index < row->targets.size(); ++index) {
    auto* observed = row->targets[index].Get();
    auto  target   = il2cpp::GcHandle::Strong(observed);
    if (target)
      applied &= row->covers[index].Set(target.Get<unity::GameObject>(), true);
    else if (!observed)
      applied &= row->covers[index].Restore();
    else
      applied = false;
  }
  return applied;
}

void CheckStreamerAccountCover(StreamerAccountViews& views, bool applied)
{
  if (applied || !views.healthy)
    return;
  views.healthy = false;
  spdlog::warn("[StreamerAccountBalances] could not apply a presentation mask");
}

void UpdateStreamerAccountRow(StreamerAccountViews& views, il2cpp::Object* owner, bool hidden,
                              const std::array<StreamerAccountTarget, 4>& targets)
{
  auto owner_root = il2cpp::GcHandle::Strong(owner);
  if (!owner_root) {
    CheckStreamerAccountCover(views, !hidden);
    return;
  }
  for (auto it = views.rows.begin(); it != views.rows.end();) {
    if ((*it)->owner.Get() || !ApplyStreamerAccountRow(*it, false)) {
      ++it;
      continue;
    }
    it = views.rows.erase(it);
  }
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  const auto row   = found == views.rows.end() ? std::make_shared<StreamerAccountRow>() : *found;
  if (found == views.rows.end()) {
    row->owner = il2cpp::GcHandle::Weak(owner);
    if (!row->owner) {
      CheckStreamerAccountCover(views, !hidden);
      return;
    }
    views.rows.push_back(row);
  }
  row->observed = true;
  for (size_t index = 0; index < targets.size(); ++index) {
    const auto& target = targets[index];
    auto        weak   = il2cpp::GcHandle::Weak(target.object);
    if (target.observed && (!target.object || weak))
      row->targets[index] = std::move(weak);
    else
      row->observed = false;
  }
  CheckStreamerAccountCover(views, ApplyStreamerAccountRow(row, hidden));
}

void ReleaseStreamerAccountRow(StreamerAccountViews& views, il2cpp::Object* owner)
{
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  if (found == views.rows.end())
    return;
  const auto row = *found;
  if (ApplyStreamerAccountRow(row, false)) {
    views.rows.erase(found);
  } else {
    row->owner.Reset();
    CheckStreamerAccountCover(views, false);
  }
}

void PlayerIdDisplay_SetPlayerIdToText_Hook(auto original, PlayerIdDisplay* _this, bool enabled)
{
  UpdateStreamerAccountRow(StreamerIdViews(), _this, streamer_mode::HideAccount(),
                           {ObserveStreamerAccountTarget(_this->Label())});
  original(_this, enabled);
}
void PlayerIdDisplay_OnDisable_Hook(auto original, PlayerIdDisplay* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerIdViews(), _this);
}

void HudResourceBalance_SetWidgetData_Hook(auto original, HudResourceBalance* _this)
{
  auto* resource = _this->Resource();
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, streamer_mode::HideBalances(),
                           {ObserveStreamerAccountTarget(resource ? resource->Quantity() : nullptr)});
  original(_this);
}
void HudResourceBalance_OnAboutToReleaseContext_Hook(auto original, HudResourceBalance* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerBalanceViews(), _this);
}

void GenericHudBalance_SetWidgetData_Hook(auto original, GenericHudBalance* _this)
{
  auto* hard     = _this->HardResource();
  auto* dropdown = _this->Dropdown();
  auto* list     = dropdown ? dropdown->List() : nullptr;
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, streamer_mode::HideBalances(),
                           {ObserveStreamerAccountTarget(hard ? hard->Quantity() : nullptr, hard != nullptr),
                            ObserveStreamerAccountTarget(_this->SoftResources(), false),
                            ObserveStreamerAccountTarget(list ? list->Canvas() : nullptr, dropdown != nullptr),
                            ObserveStreamerAccountTarget(list ? list->Content() : nullptr, dropdown != nullptr)});
  original(_this);
}
void GenericHudBalance_OnAboutToReleaseContext_Hook(auto original, GenericHudBalance* _this)
{
  original(_this);
  ReleaseStreamerAccountRow(StreamerBalanceViews(), _this);
}

void ShopShowcaseBalance_OnDidBindCanvasContext_Hook(auto original, ShopShowcaseBalance* _this)
{
  auto* resources = _this->Resources();
  UpdateStreamerAccountRow(StreamerBalanceViews(), _this, streamer_mode::HideBalances(), {{{resources, true}}});
  original(_this);
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
  if (!views.installed || views.hidden == hidden)
    return;
  views.hidden    = hidden;
  const auto rows = views.rows;
  for (const auto& row : rows)
    CheckStreamerAccountCover(views, ApplyStreamerAccountRow(row, hidden && row->owner.Get()));
}

void StreamerAccountBalancesFrame()
{
  RefreshStreamerAccountViews(StreamerIdViews(), streamer_mode::HideAccount());
  RefreshStreamerAccountViews(StreamerBalanceViews(), streamer_mode::HideBalances());
}
} // namespace

bool streamer_mode::BalancesAvailable()
{ return StreamerBalanceViews().installed && StreamerBalanceViews().healthy && FrameDeliveryAvailable(); }

const Feature kStreamerAccountBalances{"StreamerAccountBalances", &Config::Patches::streamermodehooks,
                                       InstallStreamerAccountBalances, StreamerAccountBalancesFrame};
