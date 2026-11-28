#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace MouseNavigation
{
enum class Phase { kDiscrete, kBegan, kChanged, kEnded, kCancelled };

inline int32_t Button(double x, double y)
{
  if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) <= std::abs(y))
    return -1;
  return x > 0 ? 3 : 4;
}

// The native adapter holds one mutex around this stage; it never reads settings or Unity input.
// A phased gesture keeps its originating eligibility through its end, even after an input-owner handoff.
class Stage
{
public:
  void Publish(bool eligible, uint64_t revision)
  {
    if (eligible != eligible_ || revision != revision_) {
      pulses_           = {};
      gesture_eligible_ = false;
    }
    eligible_ = eligible;
    revision_ = revision;
  }

  void Retire()
  {
    eligible_         = false;
    gesture_eligible_ = false;
    pulses_           = {};
  }

  // Whether this gesture belongs to the mod and should be consumed by the local event monitor.
  bool Observe(int32_t button, Phase phase)
  {
    const bool horizontal = button == 3 || button == 4;
    if (phase == Phase::kDiscrete) {
      active_           = false;
      gesture_eligible_ = false;
      pending_          = -1;
      if (eligible_ && horizontal)
        pulses_[button - 3] = true;
      return eligible_ && horizontal;
    }
    if (phase == Phase::kBegan) {
      active_           = true;
      gesture_eligible_ = eligible_;
      pending_          = horizontal ? button : -1;
      return gesture_eligible_ && horizontal;
    }
    if (!active_) {
      // A changed/end event without its beginning cannot acquire input after a handoff.
      if (phase == Phase::kChanged)
        active_ = true;
      gesture_eligible_ = false;
      pending_          = -1;
    }
    const bool owned = active_ && gesture_eligible_ && eligible_;
    if (owned && horizontal)
      pending_ = button;
    if (phase == Phase::kCancelled) {
      active_           = false;
      gesture_eligible_ = false;
      pending_          = -1;
      return owned;
    }
    if (phase != Phase::kEnded)
      return owned && horizontal;
    if (owned && pending_ >= 0)
      pulses_[pending_ - 3] = true;
    active_           = false;
    gesture_eligible_ = false;
    pending_          = -1;
    return owned;
  }

  std::array<bool, 2> Take()
  { return std::exchange(pulses_, {}); }

private:
  bool                eligible_ = false, active_ = false, gesture_eligible_ = false;
  uint64_t            revision_ = 0;
  int32_t             pending_  = -1;
  std::array<bool, 2> pulses_{};
};
} // namespace MouseNavigation
