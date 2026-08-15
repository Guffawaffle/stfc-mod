/**
 * @file bounded_mpsc_queue.h
 * @brief Preallocated nonblocking multi-producer, single-consumer queue.
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

template <typename T> class BoundedMpscQueue
{
  static_assert(std::is_nothrow_move_assignable_v<T>);

  struct Slot {
    std::atomic_size_t sequence{0};
    T                  value;
  };

public:
  explicit BoundedMpscQueue(const size_t byte_limit)
      : capacity_(byte_limit / sizeof(Slot))
      , slots_(capacity_ == 0 ? nullptr : std::make_unique<Slot[]>(capacity_))
  {
    for (size_t index = 0; index < capacity_; ++index) {
      slots_[index].sequence.store(index, std::memory_order_relaxed);
    }
  }

  BoundedMpscQueue(const BoundedMpscQueue&)            = delete;
  BoundedMpscQueue& operator=(const BoundedMpscQueue&) = delete;

  [[nodiscard]] bool try_enqueue(T&& value) noexcept
  {
    return try_emplace([&value](T& destination) noexcept { destination = std::move(value); });
  }

  template <typename Writer> [[nodiscard]] bool try_emplace(Writer&& writer) noexcept
  {
    if (capacity_ == 0) {
      return false;
    }

    size_t position = enqueue_position_.load(std::memory_order_relaxed);
    Slot*  slot     = nullptr;
    for (;;) {
      slot                = &slots_[position % capacity_];
      const auto sequence = slot->sequence.load(std::memory_order_acquire);
      const auto delta    = static_cast<intptr_t>(sequence) - static_cast<intptr_t>(position);
      if (delta == 0) {
        if (enqueue_position_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (delta < 0) {
        return false;
      } else {
        position = enqueue_position_.load(std::memory_order_relaxed);
      }
    }

    std::forward<Writer>(writer)(slot->value);
    slot->sequence.store(position + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool try_dequeue(T& value) noexcept
  {
    if (capacity_ == 0) {
      return false;
    }

    auto*      slot     = &slots_[dequeue_position_ % capacity_];
    const auto sequence = slot->sequence.load(std::memory_order_acquire);
    const auto delta    = static_cast<intptr_t>(sequence) - static_cast<intptr_t>(dequeue_position_ + 1);
    if (delta != 0) {
      return false;
    }

    value = std::move(slot->value);
    slot->sequence.store(dequeue_position_ + capacity_, std::memory_order_release);
    ++dequeue_position_;
    return true;
  }

  [[nodiscard]] size_t capacity() const noexcept
  { return capacity_; }

  [[nodiscard]] static constexpr size_t slot_bytes() noexcept
  { return sizeof(Slot); }

private:
  size_t                  capacity_ = 0;
  std::unique_ptr<Slot[]> slots_;
  alignas(64) std::atomic_size_t enqueue_position_{0};
  alignas(64) size_t dequeue_position_ = 0;
};
