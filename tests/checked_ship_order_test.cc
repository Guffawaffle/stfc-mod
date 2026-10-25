#include "../src/mod/checked_ship_order.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <stdexcept>

namespace
{
struct List {
  std::array<int, 3> items{11, 22, 33};
  int                reads = 0, writes = 0;
  int                fail_write = 0, throw_write = 0;
  bool               mutate_before_failure = false, refuse_restore = false;
  bool               unreadable = false, resize_after_write = false, ignored_writes = false;
  std::optional<int> Count() const
  { return resize_after_write && writes ? 2 : 3; }
  std::optional<int> Read(int index)
  {
    ++reads;
    if (unreadable)
      return {};
    return items.at(index);
  }
  bool Write(int index, int value)
  {
    ++writes;
    if (refuse_restore && writes > fail_write)
      return false;
    const bool failing = writes == fail_write || writes == throw_write;
    if (!ignored_writes && (!failing || mutate_before_failure))
      items.at(index) = value;
    if (writes == throw_write)
      throw std::runtime_error("fixture setter failed after its side effect");
    return writes != fail_write;
  }
  ship_order::Result Apply(std::span<const int> order)
  {
    static constexpr std::array original{11, 22, 33};
    return ship_order::Apply<int>(
        original, order, [&] { return Count(); }, [&](int i) { return Read(i); },
        [&](int i, int value) { return Write(i, value); });
  }
};
} // namespace

int main()
{
  using ship_order::Result;
  constexpr std::array reorder{2, 0, 1};
  constexpr std::array original{11, 22, 33};
  {
    List list;
    assert(list.Apply(reorder) == Result::Applied);
    assert((list.items == std::array{33, 11, 22}));
    assert(list.writes == 3 && list.reads == 6);
  }
  {
    List list;
    assert(list.Apply(std::array{0, 1, 2}) == Result::Unchanged && !list.writes);
    assert(list.Apply(std::array{1, 1, 2}) == Result::Refused && !list.writes);
    assert(list.Apply(std::array{0, 1, -1}) == Result::Refused && !list.writes);
    assert(list.Apply(std::array{0, 1, 3}) == Result::Refused && !list.writes);
    assert(list.Apply(std::array{0, 1}) == Result::Refused && !list.writes);
  }
  {
    List list;
    list.items[1] = 99; // The caller's snapshot is stale: do not mutate it.
    assert(list.Apply(reorder) == Result::Refused && !list.writes);
    list.items      = original;
    list.unreadable = true;
    assert(list.Apply(reorder) == Result::Refused && !list.writes);
  }
  for (int failed = 1; failed <= 3; ++failed) {
    for (bool side_effect : {false, true}) {
      List list;
      list.fail_write            = failed;
      list.mutate_before_failure = side_effect;
      assert(list.Apply(reorder) == Result::Restored);
      assert(list.items == original && list.writes == failed + 3);
    }
    List list;
    list.throw_write           = failed;
    list.mutate_before_failure = true;
    assert(list.Apply(reorder) == Result::Restored && list.items == original);
  }
  {
    List list;
    list.fail_write            = 2;
    list.mutate_before_failure = true;
    list.refuse_restore        = true;
    assert(list.Apply(reorder) == Result::Unrestored);
    assert(list.items != original);
  }
  {
    List list;
    list.resize_after_write = true;
    assert(list.Apply(reorder) == Result::Unrestored);
    assert(list.writes == 3); // Refuse restoration into a changed collection shape.
  }
  {
    List list;
    list.ignored_writes = true;
    assert(list.Apply(reorder) == Result::Restored);
    assert(list.items == original && list.writes == 6); // Setter success alone proves nothing.
  }
  {
    int                 a = 1, b = 2;
    std::array<int*, 3> items{&a, nullptr, &b};
    const auto          snapshot = items;
    assert(ship_order::Apply<int*>(
               snapshot, reorder, [] { return std::optional{3}; }, [&](int i) { return std::optional<int*>{items[i]}; },
               [&](int i, int* value) {
                 items[i] = value;
                 return true;
               })
           == Result::Applied);
    assert(items[0] == &b && items[1] == &a && !items[2]); // Observed null is not unavailable.
  }
  {
    int writes = 0;
    assert(ship_order::Apply<int>(
               original, reorder, []() -> std::optional<int> { return {}; }, [](int) { return std::optional{0}; },
               [&](int, int) {
                 ++writes;
                 return true;
               })
           == Result::Refused);
    assert(!writes);
    assert(ship_order::Apply<int>(
               original, reorder, []() -> std::optional<int> { throw std::runtime_error("count unavailable"); },
               [](int) { return std::optional{0}; },
               [&](int, int) {
                 ++writes;
                 return true;
               })
           == Result::Refused);
    assert(!writes);
  }
  std::puts("Checked ship-order observation, failed mutation and restoration fixtures passed");
}
