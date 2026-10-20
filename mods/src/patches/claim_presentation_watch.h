#pragma once
#include <cstdint>

namespace claim_trace
{
// A long presentation is evidence to inspect, not proof of an exception or lost rewards.
// Each tracked claim retains its one-shot budget across navigation and summary changes.
class PresentationWatch
{
  std::int64_t since_{-1};
  std::int32_t current_{}, next_{};
  bool         reported_{}, stalled_{}, attempted_{}, leftSection_{};

public:
  bool Observe(bool eligible, std::int32_t current, std::int32_t next, std::int64_t now)
  {
    if (reported_ && (current != current_ || next != next_))
      leftSection_ = true;
    if (!eligible || leftSection_) {
      since_   = -1;
      stalled_ = false;
      return false;
    }
    if (since_ < 0 || current != current_ || next != next_) {
      since_   = now;
      current_ = current;
      next_    = next;
    }
    stalled_ = now - since_ >= 15000;
    if (!stalled_ || reported_)
      return false;
    reported_ = true;
    return true;
  }
  void NavigationAccepted()
  {
    if (reported_) {
      leftSection_ = true;
      stalled_     = false;
      since_       = -1;
    }
  }
  bool Stalled() const
  { return stalled_; }
  bool Attempted() const
  { return attempted_; }
  bool LeftSection() const
  { return leftSection_; }
  bool ConsumeEscape(bool gates)
  {
    if (!gates || !stalled_ || attempted_)
      return false;
    attempted_ = true; // Before native invocation, including rejection or exception.
    return true;
  }
};
} // namespace claim_trace
