#include "sync/input_queue.h"

#include <atomic>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using sync_queue::Admission;
using sync_queue::BoundedQueue;
using namespace std::chrono_literals;

// Keep checks active in optimized/release builds. A hung concurrency regression
// must also fail locally instead of leaving a fixture process behind.
#define CHECK(expression)                                                                                              \
  do {                                                                                                                 \
    if (!(expression)) {                                                                                               \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #expression " failed\n";                                        \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (false)

struct Packet {
  int         sequence;
  bool        first;
  std::string body;
};

static void CapacityAndFirstSync()
{
  BoundedQueue<Packet> queue({3, 10, 8});
  int                  factories = 0;
  auto                 admit     = [&](int sequence, bool first, size_t charge) {
    return queue.TryEmplace(charge, [&] {
      ++factories;
      return Packet{sequence, first, std::to_string(sequence)};
    });
  };

  CHECK(admit(0, true, 6) == Admission::accepted);
  CHECK(admit(1, false, 4) == Admission::accepted);
  CHECK(admit(2, false, 1) == Admission::full); // Bytes, with a free count slot.
  CHECK(admit(2, false, 0) == Admission::oversized);
  CHECK(admit(2, false, 9) == Admission::oversized);
  CHECK(factories == 2); // Rejections must never copy the borrowed game payload.
  auto stats = queue.Stats();
  CHECK(stats.count == 2 && stats.bytes == 10);
  CHECK(stats.highWaterCount == 2 && stats.highWaterBytes == 10 && stats.rejected == 3);

  {
    auto first = queue.WaitPop();
    CHECK(first && first->value.first && first->value.sequence == 0 && first->value.body == "0");
    CHECK(queue.Stats().count == 2 && queue.Stats().bytes == 10);
    CHECK(admit(2, false, 1) == Admission::full); // Pop is not HTTP completion.
    {
      auto moved = std::move(*first);
      first.reset();
      CHECK(moved.value.sequence == 0 && moved.value.first);
      CHECK(queue.Stats().count == 2 && queue.Stats().bytes == 10);
    }
    CHECK(queue.Stats().count == 1 && queue.Stats().bytes == 4);
  }
  CHECK(admit(2, false, 6) == Admission::accepted);
  queue.Close();
  CHECK(admit(3, false, 1) == Admission::closed);
  CHECK(factories == 3);
  for (int expected : {1, 2}) {
    auto delta = queue.WaitPop();
    CHECK(delta && delta->value.sequence == expected && !delta->value.first);
    CHECK(delta->value.body == std::to_string(expected));
  }
  CHECK(!queue.WaitPop());
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);

  BoundedQueue<int> countLimit({1, 100, 100});
  CHECK(countLimit.TryEmplace(1, [] { return 7; }) == Admission::accepted);
  CHECK(countLimit.TryEmplace(1, [] { return 8; }) == Admission::full);
  countLimit.Close();
  CHECK(countLimit.WaitPop()->value == 7);
  CHECK(!countLimit.WaitPop());
}

static void FactoryAndHandlerFailure()
{
  BoundedQueue<int> queue({2, 16, 8});
  CHECK(queue.TryEmplace(8, []() -> int { throw std::bad_alloc(); }) == Admission::failed);
  CHECK(queue.Stats().failures == 1 && queue.Stats().rejected == 1);
  // A failed FIFO placeholder stays bounded until the consumer retires it.
  CHECK(queue.Stats().count == 1 && queue.Stats().bytes == 8);
  CHECK(queue.TryEmplace(8, [] { return 1; }) == Admission::accepted);
  try {
    auto payload = queue.WaitPop();
    CHECK(payload && payload->value == 1);
    throw std::runtime_error("simulated HTTP/handler failure");
  } catch (const std::runtime_error&) {
  }
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);
  CHECK(queue.TryEmplace(8, [] { return 2; }) == Admission::accepted);
  queue.Close();
  CHECK(queue.WaitPop()->value == 2);
  CHECK(!queue.WaitPop());
}

struct DestructionProbe {
  std::function<void()> destroy;
  explicit DestructionProbe(std::function<void()> callback)
      : destroy(std::move(callback))
  {
  }
  DestructionProbe(const DestructionProbe&) = delete;
  DestructionProbe(DestructionProbe&& other) noexcept
      : destroy(std::move(other.destroy))
  { other.destroy = {}; }
  ~DestructionProbe()
  {
    if (destroy)
      destroy();
  }
};

static void PayloadDiesBeforeChargeRelease()
{
  BoundedQueue<DestructionProbe> queue({1, 1, 1});
  int                            destroyed = 0;
  CHECK(queue.TryEmplace(1, [&] {
    return DestructionProbe([&] {
      // Reentrant statistics must be safe during payload destruction. Releasing
      // its reservation first would let another payload exceed the live budget.
      CHECK(queue.Stats().count == 1 && queue.Stats().bytes == 1);
      ++destroyed;
    });
  }) == Admission::accepted);
  {
    auto payload = queue.WaitPop();
    CHECK(payload);
  }
  CHECK(destroyed == 1 && queue.Stats().count == 0 && queue.Stats().bytes == 0);
  queue.Close();
  CHECK(!queue.WaitPop());
}

