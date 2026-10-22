#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patches/parts/claim_diagnostic_worker.h"
#include <cassert>
#include <condition_variable>
#include <future>
#include <stdexcept>
#include <vector>

using Worker = claim_trace::DiagnosticWorker<int>;
using namespace std::chrono_literals;

int main()
{
  using Receipt = claim_trace::DiagnosticReceipt;
  assert(claim_trace::PollDiagnosticReceipt(10, 5000, 9, true, 1000) == Receipt::pending);
  assert(claim_trace::PollDiagnosticReceipt(10, 5000, 10, true, 1000) == Receipt::recorded);
  assert(claim_trace::PollDiagnosticReceipt(10, 5000, 11, false, 1000) == Receipt::failed);
  assert(claim_trace::PollDiagnosticReceipt(10, 5000, 10, true, 5000) == Receipt::failed);
  assert(claim_trace::PollDiagnosticReceipt(0, 5000, 10, true, 1000) == Receipt::failed);
  // A blocked sink retains its charge. Saturation rejects without waiting for
  // the sink; accepted snapshots drain in FIFO order during orderly teardown.
  std::mutex              mutex;
  std::condition_variable changed;
  bool                    entered = false, release = false;
  std::vector<int>        observed;
  {
    Worker worker(
        [&](int value) {
          std::unique_lock lock(mutex);
          if (value == 1) {
            entered = true;
            changed.notify_all();
            changed.wait(lock, [&] { return release; });
          }
          observed.push_back(value);
        },
        {}, 2, 10);
    assert(worker.TrySubmit(1, 6));
    {
      std::unique_lock lock(mutex);
      assert(changed.wait_for(lock, 2s, [&] { return entered; }));
    }
    assert(worker.TrySubmit(2, 4));
    auto rejected = std::async(std::launch::async, [&] { return worker.TrySubmit(3, 1); });
    assert(rejected.wait_for(500ms) == std::future_status::ready);
    assert(!rejected.get());
    assert(worker.Pending() == 2 && worker.Charge() == 10);
    assert(!worker.Healthy() && worker.Rejected() == 1);
    {
      std::lock_guard lock(mutex);
      release = true;
    }
    changed.notify_all();
  }
  assert((observed == std::vector<int>{1, 2}));

  // A throwing sink is visible and does not strand the following accepted job.
  std::atomic<int> done{0}, polls{0};
  {
    Worker worker(
        [&](int value) {
          if (value == 1)
            throw std::runtime_error("simulated sink failure");
          done.store(value);
        },
        [&] { ++polls; }, 16, 100, 5ms);
    assert(worker.TrySubmit(1, 1));
    // The consumer may briefly own the queue lock: retry only in this test,
    // never in game admission, which deliberately drops instead of waiting.
    while (!worker.TrySubmit(2, 1))
      std::this_thread::yield();
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while ((!done.load() || !polls.load()) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    assert(done.load() == 2 && polls.load() > 0);
    assert(worker.Failures() == 1 && !worker.Healthy());
    worker.Stop();
    assert(!worker.TrySubmit(3, 1));
  }

  // Byte budget and single oversized items are independent of count limits.
  {
    Worker worker([](int) {}, {}, 100, 4);
    assert(!worker.TrySubmit(1, 5));
    assert(!worker.TrySubmit(2, 0));
    assert(worker.Pending() == 0 && worker.Charge() == 0 && worker.Rejected() == 2);
  }
  // Control-poller failure closes health independently of journal success.
  {
    std::atomic_bool firstPoll{true};
    Worker           worker([](int) {},
                            [&] {
                    if (firstPoll.exchange(false))
                      throw std::runtime_error("simulated control disk failure");
                            },
                            4, 4, 1ms);
    const auto       deadline = std::chrono::steady_clock::now() + 2s;
    while (!worker.Failures() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    assert(worker.Failures() == 1 && !worker.Healthy());
  }
}
