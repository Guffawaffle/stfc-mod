// Reward progress shown in the native item description.
#include "config.h"
#include "features/features.h"
#include "game/reward_upgrade_progress.h"
#include "game/ship_tech.h"

#include <il2cpp/hook.h>
#include <spdlog/fmt/fmt.h>

#include <cmath>
#include <string>

namespace
{
namespace reward_store    = alliance_store_game;
namespace reward_upgrades = reward_upgrade_progress;
reward_store::TooltipBodyFields reward_popup_body;
// Retain weak catalog handles until process exit without invoking IL2CPP after shutdown.
auto& reward_refit_index    = *new reward_upgrades::RefitIndex;
auto& reward_building_index = *new reward_upgrades::BuildingMaterialIndex;

InventoryItem* RewardItemForPopup(StoreTooltipTrigger* trigger, reward_store::Roots& roots)
{
  if (!trigger || !roots.healthy)
    return nullptr;
  if (auto* type = StoreInventoryTooltip::Class::TypeObject()) {
    auto* widget = roots.Keep(trigger->Call<"GetComponentInParent", StoreInventoryTooltip*>(type));
    if (widget
        && (widget->Field<"_tooltipTrigger", StoreTooltipTrigger*>() == trigger
            || widget->Field<"_tooltipActionTrigger", StoreTooltipAction*>() == trigger))
      return roots.Keep(widget->Context());
  }
  if (auto* type = reward_store::InventoryBaseWidget::Class::TypeObject(); type && roots.healthy) {
    auto* widget = roots.Keep(trigger->Call<"GetComponentInParent", reward_store::InventoryBaseWidget*>(type));
    if (widget && widget->Tooltip() == trigger)
      return roots.Keep(widget->Context());
  }
  return nullptr;
}

std::string RewardCount(ProgressData* progress)
{
  if (!progress)
    return {};
  const auto current = progress->TryCall<"get_CurrentValue", double()>();
  const auto maximum = progress->TryCall<"get_MaxValue", double()>();
  if (!current || !maximum || !std::isfinite(current.value) || !std::isfinite(maximum.value) || current.value < 0
      || maximum.value <= 0 || std::floor(current.value) != current.value || std::floor(maximum.value) != maximum.value)
    return {};
  return fmt::format("{:.0f}/{:.0f}", current.value, maximum.value);
}

std::string BuildingRewardStatus(const reward_store::RewardIdentity& reward, reward_store::Roots& roots)
{
  using reward_store::RewardResourceSubtype;
  if (reward.resource_id <= 0 || reward.resource_subtype == RewardResourceSubtype::Unknown
      || reward.resource_subtype == RewardResourceSubtype::Soft
      || reward.resource_subtype == RewardResourceSubtype::Hard)
    return {};
  const auto catalog = reward_building_index.Current();
  if (!catalog)
    return {};
  const auto entry = catalog->find(reward.resource_id);
  if (entry == catalog->end())
    return {};
  const auto&      modules          = entry->second;
  constexpr size_t kMaxBuildingRows = 3;
  std::string      rows;
  size_t           shown = 0;
  if (modules.size() <= kMaxBuildingRows) {
    auto* service = reward_upgrades::ExistingService<reward_upgrades::StarbaseService>(roots);
    for (const auto& use : modules) {
      const auto progress = reward_upgrades::ReadBuildingProgress(service, use.module_id, reward.resource_id, roots);
      if (!progress && modules.size() > 1)
        continue;
      const auto name = reward_upgrades::BuildingName(use.module_id, roots);
      if (name.empty())
        continue;
      if (!progress)
        return fmt::format("Used by {}", name);
      if (!rows.empty())
        rows += '\n';
      rows += fmt::format("{} · Lv {}/{}", name, progress->level, progress->max_level);
      if (progress->maxed)
        rows += " · Maxed";
      if (progress->amounts)
        rows += fmt::format(" · {}/{}", progress->amounts->owned, progress->amounts->required);
      else if (use.levels.back() <= progress->max_level) {
        if (const auto next = use.NextLevelAfter(progress->level))
          rows += fmt::format(" · Next used at Lv {}", *next);
        else
          rows += fmt::format(" · Used through Lv {}", use.levels.back());
      }
      ++shown;
    }
  }
  if (shown < modules.size()) {
    if (!rows.empty())
      rows += '\n';
    rows += fmt::format("Used by {} station {}", modules.size(), modules.size() == 1 ? "building" : "buildings");
  }
  return rows;
}

std::string RewardPopupStatus(InventoryItem* item, reward_store::Roots& roots)
{
  using reward_store::RewardResourceSubtype;
  using reward_store::RewardType;
  const auto reward = reward_store::IdentifyReward(item, roots);
  if (reward.type == RewardType::Blueprint || reward.type == RewardType::Officer
      || reward.type == RewardType::OfficerShard) {
    auto*      progress = roots.Keep(item->Call<"GetProgress", ProgressData*(reward_upgrades::InventoryView)>(
        reward_upgrades::InventoryView::OwnedShards));
    const auto count    = RewardCount(progress);
    return count.empty() ? count : count + (reward.type == RewardType::Blueprint ? " blueprints" : " shards");
  }
  if (reward.type == RewardType::TechShard || reward.type == RewardType::Tech) {
    auto*      tech   = roots.Keep(item->Call<"get_ForbiddenTech", ForbiddenTech*()>());
    const auto locked = tech ? tech->TryCall<"get_IsLocked", bool()>() : il2cpp::CallResult<bool>{};
    if (!locked)
      return {};
    if (!locked.value)
      return "Unlocked";
    auto*      spec     = roots.Keep(tech->Call<"get_ForbiddenTechSpec", ship_tech_binding::TechSpec*()>());
    const auto owned    = tech->TryCall<"get_ShardCount", int32_t()>();
    const auto required = spec ? spec->TryCall<"get_ShardsToUnlock", int32_t()>() : il2cpp::CallResult<int32_t>{};
    if (owned && required && owned.value >= 0 && required.value > 0)
      return fmt::format("{}/{} shards", owned.value, required.value);
    return {};
  }
  // Some cloak and refit shards are marked Material. The ShipCosmetics catalog
  // identifies the unlock without relying on the resource subtype alone.
  if (reward.IsCosmeticShard() || (!reward.artifact && reward.resource_subtype == RewardResourceSubtype::Material)) {
    const auto progress = reward_upgrades::ReadRefitProgress(reward.resource_id, reward_refit_index, roots);
    if (progress)
      return progress->unlocked ? "Unlocked"
                                : fmt::format("{}/{} shards", progress->shards.owned, progress->shards.required);
    if (reward.IsCosmeticShard())
      return {};
  }
  auto* artifact = reward.artifact;
  if (!artifact)
    return BuildingRewardStatus(reward, roots);
  const auto level     = artifact->TryCall<"get_CurrentLevel", int32_t()>();
  const auto max_level = artifact->TryCall<"get_MaxLevel", int32_t()>();
  const auto maxed     = artifact->TryCall<"get_IsMaxLevel", bool()>();
  const auto dust      = artifact->TryCall<"get_DoesNextLevelRequireDust", bool()>();
  if (!level || !max_level || !maxed || !dust || level.value < 0 || max_level.value <= 0)
    return {};
  auto text = fmt::format("Lv {}/{}", level.value, max_level.value);
  if (maxed.value)
    return text + " · Maxed";
  if (dust.value)
    return text + " · No shard upgrade available";
  auto*      progress = roots.Keep(artifact->Call<"get_CurrentLevelProgress", ProgressData*()>());
  const auto count    = RewardCount(progress);
  return count.empty() ? text : text + " · " + count + (level.value == 0 ? " shards to unlock" : " shards");
}

void TooltipWidget_OnDidBindContext_Hook(auto original, reward_store::TooltipWidget* _this)
{
  reward_store::Roots roots;
  auto*               popup   = _this->Is<reward_store::TooltipWidget>() ? roots.Keep(_this) : nullptr;
  auto*               trigger = popup ? reward_store::Trigger(popup, roots) : nullptr;
  auto*               item    = RewardItemForPopup(trigger, roots);
  auto                status  = item && roots.healthy ? RewardPopupStatus(item, roots) : std::string{};
  if (status.empty() || !roots.healthy) {
    original(_this);
    return;
  }

  const auto& fields            = reward_popup_body;
  const bool  previous_override = fields.override_locale.Read(trigger);
  auto*       previous          = roots.Keep(fields.description_override.Read(trigger));
  const auto  body              = reward_store::Description(trigger, roots, fields);
  if (body && body.value) {
    const auto description = il2cpp::ToUtf8(body.value);
    if (!description.empty())
      status += "\n\n" + description;
  }
  auto* text = body && roots.healthy ? roots.Keep(il2cpp::NewString(status)) : nullptr;
  if (!text || !roots.healthy || reward_store::Trigger(popup, roots) != trigger
      || RewardItemForPopup(trigger, roots) != item || !roots.healthy
      || fields.description_override.Read(trigger) != previous
      || fields.override_locale.Read(trigger) != previous_override) {
    original(_this);
    return;
  }

  // Use the native body layout. The trigger's fields are borrowed only during
  // the synchronous render, so recycled cards keep their own descriptions.
  struct RestoreBody {
    const reward_store::TooltipBodyFields& fields;
    StoreTooltipTrigger*                   trigger;
    Il2CppString*                          text;
    Il2CppString*                          previous;
    bool                                   previous_override;
    ~RestoreBody()
    {
      if (fields.description_override.Read(trigger) == text) {
        fields.description_override.Write(trigger, previous);
        fields.override_locale.Write(trigger, previous_override);
      }
    }
  };
  {
    const RestoreBody restore{fields, trigger, text, previous, previous_override};
    if (fields.description_override.Write(trigger, text) && fields.override_locale.Write(trigger, true)) {
      original(_this);
      return;
    }
  }
  original(_this);
}

bool InstallRewardTooltipProgress()
{
  auto* trigger = StoreTooltipTrigger::Class::Raw();
  if (!trigger || !reward_popup_body.Resolve(trigger))
    return false;
  using Popup           = reward_store::TooltipWidget::Class;
  const auto      bind  = Popup::Method<void()>("OnDidBindContext");
  const auto      entry = bind && bind.Info()->klass == Popup::Raw() ? il2cpp::CheckedEntry(bind) : nullptr;
  il2cpp::HookSet hooks("RewardTooltipProgress");
  IL2CPP_STAGE_AT(hooks, entry, TooltipWidget_OnDidBindContext_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kRewardTooltipProgress{"RewardTooltipProgress", &Config::Patches::rewardtooltipprogresshooks,
                                     InstallRewardTooltipProgress};
