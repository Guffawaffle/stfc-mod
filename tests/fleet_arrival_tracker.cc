#include "patches/fleet_audio_activation.h"
#include "patches/fleet_arrival_tracker.h"
#include <cassert>
#include <iostream>

int main()
{
  // The fleet changed before a new audio alert was enabled while other alerts
  // kept the observer active. Its first sampled transition must remain silent.
  FleetAudioActivation activation;
  activation.Enable(fleet_notification_bit(FleetNotificationKind::Docked));
  assert(!activation.Allows(2, FleetNotificationKind::Docked));
  activation.Observe(2);
  assert(!activation.Allows(2, FleetNotificationKind::Docked));
  assert(activation.Allows(2, FleetNotificationKind::RepairComplete));
  assert(!activation.Allows(3, FleetNotificationKind::Docked));
  activation.Observe(2);
  assert(activation.Allows(2, FleetNotificationKind::Docked));
  // Each slot establishes its own baseline; enabling again suppresses anew.
  activation.Observe(3);
  assert(!activation.Allows(3, FleetNotificationKind::Docked));
  activation.Enable(fleet_notification_bit(FleetNotificationKind::Docked));
  activation.Observe(2);
  assert(!activation.Allows(2, FleetNotificationKind::Docked));

  using P = FleetArrivalPhase;
  FleetArrivalTracker t;
  auto step = [&](P before, P after, bool native = false, uint64_t epoch = 1, uint64_t fleet = 10) {
    return t.Observe(fleet, epoch, before, after, native);
  };
  // Normal arrival, followed by the same native history resurfacing: only once.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Charging, P::Warping));
  assert(step(P::Warping, P::Impulsing, true));
  assert(!step(P::Impulsing, P::Other));
  assert(!step(P::Other, P::Impulsing, true));
  // Captured fleet A recall: native previous remains Warping through transient Docked.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Charging, P::Warping));
  assert(!step(P::Warping, P::Other));
  assert(step(P::Other, P::Impulsing, true));
  // Vindicator sequence: polling skipped Warping. Native history is required.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Charging, P::Other));
  assert(!step(P::Other, P::Impulsing, false));
  assert(step(P::Other, P::Impulsing, true));
  // Any intermediate state is accepted, not just the captured Docked value.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Charging, P::Other));
  assert(step(P::Other, P::Impulsing, true));
  // Captured outbound fleet A: Warping was skipped, then arrival resurfaced.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Charging, P::Other));
  assert(!step(P::Other, P::Charging));
  assert(step(P::Charging, P::Impulsing, true));
  assert(!step(P::Impulsing, P::Other));
  assert(!step(P::Other, P::Impulsing, true));
  // Startup/ordinary undock cannot replay a completed journey.
  assert(!step(P::Other, P::Impulsing, true));
  // Reseeding and fleet replacement discard prior journey observations.
  assert(!step(P::Other, P::Charging));
  assert(!step(P::Other, P::Impulsing, true, 2));
  assert(!step(P::Other, P::Charging, false, 2));
  assert(!step(P::Other, P::Impulsing, true, 2, 11));
  // Baseline already in warp: its eventual normal arrival still fires.
  assert(step(P::Warping, P::Impulsing, true, 3, 12));
  // Baseline in charging also establishes an in-flight journey, even if warp is skipped.
  assert(step(P::Charging, P::Impulsing, true, 4, 13));
  assert(!step(P::Charging, P::Impulsing, false, 5, 14));
  std::cout << "Fleet arrival regression cases passed\n";
}
