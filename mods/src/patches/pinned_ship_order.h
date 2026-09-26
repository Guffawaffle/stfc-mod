#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

namespace pinned_ship_order
{
inline bool Contains(const std::vector<int64_t>& pins, int64_t id)
{ return std::find(pins.begin(), pins.end(), id) != pins.end(); }

inline bool Toggle(std::vector<int64_t>& pins, int64_t id)
{
  if (id <= 0)
    return false;
  const auto found = std::find(pins.begin(), pins.end(), id);
  if (found == pins.end())
    pins.push_back(id);
  else
    pins.erase(found);
  return true;
}

inline bool MoveToTarget(std::vector<int64_t>& pins, int64_t source_id, int64_t target_id)
{
  if (source_id <= 0 || source_id == target_id)
    return false;
  const auto source = std::find(pins.begin(), pins.end(), source_id);
  const auto target = std::find(pins.begin(), pins.end(), target_id);
  if (target == pins.end())
    return false;
  const auto target_index = static_cast<std::size_t>(target - pins.begin());
  if (source != pins.end())
    pins.erase(source);
  pins.insert(pins.begin() + target_index, source_id);
  return true;
}

inline bool MoveToEnd(std::vector<int64_t>& pins, int64_t id)
{
  if (id <= 0)
    return false;
  const auto found = std::find(pins.begin(), pins.end(), id);
  if (found != pins.end()) {
    if (found + 1 == pins.end())
      return false;
    pins.erase(found);
  }
  pins.push_back(id);
  return true;
}

inline std::vector<int> SortedIndices(const std::vector<std::optional<int64_t>>& ship_ids,
                                      const std::vector<int64_t>& pins, const std::vector<bool>& legacy_pins = {},
                                      const std::vector<bool>& non_ships = {})
{
  std::vector<int> indices(ship_ids.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::vector<int> ranks(ship_ids.size(), static_cast<int>(pins.size()) + 1);
  for (std::size_t index = 0; index < ship_ids.size(); ++index) {
    if (index < non_ships.size() && non_ships[index]) {
      ranks[index] = -1;
      continue;
    }
    if (!ship_ids[index])
      continue;
    const auto found = std::find(pins.begin(), pins.end(), *ship_ids[index]);
    if (found != pins.end())
      ranks[index] = static_cast<int>(found - pins.begin());
    else if (index < legacy_pins.size() && legacy_pins[index])
      ranks[index] = static_cast<int>(pins.size());
  }
  std::stable_sort(indices.begin(), indices.end(), [&](int left, int right) { return ranks[left] < ranks[right]; });
  return indices;
}
} // namespace pinned_ship_order
