/**
 * @file fleet_notification_cache_policy.h
 * @brief Bounded fleet-cache cardinality helpers.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

template <typename Map>
[[nodiscard]] size_t CountStaleFleetCacheEntries(const Map& cache, const std::span<const uint64_t> current_fleet_ids)
{
  size_t represented = 0;
  for (size_t index = 0; index < current_fleet_ids.size(); ++index) {
    const auto fleet_id = current_fleet_ids[index];
    if (std::find(current_fleet_ids.begin(), current_fleet_ids.begin() + index, fleet_id)
        == current_fleet_ids.begin() + index) {
      represented += cache.contains(fleet_id) ? 1 : 0;
    }
  }
  return cache.size() - std::min(cache.size(), represented);
}
