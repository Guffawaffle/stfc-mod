#pragma once
#include "fleet_notification_types.h"
#include <array>

// A newly enabled sound starts after each slot's first fresh observation.
// Keep desktop notifications and already-active audio independent of this gate.
struct FleetAudioActivation {
  std::array<FleetNotificationMask, 10> pending{}, suppressed{};
  void Enable(FleetNotificationMask mask)
  { for (auto& slot : pending) slot |= mask; }
  void Observe(int slot)
  {
    if (slot < 0 || slot >= static_cast<int>(pending.size())) return;
    suppressed[slot] = pending[slot];
    pending[slot] = 0;
  }
  bool Allows(int slot, FleetNotificationKind kind) const
  {
    return slot < 0 || slot >= static_cast<int>(pending.size())
           || ((pending[slot] | suppressed[slot]) & fleet_notification_bit(kind)) == 0;
  }
};
