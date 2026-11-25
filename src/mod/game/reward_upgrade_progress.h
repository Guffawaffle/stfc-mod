#pragma once

#include "alliance_store_research.h"
#include "fleet.h"

#include <algorithm>
#include <limits>

namespace reward_upgrade_progress
{
namespace store = alliance_store_game;

struct ServiceRegistry : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Networking.Core", "GSServiceRegistry">;
};
struct RegisteredService : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Networking.Core", "IGSService">;
};
struct ResourcesService : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResourcesService">;
};
struct StarbaseService : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "StarbaseService">;
};
struct ResearchProject : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResearchProject">;
};
struct ModuleData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ModuleData">;
};
struct ModuleSpec : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StarbaseSpec">;
};
struct ModuleLevelSpec : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StarbaseSpec/Types/Level">;
};
struct ResourceData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceData">;
};
struct ResourceCostData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceCostData">;
};

template <typename Service> Service* ExistingService(store::Roots& roots)
{
  auto*           app       = roots.Keep(Hub::App());
  auto*           registry  = app ? roots.Keep(app->Call<"get_Services", ServiceRegistry*>()) : nullptr;
  const auto      resetting = registry ? registry->TryCall<"get_Resetting", bool()>() : il2cpp::CallResult<bool>{};
  il2cpp::Object* services =
      resetting && !resetting.value ? roots.Keep(registry->Field<"_services", ServerManagedReference*>()) : nullptr;
  auto*              type   = Service::Class::TypeObject();
  RegisteredService* result = nullptr;
  const auto         found =
      services && type
          ? services->TryCall<"TryGetValue", bool(Il2CppReflectionType*, RegisteredService*&)>(type, result)
          : il2cpp::CallResult<bool>{};
  return found && found.value && result ? roots.Keep(result->template As<Service>()) : nullptr;
}

enum class InventoryView : int32_t { OwnedShards = 3 };

struct Amounts {
  int64_t owned = 0, required = 0;
};

struct RefitUnlockCost {
  int64_t     project = 0, required = 0;
  friend bool operator==(RefitUnlockCost, RefitUnlockCost) = default;
};
using RefitCosts = std::unordered_map<int64_t, RefitUnlockCost>;

inline std::optional<RefitCosts> ReadRefitCosts(il2cpp::Object* trees, il2cpp::Object* projects)
{
  RefitCosts by_shard;
  const auto read = store::VisitResearchProjects(
      trees, projects, store::ResearchTree::ShipCosmetics,
      [&](int64_t project_id, store::ResearchProjectSpec* project) {
        const auto      observed = project->TryCall<"get_Levels", ServerManagedReference*>();
        il2cpp::Object* levels   = observed ? observed.value : nullptr;
        const auto      count    = levels ? levels->TryCall<"get_Count", int32_t()>() : il2cpp::CallResult<int32_t>{};
        if (!count || count.value < 0)
          return false;
        if (count.value != 1)
          return true;
        const auto level = levels->TryCall<"get_Item", store::ResearchProjectLevel*(int32_t)>(int32_t{0});
        return level && store::VisitResearchCosts(level.value, [&](store::ResearchCost* cost) {
                 if (!cost)
                   return false;
                 const auto id     = cost->TryCall<"get_ResourceId", int64_t()>();
                 const auto amount = cost->TryCall<"get_Value", int64_t()>();
                 if (id && amount && id.value > 0 && amount.value > 0) {
                   const RefitUnlockCost unlock{project_id, amount.value};
                   const auto [entry, inserted] = by_shard.try_emplace(id.value, unlock);
                   if (!inserted && entry->second != unlock)
                     entry->second = {};
                 }
                 return id && amount;
               });
      });
  return read && !by_shard.empty() ? std::optional(std::move(by_shard)) : std::nullopt;
}
using RefitIndex = store::ResearchCatalogIndex<RefitCosts, ReadRefitCosts>;

using ManagedActionType = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ActionType">;

template <typename Owner, typename Signature> const MethodInfo* UpgradeMethod(std::string_view name)
{
  const auto* method = il2cpp::detail::MethodCall<Signature>::Resolve(Owner::Class::Raw(), name, false);
  return method
                 && il2cpp::detail::ValueClassMatches(method->parameters[0], ManagedActionType::Raw(),
                                                      sizeof(ActionType))
             ? method
             : nullptr;
}

struct RefitProgress {
  bool    unlocked = false;
  Amounts shards;
};

