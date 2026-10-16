#include "patches/claim_notice.h"
#include <cassert>
#include <iostream>

int main()
{
  using namespace claim_trace;
  NoticeState state;
  assert(state.kind == NoticeKind::none);
  state.Capture(NoticeKind::rejected, 7, true);
  state.BeginRecovery(7, 10, 100);
  state.Returned(7, 10, true, 200);
  assert(state.kind == NoticeKind::rejected); // Acceptance alone never claims a fix.
  state.Hidden(8, 250);
  assert(state.kind == NoticeKind::rejected);
  state.Hidden(7, 250);
  assert(state.kind == NoticeKind::recovered && state.injected);

  state.Capture(NoticeKind::rejected, 8);
  state.BeginRecovery(8, 11, 100);
  state.Hidden(8, 150); // Native callbacks can hide synchronously before returning.
  assert(state.kind == NoticeKind::rejected);
  state.Returned(8, 11, true, 200);
  assert(state.kind == NoticeKind::recovered);

  for (int scenario = 0; scenario < 4; ++scenario) {
    state.Capture(NoticeKind::rejected, 9);
    state.BeginRecovery(9, 12, 100);
    if (scenario == 0) state.CancelConfirmation(); // Manual navigation invalidates attribution.
    if (scenario == 1) state.Returned(9, 12, false, 200);
    if (scenario == 2) state.Returned(9, 13, true, 200); // Unrelated attempt.
    if (scenario == 3) state.Returned(9, 12, true, 5200); // Expired observation.
    state.Hidden(9, scenario == 3 ? 5300 : 250);
    assert(state.kind == NoticeKind::rejected);
  }
  state.Capture(NoticeKind::preview);
  state.BeginRecovery(9, 12, 100);
  state.Returned(9, 12, true, 150);
  state.Hidden(9, 200);
  assert(state.kind == NoticeKind::preview);
  state = {};
  assert(state.kind == NoticeKind::none);
  std::cout << "Claim notice attribution tests passed\n";
}
