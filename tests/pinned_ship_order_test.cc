#ifdef NDEBUG
#undef NDEBUG
#endif

#include "patches/pinned_ship_order.h"
#include "patches/pinned_ship_state.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <vector>

int main()
{
  using namespace pinned_ship_order;
  std::vector<int64_t> pins;
  assert(!Toggle(pins, 0));
  assert(Toggle(pins, 101));
  assert(Toggle(pins, 202));
  assert(Toggle(pins, 303));
  assert(Contains(pins, 202));
  assert((pins == std::vector<int64_t>{101, 202, 303}));
  assert(MoveToTarget(pins, 101, 202));
  assert((pins == std::vector<int64_t>{202, 101, 303}));
  assert(MoveToTarget(pins, 101, 202));
  assert((pins == std::vector<int64_t>{101, 202, 303}));
  assert(!MoveToTarget(pins, 101, 101));
  assert(!MoveToTarget(pins, 101, 999));
  assert(!MoveToTarget(pins, 0, 303));
  assert(MoveToTarget(pins, 404, 303));
  assert((pins == std::vector<int64_t>{101, 202, 404, 303}));
  assert(!MoveToTarget(pins, 404, 999));
  assert(Toggle(pins, 404));
  std::vector<int64_t> end_pins{101, 202, 303};
  assert(MoveToEnd(end_pins, 202));
  assert((end_pins == std::vector<int64_t>{101, 303, 202}));
  assert(!MoveToEnd(end_pins, 202));
  assert(MoveToEnd(end_pins, 404));
  assert((end_pins == std::vector<int64_t>{101, 303, 202, 404}));
  assert(!MoveToEnd(end_pins, 0));
  std::vector<int64_t> empty_pins;
  assert(MoveToEnd(empty_pins, 505));
  assert((empty_pins == std::vector<int64_t>{505}));

  const std::vector<std::optional<int64_t>> ships{202, 900, 101, 303, 901, 202, std::nullopt};
  assert((SortedIndices(ships, pins) == std::vector<int>{2, 0, 5, 3, 1, 4, 6}));
  assert((SortedIndices(ships, pins, {}, {false, false, false, false, false, false, true})
          == std::vector<int>{6, 2, 0, 5, 3, 1, 4}));

  assert(Toggle(pins, 303));
  assert((pins == std::vector<int64_t>{101, 202}));
  assert((SortedIndices(ships, pins) == std::vector<int>{2, 0, 5, 1, 3, 4, 6}));
  assert((SortedIndices(ships, {}) == std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
  assert((SortedIndices(ships, pins, {false, false, false, true, false, false, false})
          == std::vector<int>{2, 0, 5, 3, 1, 4, 6}));
  assert((SortedIndices({std::nullopt, 101, 202}, pins, {true, false, false})
          == std::vector<int>{1, 2, 0}));

  const std::vector<std::string> duplicate_names{"AMALGAM", "AMALGAM"};
  const std::vector<std::optional<int64_t>> duplicate_ids{101, 202};
  auto migrated_high = MigrateLegacyNames(duplicate_names, duplicate_ids, 101);
  assert(migrated_high.selected_was_legacy && (migrated_high.pins == std::vector<int64_t>{202}));
  assert(migrated_high.unresolved_names.empty());
  auto migrated_low = MigrateLegacyNames(duplicate_names, duplicate_ids, 202);
  assert(migrated_low.selected_was_legacy && (migrated_low.pins == std::vector<int64_t>{101}));
  auto migrated_other = MigrateLegacyNames(duplicate_names, duplicate_ids, 303);
  assert(!migrated_other.selected_was_legacy
         && (migrated_other.pins == std::vector<int64_t>{101, 202}));
  auto migrated_missing = MigrateLegacyNames(duplicate_names, {101, std::nullopt}, 303);
  assert((migrated_missing.pins == std::vector<int64_t>{101})
         && (migrated_missing.unresolved_names == std::vector<std::string>{"AMALGAM"}));
  auto migrated_ambiguous = MigrateLegacyNames(duplicate_names, {101, std::nullopt}, 101);
  assert(AmbiguousUnpin(migrated_ambiguous, false));
  assert(AmbiguousUnpin(migrated_missing, true));
  assert(!AmbiguousUnpin(migrated_missing, false));
  assert(!AmbiguousUnpin(migrated_high, false));

  nlohmann::json state = {{"version", 1}};
  assert(!pinned_ship_state::ReadOrder(state));
  state[pinned_ship_state::Key] = {"101", "202", "101", "bad", "0", -1};
  assert((pinned_ship_state::ReadOrder(state) == std::vector<int64_t>{101, 202}));
  pinned_ship_state::WriteOrder(state, pins);
  assert((pinned_ship_state::ReadOrder(state) == pins));
  state[pinned_ship_state::Key] = nlohmann::json::object();
  assert(!pinned_ship_state::ReadOrder(state));
  state[pinned_ship_state::Key] = nlohmann::json::array();
  assert(pinned_ship_state::ReadOrder(state)->empty());
  assert(pinned_ship_state::ReadLegacyNames(state).empty());
  pinned_ship_state::WriteLegacyNames(state, {"AMALGAM", "SEKIE"});
  assert((pinned_ship_state::ReadLegacyNames(state) == std::vector<std::string>{"AMALGAM", "SEKIE"}));
  pinned_ship_state::WriteLegacyNames(state, {"AMALGAM", "AMALGAM"});
  assert((pinned_ship_state::ReadLegacyNames(state) == std::vector<std::string>{"AMALGAM", "AMALGAM"}));
}
