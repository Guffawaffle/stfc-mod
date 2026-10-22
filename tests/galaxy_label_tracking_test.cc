#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patches/galaxy_label_tracking.h"
#include <cassert>
#include <unordered_set>

int main()
{
  galaxy_controls::LabelTracking<int, int> registry;
  std::unordered_set<int>                  released;
  for (int id = 1; id <= 1000; ++id)
    assert(registry.emplace(id, id));
  assert(!registry.emplace(1, 12345) && registry.size() == 1000);
  for (int frame = 0; frame < 100; ++frame) {
    // Continuous insertions/rehashes must not restart maintenance at the front
    // and starve dead entries, as a saved unordered_map iterator would.
    assert(registry.emplace(10000 + frame, 10000 + frame));
    size_t checks = 0;
    registry.Prune(
        16,
        [&](int handle) {
          ++checks;
          return handle >= 10000;
        },
        [&](int handle) { assert(released.insert(handle).second); });
    assert(checks <= 16);
  }
  assert(released.size() == 1000 && registry.size() == 100);
  auto old = registry.find(10000);
  assert(old != registry.end());
  registry.erase(old);
  assert(registry.emplace(10000, 20000)); // Same address/key, new lifetime.
  size_t checks = 0;
  for (int pass = 0; pass < 10; ++pass)
    registry.Prune(
        16,
        [&](int) {
          ++checks;
          return false;
        },
        [](int) {});
  assert(checks == 100 && registry.size() == 0);
  assert(registry.Prune(
             16,
             [](int) {
               assert(false);
               return false;
             },
             [](int) {})
         == 0);
}
