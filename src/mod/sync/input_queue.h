#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace sync_queue
{
enum class Admission { accepted, full, oversized, closed, failed };

struct Limits {
  size_t count;
  size_t bytes;
  size_t maxItemBytes;
};

struct Snapshot {
  size_t   count{}, bytes{}, highWaterCount{}, highWaterBytes{};
  uint64_t rejected{}, failures{}, oldestAgeMs{};
};

// One ordered consumer. A lease keeps its charge until the owned value is
// destroyed, including while the handler waits on HTTP. Factories run only
// after a FIFO position and budget are reserved. They may copy concurrently;
// the consumer still observes reservation order. Factories may synchronously
// copy borrowed bytes into owned data, never do I/O, parse a payload or call
// the game. The queue must outlive producer factories and consumer leases.
template <class T> class BoundedQueue
{
  using Clock = std::chrono::steady_clock;
  enum class State { copying, ready, failed };
  struct Entry {
    std::optional<T>  value;
    size_t            charge;
    Clock::time_point admitted;
    State             state = State::copying;
  };

public:
  class Lease
  {
    friend class BoundedQueue;
    std::unique_ptr<Entry> entry_;
    BoundedQueue*          owner_;
    Lease(std::unique_ptr<Entry> entry, BoundedQueue* owner) noexcept
        : entry_(std::move(entry))
        , owner_(owner)
        , value(*entry_->value)
    {
    }

  public:
    T& value;
    Lease(const Lease&)            = delete;
    Lease& operator=(const Lease&) = delete;
    Lease& operator=(Lease&&)      = delete;
    Lease(Lease&& other) noexcept
        : entry_(std::move(other.entry_))
        , owner_(std::exchange(other.owner_, nullptr))
        , value(other.value)
    {
    }
    ~Lease()
    {
      if (owner_) {
        const auto charge = entry_->charge;
        entry_.reset();
        owner_->Release(charge);
      }
    }
  };

  explicit BoundedQueue(Limits limits)
      : limits_(limits)
  {
  }
  BoundedQueue(const BoundedQueue&)            = delete;
  BoundedQueue& operator=(const BoundedQueue&) = delete;

  template <class Factory> Admission TryEmplace(size_t charge, Factory&& factory) noexcept
  {
    Entry* reserved = nullptr;
    {
      std::lock_guard lock(mutex_);
      if (closed_) {
        ++stats_.rejected;
        return Admission::closed;
      }
      if (!charge || charge > limits_.maxItemBytes || charge > limits_.bytes) {
        ++stats_.rejected;
        return Admission::oversized;
      }
      if (stats_.count >= limits_.count || stats_.bytes > limits_.bytes - charge) {
        ++stats_.rejected;
        return Admission::full;
      }
      try {
        auto entry = std::make_unique<Entry>(Entry{std::nullopt, charge, Clock::now()});
        reserved   = entry.get();
        queue_.push_back(std::move(entry));
      } catch (...) {
        ++stats_.rejected;
        ++stats_.failures;
        return Admission::failed;
      }
      ++stats_.count;
      stats_.bytes += charge;
      stats_.highWaterCount = std::max(stats_.highWaterCount, stats_.count);
      stats_.highWaterBytes = std::max(stats_.highWaterBytes, stats_.bytes);
    }
    // The placeholder reserves both budget and FIFO position. Later producers
    // can copy concurrently without waiting behind a large earlier payload.
    // Only publishing readiness uses the metadata lock.
    try {
      reserved->value.emplace(std::forward<Factory>(factory)());
    } catch (...) {
      std::lock_guard lock(mutex_);
      reserved->state = State::failed;
      ++stats_.rejected;
      ++stats_.failures;
      wake_.notify_all();
      return Admission::failed;
    }
    {
      std::lock_guard lock(mutex_);
      reserved->state = State::ready;
      wake_.notify_all();
    }
    return Admission::accepted;
  }

  std::optional<Lease> WaitPop()
  {
    std::unique_lock lock(mutex_);
    for (;;) {
      wake_.wait(lock, [&] {
        return (!inFlight_ && !queue_.empty() && queue_.front()->state != State::copying)
               || (closed_ && queue_.empty());
      });
      if (queue_.empty())
        return std::nullopt;
      auto entry = std::move(queue_.front());
      queue_.pop_front();
      if (entry->state == State::failed) {
        // Failed placeholders remain charged until retired, so repeated factory
        // failures behind an earlier copy cannot create an unbounded list.
        --stats_.count;
        stats_.bytes -= entry->charge;
        continue;
      }
      inFlight_      = true;
      inFlightSince_ = entry->admitted;
      return Lease(std::move(entry), this);
    }
  }

  // The uploader also serves ordinary state. Optional observations must not
  // park it behind an unfinished producer copy or an empty diagnostic queue.
  std::optional<Lease> TryPop()
  {
    std::lock_guard lock(mutex_);
    while (!inFlight_ && !queue_.empty() && queue_.front()->state != State::copying) {
      auto entry = std::move(queue_.front());
      queue_.pop_front();
      if (entry->state == State::failed) {
        --stats_.count;
        stats_.bytes -= entry->charge;
        continue;
      }
      inFlight_      = true;
      inFlightSince_ = entry->admitted;
      return Lease(std::move(entry), this);
    }
    return std::nullopt;
  }

  // Stop admission and drain the accepted prefix. Does not wait for a handler
  // and must never be used as a join from a DLL loader callback.
  void Close()
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
    wake_.notify_all();
  }

  Snapshot Stats() const
  {
    std::lock_guard lock(mutex_);
    auto            snapshot = stats_;
    if (inFlight_ || !queue_.empty()) {
      const auto oldest    = inFlight_ ? inFlightSince_ : queue_.front()->admitted;
      snapshot.oldestAgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - oldest).count();
    }
    return snapshot;
  }

private:
  void Release(size_t charge)
  {
    std::lock_guard lock(mutex_);
    --stats_.count;
    stats_.bytes -= charge;
    inFlight_ = false;
    wake_.notify_all();
  }

  const Limits                       limits_;
  mutable std::mutex                 mutex_;
  std::condition_variable            wake_;
  std::deque<std::unique_ptr<Entry>> queue_;
  Snapshot                           stats_;
  bool                               closed_   = false;
  bool                               inFlight_ = false;
  Clock::time_point                  inFlightSince_{};
};
} // namespace sync_queue