static void CloseWakesIdleConsumer()
{
  BoundedQueue<int>  queue({1, 1, 1});
  std::promise<void> entered;
  auto               consumer = std::async(std::launch::async, [&] {
    entered.set_value();
    return queue.WaitPop().has_value();
  });
  CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
  queue.Close();
  CHECK(consumer.wait_for(2s) == std::future_status::ready);
  CHECK(!consumer.get());
  int factories = 0;
  CHECK(queue.TryEmplace(1, [&] {
    ++factories;
    return 0;
  }) == Admission::closed);
  CHECK(factories == 0);
  queue.Close(); // Idempotent close, including after full drainage.
  CHECK(!queue.WaitPop());
}

static void StalledTargetDoesNotBlockAnother()
{
  BoundedQueue<Packet> stalled({2, 8, 8}), healthy({2, 8, 8});
  CHECK(stalled.TryEmplace(6, [] { return Packet{0, true, "first"}; }) == Admission::accepted);
  std::promise<void> entered, resume;
  auto               release = resume.get_future();
  std::vector<int>   stalledObserved;
  auto               blockedConsumer = std::async(std::launch::async, [&] {
    while (auto item = stalled.WaitPop()) {
      if (item->value.first) {
        entered.set_value();
        release.wait(); // The target's HTTP request has not completed.
      }
      stalledObserved.push_back(item->value.sequence);
    }
  });
  CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
  CHECK(stalled.Stats().count == 1 && stalled.Stats().bytes == 6);
  CHECK(stalled.TryEmplace(2, [] { return Packet{1, false, "delta"}; }) == Admission::accepted);
  auto rejection = std::async(std::launch::async,
                              [&] { return stalled.TryEmplace(1, [] { return Packet{2, false, "overflow"}; }); });
  CHECK(rejection.wait_for(500ms) == std::future_status::ready);
  CHECK(rejection.get() == Admission::full);

  CHECK(healthy.TryEmplace(4, [] { return Packet{0, true, "first"}; }) == Admission::accepted);
  CHECK(healthy.TryEmplace(4, [] { return Packet{1, false, "delta"}; }) == Admission::accepted);
  healthy.Close();
  auto healthyConsumer = std::async(std::launch::async, [&] {
    std::vector<int> observed;
    while (auto item = healthy.WaitPop())
      observed.push_back(item->value.sequence);
    return observed;
  });
  CHECK(healthyConsumer.wait_for(2s) == std::future_status::ready);
  CHECK((healthyConsumer.get() == std::vector<int>{0, 1}));
  CHECK(stalled.Stats().count == 2 && stalled.Stats().bytes == 8);

  stalled.Close(); // Closing an in-flight target preserves its accepted delta.
  CHECK(stalled.TryEmplace(1, [] { return Packet{2, false, "late"}; }) == Admission::closed);
  resume.set_value();
  CHECK(blockedConsumer.wait_for(2s) == std::future_status::ready);
  blockedConsumer.get();
  CHECK((stalledObserved == std::vector<int>{0, 1}));
  CHECK(stalled.Stats().count == 0 && stalled.Stats().bytes == 0);
}

static void ConcurrentAdmissionKeepsWholePackets()
{
  constexpr int producers = 4, each = 400;
  struct Message {
    int producer, sequence;
  };
  BoundedQueue<Message>    queue({8, 8, 1});
  std::atomic<int>         factoryCalls{0};
  auto                     consumer = std::async(std::launch::async, [&] {
    std::vector<int> next(producers, 0);
    int              delivered = 0;
    while (auto item = queue.WaitPop()) {
      const auto& message = item->value;
      CHECK(message.producer >= 0 && message.producer < producers);
      CHECK(message.sequence == next[message.producer]++);
      auto stats = queue.Stats();
      CHECK(stats.count >= 1 && stats.count <= 8 && stats.bytes == stats.count);
      ++delivered;
    }
    for (int count : next)
      CHECK(count == each);
    return delivered;
  });
  std::vector<std::thread> sources;
  for (int source = 0; source < producers; ++source) {
    sources.emplace_back([&, source] {
      for (int sequence = 0; sequence < each; ++sequence) {
        for (;;) {
          const auto admission = queue.TryEmplace(1, [&] {
            ++factoryCalls;
            return Message{source, sequence};
          });
          if (admission == Admission::accepted)
            break;
          CHECK(admission == Admission::full);
          // Retrying is only a fixture stress technique; native callbacks must
          // apply the caller's explicit overload policy without waiting.
          std::this_thread::yield();
        }
      }
    });
  }
  for (auto& source : sources)
    source.join();
  queue.Close();
  CHECK(consumer.wait_for(2s) == std::future_status::ready);
  CHECK(consumer.get() == producers * each && factoryCalls == producers * each);
  const auto stats = queue.Stats();
  CHECK(stats.count == 0 && stats.bytes == 0);
  CHECK(stats.highWaterCount <= 8 && stats.highWaterBytes <= 8 && stats.failures == 0);
}

