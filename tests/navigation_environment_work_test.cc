#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patches/navigation_environment_work.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <stdexcept>

using namespace navigation_environment_work;
using namespace std::chrono_literals;

template <class Predicate> void Until(Predicate predicate)
{
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (!predicate()) {
    assert(std::chrono::steady_clock::now() < deadline);
    std::this_thread::sleep_for(1ms);
  }
}

static void LatestOnlyAndCancellation()
{
  std::mutex              mutex;
  std::condition_variable changed;
  bool                    entered = false, release = false;
  std::vector<uint32_t>   calls;
  std::atomic_int         active{0}, peak{0};
  const auto              owner = std::this_thread::get_id();
  PixelWorker             worker([&](int width, int height, auto, uint32_t seed) {
    assert(std::this_thread::get_id() != owner);
    assert(width == 1024 && height == 512);
    const auto count = ++active;
    peak.store(std::max(peak.load(), count));
    {
      std::unique_lock lock(mutex);
      calls.push_back(seed);
      if (seed == navigation_environment_art::Seed("active")) {
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return release; });
      }
    }
    Pixels result(kPixels);
    result.front().r = static_cast<uint8_t>(seed);
    --active;
    return result;
  });
  worker.Prepare("active");
  {
    std::unique_lock lock(mutex);
    assert(changed.wait_for(lock, 2s, [&] { return entered; }));
  }
  Until([&] {
    worker.Prepare("replaced");
    return true;
  });
  // Wait-free owner retries may report busy; repeating the latest key must not
  // create duplicate tasks while the active generator remains blocked.
  for (int i = 0; i < 20; ++i)
    worker.Prepare("latest");
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  changed.notify_all();
  PixelWorker::Result ready;
  Until([&] {
    ready = worker.Prepare("latest");
    return bool(ready.pixels);
  });
  assert(peak == 1 && ready.pixels->size() * sizeof(navigation_environment_art::Pixel) == 2 * 1024 * 1024);
  {
    std::lock_guard lock(mutex);
    assert(calls.size() == 2 && calls[1] == navigation_environment_art::Seed("latest"));
  }
  const auto immutable  = ready.pixels;
  const auto generation = ready.generation;
  worker.Cancel();
  assert(immutable->front().r == static_cast<uint8_t>(navigation_environment_art::Seed("latest")));
  Until([&] {
    ready = worker.Prepare("latest");
    return bool(ready.pixels);
  });
  assert(ready.generation != generation && ready.pixels != immutable);
}

static void ClearWhileActiveAndFailure()
{
  std::mutex              mutex;
  std::condition_variable changed;
  bool                    entered = false, release = false;
  std::vector<uint32_t>   calls;
  PixelWorker             worker([&](int, int, auto, uint32_t seed) {
    std::unique_lock lock(mutex);
    calls.push_back(seed);
    if (seed == navigation_environment_art::Seed("cancel-active")) {
      entered = true;
      changed.notify_all();
      changed.wait(lock, [&] { return release; });
    }
    if (seed == navigation_environment_art::Seed("throw"))
      throw std::runtime_error("fixture");
    return Pixels(kPixels);
  });
  worker.Prepare("cancel-active");
  {
    std::unique_lock lock(mutex);
    assert(changed.wait_for(lock, 2s, [&] { return entered; }));
  }
  worker.Prepare("cancel-pending");
  worker.Cancel();
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  changed.notify_all();
  PixelWorker::Result ready;
  Until([&] {
    ready = worker.Prepare("replacement");
    return bool(ready.pixels);
  });
  {
    std::lock_guard lock(mutex);
    assert(calls.size() == 2 && calls[1] == navigation_environment_art::Seed("replacement"));
  }
  Until([&] {
    ready = worker.Prepare("throw");
    return ready.failed;
  });
  assert(!ready.pixels);
  const auto failedGeneration = ready.generation;
  for (int i = 0; i < 100; ++i) {
    ready = worker.Prepare("throw");
    if (ready.generation)
      assert(ready.failed && ready.generation == failedGeneration);
  }
  assert(worker.Prepare(std::string(257, 'x')).failed);
}

