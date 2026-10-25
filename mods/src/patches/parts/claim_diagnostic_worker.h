#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <optional>
#include <semaphore>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <thread>
#include <utility>

namespace claim_trace
{
enum class DiagnosticReceipt { pending, recorded, failed };
enum class DiagnosticRejection { contention, stopped, invalid_charge, count_limit, byte_limit, exception, count };
inline DiagnosticReceipt PollDiagnosticReceipt(uint64_t sequence, int64_t deadline, uint64_t flushed, bool healthy,
                                               int64_t now)
{
  if (!sequence || !healthy || now >= deadline)
    return DiagnosticReceipt::failed;
  return flushed >= sequence ? DiagnosticReceipt::recorded : DiagnosticReceipt::pending;
}

// Claim journals share one ordered consumer and a preallocated MPSC ring.
// Admission uses atomic reservations, never a consumer-owned queue mutex. The charge includes the in-flight item, so a slow
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
      , slots_(std::make_unique<Slot[]>(countLimit))
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
    if (!charge || charge > chargeLimit_)
      return Reject(DiagnosticRejection::invalid_charge);
    // Count includes reservations and the consumer's in-flight job. Reserving
    // before checking stop prevents shutdown from missing an accepted producer.
    if (count_.fetch_add(1) >= countLimit_) {
      --count_;
      return Reject(DiagnosticRejection::count_limit);
    }
    if (stopping_.load()) {
      --count_;
      Signal();
      return Reject(DiagnosticRejection::stopped);
    }
    auto used = charge_.load();
    for (;;) {
      if (used > chargeLimit_ - charge) {
        --count_;
        Signal();
        return Reject(DiagnosticRejection::byte_limit);
      }
      if (charge_.compare_exchange_weak(used, used + charge))
        break;
    }
    // At most countLimit reservations exist, and the consumer releases count
    // only after freeing its slot and completing the handler. Thus each ticket
    // owns a free slot, even when producers publish out of ticket order.
    auto& slot = slots_[tail_.fetch_add(1) % countLimit_];
    slot.charge = charge;
    slot.value.emplace(std::move(value));
    slot.ready.store(true, std::memory_order_release);
    Signal();
    return true;
  }
  void Stop()
  {
    stopping_.store(true);
    Signal();
  }
  bool Healthy() const
  { return healthy_.load(); }
  uint64_t Rejected() const
  { return rejected_.load(); }
  uint64_t Rejected(DiagnosticRejection reason) const
  { return rejections_[static_cast<size_t>(reason)].load(); }
  uint64_t Failures() const
  { return failures_.load(); }
  size_t Pending() const
  { return count_.load(); }
  size_t Charge() const
  { return charge_.load(); }

private:
  bool Reject(DiagnosticRejection reason) noexcept
  {
    ++rejections_[static_cast<size_t>(reason)];
    ++rejected_;
    healthy_.store(false);
    return false;
  }
  // No allocation, mutex acquisition or throwing move occurs on admission.
  static_assert(std::is_nothrow_move_constructible_v<T>);
  struct Slot {
    std::optional<T> value;
    size_t charge{};
    std::atomic_bool ready{false};
  };
  void Signal() noexcept
  {
    if (!wakePending_.exchange(true))
      wake_.release();
  }
  void Run() noexcept
  {
    auto nextPoll = std::chrono::steady_clock::now();
    for (;;) {
      std::optional<T> item;
      size_t charge{};
      if (countLimit_) {
        auto& slot = slots_[head_ % countLimit_];
        if (slot.ready.load(std::memory_order_acquire)) {
          item.emplace(std::move(*slot.value));
          charge = slot.charge;
          slot.value.reset();
          slot.ready.store(false, std::memory_order_release);
          ++head_;
        }
      }
      if (item) {
        try {
          handler_(std::move(*item));
        } catch (...) {
          ++failures_;
          healthy_.store(false);
        }
        charge_.fetch_sub(charge);
        --count_;
      }
      const auto now = std::chrono::steady_clock::now();
      if (now >= nextPoll) {
        nextPoll = now + interval_;
        try {
          if (!stopping_.load() && poller_)
            poller_();
        } catch (...) {
          ++failures_;
          healthy_.store(false);
        }
      }
      if (stopping_.load() && count_.load() == 0)
        return;
      if (item)
        continue;
      // A coalesced semaphore notification cannot be lost between testing the
      // head slot and sleeping. A producer never acquires the consumer's lock.
      if (wake_.try_acquire_until(nextPoll))
        wakePending_.store(false);
    }
  }
  Handler                         handler_;
  Poller                          poller_;
  const size_t                    countLimit_, chargeLimit_;
  const std::chrono::milliseconds interval_;
  std::unique_ptr<Slot[]>          slots_;
  std::atomic_size_t              tail_{0};
  size_t                          head_{}; // Single consumer only.
  std::binary_semaphore           wake_{0};
  std::atomic_bool                wakePending_{false}, stopping_{false};
  std::atomic_bool                healthy_{true};
  std::atomic<uint64_t>           rejected_{0}, failures_{0};
  std::array<std::atomic<uint64_t>, static_cast<size_t>(DiagnosticRejection::count)> rejections_{};
  std::atomic<size_t>             count_{0}, charge_{0};
  std::thread                     worker_;
};
} // namespace claim_trace
