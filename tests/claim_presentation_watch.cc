#include "patches/claim_presentation_watch.h"
#include <cassert>

int main()
{
  claim_trace::PresentationWatch watch;
  assert(!watch.Observe(false, 1, 2, 0));
  assert(!watch.Observe(true, 1, 2, 100));
  assert(!watch.Observe(true, 1, 2, 15099));
  assert(!watch.ConsumeEscape(true));
  assert(watch.Observe(true, 1, 2, 15100));
  assert(!watch.Observe(true, 1, 2, 20000));
  assert(!watch.ConsumeEscape(false)); // Disabled/busy/input/changed claim do not consume.
  assert(!watch.Attempted());
  assert(watch.ConsumeEscape(true));
  assert(!watch.ConsumeEscape(true)); // Consume before calling native code, including rejection.
  assert(!watch.Observe(false, 1, 2, 20001));
  assert(!watch.Stalled());
  assert(!watch.Observe(true, 1, 2, 30000));
  assert(!watch.Observe(true, 1, 2, 45000));
  assert(watch.Stalled());
  assert(!watch.ConsumeEscape(true)); // Summary/context changes cannot replenish budget.

  claim_trace::PresentationWatch navigation;
  assert(!navigation.Observe(true, 1, 2, 0));
  assert(!navigation.Observe(true, 3, 4, 14999)); // A changed section restarts initial observation.
  assert(!navigation.Observe(true, 3, 4, 29998));
  assert(navigation.Observe(true, 3, 4, 29999));
  assert(!navigation.Observe(true, 5, 6, 30000)); // Human escape ends the incident.
  assert(navigation.LeftSection());
  assert(!navigation.Stalled());
  assert(!navigation.Observe(true, 5, 6, 90000)); // Old Presenting claim must not target later gameplay.
  assert(!navigation.ConsumeEscape(true));
  assert(!navigation.Observe(true, 3, 4, 100000)); // Returning cannot re-arm the old incident.
  assert(!navigation.Stalled());
  claim_trace::PresentationWatch immediate;
  assert(!immediate.Observe(true, 1, 2, 0));
  assert(immediate.Observe(true, 1, 2, 15000));
  immediate.NavigationAccepted(); // Retire before another 5-second observation can miss a rapid escape/return.
  assert(immediate.LeftSection());
  assert(!immediate.Stalled());
  assert(!immediate.ConsumeEscape(true));
  assert(!immediate.Observe(true, 1, 2, 50000));
}
