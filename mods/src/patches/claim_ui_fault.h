#pragma once

#include <cstdint>

namespace claim_trace
{
// Game-thread test policy. A rejection is confined to one bound summary and one
// native navigation attempt; it never submits or retries a claim.
class UiRejectOnce
{
  uintptr_t summary_{};
  int64_t   expires_{};

public:
  bool Arm(uintptr_t summary, int64_t now)
  {
    if (!summary)
      return false;
    summary_ = summary;
    expires_ = now + 60000;
    return true;
  }
  void Cancel()
  { summary_ = 0; }
  bool Armed(int64_t now)
  {
    if (now >= expires_)
      Cancel();
    return summary_ != 0;
  }
  bool Consume(uintptr_t summary, bool inside_continue, int64_t now)
  {
    if (!Armed(now) || !inside_continue || !summary || summary != summary_)
      return false;
    Cancel();
    return true;
  }
};
} // namespace claim_trace
