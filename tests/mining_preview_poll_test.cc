#include "patches/mining_preview_poll.h"
#include <cstdlib>
#include <iostream>

void Check(bool condition)
{
  if (!condition)
    std::abort();
}

int main()
{
  using namespace std::chrono_literals;
  using mining_preview::PreviewPoll;
  const PreviewPoll::Time start{};
  PreviewPoll             poll;
  poll.Start(start);
  Check(poll.Due(start));
  for (int ms = 1; ms < 100; ++ms)
    Check(!poll.Due(start + std::chrono::milliseconds(ms)));
  Check(poll.Due(start + 100ms));
  Check(!poll.Ready());
  // Explicit input can observe readiness between scheduled checks.
  Check(!poll.Due(start + 115ms));
  poll.ObserveReady(start + 115ms);
  Check(poll.Ready());
  Check(!poll.Due(start + 364ms));
  Check(poll.Due(start + 365ms));
  Check(!poll.Due(start + 366ms));
  Check(!poll.Expired(start + 1h));
  // A minute of high-rate updates causes four presence snapshots/second.
  poll.Start(start);
  poll.ObserveReady(start);
  int presenceChecks = 0;
  for (int ms = 1; ms <= 60'000; ++ms)
    presenceChecks += poll.Due(start + std::chrono::milliseconds(ms));
  Check(presenceChecks == 240);
  // Slow frames do not cause catch-up bursts; new contexts reset cadence.
  Check(poll.Due(start + 5min));
  Check(!poll.Due(start + 5min));
  poll.Start(start + 5min);
  Check(!poll.Ready() && poll.Due(start + 5min));
  Check(!poll.Expired(start + 5min + 5s));
  Check(poll.Expired(start + 5min + 5001ms));
  // Deadline forces a final current-state check between ordinary polling slots.
  poll.Start(start);
  Check(poll.Due(start + 4999ms));
  Check(poll.Due(start + 5001ms));
  poll.ObserveReady(start + 5001ms);
  Check(!poll.Expired(start + 10s));
  std::cout << "Mining preview polling: cadence, immediate readiness, timeout/reset and no catch-up passed\n";
}
