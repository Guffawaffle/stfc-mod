# PLAY-NOTIFY-FLEET — fleet transition and mining alerts

Frozen `1baa55fe`: full `parts/fleet_notifications.cc`, fleet_watch, arrival,
OPC/audio activation/catalog policies and config/settings/callers. Target retains
those scalar policies in `features/fleet_notifications.cc`, shared fleet watcher
and typed fleet/notification bindings. [Cohort](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Seven distinct producers: system arrival, destination arrival, started mining,
  depleted node, docked, repair complete, mining over protected cargo.
  Repairing→Docked is repair completion, not an extra dock event.
- Enabled events are (desktop mask OR prepared enabled audio) AND available
  producers. Sound-only interests keep the watcher active; unavailable hook/frame
  readiness cannot be manufactured by changing settings. Refresh reads current
  desktop settings; frozen desktop mask was captured at installation.
- Newly enabled audio is silent through each slot's first fresh observation;
  desktop and previously enabled sound stay independent. Arrival history resets
  on observer epochs/fleet replacement; OPC requires known cargo and preserves
  first-observation/baseline rules. Existing shared policy fixtures retained.
- Depletion original runs once first; scoped suppression prevents generic
  desktop duplication, then a two-second fleet-ID ring suppresses repeated
  dedicated alerts. Target independently installs this producer and refreshes
  masks even if shared observer/frame registration is unavailable. Nonpositive
  native IDs delegate without emitting a synthesized fleet notification.
- Existing watcher polling remains: baseline stabilization/reseed, 250ms
  round-robin probes, bounded fast polling/backoff, no added timer. An event-driven
  redesign is not part of source-preserving qualification. Lifecycle source and
  callback custody remain in the shared owner; subscriptions do not retain
  transition managed pointers.

Controlled consumers test audio-only interest, fresh activation, live disable,
producer masks, dock routing and depletion original/suppression scope. Existing
arrival/OPC tests cover scalar histories. Subscription/frame/clock and native
objects are not real game services; live transitions/delivery and long-running
account switches require separate qualification. No fleet action was requested.
