#include "features/navigation_environment_pixels.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace navigation_environment_art;

namespace
{
unsigned checks = 0;

void Check(bool condition, const char* message)
{
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}

struct BlockedGeneration {
  std::mutex                       mutex;
  std::condition_variable          changed;
  std::vector<PixelGenerator::Key> calls;
  std::set<std::thread::id>        workers;
  std::set<size_t>                 released;
  size_t                           active = 0, maximum_active = 0;
  bool                             release_all = false;

  PixelGenerator::Pixels Generate(PixelGenerator::Key key)
  {
    std::unique_lock lock(mutex);
    const size_t     invocation = calls.size() + 1;
    calls.push_back(key);
    workers.insert(std::this_thread::get_id());
    ++active;
    maximum_active = std::max(maximum_active, active);
    changed.notify_all();
    changed.wait(lock, [&] { return release_all || released.contains(invocation); });
    --active;
    changed.notify_all();
    return {{uint8_t(key.seed), uint8_t(key.seed >> 8), uint8_t(key.theme), 255}};
  }
  void WaitForCall(size_t count)
  {
    std::unique_lock lock(mutex);
    Check(changed.wait_for(lock, 5s, [&] { return calls.size() >= count; }),
          "controlled generation enters before watchdog");
  }
  void Release(size_t invocation)
  {
    std::lock_guard lock(mutex);
    released.insert(invocation);
    changed.notify_all();
  }
  void ReleaseAll()
  {
    std::lock_guard lock(mutex);
    release_all = true;
    changed.notify_all();
  }
};

// Declared after PixelGenerator, so assertion failure releases its fake CPU work before destruction joins it.
struct ReleaseBeforeJoin {
  std::shared_ptr<BlockedGeneration> state;
  ~ReleaseBeforeJoin()
  { state->ReleaseAll(); }
};

template <typename F> auto WhileWorkerBlocked(const std::shared_ptr<BlockedGeneration>& state, F operation)
{
  auto       future   = std::async(std::launch::async, std::move(operation));
  const bool returned = future.wait_for(2s) == std::future_status::ready;
  if (!returned)
    state->ReleaseAll(); // Also make a blocking regression safe to diagnose and tear down.
  auto result = future.get();
  Check(returned, "controller operation finishes while generation is deliberately blocked");
  {
    std::lock_guard lock(state->mutex);
    Check(state->active == 1 && !state->release_all, "controller completion is observed during the blocked interval");
  }
  return result;
}

PixelGenerator::Pixels Await(PixelGenerator& generator, uint64_t ticket)
{
  // The public API intentionally exposes polling, not a completion notification. Yield until it publishes;
  // this watchdog does not assume that publication follows a callback notification within a fixed sleep.
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  do {
    if (auto result = generator.Take(ticket)) {
      Check(true, "current result becomes available");
      return std::move(*result);
    }
    std::this_thread::yield();
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("pixel publication exceeded watchdog");
}

void BlockingCancellationAndReplacement()
{
  using Key = PixelGenerator::Key;
  constexpr Key  a{Theme::Cool, 11}, b{Theme::Warm, 22}, c{Theme::Purple, 22}, d{Theme::Vger, 44}, e{Theme::Cool, 55};
  auto           state = std::make_shared<BlockedGeneration>();
  PixelGenerator generator([state](Key key) { return state->Generate(key); });
  ReleaseBeforeJoin release{state};
  {
    std::lock_guard lock(state->mutex);
    Check(state->calls.empty(), "construction starts no generation");
  }
  const auto first = generator.Request(a);
  Check(first != 0, "first request has a nonzero ticket");
  state->WaitForCall(1);
  const auto same = WhileWorkerBlocked(state, [&] { return generator.Request(a); });
  Check(same == first, "same active key returns the existing ticket without restart");
  Check(!generator.Take(first), "blocked generation has no result");

  const auto replacements                        = WhileWorkerBlocked(state, [&] {
    generator.Cancel();
    const auto second     = generator.Request(b);
    const auto third      = generator.Request(c);
    const auto same_third = generator.Request(c);
    const auto fourth     = generator.Request(d);
    return std::array{second, third, same_third, fourth};
  });
  const auto [second, third, same_third, fourth] = replacements;
  Check(second > first && third > second && fourth > third, "cancel and replacements advance ticket generations");
  Check(same_third == third, "same pending key retains its ticket");
  {
    std::lock_guard lock(state->mutex);
    Check(state->calls.size() == 1 && state->calls.front() == a,
          "pending replacement never starts another worker alongside blocked active work");
  }

  state->Release(1);
  state->WaitForCall(2);
  {
    std::lock_guard lock(state->mutex);
    Check(state->calls.size() == 2 && state->calls.back() == d,
          "only latest pending request runs; intermediate keys are never generated");
    Check(state->maximum_active == 1 && state->workers.size() == 1,
          "one persistent worker runs at most one generation at a time");
    Check(!state->workers.contains(std::this_thread::get_id()), "generation runs off the controller thread");
  }
  // Reaching the second callback proves the first callback returned and its completion was reconciled.
  Check(!generator.Take(first) && !generator.Take(second) && !generator.Take(third),
        "cancelled and replaced tickets cannot retrieve the stale active completion");
  Check(!generator.Take(fourth), "latest blocked generation has not published early");
  state->Release(2);
  const auto pixels = Await(generator, fourth);
  Check(pixels.size() == 1 && pixels.front().r == d.seed && pixels.front().b == uint8_t(d.theme),
        "latest ticket receives its own generated values");
  Check(!generator.Take(fourth), "completion is consumed only once");

  generator.Cancel();
  const auto restarted = generator.Request(d);
  Check(restarted > fourth, "cancel permits a fresh generation of the same key");
  state->WaitForCall(3);
  const auto last = WhileWorkerBlocked(state, [&] {
    generator.Cancel();
    return generator.Request(e);
  });
  state->Release(3);
  state->WaitForCall(4);
  Check(!generator.Take(restarted), "second cancelled active result is also rejected");
  state->Release(4);
  const auto final_pixels = Await(generator, last);
  Check(final_pixels.size() == 1 && final_pixels.front().r == e.seed,
        "worker remains usable through cancellation and same-key restart");
  {
    std::lock_guard lock(state->mutex);
    Check(state->calls.size() == 4 && state->calls[2] == d && state->calls[3] == e,
          "observed call history contains exactly the active and latest jobs");
    Check(state->maximum_active == 1 && state->workers.size() == 1,
          "all generations reuse one worker without overlapping work");
  }
}

void DefaultPixelsMatchSynchronousArtwork()
{
  const PixelGenerator::Key key{Theme::Purple, Seed("pixel-worker-regression")};
  PixelGenerator            generator;
  const auto                ticket   = generator.Request(key);
  const auto                expected = Generate(1024, 512, key.theme, key.seed);
  const auto                actual   = Await(generator, ticket);
  Check(actual.size() == 1024u * 512u && actual.size() == expected.size(),
        "default worker retains the full production artwork dimensions");
  static_assert(sizeof(Pixel) == 4);
  Check(std::memcmp(actual.data(), expected.data(), expected.size() * sizeof(Pixel)) == 0,
        "default worker pixels exactly match synchronous Generate");
}
} // namespace

int main()
try {
  BlockingCancellationAndReplacement();
  DefaultPixelsMatchSynchronousArtwork();
  std::cout << "PixelGenerator: " << checks << " checks passed\n";
  return 0;
} catch (const std::exception& error) {
  std::cerr << "PixelGenerator failed after " << checks << " checks: " << error.what() << '\n';
  return 1;
}
