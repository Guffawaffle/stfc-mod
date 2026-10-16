#include "patches/claim_ui_fault.h"

#include <cassert>

int main()
{
  claim_trace::UiRejectOnce fault;
  assert(!fault.Arm(0, 0));
  assert(!fault.Consume(1, true, 0));
  assert(fault.Arm(1, 100));
  assert(!fault.Consume(1, false, 101)); // Ordinary navigation remains native.
  assert(!fault.Consume(2, true, 102));  // Another summary cannot spend the arm.
  assert(fault.Consume(1, true, 103));
  assert(!fault.Consume(1, true, 104)); // Exactly one rejected UI attempt.
  assert(fault.Arm(1, 200));
  assert(!fault.Consume(1, true, 60200)); // Expired before use.
  assert(fault.Arm(1, 70000));
  fault.Cancel();
  assert(!fault.Consume(1, true, 70001));
}
