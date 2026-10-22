#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace selection_capture
{
inline constexpr std::size_t kRows = 256, kSlots = 512, kObservations = 8;
inline constexpr int64_t     kWindowMs = 100;

enum class Reason { none, worker_pressure, burst, busy, extent };
inline const char* Name(Reason reason)
{
  switch (reason) {
    case Reason::none:
      return "admitted";
    case Reason::worker_pressure:
      return "worker_pressure";
    case Reason::burst:
      return "rolling_capture_budget";
    case Reason::busy:
      return "capture_admission_busy";
    case Reason::extent:
      return "observation_extent_budget";
  }
  return "unknown";
}

struct Allowance {
  Reason      reason   = Reason::worker_pressure;
  bool        admitted = false;
  std::size_t rows = 0, slots = 0;
  explicit Allowance(Reason value = Reason::worker_pressure)
      : reason(value)
      , admitted(value == Reason::none)
      , rows(admitted ? kRows : 0)
      , slots(admitted ? kSlots : 0)
  {
  }
  bool Take(std::size_t requestedRows, std::size_t requestedSlots)
  {
    if (requestedRows > rows || requestedSlots > slots) {
      if (reason == Reason::none)
        reason = Reason::extent;
      return false;
    }
    rows -= requestedRows;
    slots -= requestedSlots;
    return true;
  }
};

class Admission
{
public:
  Allowance Try(int64_t now, bool workerHealthy, std::size_t pending, std::size_t charged)
  {
    if (!workerHealthy || pending >= 96 || charged >= 12 * 1024 * 1024)
      return Allowance(Reason::worker_pressure);
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
      return Allowance(Reason::busy);
    while (count_ && now >= times_[head_] && now - times_[head_] >= kWindowMs) {
      head_ = (head_ + 1) % kObservations;
      --count_;
    }
    if (count_ == kObservations)
      return Allowance(Reason::burst);
    times_[(head_ + count_) % kObservations] = now;
    ++count_;
    return Allowance(Reason::none);
  }

private:
  std::mutex                         mutex_;
  std::array<int64_t, kObservations> times_{};
  std::size_t                        head_ = 0, count_ = 0;
};
} // namespace selection_capture