inline std::optional<RefitProgress> ReadRefitProgress(int64_t resource_id, RefitIndex& index, store::Roots& roots)
{
  const auto catalog = index.Current();
  if (!catalog)
    return std::nullopt;
  const auto entry = catalog->find(resource_id);
  if (entry == catalog->end() || entry->second.project <= 0)
    return std::nullopt;
  auto* manager = roots.Keep(Hub::Class::Call<"get_ResearchManager", StoreResearchManager*>());
  auto* project =
      manager ? roots.Keep(manager->Call<"GetResearchProject", ResearchProject*(int64_t)>(entry->second.project))
              : nullptr;
  // The refit screen considers the one-level upgrade complete at Disabled_Maximum.
  constexpr int32_t  kDisabledMaximum = 5;
  static const auto* status_method    = UpgradeMethod<ResearchProject, int32_t(ActionType)>("GetActionStatus");
  const auto         upgrade          = ManagedActionType::Field<"Upgrade", ActionType>();
  const auto         status           = project && upgrade.flagValue && status_method
                                            ? il2cpp::detail::TryInvoke<int32_t>(status_method, project, upgrade)
                                            : il2cpp::CallResult<int32_t>{};
  if (!status || status.value < 0 || !roots.healthy)
    return std::nullopt;
  if (status.value == kDisabledMaximum)
    return RefitProgress{.unlocked = true};
  auto*         service = ExistingService<ResourcesService>(roots);
  ResourceData* data    = nullptr;
  const auto found = service ? service->TryCall<"TryGetResourceByID", bool(int64_t, ResourceData*&)>(resource_id, data)
                             : il2cpp::CallResult<bool>{};
  if (!found || (found.value && !roots.Keep(data)) || !roots.healthy)
    return std::nullopt;
  // The native balance getter uses zero when the resource is absent from the current inventory.
  const auto owned = found.value ? data->TryCall<"get_Amount", int64_t()>() : il2cpp::CallResult<int64_t>{true, 0};
  return owned && owned.value >= 0 ? std::optional(RefitProgress{false, {owned.value, entry->second.required}})
                                   : std::nullopt;
}

struct BuildingMaterialUse {
  int32_t              module_id;
  std::vector<int32_t> levels;

  std::optional<int32_t> NextLevelAfter(int32_t current) const
  {
    const auto next = std::ranges::upper_bound(levels, current);
    return next == levels.end() ? std::nullopt : std::optional(*next);
  }
};

using BuildingMaterials = std::unordered_map<int64_t, std::vector<BuildingMaterialUse>>;

inline std::optional<BuildingMaterials> ReadBuildingMaterials(il2cpp::Object* catalog, store::Roots& roots)
{
  if (!catalog)
    return std::nullopt;
  il2cpp::Object* keys  = roots.Keep(catalog->Call<"get_Keys", ServerManagedReference*>());
  const auto      count = keys ? keys->TryCall<"get_Count", int32_t()>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value <= 0)
    return std::nullopt;
  auto* element = il2cpp::PrimitiveClass<int64_t>();
  auto* ids = element ? roots.Keep(il2cpp::Array<int64_t>::New(element, static_cast<size_t>(count.value))) : nullptr;
  if (!ids || !roots.healthy || !keys->TryCall<"CopyTo", void(il2cpp::Array<int64_t>*, int32_t)>(ids, int32_t{0}))
    return std::nullopt;

  BuildingMaterials materials;
  for (const int64_t id : *ids) {
    if (id < 0 || id > std::numeric_limits<int32_t>::max())
      continue;
    ModuleSpec* module = nullptr;
    const auto  found  = catalog->TryCall<"TryGetValue", bool(int64_t, ModuleSpec*&)>(id, module);
    // The catalog can deserialize a spec into a weak cache on demand.
    auto module_root = il2cpp::GcHandle::Strong(found && found.value ? module : nullptr);
    if (!module_root || module_root.Get() != module)
      return std::nullopt;
    const auto section = module->TryCall<"get_Section", int32_t()>();
    if (!section)
      return std::nullopt;
    // The player's station owns the interior and exterior sections.
    if (section.value != 0 && section.value != 1)
      continue;
    const auto spec_id = module->TryCall<"get_Id", int64_t()>();
    if (!spec_id || spec_id.value != id)
      return std::nullopt;
    auto*      levels       = module->Call<"get_Levels", ServerManagedReference*>();
    int32_t    target_level = 0;
    const auto read         = store::VisitResearchList<ModuleLevelSpec*>(levels, [&](ModuleLevelSpec* level) {
      // ModuleData.GetLevelData uses Levels[level - 1], including levels with no costs.
      ++target_level;
      auto* costs = level ? level->Call<"get_Resources", ServerManagedReference*>() : nullptr;
      return store::VisitResearchList<store::ResearchCost*>(costs, [&](store::ResearchCost* cost) {
        if (!cost)
          return false;
        const auto resource_id = cost->TryCall<"get_ResourceId", int64_t()>();
        const auto amount      = cost->TryCall<"get_Value", int64_t()>();
        if (resource_id && amount && resource_id.value > 0 && amount.value > 0) {
          auto& modules = materials[resource_id.value];
          if (modules.empty() || modules.back().module_id != id)
            modules.push_back({static_cast<int32_t>(id), {}});
          auto& uses = modules.back().levels;
          if (uses.empty() || uses.back() != target_level)
            uses.push_back(target_level);
        }
        return resource_id && amount;
      });
    });
    if (!read)
      return std::nullopt;
  }
  for (auto& [resource, modules] : materials)
    std::ranges::sort(modules, {}, &BuildingMaterialUse::module_id);
  return materials.empty() ? std::nullopt : std::optional(std::move(materials));
}

