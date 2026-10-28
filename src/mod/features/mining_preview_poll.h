#pragma once

#include <chrono>

namespace mining_preview
{
// Owner-thread scheduling only. User actions bypass the schedule and validate
// current native state immediately; no widget/model pointer is cached here.
class PreviewPoll
{
public:
  using Clock                            = std::chrono::steady_clock;
  using Time                             = Clock::time_point;
  static constexpr auto WaitingInterval  = std::chrono::milliseconds(100);
  static constexpr auto PresenceInterval = std::chrono::milliseconds(250);
  static constexpr auto InitialTimeout   = std::chrono::seconds(5);

  void Start(Time now)
  {
    ready_    = false;
    deadline_ = now + InitialTimeout;
    next_     = now;
  }
  bool Due(Time now)
  {
    // Expiration gets a final current-state check between scheduled polls.
    if (now < next_ && !Expired(now))
      return false;
    next_ = now + (ready_ ? PresenceInterval : WaitingInterval);
    return true;
  }
  void ObserveReady(Time now)
  {
    ready_ = true;
    next_  = now + PresenceInterval;
  }
  bool Ready() const
  { return ready_; }
  bool Expired(Time now) const
  { return !ready_ && now > deadline_; }

private:
  Time deadline_{}, next_{};
  bool ready_ = false;
};
} // namespace mining_preview
