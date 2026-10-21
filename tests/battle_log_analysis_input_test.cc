#include "patches/parts/battle_log_analysis_input.h"

#include <cassert>
#include <iostream>

int main()
{
  battle_analysis::PointerGesture gesture;
  // Tabs tolerate pointer motion while the release still hits the same tab.
  gesture.press(100, 100, false, 5);
  gesture.move(111, 103);
  assert(gesture.release(112, 104));
  // Motion after release must not retroactively cancel the completed click.
  gesture.press(100, 100, false, 5);
  assert(gesture.release(100, 100));
  gesture.move(200, 100);
  assert(!gesture.active && !gesture.dragged);
  // Press/up in a single sampled frame works too.
  gesture.press(100, 100, false, 5);
  assert(gesture.release(100, 100));
  // Scroll rows permit a normal click, but reject actual drags.
  gesture.press(100, 100, true, 5);
  assert(gesture.release(102, 102));
  gesture.press(100, 100, true, 5);
  gesture.move(108, 100);
  assert(!gesture.release(108, 100));
  // Returning to the starting point after a drag does not turn it into a click.
  gesture.press(100, 100, true, 5);
  gesture.move(108, 100);
  assert(!gesture.release(100, 100));
  // Respect the native radial threshold, including a configurable larger one.
  gesture.press(100, 100, true, 5);
  assert(!gesture.release(104, 104));
  gesture.press(100, 100, true, 10);
  assert(gesture.release(104, 104));
  // Losing focus/closing a report must clear the press.
  gesture.press(100, 100, false, 5);
  gesture.cancel();
  assert(!gesture.release(100, 100));
  assert(!gesture.release(100, 100));
  // The next click after a rejected drag starts with clean state.
  gesture.press(100, 100, true, 5);
  gesture.move(150, 100);
  assert(!gesture.release(150, 100));
  gesture.press(100, 100, false, 5);
  assert(gesture.release(100, 100));
  std::cout << "PASS: fixed controls, release timing, same-frame clicks, scroll drags, thresholds, focus and reset\n";
}
