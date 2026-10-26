#include "galaxy_selection_policy.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
using namespace std::chrono_literals;
int main()
{
  using galaxy_selection::CanRetire;
  assert(!CanRetire(10, 5, 5s, false, false));
  assert(!CanRetire(10, 5, 5s, true, true));
  assert(!CanRetire(10, 8, 5s, true, false));
  assert(!CanRetire(10, 5, 1999ms, true, false));
  assert(!CanRetire(1, 9, 5s, true, false));
  assert(CanRetire(10, 7, 2s, true, false));
  assert(galaxy_selection::MaximumProxies == 64 && galaxy_selection::MaximumClickCandidates == 16);
  struct Host {
    std::map<int, bool> active{{1, true}, {2, true}};
    bool                observed = true, fail_before = false, fail_after = false;
    std::optional<bool> Alive(int)
    { return observed ? std::optional(true) : std::nullopt; }
    std::optional<bool> Active(int object)
    { return observed ? std::optional(active.at(object)) : std::nullopt; }
    void Disable(int object)
    {
      if (!fail_before)
        active[object] = false;
      (void)fail_after;
    }
  } host;
  const std::vector objects{1, 2};
  host.fail_before = true;
  assert(!galaxy_selection::Deactivate(objects, host) && host.active.at(1));
  host.fail_before = false;
  host.observed    = false;
  assert(!galaxy_selection::Deactivate(objects, host) && host.active.at(1));
  host.observed   = true;
  host.fail_after = true;
  assert(galaxy_selection::Deactivate(objects, host) && !host.active.at(1) && !host.active.at(2));
  int                        first_manager, other_manager;
  galaxy_selection::TapWatch tap{&first_manager, 12.f, 24.f};
  assert(tap.Matches(&first_manager, 12.f, 24.f));
  assert(!tap.Matches(&other_manager, 12.f, 24.f) && !tap.Matches(&first_manager, 13.f, 24.f));
  std::cout << "Immutable Galaxy targets: recent clicks, owner pins, unknowns and bounded pool passed\n";
}