static void OrderedSelection()
{
  int          owner1 = 0, owner2 = 0, pool1 = 0, backdrop = 0;
  PoolIdentity identity{&owner1, &pool1, &backdrop, 128, 7, 2};
  OrderedPool  scan;
  size_t       checked = 0;
  PoolChoice   choice;
  int          frame = 0;
  do {
    size_t budget = 5, frameChecks = 0;
    choice = scan.Advance(identity, budget, [&](bool background, size_t index) {
      ++checked;
      ++frameChecks;
      return !background && index == 127;
    });
    assert(frameChecks <= 5);
    ++frame;
  } while (choice.state == PoolChoice::pending);
  assert(choice.state == PoolChoice::matched && choice.index == 127 && frame == 26 && checked == 129);
  scan.Reset();
  size_t budget = 5;
  choice = scan.Advance(identity, budget, [](bool background, size_t index) { return !background && index == 3; });
  assert(choice.state == PoolChoice::matched && choice.index == 3); // Earlier streamed art wins a new pass.

  // A retained cold discovery cohort must finish owner1's incompatible pool,
  // then continue to a compatible later loader instead of restarting owner1.
  scan.Reset();
  size_t ownerCursor = 0;
  for (int pass = 0; pass < 27 && ownerCursor != 2; ++pass) {
    size_t remaining = 5;
    while (remaining && ownerCursor < 2) {
      auto current  = identity;
      current.owner = ownerCursor == 0 ? &owner1 : &owner2;
      current.count = ownerCursor == 0 ? 128 : 0;
      choice =
          scan.Advance(current, remaining, [&](bool background, size_t) { return ownerCursor == 1 && background; });
      if (choice.state == PoolChoice::pending)
        break;
      if (choice.state == PoolChoice::matched) {
        ownerCursor = 2;
        break;
      }
      ++ownerCursor;
    }
  }
  assert(ownerCursor == 2 && choice.state == PoolChoice::matched);
  // Each invalidation restarts ordered qualification before accepting a result.
  for (int change = 0; change < 6; ++change) {
    auto changedIdentity = identity;
    if (change == 0)
      changedIdentity.owner = &owner2;
    if (change == 1)
      changedIdentity.pool = &owner2;
    if (change == 2)
      changedIdentity.background = &owner2;
    if (change == 3)
      ++changedIdentity.mask;
    if (change == 4)
      ++changedIdentity.layer;
    if (change == 5)
      --changedIdentity.count;
    size_t remaining = 1;
    choice           = scan.Advance(changedIdentity, remaining, [](bool background, size_t) {
      assert(background);
      return true;
    });
    assert(choice.state == PoolChoice::matched && choice.background);
  }
}

static void RealGenerator()
{
  PixelWorker         worker;
  PixelWorker::Result ready;
  Until([&] {
    ready = worker.Prepare("SystemAmbientSmoke");
    return ready.failed || bool(ready.pixels);
  });
  assert(ready.pixels && !ready.failed && ready.pixels->size() == kPixels);
  const auto seed = navigation_environment_art::Seed("SystemAmbientSmoke");
  const auto expected =
      navigation_environment_art::Sample(navigation_environment_art::SkyDirection(0, 0),
                                         navigation_environment_art::SelectTheme("SystemAmbientSmoke"), seed);
  assert(ready.pixels->front().r == expected.r && ready.pixels->front().a == 255);
  for (int row = 0; row < kHeight; ++row) {
    const auto first = (*ready.pixels)[std::size_t(row) * kWidth];
    const auto last  = (*ready.pixels)[std::size_t(row) * kWidth + kWidth - 1];
    assert(first.r == last.r && first.g == last.g && first.b == last.b);
  }
}

int main()
{
  LatestOnlyAndCancellation();
  ClearWhileActiveAndFailure();
  OrderedSelection();
  RealGenerator();
}
