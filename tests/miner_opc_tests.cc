#include "patches/miner_opc_tracker.h"

// Each scenario models a reachable observation sequence; compile-time checks also run in release builds.
constexpr bool crossing_and_rearm()
{
  MinerOpcTracker tracker;
  if (tracker.Observe(1, true, true, false, false))
    return false;
  if (!tracker.Observe(1, true, true, true, true))
    return false;
  if (tracker.Observe(1, true, true, true, true))
    return false;
  if (tracker.Observe(1, true, true, false, true))
    return false;
  return tracker.Observe(1, true, true, true, true);
}
constexpr bool quiet_baselines()
{
  MinerOpcTracker tracker;
  if (tracker.Observe(1, true, true, true, true))
    return false; // Already OPC at first sight.
  tracker.Observe(1, true, true, false, true);
  if (tracker.Observe(1, true, true, true, false))
    return false; // Reseeding.
  if (tracker.Observe(1, true, true, true, true))
    return false;
  tracker.Observe(1, true, true, false, true);
  return !tracker.Observe(2, true, true, true, true); // Slot replacement.
}
constexpr bool interrupted_mining()
{
  MinerOpcTracker tracker;
  tracker.Observe(1, true, true, false, true);
  tracker.Observe(1, false, false, false, true);
  if (tracker.Observe(1, true, true, true, true))
    return false;
  tracker.Observe(1, true, true, false, true);
  tracker.Observe(1, true, false, false, true); // Missing/non-finite cargo breaks continuity.
  return !tracker.Observe(1, true, true, true, true);
}
static_assert(crossing_and_rearm());
static_assert(quiet_baselines());
static_assert(interrupted_mining());
int main() {}
