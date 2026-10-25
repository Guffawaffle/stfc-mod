#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patches/parts/claim_diagnostic_worker.h"
#include <algorithm>
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
  using Rejection = claim_trace::DiagnosticRejection;
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
    assert(worker.Rejected(Rejection::count_limit) == 1);
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
    assert(worker.TrySubmit(2, 1));
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
    assert(worker.Rejected(Rejection::invalid_charge) == 2);
  }
  // A byte limit must be distinguishable from count saturation and disk failure.
  {
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    Worker worker([&](int) { entered.set_value(); released.wait(); }, {}, 8, 10);
    assert(worker.TrySubmit(1, 6));
    assert(entered.get_future().wait_for(2s) == std::future_status::ready);
    assert(!worker.TrySubmit(2, 5));
    assert(worker.Rejected(Rejection::byte_limit) == 1 && worker.Failures() == 0);
    release.set_value();
  }
  // Independent producers must not drop jobs just because another producer or
  // the consumer is active. Hold the sink and keep the batch below both budgets.
  {
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    std::vector<int> values;
    {
      Worker worker([&](int value) {
        if (value == 0) { entered.set_value(); released.wait(); }
        values.push_back(value);
      }, {}, 1024, 1024);
      assert(worker.TrySubmit(0, 1));
      assert(entered.get_future().wait_for(2s) == std::future_status::ready);
      std::vector<std::thread> producers;
      for (int producer = 0; producer < 4; ++producer)
        producers.emplace_back([&, producer] {
          for (int i = 1; i <= 128; ++i)
            assert(worker.TrySubmit(producer * 128 + i, 1));
        });
      for (auto& producer : producers) producer.join();
      assert(worker.Rejected() == 0 && worker.Pending() == 513);
      release.set_value();
    }
    std::sort(values.begin(), values.end());
    assert(values.size() == 513);
    for (int i = 0; i <= 512; ++i) assert(values[i] == i);
  }
  // Reusing a small ring must preserve FIFO order through many wraps.
  {
    std::atomic<int> observed{0};
    Worker worker([&](int value) {
      assert(value == observed.load() + 1);
      observed.store(value);
    }, {}, 4, 4);
    for (int i = 1; i <= 1000; ++i) {
      assert(worker.TrySubmit(i, 1));
      const auto deadline = std::chrono::steady_clock::now() + 2s;
      while (observed.load() != i && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
      assert(observed.load() == i);
    }
    assert(worker.Rejected() == 0 && worker.Healthy());
  }
  // Concurrent producers also retain each producer's order across ring reuse.
  // Capacity retries belong only to this stress test, never game admission.
  {
    std::array<int, 4> seen{};
    {
      Worker worker([&](int value) {
        const int producer = value / 10000;
        const int sequence = value % 10000;
        assert(sequence == ++seen[producer]);
      }, {}, 32, 32);
      std::vector<std::thread> producers;
      for (int producer = 0; producer < 4; ++producer)
        producers.emplace_back([&, producer] {
          for (int i = 1; i <= 1000; ++i) {
            const auto deadline = std::chrono::steady_clock::now() + 2s;
            while (!worker.TrySubmit(producer * 10000 + i, 1)) {
              assert(std::chrono::steady_clock::now() < deadline);
              std::this_thread::yield();
            }
          }
        });
      for (auto& producer : producers) producer.join();
      assert(worker.Rejected(Rejection::contention) == 0);
    }
    for (int count : seen) assert(count == 1000);
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
