#pragma once

#include "navigation_environment_art.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace navigation_environment_work
{
using Pixels                        = std::vector<navigation_environment_art::Pixel>;
inline constexpr int         kWidth = 1024, kHeight = 512;
inline constexpr std::size_t kPixels = std::size_t(kWidth) * kHeight;
static_assert(sizeof(navigation_environment_art::Pixel) == 4);

// One active pure generation, one replaceable pending name and one ready result.
// The native adapter deliberately gives this worker process lifetime. Tests can
// destroy it explicitly after releasing their generator, without loader-lock work.
class PixelWorker
{
public:
  using Generator = std::function<Pixels(int, int, navigation_environment_art::Theme, uint32_t)>;
  struct Result {
    std::shared_ptr<const Pixels> pixels;
    bool                          failed      = false;
    uint64_t                      generation  = 0;
    int64_t                       generatedMs = 0;
  };
  explicit PixelWorker(Generator generate = navigation_environment_art::Generate)
      : generate_(std::move(generate))
      , worker_([this] { Run(); })
  {
  }
  PixelWorker(const PixelWorker&)            = delete;
  PixelWorker& operator=(const PixelWorker&) = delete;
  ~PixelWorker()
  {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      ++generation_;
      pending_.reset();
    }
    changed_.notify_one();
    worker_.join();
  }

  Result Prepare(const std::string& name)
  {
    if (name.size() > 256)
      return {{}, true};
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
      return {}; // A later owner frame retries; never wait on generation.
    if (name != desired_ || !requested_) {
      desired_   = name;
      requested_ = true;
      ++generation_;
      pending_ = Job{name, generation_};
      ready_   = {};
      changed_.notify_one();
    }
    return ready_;
  }

  void Cancel()
  {
    std::lock_guard lock(mutex_);
    ++generation_;
    requested_ = false;
    desired_.clear();
    pending_.reset();
    ready_ = {};
  }

private:
  struct Job {
    std::string name;
    uint64_t    generation;
  };
  void Run() noexcept
  {
    for (;;) {
      Job job;
      {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
        if (stopping_)
          return;
        job = std::move(*pending_);
        pending_.reset();
      }
      Result result;
      result.generation  = job.generation;
      const auto started = std::chrono::steady_clock::now();
      try {
        auto pixels   = generate_(kWidth, kHeight, navigation_environment_art::SelectTheme(job.name),
                                  navigation_environment_art::Seed(job.name));
        result.failed = pixels.size() != kPixels || pixels.capacity() > kPixels;
        if (!result.failed)
          result.pixels = std::make_shared<const Pixels>(std::move(pixels));
      } catch (...) {
        result.failed = true;
      }
      result.generatedMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
      std::lock_guard lock(mutex_);
      if (!stopping_ && job.generation == generation_)
        ready_ = std::move(result);
    }
  }

  Generator               generate_;
  std::mutex              mutex_;
  std::condition_variable changed_;
  std::string             desired_;
  bool                    requested_ = false, stopping_ = false;
  uint64_t                generation_ = 0;
  std::optional<Job>      pending_;
  Result                  ready_;
  std::thread             worker_;
};

struct PoolIdentity {
  const void *owner{}, *pool{}, *background{};
  std::size_t count{};
  int         mask{}, layer{};
  bool        operator==(const PoolIdentity&) const = default;
};
struct PoolChoice {
  enum State { pending, exhausted, matched } state = pending;
  std::size_t index                                = 0;
  bool        background                           = false;
};

class OrderedPool
{
public:
  void Reset()
  { bound_ = false; }
  template <class Check> PoolChoice Advance(PoolIdentity identity, std::size_t& budget, Check check)
  {
    if (!bound_ || identity != identity_) {
      identity_   = identity;
      cursor_     = 0;
      background_ = true;
      bound_      = true;
    }
    if (background_ && budget) {
      --budget;
      background_ = false;
      if (check(true, 0))
        return {PoolChoice::matched, 0, true};
    }
    while (!background_ && cursor_ < identity.count && budget) {
      --budget;
      const auto index = cursor_++;
      if (check(false, index))
        return {PoolChoice::matched, index, false};
    }
    return {!background_ && cursor_ == identity.count ? PoolChoice::exhausted : PoolChoice::pending};
  }

private:
  bool         bound_ = false, background_ = true;
  std::size_t  cursor_ = 0;
  PoolIdentity identity_;
};
} // namespace navigation_environment_work
