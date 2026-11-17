#pragma once

// One same-size ship-list permutation. Accessors return an optional observation
// (a null item may still be observed) and explicit write success. No Clear/Add,
// guessed defaults or count mutation is allowed at this boundary.
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ship_order
{
enum class Result { Unchanged, Applied, Refused, Restored, Unrestored };

template <typename T, typename Count, typename Read, typename Write>
Result Apply(std::span<const T> original, std::span<const int> order, Count count, Read read, Write write) noexcept
{
  bool       mutation_started = false;
  const auto matches          = [&](bool reordered) {
    const auto size = count();
    if (!size || *size < 0 || static_cast<std::size_t>(*size) != original.size())
      return false;
    for (std::size_t i = 0; i < original.size(); ++i) {
      const auto item = read(static_cast<int>(i));
      if (!item || *item != original[reordered ? static_cast<std::size_t>(order[i]) : i])
        return false;
    }
    return true;
  };
  const auto restore = [&]() noexcept {
    try {
      const auto size = count();
      if (!size || *size < 0 || static_cast<std::size_t>(*size) != original.size())
        return Result::Unrestored;
      // A failing setter may have mutated before reporting failure. Attempt all
      // original slots and judge restoration by observed readback, not returns.
      for (std::size_t i = 0; i < original.size(); ++i) {
        try {
          (void)write(static_cast<int>(i), original[i]);
        } catch (...) {
        }
      }
      return matches(false) ? Result::Restored : Result::Unrestored;
    } catch (...) {
      return Result::Unrestored;
    }
  };

  try {
    if (order.size() != original.size())
      return Result::Refused;
    std::vector<bool> seen(order.size(), false);
    bool              changed = false;
    for (std::size_t i = 0; i < order.size(); ++i) {
      if (order[i] < 0 || static_cast<std::size_t>(order[i]) >= order.size() || seen[order[i]])
        return Result::Refused;
      seen[order[i]] = true;
      changed |= static_cast<std::size_t>(order[i]) != i;
    }
    if (!matches(false))
      return Result::Refused;
    if (!changed)
      return Result::Unchanged;
    for (std::size_t i = 0; i < order.size(); ++i) {
      if (static_cast<std::size_t>(order[i]) == i)
        continue;
      mutation_started = true;
      if (!write(static_cast<int>(i), original[order[i]]))
        return restore();
    }
    return matches(true) ? Result::Applied : restore();
  } catch (...) {
    return mutation_started ? restore() : Result::Refused;
  }
}
} // namespace ship_order
