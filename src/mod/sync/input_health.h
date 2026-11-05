#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace data_sync
{
// Failed generations never resume. A complete native session reset gets a new,
// higher generation; a delayed old failure cannot poison that replacement.
// A terminal metadata/worker failure requires a fresh process, not another login.
class InputHealth
{
public:
  bool Allows(uint64_t generation) const noexcept
  {
    return generation && !stopped_.load(std::memory_order_acquire)
           && generation > sealed_through_.load(std::memory_order_acquire);
  }

  bool Seal(uint64_t generation) noexcept
  {
    auto previous = sealed_through_.load(std::memory_order_acquire);
    while (previous < generation) {
      if (sealed_through_.compare_exchange_weak(previous, generation, std::memory_order_acq_rel))
        return true;
    }
    return false;
  }

  void Stop() noexcept
  { stopped_.store(true, std::memory_order_release); }

  uint64_t SealedThrough() const noexcept
  { return sealed_through_.load(std::memory_order_acquire); }

  bool Stopped() const noexcept
  { return stopped_.load(std::memory_order_acquire); }

private:
  std::atomic_uint64_t sealed_through_{0};
  std::atomic_bool     stopped_{false};
};

// Only owned sync bookkeeping belongs here. Call the game's original outside
// this boundary so an allocation failure cannot skip it or make it run twice.
template <typename Update> bool TrySessionUpdate(InputHealth& health, Update&& update) noexcept
{
  try {
    std::forward<Update>(update)();
    return true;
  } catch (...) {
    health.Stop();
    return false;
  }
}

// Both existing ordinary worker loops use this terminal boundary. It adds no
// scheduling or thread ownership: an escaped allocation/log/queue failure, or
// an unexpected normal exit, cannot leave a dead consumer looking healthy.
template <typename Run> void ContainWorkerExit(InputHealth& health, Run&& run) noexcept
{
  try {
    std::forward<Run>(run)();
  } catch (...) {
  }
  health.Stop();
}

inline constexpr size_t kInputQueueCount = 128;
inline constexpr size_t kInputQueueBytes = 32 * 1024 * 1024;
inline constexpr size_t kInputItemBytes  = 16 * 1024 * 1024;

// Admission charge, not allocator-exact retained memory or process RSS. UTF-8
// needs at most three bytes per UTF-16 code unit, including replacement chars.
// Reject before multiplication/addition or before constructing owned strings.
inline std::optional<size_t> InputCharge(size_t payload_bytes, size_t user_units, size_t overhead) noexcept
{
  if (overhead > kInputItemBytes || payload_bytes > kInputItemBytes - overhead)
    return std::nullopt;
  const auto base = overhead + payload_bytes;
  if (user_units > (kInputItemBytes - base) / 3)
    return std::nullopt;
  return base + user_units * 3;
}
} // namespace data_sync
