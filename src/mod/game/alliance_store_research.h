#pragma once

#include "alliance_store.h"
#include "spec_service.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace alliance_store_game
{
struct StaticResearchData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "StaticSyncDataContainer">;
};
struct ResearchTreeSpec : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResearchTreeSpec">;
};
struct ResearchProjectSpec : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResearchProjectSpec">;
};
struct ResearchProjectLevel : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResearchProjectLevel">;
};
struct ResearchCost : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Resource">;
};

using ResearchResources = std::unordered_set<int64_t>;

// A rooted catalog owns this graph. Resolve collection methods on the concrete
// runtime classes after qualifying the references returned by the spec getters.
template <typename T, typename Visit> bool VisitResearchList(il2cpp::Object* list, Visit&& visit)
{
  const auto count = list ? list->TryCall<"get_Count", int32_t()>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return false;
  for (int32_t index = 0; index < count.value; ++index) {
    const auto entry = list->TryCall<"get_Item", T(int32_t)>(index);
    if (!entry || !visit(entry.value))
      return false;
  }
  return true;
}

template <typename Visit> bool VisitResearchCosts(ResearchProjectLevel* level, Visit&& visit)
{
  const auto costs =
      level ? level->TryCall<"get_Costs", ServerManagedReference*>() : il2cpp::CallResult<ServerManagedReference*>{};
  return costs && VisitResearchList<ResearchCost*>(costs.value, std::forward<Visit>(visit));
}

inline bool ReadResearchProjectCosts(ResearchProjectSpec* project, ResearchResources& resources)
{
  const auto levels = project->TryCall<"get_Levels", ServerManagedReference*>();
  return levels && VisitResearchList<ResearchProjectLevel*>(levels.value, [&](ResearchProjectLevel* level) {
           return VisitResearchCosts(level, [&](ResearchCost* cost) {
             const auto id = cost ? cost->TryCall<"get_ResourceId", int64_t()>() : il2cpp::CallResult<int64_t>{};
             if (id && id.value > 0)
               resources.insert(id.value);
             return id.succeeded;
           });
         });
}

template <typename Visit>
bool VisitResearchProjects(il2cpp::Object* trees, il2cpp::Object* projects, ResearchTree kind, Visit&& visit)
{
  const auto project_count = projects->TryCall<"get_Count", int32_t()>();
  if (!project_count || project_count.value <= 0)
    return false;
  return VisitResearchList<ResearchTreeSpec*>(trees, [&](ResearchTreeSpec* tree) {
    const auto type = tree ? tree->TryCall<"get_Type", ResearchTree()>() : il2cpp::CallResult<ResearchTree>{};
    if (!type)
      return false;
    if (type.value != kind)
      return true;
    const auto ids = tree->TryCall<"get_Projects", ServerManagedReference*>();
    return ids && VisitResearchList<int64_t>(ids.value, [&](int64_t id) {
             ResearchProjectSpec* project = nullptr;
             const auto found = projects->TryCall<"TryGetValue", bool(int64_t, ResearchProjectSpec*&)>(id, project);
             // The native tree can list a project with no spec in the current catalog.
             return found && (!found.value || (project && visit(id, project)));
           });
  });
}

inline std::optional<ResearchResources> ReadResearchResources(il2cpp::Object* trees, il2cpp::Object* projects)
{
  ResearchResources resources;
  const bool        read =
      VisitResearchProjects(trees, projects, ResearchTree::Standard,
                            [&](int64_t, ResearchProjectSpec* p) { return ReadResearchProjectCosts(p, resources); });
  return read && !resources.empty() ? std::optional(std::move(resources)) : std::nullopt;
}

template <typename Entries, auto Read> struct ResearchCatalogIndex {
  il2cpp::GcHandle               trees, projects;
  std::shared_ptr<const Entries> resources;

  std::shared_ptr<const Entries> Current()
  {
    Roots roots;
    auto* service       = roots.Keep(spec_service::ExistingSpecService());
    auto* data          = service ? roots.Keep(service->Field<"_dataContainer", StaticResearchData*>()) : nullptr;
    auto* live_trees    = data ? roots.Keep(data->Field<"ResearchTreeSpecs", ServerManagedReference*>()) : nullptr;
    auto* live_projects = data ? roots.Keep(data->Field<"ResearchProjectSpecs", ServerManagedReference*>()) : nullptr;
    if (!roots.healthy || !live_trees || !live_projects)
      return {};
    if (resources && trees.Get() == live_trees && projects.Get() == live_projects)
      return resources;

    // The game's static sync handler replaces these two collections together.
    auto next = Read(live_trees, live_projects);
    if (!next || data->Field<"ResearchTreeSpecs", ServerManagedReference*>() != live_trees
        || data->Field<"ResearchProjectSpecs", ServerManagedReference*>() != live_projects)
      return {};
    auto next_trees    = il2cpp::GcHandle::Weak(live_trees);
    auto next_projects = il2cpp::GcHandle::Weak(live_projects);
    if (!next_trees || !next_projects)
      return {};
    trees     = std::move(next_trees);
    projects  = std::move(next_projects);
    resources = std::make_shared<const Entries>(std::move(*next));
    return resources;
  }
};

using ResearchMaterialIndex = ResearchCatalogIndex<ResearchResources, ReadResearchResources>;

struct ResearchChecks {
  ResearchMaterialIndex&                                  index;
  std::unordered_map<int64_t, ResearchAvailability>       results;
  std::optional<std::shared_ptr<const ResearchResources>> catalog;
};

inline il2cpp::CallResult<ResearchAvailability> ResearchForReward(InventoryItem* item, ResearchChecks& checks)
{
  Roots roots;
  if (!roots.Keep(item))
    return {};
  const auto reward = IdentifyReward(item, roots);
  if (!roots.healthy)
    return {};
  if (reward.HasCollectibleProgress())
    return {true, ResearchAvailability::kUnrelated};
  const auto id = reward.resource_id;
  if (id <= 0)
    return {};
  if (auto cached = checks.results.find(id); cached != checks.results.end())
    return {true, cached->second};

  using HubClass      = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Core", "Hub">;
  const auto observed = HubClass::TryCall<"get_ResearchManager", StoreResearchManager*>();
  auto*      manager  = observed ? roots.Keep(observed.value) : nullptr;
  const auto eligible = manager
                            ? manager->TryCall<"IsResourceRequiredByVisibleResearches", bool(int64_t, ResearchTree)>(
                                  id, ResearchTree::Standard)
                            : il2cpp::CallResult<bool>{};
  if (!eligible)
    return {};

  auto availability = ResearchAvailability::kAvailable;
  if (!eligible.value) {
    if (!checks.catalog)
      checks.catalog = checks.index.Current();
    if (!*checks.catalog)
      return {};
    availability =
        (*checks.catalog)->contains(id) ? ResearchAvailability::kNoneAvailable : ResearchAvailability::kUnrelated;
  }
  checks.results.emplace(id, availability);
  return {true, availability};
}
} // namespace alliance_store_game
