#pragma once

#include <cstdint>

namespace claim_trace
{
// One budget per summary generation, including cancelled/expired candidates.
// This policy never clears a native lock or creates a navigation destination.
class NavigationRecovery
{
  uint64_t usedGeneration_{};
  bool     pending_{};
  int64_t  notBefore_{}, deadline_{}, idleSince_{-1};

public:
  struct Gates {
    bool enabled, healthy, sameSummary, sameContext, knownState, sameOrigin, atDestination, busy, inputBlocked;
  };
  enum class Decision { none, wait, retry, disabled, unhealthy, changed, unknown, destination, expired, input };

  bool Schedule(uint64_t generation, int64_t now)
  {
    if (!generation || generation == usedGeneration_ || pending_)
      return false;
    usedGeneration_ = generation;
    pending_        = true;
    notBefore_      = now + 250;
    deadline_       = now + 5000;
    idleSince_      = -1;
    return true;
  }
  bool Pending() const
  { return pending_; }
  void Cancel()
  { pending_ = false; }
  Decision Poll(const Gates& gates, int64_t now)
  {
    if (!pending_)
      return Decision::none;
    Decision stop = Decision::none;
    if (!gates.enabled)
      stop = Decision::disabled;
    else if (!gates.healthy)
      stop = Decision::unhealthy;
    else if (!gates.sameSummary || !gates.sameContext)
      stop = Decision::changed;
    else if (!gates.knownState)
      stop = Decision::unknown;
    else if (!gates.sameOrigin)
      stop = Decision::changed;
    else if (gates.atDestination)
      stop = Decision::destination;
    else if (gates.inputBlocked)
      stop = Decision::input;
    else if (now >= deadline_)
      stop = Decision::expired;
    if (stop != Decision::none) {
      Cancel();
      return stop;
    }
    if (gates.busy) {
      idleSince_ = -1;
      return Decision::wait;
    }
    if (idleSince_ < 0)
      idleSince_ = now;
    if (now < notBefore_ || now - idleSince_ < 100)
      return Decision::wait;
    Cancel(); // Consume before invoking native code; no recursive retry.
    return Decision::retry;
  }
};
} // namespace claim_trace