struct BuildingMaterialIndex {
  il2cpp::GcHandle                         catalog;
  std::shared_ptr<const BuildingMaterials> materials;

  std::shared_ptr<const BuildingMaterials> Current()
  {
    store::Roots roots;
    // Inventory and Refinery can open before the UI SpecManager has fetched its service.
    auto* service = ExistingService<::SpecService>(roots);
    auto* data    = service ? roots.Keep(service->Field<"_dataContainer", store::StaticResearchData*>()) : nullptr;
    auto* specs   = data ? roots.Keep(data->Field<"StarbaseSpecs", ServerManagedReference*>()) : nullptr;
    if (!specs || !roots.healthy)
      return {};
    if (materials && catalog.Get() == specs)
      return materials;
    // LazyMapLoader supports a key snapshot and lookups, while its Values getter can throw.
    auto next = ReadBuildingMaterials(specs, roots);
    if (!next)
      return {};
    if (data->Field<"StarbaseSpecs", ServerManagedReference*>() != specs)
      return {};
    auto cache_key = il2cpp::GcHandle::Weak(specs);
    if (!cache_key)
      return {};
    catalog   = std::move(cache_key);
    materials = std::make_shared<const BuildingMaterials>(std::move(*next));
    return materials;
  }
};

struct BuildingProgress {
  int32_t                level = 0, max_level = 0;
  bool                   maxed = false;
  std::optional<Amounts> amounts;
};

inline std::string BuildingName(int32_t module_id, store::Roots& roots)
{
  auto  identifier = std::string("starbase_module_") + std::to_string(module_id) + "_name";
  auto* key        = roots.Keep(il2cpp::NewString(identifier));
  auto* category   = roots.Keep(il2cpp::NewString("starbase_modules"));
  auto* context =
      key && category && roots.healthy ? roots.Keep(il2cpp::New<LocaleTextContext>(key, category)) : nullptr;
  auto* localized = context ? roots.Keep(LocaleUtilities::Localize(context)) : nullptr;
  auto  name      = localized ? il2cpp::ToUtf8(localized) : std::string{};
  return name.find(identifier) == std::string::npos ? name : std::string{};
}

inline std::optional<BuildingProgress> ReadBuildingProgress(StarbaseService* service, int32_t module_id,
                                                            int64_t resource_id, store::Roots& roots)
{
  ModuleData* module = nullptr;
  const auto  found  = service ? service->TryCall<"TryGetModuleData", bool(int32_t, ModuleData*&)>(module_id, module)
                               : il2cpp::CallResult<bool>{};
  if (!found || !found.value || !roots.Keep(module))
    return std::nullopt;
  auto*      spec      = roots.Keep(module->Call<"get_ConfigData", ModuleSpec*>());
  const auto id        = spec ? spec->TryCall<"get_Id", int64_t()>() : il2cpp::CallResult<int64_t>{};
  const auto level     = module->TryCall<"get_CurrentLevel", int32_t()>();
  const auto max_level = module->TryCall<"get_MaxLevel", int32_t()>();
  const auto maxed     = module->TryCall<"get_IsMaxLevel", bool()>();
  if (!id || id.value != module_id || !level || !max_level || !maxed || !roots.healthy || level.value < 0
      || max_level.value <= 0 || level.value > max_level.value)
    return std::nullopt;
  BuildingProgress progress{level.value, max_level.value, maxed.value};
  if (progress.maxed)
    return progress;

  const auto         next    = module->TryCall<"get_NextLevel", int32_t()>();
  const auto         upgrade = ManagedActionType::Field<"Upgrade", ActionType>();
  static const auto* cost_method =
      UpgradeMethod<ModuleData, ServerManagedReference*(ActionType, int64_t)>("GetActionCost");
  if (!next || next.value <= level.value || !upgrade.flagValue || !cost_method)
    return progress;
  // Query the native single-level cost, independent of the currently selected bulk-upgrade target.
  il2cpp::Object* costs = roots.Keep(
      il2cpp::detail::Invoke<ServerManagedReference*>(cost_method, module, upgrade, static_cast<int64_t>(next.value)));
  if (!costs)
    return progress;
  std::optional<Amounts> amounts;
  const auto             read = store::VisitResearchList<ResourceCostData*>(costs, [&](ResourceCostData* cost) {
    auto*      required = cost ? roots.Keep(cost->Call<"get_Cost", ResourceData*>()) : nullptr;
    const auto cost_id  = required ? required->TryCall<"get_ID", int64_t()>() : il2cpp::CallResult<int64_t>{};
    if (cost_id && cost_id.value == resource_id) {
      auto*      available = roots.Keep(cost->Call<"get_Owned", ResourceData*>());
      const auto want      = required->TryCall<"get_Amount", int64_t()>();
      const auto have      = available ? available->TryCall<"get_Amount", int64_t()>() : il2cpp::CallResult<int64_t>{};
      if (!want || !have || want.value <= 0 || have.value < 0 || amounts)
        return false;
      amounts = Amounts{have.value, want.value};
    }
    return cost_id.succeeded && roots.healthy;
  });
  if (read)
    progress.amounts = amounts;
  return progress;
}
} // namespace reward_upgrade_progress
