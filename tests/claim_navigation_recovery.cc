#include "patches/claim_navigation_recovery.h"

#include <cassert>
#include <initializer_list>

using Recovery = claim_trace::NavigationRecovery;
int main()
{
  Recovery        r;
  Recovery::Gates gates{true, true, true, true, true, true, false, false, false};
  assert(!r.Schedule(0, 0));
  assert(r.Schedule(1, 0));
  assert(r.Poll(gates, 0) == Recovery::Decision::wait);
  assert(r.Poll(gates, 249) == Recovery::Decision::wait);
  assert(r.Poll(gates, 250) == Recovery::Decision::retry);
  assert(r.Poll(gates, 251) == Recovery::Decision::none);
  assert(!r.Schedule(1, 252)); // Manual clicks cannot replenish the budget.
  assert(r.Schedule(2, 300));
  gates.busy = true;
  assert(r.Poll(gates, 600) == Recovery::Decision::wait);
  gates.busy = false;
  assert(r.Poll(gates, 700) == Recovery::Decision::wait);
  assert(r.Poll(gates, 800) == Recovery::Decision::retry);
  assert(r.Schedule(3, 900));
  gates.sameContext = false;
  assert(r.Poll(gates, 1200) == Recovery::Decision::changed);
  gates.sameContext = true;
  for (auto member : {&Recovery::Gates::enabled, &Recovery::Gates::healthy, &Recovery::Gates::sameSummary,
                      &Recovery::Gates::knownState, &Recovery::Gates::sameOrigin}) {
    static uint64_t generation = 4;
    assert(r.Schedule(generation++, 0));
    gates.*member = false;
    assert(r.Poll(gates, 300) != Recovery::Decision::retry);
    assert(!r.Pending());
    gates.*member = true;
  }
  assert(r.Schedule(20, 0));
  gates.atDestination = true;
  assert(r.Poll(gates, 300) == Recovery::Decision::destination);
  gates.atDestination = false;
  assert(r.Schedule(21, 0));
  gates.inputBlocked = true;
  assert(r.Poll(gates, 300) == Recovery::Decision::input);
  gates.inputBlocked = false;
  assert(r.Schedule(22, 0));
  gates.busy = true;
  assert(r.Poll(gates, 5000) == Recovery::Decision::expired);
  gates.busy = false;
  assert(r.Schedule(23, 0));
  r.Cancel();
  assert(r.Poll(gates, 300) == Recovery::Decision::none);
  assert(!r.Schedule(23, 301));
}
