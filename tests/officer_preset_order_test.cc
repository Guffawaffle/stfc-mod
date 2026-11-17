#include "officer_preset_order.h"

#include <array>
#include <cassert>
#include <iostream>

using namespace officer_preset_order;

Row Filled(std::int64_t slot, std::int32_t index)
{
  return {.qualified        = true,
          .present          = true,
          .slot             = slot,
          .order            = index,
          .presentation     = index % 3,
          .name_present     = true,
          .officers_present = true,
          .assigned         = true};
}
Row Empty(std::int64_t slot, std::int32_t index)
{
  auto row       = Filled(slot, index);
  row.name_empty = true;
  row.assigned   = false;
  return row;
}
int main()
{
  // Synthetic independent observations: no copied game names, contents or stores.
  constexpr std::int64_t large = 9007199254740993LL;
  std::array rows{Filled(0, 0), Empty(77, 1), Filled(large, 2), Empty(77, 3), Row{.qualified = true}, Filled(9, 5)};
  const auto original = rows;
  const std::array saved{std::int64_t{404}, large, std::int64_t{0}};
  const auto       plan = Prepare(rows, saved);
  assert(plan.status == Status::Ready);
  assert((plan.view_to_canonical == std::vector<std::size_t>{2, 1, 0, 3, 4, 5}));
  assert((plan.session_order == std::vector<std::int64_t>{404, large, 0, 9}));
  assert(rows == original); // Including every native OrderId and Presentation.

  const auto moved = Move(rows, saved, 9, -1);
  assert(moved.status == MoveStatus::Changed);
  assert((moved.session_order == std::vector<std::int64_t>{404, large, 9, 0}));
  const auto view = Prepare(rows, moved.session_order);
  assert((view.view_to_canonical == std::vector<std::size_t>{2, 1, 5, 3, 4, 0}));
  const auto down = Move(rows, saved, large, 1);
  assert(down.status == MoveStatus::Changed);
  assert((down.session_order == std::vector<std::int64_t>{404, 0, large, 9}));
  assert((Prepare(rows, down.session_order).view_to_canonical == std::vector<std::size_t>{0, 1, 2, 3, 4, 5}));
  assert(Move(rows, saved, large, -1).status == MoveStatus::Boundary);
  assert(Move(rows, saved, 77, 1).status == MoveStatus::Refused);
  assert(Move(rows, saved, 123, 1).status == MoveStatus::Refused);
  assert(Move(rows, saved, 0, 0).status == MoveStatus::Refused);
  assert(rows == original);

  auto named_unassigned        = rows;
  named_unassigned[5].assigned = false; // Named is not filled; also not a recognized empty row.
  assert((Prepare(named_unassigned, saved).view_to_canonical == std::vector<std::size_t>{2, 1, 0, 3, 4, 5}));
  assert(Move(named_unassigned, saved, 0, 1).status == MoveStatus::Boundary);
  assert(Move(named_unassigned, saved, 9, -1).status == MoveStatus::Refused);

  auto unsafe    = rows;
  unsafe[2].slot = 0;
  assert(Prepare(unsafe, saved).status == Status::Invalid);
  unsafe             = rows;
  unsafe[3].assigned = true; // Shared SlotId involving filled content is unsafe.
  assert(Prepare(unsafe, saved).status == Status::Invalid);
  unsafe                 = rows;
  unsafe[3].name_present = false;
  assert(Prepare(unsafe, saved).status == Status::Invalid);
  unsafe          = rows;
  unsafe[5].order = 2;
  assert(Prepare(unsafe, saved).status == Status::Invalid);
  unsafe              = rows;
  unsafe[5].qualified = false;
  assert(Prepare(unsafe, saved).status == Status::Unavailable);

  auto sentinel    = rows;
  sentinel[5].slot = -1;
  assert(Prepare(sentinel, saved).status == Status::Ready);
  assert(Move(sentinel, saved, -1, -1).status == MoveStatus::Refused);
  const std::array malformed_saved{std::int64_t{-1}, std::int64_t{0}, std::int64_t{0}, large};
  assert((Prepare(rows, malformed_saved).session_order == std::vector<std::int64_t>{0, large, 9}));
  assert(Prepare({}, {}).status == Status::Ready);
  std::cout << "Officer preset policy passed: exact identities, fixed empty rows, canonical invariants and moves\n";
}
