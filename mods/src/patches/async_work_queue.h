/**
 * @file async_work_queue.h
 * @brief Small reusable producer/consumer queue with diagnostics.
 */
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

template <typename WorkItem>
class AsyncWorkQueue
{
public:
  struct Diagnostics {
    size_t   depth = 0;
    bool     shutdown_requested = false;
    bool     worker_active = false;
    uint64_t enqueued = 0;
    uint64_t dequeued = 0;
    uint64_t worker_errors = 0;
  };

  AsyncWorkQueue() = default;
  AsyncWorkQueue(const AsyncWorkQueue&) = delete;
  AsyncWorkQueue& operator=(const AsyncWorkQueue&) = delete;

  bool enqueue(WorkItem item)
  {
    {
      std::lock_guard lock(mutex_);
      if (shutdown_requested_) {
        return false;
      }

      queue_.emplace_back(std::move(item));
      ++enqueued_;
    }

    condition_.notify_one();
    return true;
  }

  bool try_pop(WorkItem& item)
  {
    std::lock_guard lock(mutex_);
    if (queue_.empty()) {
      return false;
    }

    item = std::move(queue_.front());
    queue_.pop_front();
    ++dequeued_;
    return true;
  }

  std::vector<WorkItem> drain()
  {
    std::lock_guard lock(mutex_);
    return drain_locked();
  }

  template <typename Rep, typename Period>
  std::vector<WorkItem> wait_for_batch_after_quiet(std::chrono::duration<Rep, Period> quiet_for)
  {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return shutdown_requested_ || !queue_.empty(); });

    if (queue_.empty()) {
      return {};
    }

    auto observed_depth = queue_.size();
    while (!shutdown_requested_ && condition_.wait_for(lock, quiet_for, [this, observed_depth] {
      return queue_.size() != observed_depth || shutdown_requested_;
    })) {
      observed_depth = queue_.size();
    }

    return drain_locked();
  }

  void request_shutdown()
  {
    {
      std::lock_guard lock(mutex_);
      shutdown_requested_ = true;
    }

    condition_.notify_all();
  }

  bool shutdown_requested() const
  {
    std::lock_guard lock(mutex_);
    return shutdown_requested_;
  }

  void set_worker_active(bool active)
  {
    std::lock_guard lock(mutex_);
    worker_active_ = active;
  }

  void record_worker_error()
  {
    std::lock_guard lock(mutex_);
    ++worker_errors_;
  }

  Diagnostics diagnostics() const
  {
    std::lock_guard lock(mutex_);
    return {
      queue_.size(),
      shutdown_requested_,
      worker_active_,
      enqueued_,
      dequeued_,
      worker_errors_,
    };
  }

private:
  std::vector<WorkItem> drain_locked()
  {
    std::vector<WorkItem> batch;
    batch.reserve(queue_.size());

    while (!queue_.empty()) {
      batch.emplace_back(std::move(queue_.front()));
      queue_.pop_front();
    }

    dequeued_ += batch.size();
    return batch;
  }

  mutable std::mutex      mutex_;
  std::condition_variable condition_;
  std::deque<WorkItem>    queue_;
  bool                    shutdown_requested_ = false;
  bool                    worker_active_ = false;
  uint64_t                enqueued_ = 0;
  uint64_t                dequeued_ = 0;
  uint64_t                worker_errors_ = 0;
};