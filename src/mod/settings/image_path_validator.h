#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace mod_settings
{
// One active filesystem probe and one replaceable, debounced request. Only
// owned UTF-8 paths cross threads; a stalled network stat cannot create workers
// or an accumulating queue. Native UI and setting writes remain on the owner.
class ImagePathValidator
{
public:
  enum class State { Pending, Valid, Invalid, Failed, Stale };
  struct Result {
    uint64_t generation = 0;
    State    state      = State::Stale;
  };
  using Probe = std::function<bool(const std::string&)>;
  explicit ImagePathValidator(Probe                     probe    = RegularFile,
                              std::chrono::milliseconds debounce = std::chrono::milliseconds(250))
      : probe_(std::move(probe))
      , debounce_(debounce)
  {
  }
  // Explicit fixture owners only. The game service below has process lifetime
  // and never joins a potentially stalled filesystem probe on a UI callback.
  ~ImagePathValidator()
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
  ImagePathValidator(const ImagePathValidator&)            = delete;
  ImagePathValidator& operator=(const ImagePathValidator&) = delete;

  Result Check(std::string_view path)
  {
    std::lock_guard lock(mutex_);
    if (stopping_)
      return {};
    if (has_path_ && path == path_)
      return {generation_, state_};
    ++generation_;
    pending_.reset();
    path_.clear();
    has_path_ = false;
    if (path.size() > MaximumPathBytes) {
      state_ = State::Invalid;
      return {generation_, state_};
    }
    path_     = path;
    has_path_ = true;
    state_    = path.empty() ? State::Valid : State::Pending;
    if (!path.empty()) {
      try {
        pending_ = Work{generation_, path_, Clock::now() + debounce_};
        if (!worker_.joinable())
          worker_ = std::thread([this] { Run(); });
        wake_.notify_one();
      } catch (...) {
        pending_.reset();
        state_ = State::Failed;
      }
    }
    return {generation_, state_};
  }
  Result Get(uint64_t generation)
  {
    std::lock_guard lock(mutex_);
    return generation == generation_ ? Result{generation_, state_} : Result{};
  }
  void Cancel()
  {
    std::lock_guard lock(mutex_);
    ++generation_;
    path_.clear();
    has_path_ = false;
    pending_.reset();
    state_ = State::Stale;
    wake_.notify_one();
  }
  uint64_t Sequence() const noexcept
  { return sequence_.load(std::memory_order_acquire); }
  static constexpr size_t MaximumPathBytes = 128 * 1024;

private:
  using Clock = std::chrono::steady_clock;
  struct Work {
    uint64_t          generation;
    std::string       path;
    Clock::time_point ready;
  };
  static bool RegularFile(const std::string& path)
  {
    std::error_code error;
    const auto      utf8 = std::u8string_view(reinterpret_cast<const char8_t*>(path.data()), path.size());
    return std::filesystem::is_regular_file(std::filesystem::path(utf8), error) && !error;
  }
  void Run() noexcept
  {
    for (;;) {
      Work work;
      {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
        if (stopping_)
          return;
        const auto generation = pending_->generation;
        const auto ready      = pending_->ready;
        if (wake_.wait_until(lock, ready, [&] { return stopping_ || !pending_ || pending_->generation != generation; }))
          continue;
        work = std::move(*pending_);
        pending_.reset();
      }
      State result = State::Failed;
      try {
        result = probe_(work.path) ? State::Valid : State::Invalid;
      } catch (...) {
      }
      {
        std::lock_guard lock(mutex_);
        if (work.generation == generation_) {
          state_ = result;
          sequence_.fetch_add(1, std::memory_order_release);
        }
      }
    }
  }
  Probe                     probe_;
  std::chrono::milliseconds debounce_;
  std::mutex                mutex_;
  std::condition_variable   wake_;
  std::optional<Work>       pending_;
  std::string               path_;
  uint64_t                  generation_ = 0;
  State                     state_      = State::Stale;
  bool                      stopping_   = false;
  bool                      has_path_   = false;
  std::atomic_uint64_t      sequence_{0};
  std::thread               worker_;
};

inline ImagePathValidator& LoadingImagePathValidator()
{
  static auto* validator = new ImagePathValidator;
  return *validator;
}
} // namespace mod_settings
