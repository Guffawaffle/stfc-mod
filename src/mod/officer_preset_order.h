#pragma once

// Presentation-only preset policy: owned observed row summaries, never native pointers.
// Qualified array/name observations distinguish an empty slot from unavailable
// evidence. Canonical positions/identities are inputs and are never rewritten.
// A native consumer applies rows[view_index].presentation to the mapped item;
// this policy only supplies the permutation and never mutates native fields.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace officer_preset_order
{
struct Row {
  bool         qualified    = false;
  bool         present      = false;
  std::int64_t slot         = -1;
  std::int32_t order        = -1;
  std::int32_t presentation = 0;
  bool         name_present = false, name_empty = false;
  bool         officers_present = false, assigned = false;

  bool NativeIdentity() const noexcept
  { return present && slot >= 0 && order >= 0; }
  bool Empty() const noexcept
  { return present && name_present && name_empty && officers_present && !assigned; }
  bool Reorderable() const noexcept
  { return NativeIdentity() && name_present && officers_present && assigned; }
  friend bool operator==(const Row&, const Row&) = default;
};

enum class Status { Ready, Unavailable, Invalid };
struct Plan {
  Status                    status = Status::Unavailable;
  std::vector<std::size_t>  view_to_canonical;
  std::vector<std::int64_t> session_order;
};

inline Plan Prepare(std::span<const Row> rows, std::span<const std::int64_t> saved)
{
  Plan result;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    if (!row.qualified)
      return result;
    if (!row.NativeIdentity())
      continue;
    if (static_cast<std::size_t>(row.order) != i) {
      result.status = Status::Invalid;
      return result;
    }
    for (std::size_t previous = 0; previous < i; ++previous) {
      const auto& other = rows[previous];
      if (other.NativeIdentity() && other.slot == row.slot && !(other.Empty() && row.Empty())) {
        result.status = Status::Invalid;
        return result;
      }
    }
  }
  // The persisted reader already performs this normalization. Keeping its
  // contract here makes a malformed proposed order safe at the pure boundary.
  for (const auto id : saved)
    if (id >= 0 && std::ranges::find(result.session_order, id) == result.session_order.end())
      result.session_order.push_back(id);
  std::vector<std::size_t> positions;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    result.view_to_canonical.push_back(i);
    if (!rows[i].Reorderable())
      continue;
    positions.push_back(i);
    if (std::ranges::find(result.session_order, rows[i].slot) == result.session_order.end())
      result.session_order.push_back(rows[i].slot);
  }
  auto occupied = positions;
  std::stable_sort(occupied.begin(), occupied.end(), [&](std::size_t left, std::size_t right) {
    return std::ranges::find(result.session_order, rows[left].slot)
           < std::ranges::find(result.session_order, rows[right].slot);
  });
  for (std::size_t i = 0; i < positions.size(); ++i)
    result.view_to_canonical[positions[i]] = occupied[i];
  result.status = Status::Ready;
  return result;
}

enum class MoveStatus { Refused, Boundary, Changed };
struct MoveResult {
  MoveStatus                status = MoveStatus::Refused;
  std::vector<std::int64_t> session_order;
};

inline MoveResult Move(std::span<const Row> rows, std::span<const std::int64_t> saved, std::int64_t selected_slot,
                       int direction)
{
  MoveResult result;
  if (direction != -1 && direction != 1)
    return result;
  auto plan = Prepare(rows, saved);
  if (plan.status != Status::Ready)
    return result;
  const auto selected = std::ranges::find_if(
      plan.view_to_canonical, [&](std::size_t i) { return rows[i].Reorderable() && rows[i].slot == selected_slot; });
  if (selected == plan.view_to_canonical.end())
    return result;
  auto next = selected - plan.view_to_canonical.begin() + direction;
  while (next >= 0 && static_cast<std::size_t>(next) < rows.size()
         && !rows[plan.view_to_canonical[static_cast<std::size_t>(next)]].Reorderable())
    next += direction;
  result.session_order = std::move(plan.session_order);
  if (next < 0 || static_cast<std::size_t>(next) >= rows.size()) {
    result.status = MoveStatus::Boundary;
    return result;
  }
  const auto target_slot = rows[plan.view_to_canonical[static_cast<std::size_t>(next)]].slot;
  const auto source      = std::ranges::find(result.session_order, selected_slot);
  const auto target      = std::ranges::find(result.session_order, target_slot);
  std::iter_swap(source, target);
  result.status = MoveStatus::Changed;
  return result;
}
} // namespace officer_preset_order
