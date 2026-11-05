# PLAY-NOTIFY-INCOMING — own-ship incoming player alerts

Frozen `1baa55fe`: `patches/incoming_player_attack.cc`, tracker and
notification_service.cc, Toast/models, audio config and settings. Target preserves
the checked namespace/field/runtime-enum adapter in `game/notifications.cc` and
dispatch in `features/toasts.cc`. [Cohort](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Require player attack enum, real targeted ship, positive fleet identity and
  local owner; reject station/faction/self attacks. Prefer PrimeLib model
  assembly, then Assembly-CSharp. Do not substitute unrelated identity namespaces.
- Own only fleet/slot/attacker/event scalars. Native managed roots are operation
  scoped; no borrowed payload is retained for later dispatch. Missing/malformed
  payload leaves the generic/native path available.
- Deduplicate per event+fleet, with bounded UUID history and short fleet/attacker
  fallback. Suppressed repeats do not replay audio/desktop, but native banner
  filtering remains independent. Existing tracker fixture covers expiry/bounds.
- Admission checks live dedicated sound/desktop settings; after native payload
  reads, reacquire current settings and retain a cue value before playing it.
  This pass corrects a retained reference across native getters. Frozen config
  was a stable singleton; target activation replaces its owning object, making
  that source pattern unsafe in the new lifecycle.
- Desktop body uses supported localized text, otherwise owned attacker/slot
  fallback. Desktop permission and Toasts installation readiness stay independent
  from native payload metadata availability. Defaults remain false/silent.

Actual dispatch fixture reproduces stale semantics with the old settings object
kept alive, then verifies current cue and duplicate behavior after correction.
Payload decoding and playback are controlled substitutes there: this does **not**
qualify every real payload/schema/GC failure or prove native reentrant reload
occurs during ordinary play. No induced attack, game action or notification.