static void ReservedFactoryKeepsOrder(bool firstFails)
{
  BoundedQueue<Packet> queue({2, 8, 8});
  std::promise<void>   entered, resume;
  auto                 release = resume.get_future();
  auto                 first   = std::async(std::launch::async, [&] {
    return queue.TryEmplace(4, [&]() -> Packet {
      entered.set_value();
      release.wait(); // Simulate a slow copy, with its reservation already held.
      if (firstFails)
        throw std::bad_alloc();
      return Packet{0, true, "first"};
    });
  });
  CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
  CHECK(queue.Stats().count == 1 && queue.Stats().bytes == 4);

  auto consumer = std::async(std::launch::async, [&] {
    std::vector<int> observed;
    while (auto item = queue.WaitPop())
      observed.push_back(item->value.sequence);
    return observed;
  });
  auto second =
      std::async(std::launch::async, [&] { return queue.TryEmplace(4, [] { return Packet{1, false, "delta"}; }); });
  // The slow copy cannot hold the metadata lock or block an independent copy.
  CHECK(second.wait_for(500ms) == std::future_status::ready);
  CHECK(second.get() == Admission::accepted);
  CHECK(queue.Stats().count == 2 && queue.Stats().bytes == 8);
  CHECK(queue.TryEmplace(1, [] { return Packet{2, false, "overflow"}; }) == Admission::full);
  CHECK(consumer.wait_for(50ms) == std::future_status::timeout); // Ready delta cannot overtake its snapshot.

  queue.Close(); // Close must drain reservations that were accepted before it.
  resume.set_value();
  CHECK(first.wait_for(2s) == std::future_status::ready);
  CHECK(first.get() == (firstFails ? Admission::failed : Admission::accepted));
  CHECK(consumer.wait_for(2s) == std::future_status::ready);
  const auto observed = consumer.get();
  CHECK((observed == (firstFails ? std::vector<int>{1} : std::vector<int>{0, 1})));
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);
  CHECK(queue.Stats().failures == (firstFails ? 1 : 0));
}

static void ByteArithmeticCannotWrap()
{
  const auto        maximum = std::numeric_limits<size_t>::max();
  BoundedQueue<int> queue({2, maximum, maximum});
  CHECK(queue.TryEmplace(maximum, [] { return 1; }) == Admission::accepted);
  int factories = 0;
  CHECK(queue.TryEmplace(1, [&] {
    ++factories;
    return 2;
  }) == Admission::full);
  CHECK(factories == 0 && queue.Stats().bytes == maximum);
  {
    auto item = queue.WaitPop();
    CHECK(item && item->value == 1);
  }
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);
  queue.Close();
  CHECK(!queue.WaitPop());
}

static void FailedCopiesCannotAccumulateBehindBlockedCopy()
{
  BoundedQueue<int>  queue({3, 9, 3});
  std::promise<void> entered, resume;
  auto               release = resume.get_future();
  auto               first   = std::async(std::launch::async, [&] {
    return queue.TryEmplace(3, [&] {
      entered.set_value();
      release.wait();
      return 7;
    });
  });
  CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
  for (int failed = 0; failed < 2; ++failed)
    CHECK(queue.TryEmplace(3, []() -> int { throw std::bad_alloc(); }) == Admission::failed);
  CHECK(queue.Stats().count == 3 && queue.Stats().bytes == 9);
  int factories = 0;
  CHECK(queue.TryEmplace(1, [&] {
    ++factories;
    return 8;
  }) == Admission::full);
  CHECK(factories == 0 && queue.Stats().failures == 2);
  queue.Close();
  resume.set_value();
  CHECK(first.wait_for(2s) == std::future_status::ready && first.get() == Admission::accepted);
  {
    auto item = queue.WaitPop();
    CHECK(item && item->value == 7);
  }
  CHECK(!queue.WaitPop()); // Retires both failed placeholders before reporting drainage.
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);
}

int main()
{
  // The watchdog is joined promptly on success. It uses no game/native runtime.
  std::mutex              watchdogMutex;
  std::condition_variable watchdogWake;
  bool                    finished = false;
  std::thread             watchdog([&] {
    std::unique_lock lock(watchdogMutex);
    if (!watchdogWake.wait_for(lock, 20s, [&] { return finished; })) {
      std::cerr << "sync bounded queue fixture timed out\n";
      std::_Exit(1);
    }
  });

  CapacityAndFirstSync();
  FactoryAndHandlerFailure();
  PayloadDiesBeforeChargeRelease();
  CloseWakesIdleConsumer();
  StalledTargetDoesNotBlockAnother();
  ConcurrentAdmissionKeepsWholePackets();
  ReservedFactoryKeepsOrder(false);
  ReservedFactoryKeepsOrder(true);
  ByteArithmeticCannotWrap();
  FailedCopiesCannotAccumulateBehindBlockedCopy();

  {
    std::lock_guard lock(watchdogMutex);
    finished = true;
  }
  watchdogWake.notify_one();
  watchdog.join();
  std::cout << "sync bounded queue checks passed\n";
}
