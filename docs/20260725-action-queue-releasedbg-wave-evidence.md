# 2026-07-25 Action Queue Releasedbg Wave Evidence

This is the live evidence notebook for the releasedbg wave following the first deployed exact-suffix recovery run.
Times are America/Chicago (UTC-05:00). Entries are appended while the game remains running; no cycle or runtime
configuration change is part of the evidence capture.

## Enterprise-D small stall

- Human report: a small Enterprise-D stall approximately ten seconds before the marker.
- Marker: `cd2ae9a0-45b9-47cf-9072-2641a162b2c8`
- Marker time: 19:49:22.884
- Player fleet: `2644013931949275840`
- Estimated observation time: approximately 19:49:12.884

Failure classification: user-visible Enterprise-D queue stall. The report is authoritative for the UX failure, but
the exact queue transition is not retained by the current diagnostic surface.

The retrospective interval from 19:49:00 through the marker contained no non-empty Enterprise-D queue snapshot and no
Enterprise-D `HandleStall` call. The only Enterprise-D action record in that interval was a new `set-course` action at
19:49:20.870. A `fleet-slot-impulse-started` recent event occurred at 19:49:15.900, but its redacted payload does not
identify the fleet and cannot be claimed as the recovery boundary.

The sequence below occurred after the marker and is therefore a separate later handoff, not the reported failure:

```text
[19:49:20.870] SpaceActionDiag outcome=set-course Enterprise-D
[19:49:23.050] SpaceActionDiag outcome=engage-prescan Enterprise-D
[19:49:26.367] SpaceActionDiag outcome=queue-add Enterprise-D
[19:49:27.754] HandleStall.before Enterprise-D state=1 battling=false count=1 head=2774466260661305582 engaging=0 last=0 pending=2774466259226852122
[19:49:27.754] HandleStall.after  delta=0 candidate=false result=-1 count=1 head=2774466260661305582 engaging=0 last=0 pending=2774466259226852122
[19:49:30.766] HandleStall.before Enterprise-D count=1 head=2774466260661305582 engaging=0 last=2774466259226852122 pending=0
[19:49:30.766] HandleStall.after  delta=0 candidate=false result=-1 count=1 head=2774466260661305582 engaging=0 last=2774466259226852122 pending=0
[19:49:33.046] OnFleetsDisposed includes prior target 2774466259226852122 destroyed=1; new head is already engaging
[19:49:33.778] HandleStall queue={count=1 head=2774466260661305582 engaging=1 last=0 pending=0}
[19:49:35.968] OnFleetsDisposed queue={count=0 head=0 engaging=0 last=2774466260661305582 pending=0}
```

That later sequence is still useful as a negative control. The current exact-prefix guard could not safely act:

- Native `HandleStall` did not prune the queue, so there was no verified surviving suffix.
- At the first watchdog pass, `pending` still named the prior target rather than the new head.
- At the second watchdog pass, `last` still named the prior target and Enterprise-D was still battling.
- By the time the prior target's destroyed disposal arrived, native state had already begun engaging the new head.

An eager same-head replay at either watchdog pass would not be safely fenced: it could overlap the prior engagement
and reproduce the historical duplicate-reengage or skip regression. The next diagnostic build should capture
queue-add identity plus before/after state at `OnFleetStateChangeEventHandler`,
`OnPlayerFleetStateChangedEventHandler`, and the native plan/engage entry. This closes the current blind spot between
the user's visible stall and the three-second watchdog samples. Do not broaden the recovery predicate from elapsed
idleness or a mismatched old `pending`/`last` latch alone.

## Follow-up instrumentation prepared

Prepared locally after the failure classification; not deployed during the active wave:

- `SpaceActionDiag` now includes the resolved target fleet ID for queue-add actions.
- Science-tier diagnostics capture active-queue before/after state at
  `OnFleetStateChangeEventHandler` and `OnPlayerFleetStateChangedEventHandler`.
- Science-tier diagnostics capture `DoPlanPathAndEngageTarget` entry, result, player state, and queue postcondition.
- State-transition logging is suppressed when no queue is active before or after the callback.
- `AddActionToQueue` remains untouched. The dormant investigation records that detouring it suppressed native enqueue
  on this build shape, so it is not a safe diagnostic seam.
