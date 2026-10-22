#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patches/parts/claim_selection_budget.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

int main()
{
  using namespace selection_capture;
  Admission admission;
  auto      span = admission.Try(0, true, 0, 0);
  assert(span.admitted && span.Take(100, 200)); // Before state.
  assert(span.Take(20, 20));                    // Details share the allowance.
  assert(span.Take(136, 292));                  // After state.
  assert(!span.Take(1, 0) && span.reason == Reason::extent);
  assert(span.rows == 0 && span.slots == 0);
  auto sparse = admission.Try(10, true, 0, 0);
  assert(!sparse.Take(1, 4096)); // Free dictionary slots also cost inspection.
  assert(sparse.rows == kRows && sparse.slots == kSlots);
  for (int64_t now = 20; now <= 70; now += 10)
    assert(admission.Try(now, true, 0, 0).admitted);
  assert(admission.Try(99, true, 0, 0).reason == Reason::burst);
  assert(admission.Try(100, true, 0, 0).admitted);
  assert(!admission.Try(101, true, 0, 0).admitted); // No tumbling-window double burst.
  assert(admission.Try(110, true, 0, 0).admitted);

  Admission pressured;
  assert(pressured.Try(0, false, 0, 0).reason == Reason::worker_pressure);
  assert(pressured.Try(0, true, 96, 0).reason == Reason::worker_pressure);
  assert(pressured.Try(0, true, 0, 12 * 1024 * 1024).reason == Reason::worker_pressure);
  auto skipped = pressured.Try(0, false, 0, 0);
  assert(!skipped.admitted && !skipped.Take(1, 1));
  for (int i = 0; i < 8; ++i)
    assert(pressured.Try(0, true, 0, 0).admitted); // Skips do not consume admission.

  Admission                concurrent;
  std::atomic_bool         start{false};
  std::atomic_int          accepted{0};
  std::vector<std::thread> callers;
  for (int i = 0; i < 32; ++i)
    callers.emplace_back([&] {
      while (!start.load()) {}
      if (concurrent.Try(0, true, 0, 0).admitted)
        ++accepted;
    });
  start.store(true);
  for (auto& caller : callers)
    caller.join();
  assert(accepted > 0 && accepted <= 8);
}
