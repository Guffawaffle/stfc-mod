#include "patches/incoming_player_attack_tracker.h"
#include <cassert>
#include <iostream>

int main()
{
  using namespace std::chrono_literals;
  IncomingPlayerAttackTracker tracker;
  const auto                  now = IncomingPlayerAttackTracker::Clock::time_point{};
  assert(tracker.Accept("attack-1", 10, "Player", now));
  assert(!tracker.Accept("attack-1", 10, "Player", now + 1s)); // Two enqueue paths, one alert.
  assert(!tracker.Accept("attack-1", 10, "Renamed Player", now + 2s));
  assert(tracker.Accept("attack-2", 10, "Player", now + 2s)); // Distinct attack on the same ship.
  assert(tracker.Accept("attack-1", 11, "Player", now + 2s)); // Another targeted ship.
  assert(tracker.Accept("attack-1", 10, "Player", now + 60s));
  assert(tracker.Accept("", 10, "Player", now + 61s));
  assert(!tracker.Accept("", 10, "Player", now + 62s));
  assert(tracker.Accept("", 10, "Player", now + 63s));
  for (int i = 0; i < 256; ++i)
    assert(tracker.Accept("burst-" + std::to_string(i), 10, "Player", now + 64s));
  assert(!tracker.Accept("burst-255", 10, "Player", now + 65s));
  assert(tracker.Accept("attack-after-idle", 10, "Player", now + 1h));
  std::cout << "Incoming player attack duplicate, expiry, per-ship and bounded-history checks passed\n";
}
