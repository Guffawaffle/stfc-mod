# Fleet Arrival Hidden-UI Runtime Evidence — 2026-07-25

## Decision

Fleet-arrival notifications no longer need the Fleet Bar to remain visible. A bounded `FleetsManager` scan from the
shared frame tick detected the arrival transition and delivered both configured outputs while a full-screen game
window obscured the Fleet Bar.

## Evidence

The first canary used `FleetEvents.TriggerPlayerFleetsChangedEvent` as the proposed UI-independent source. Its hook
installed successfully, but a one-system hidden-Fleet-Bar arrival produced neither observer traffic nor notification
delivery. Recalling with the Fleet Bar visible immediately produced sound, isolating the failure to observation rather
than audio or notification policy.

The replacement subscriber:

- is armed by a safe fleet widget or event observation of an active transition;
- scans all fleet slots at 250 ms for the first 30 seconds, then backs off to five seconds while a fleet still
  requires transition follow-through;
- shuts itself off after all observed fleets reach stable states;
- suspends after eight consecutive zero-fleet reads or a runtime access failure until a safe observation rearms it;
- expires after a hard 24-hour lifetime so one request cannot poll forever;
- feeds the existing deduplicating fleet-state notification machine.

The first guarded canary established that the scanner could activate safely:

```text
23:18:56.287 [FleetState] source=fleet-state-widget status=runtime-ready
23:18:56.302 [FleetState] source=fleets-manager-scan status=active cadenceMs=250 fleetCount=7
```

Two hidden-Fleet-Bar arrivals then completed the full delivery path:

```text
23:35:39.680 [FleetState] source=fleets-manager-scan kind=ARRIVED_IN_SYSTEM
23:35:40.104 [NotifyAudio] Played notification sound event=fleet.arrived_in_system sound=arrival
23:35:40.444 [Notify] WinRT notification requested title='Fleet Arrived'

23:39:36.614 [FleetState] source=fleets-manager-scan kind=ARRIVED_IN_SYSTEM
23:39:37.000 [NotifyAudio] Played notification sound event=fleet.arrived_in_system sound=arrival
23:39:37.371 [Notify] WinRT notification requested title='Fleet Arrived'
```

The human observer confirmed the second hidden-ui notification and sound were received.

Before publication, the canary was tightened from continuous post-login scanning to activity-triggered bounded
follow-through so it does not become a broad frame poller.

The final bounded build then passed back-to-back human tests with the Faction and Refinery surfaces open:

```text
23:47:00.748 [FleetState] source=fleet-state-widget status=scan-requested state=128
23:47:00.761 [FleetState] source=fleets-manager-scan status=active cadenceMs=250 fleetCount=7 followThrough=1

23:47:14.934 [FleetState] source=fleets-manager-scan kind=ARRIVED_IN_SYSTEM
23:47:15.351 [NotifyAudio] Played notification sound event=fleet.arrived_in_system sound=arrival
23:47:15.698 [Notify] WinRT notification requested title='Fleet Arrived'

23:47:38.358 [FleetState] source=fleets-manager-scan kind=ARRIVED_IN_SYSTEM
23:47:38.743 [NotifyAudio] Played notification sound event=fleet.arrived_in_system sound=arrival
23:47:39.112 [Notify] WinRT notification requested title='Fleet Arrived'

23:47:45.951 [FleetState] source=fleets-manager-scan status=idle
```

This proves the final subscriber remained dormant before activity, followed the active fleet across both modal tests,
delivered each arrival exactly once, and stopped after the last fleet reached a stable state.

On 2026-07-26, the human observer also confirmed arrival audio while the Claims window was open. This completes the
named Faction, Claims, and representative modal-surface acceptance matrix.

## Final hook decision

The `FleetEvents.TriggerPlayerFleetsChangedEvent` canary was removed before release. It produced zero observations in
the relevant runtime test and is not required by the validated widget-triggered, `FleetsManager` follow-through path.
