#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace claim_trace
{
enum class DiagnosticReceipt { pending, recorded, failed };
inline DiagnosticReceipt PollDiagnosticReceipt(uint64_t sequence, int64_t deadline, uint64_t flushed, bool healthy,
                                               int64_t now)
{
  if (!sequence || !healthy || now >= deadline)
    return DiagnosticReceipt::failed;
  return flushed >= sequence ? DiagnosticReceipt::recorded : DiagnosticReceipt::pending;
}

// Claim journals share one ordered consumer. Game callers never wait for the
// consumer or a queue lock. The charge includes the in-flight item, so a slow
// sink cannot retain an unbounded backlog. Only owned snapshots may be submitted.
template <class T> class DiagnosticWorker
{
public:
  using Handler = std::function<void(T)>;
  using Poller  = std::function<void()>;
  DiagnosticWorker(Handler handler, Poller poller, size_t countLimit, size_t chargeLimit,
                   std::chrono::milliseconds interval = std::chrono::milliseconds(500))
      : handler_(std::move(handler))
      , poller_(std::move(poller))
      , countLimit_(countLimit)
      , chargeLimit_(chargeLimit)
      , interval_(interval)
      , worker_([this] { Run(); })
  {
  }
  // Explicit owners/tests drain at destruction. The hooked game adapter has
  // process lifetime, like RuntimeConfigWriter; it never joins under loader lock.
  ~DiagnosticWorker()
  {
    Stop();
    worker_.join();
  }
  DiagnosticWorker(const DiagnosticWorker&)            = delete;
  DiagnosticWorker& operator=(const DiagnosticWorker&) = delete;

  bool TrySubmit(T value, size_t charge) noexcept
  {
    try {
      std::unique_lock lock(mutex_, std::try_to_lock);
      if (!lock.owns_lock() || stopping_ || !charge || charge > chargeLimit_ || count_ >= countLimit_
          || charge_ > chargeLimit_ - charge) {
        ++rejected_;
        healthy_.store(false);
        return false;
      }
      queue_.push_back({std::move(value), charge});
      ++count_;
      charge_ += charge;
      wake_.notify_one();
      return true;
    } catch (...) {
      ++rejected_;
      healthy_.store(false);
      return false;
    }
  }
  void Stop()
  {
    std::lock_guard lock(mutex_);
    stopping_ = true;
    wake_.notify_one();
  }
  bool Healthy() const
  { return healthy_.load(); }
  uint64_t Rejected() const
  { return rejected_.load(); }
  uint64_t Failures() const
  { return failures_.load(); }
  size_t Pending() const
  { return count_.load(); }
  size_t Charge() const
  { return charge_.load(); }

private:
  struct Entry {
    T      value;
    size_t charge;
  };
  void Run() noexcept
  {
    auto nextPoll = std::chrono::steady_clock::now();
    for (;;) {
      Entry item{};
      bool  haveItem = false, poll = false;
      {
        std::unique_lock lock(mutex_);
        wake_.wait_until(lock, nextPoll, [&] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty())
          return;
        if (!queue_.empty()) {
          item = std::move(queue_.front());
          queue_.pop_front();
          haveItem = true;
        }
        const auto now = std::chrono::steady_clock::now();
        poll           = !stopping_ && now >= nextPoll;
        if (poll)
          nextPoll = now + interval_;
      }
      try {
        if (haveItem)
          handler_(std::move(item.value));
      } catch (...) {
        ++failures_;
        healthy_.store(false);
      }
      if (haveItem) {
        std::lock_guard lock(mutex_);
        --count_;
        charge_ -= item.charge;
      }
      try {
        if (poll && poller_)
          poller_();
      } catch (...) {
        ++failures_;
        healthy_.store(false);
      }
    }
  }
  Handler                         handler_;
  Poller                          poller_;
  const size_t                    countLimit_, chargeLimit_;
  const std::chrono::milliseconds interval_;
  std::mutex                      mutex_;
  std::condition_variable         wake_;
  std::deque<Entry>               queue_;
  bool                            stopping_ = false;
  std::atomic_bool                healthy_{true};
  std::atomic<uint64_t>           rejected_{0}, failures_{0};
  std::atomic<size_t>             count_{0}, charge_{0};
  std::thread                     worker_;
};
} // namespace claim_trace
