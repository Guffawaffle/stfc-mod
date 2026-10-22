#include "patches/object_tracking.h"

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Class {
  Class* parent;
};
using Registry    = std::unordered_map<Class*, std::vector<std::uintptr_t>>;
using Memberships = std::unordered_map<std::uintptr_t, Class*>;

void Check(bool value, const char* message)
{
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main()
{
  Class       root{}, base{&root}, first{&base}, second{&base}, unrelated{};
  Registry    registry;
  Memberships memberships;
  auto        parent = [](Class* klass) { return klass->parent; };
  Check(!object_tracking::Add(registry, memberships, 0, &first, parent), "null object admitted");
  Check(!object_tracking::Add(registry, memberships, 1, static_cast<Class*>(nullptr), parent), "null class admitted");
  for (std::uintptr_t object = 1; object <= 5000; ++object)
    Check(object_tracking::Add(registry, memberships, object, object % 2 ? &first : &second, parent), "add failed");
  Check(!object_tracking::Add(registry, memberships, 1, &first, parent), "duplicate constructor added entries");
  Check(registry[&root].size() == 5000 && registry[&first].size() == 2500, "ancestor membership wrong");
  registry[&unrelated] = {};
  std::size_t parents  = 0;
  object_tracking::Remove(registry, memberships, 4999, [&](Class* klass) {
    ++parents;
    return klass->parent;
  });
  Check(parents == 3 && registry[&unrelated].empty(), "single removal walked unrelated classes");
  Check(registry[&root].back() == 5000 && registry[&first].back() == 4997, "single removal changed latest order");
  object_tracking::Remove(registry, memberships, 4999, parent);
  std::size_t reads = 0, notices = 0;
  auto        collected = object_tracking::Collect(
      registry, memberships,
      [&](std::uintptr_t object) {
        ++reads;
        return object % 2 == 0;
      },
      [&](std::uintptr_t, Class*) { ++notices; });
  Check(reads == 4999 && collected == 2500 && notices == 2500, "liveness was not one pass over unique objects");
  Check(memberships.size() == 2499 && registry[&root].size() == 2499 && registry[&second].empty(),
        "dead memberships retained");
  Check(registry[&root].front() == 1 && registry[&root].back() == 4997, "batch collection changed insertion order");
  reads = 0;
  Check(object_tracking::Collect(
            registry, memberships,
            [&](std::uintptr_t) {
              ++reads;
              return false;
            },
            [](std::uintptr_t, Class*) {})
                == 0
            && reads == 2499,
        "no-death pass changed membership");
  object_tracking::Collect(registry, memberships, [](std::uintptr_t) { return true; }, [](std::uintptr_t, Class*) {});
  Check(memberships.empty(), "reverse index retains dead objects");
  for (auto& [klass, objects] : registry)
    Check(objects.empty(), "class bucket retains dead objects");
  Check(object_tracking::Add(registry, memberships, 1, &second, parent), "reused address not admitted");
  Check(registry[&first].empty() && registry[&second].back() == 1, "reused address kept previous class");
  std::cout << "object tracking regression passed\n";
}
