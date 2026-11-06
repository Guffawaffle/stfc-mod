#pragma once

#include "navigation_environment_art.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace navigation_environment_art
{
// Pure CPU work only. One active generation, one replaceable pending request,
// and one completion. Scene cancellation never joins or waits for generation.
class PixelGenerator
{
public:
  struct Key {
    Theme    theme;
    uint32_t seed;
    bool     operator==(const Key&) const = default;
  };
  using Pixels         = std::vector<Pixel>;
  using GeneratePixels = std::function<Pixels(Key)>;
  explicit PixelGenerator(GeneratePixels generate = [](Key key) { return Generate(1024, 512, key.theme, key.seed); })
      : generate_(std::move(generate))
  {
  }
  ~PixelGenerator()
  {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      pending_.reset();
    }
    wake_.notify_one();
    if (worker_.joinable())
      worker_.join();
  }
  uint64_t Request(Key key)
  {
    std::lock_guard lock(mutex_);
    if (key_ && *key_ == key)
      return generation_;
    key_                  = key;
    const auto generation = ++generation_;
    ready_.reset();
    pending_ = Work{generation, key};
    try {
      if (!worker_.joinable())
        worker_ = std::thread([this] { Run(); });
    } catch (...) {
      pending_.reset();
      ready_ = Result{generation, {}};
    }
    wake_.notify_one();
    return generation;
  }
  std::optional<Pixels> Take(uint64_t generation)
  {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock || !ready_ || ready_->generation != generation || generation != generation_)
      return {};
    auto pixels = std::move(ready_->pixels);
    ready_.reset();
    return pixels;
  }
  void Cancel()
  {
    std::lock_guard lock(mutex_);
    ++generation_;
    key_.reset();
    pending_.reset();
    ready_.reset();
  }

private:
  struct Work {
    uint64_t generation;
    Key      key;
  };
  struct Result {
    uint64_t generation;
    Pixels   pixels;
  };
  void Run() noexcept
  {
    for (;;) {
      Work work;
      {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
        if (stopping_)
          return;
        work = *pending_;
        pending_.reset();
      }
      Pixels pixels;
      try {
        pixels = generate_(work.key);
      } catch (...) {
      }
      {
        std::lock_guard lock(mutex_);
        if (!stopping_ && work.generation == generation_)
          ready_ = Result{work.generation, std::move(pixels)};
      }
    }
  }
  GeneratePixels          generate_;
  std::mutex              mutex_;
  std::condition_variable wake_;
  std::optional<Key>      key_;
  std::optional<Work>     pending_;
  std::optional<Result>   ready_;
  uint64_t                generation_ = 0;
  bool                    stopping_   = false;
  std::thread             worker_;
};
} // namespace navigation_environment_art
