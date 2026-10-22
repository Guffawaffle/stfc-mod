#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

// Algorithms for the object tracker's class buckets and reverse membership
// index. The caller owns synchronization; neither container roots Unity objects.
namespace object_tracking
{
template <typename Registry, typename Memberships, typename Parent>
bool Add(Registry& registry, Memberships& memberships, std::uintptr_t object, typename Memberships::mapped_type klass,
         Parent parent)
{
  if (!object || !klass || !memberships.emplace(object, klass).second)
    return false;
  for (; klass; klass = parent(klass))
    registry[klass].push_back(object);
  return true;
}

template <typename Registry, typename Memberships, typename Parent>
void Remove(Registry& registry, Memberships& memberships, std::uintptr_t object, Parent parent)
{
  const auto found = memberships.find(object);
  if (found == memberships.end())
    return;
  auto klass = found->second;
  memberships.erase(found);
  // Preserve insertion order: ObjectFinder::Get returns the latest live entry.
  // Never inspect the potentially finalized object's class to find its buckets.
  for (; klass; klass = parent(klass)) {
    const auto bucket = registry.find(klass);
    if (bucket != registry.end()) {
      auto& objects = bucket->second;
      objects.erase(std::remove(objects.begin(), objects.end(), object), objects.end());
    }
  }
}

template <typename Registry, typename Memberships, typename Dead, typename Removed>
std::size_t Collect(Registry& registry, Memberships& memberships, Dead dead, Removed removed)
{
  std::size_t count = 0;
  for (auto it = memberships.begin(); it != memberships.end();) {
    if (dead(it->first)) {
      removed(it->first, it->second);
      it = memberships.erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  if (!count)
    return 0;
  // One stable compaction per class, rather than scanning every class again
  // for each dead object. Native liveness is tested once per unique object.
  for (auto& [klass, objects] : registry)
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [&](std::uintptr_t object) { return memberships.find(object) == memberships.end(); }),
                  objects.end());
  return count;
}
} // namespace object_tracking
