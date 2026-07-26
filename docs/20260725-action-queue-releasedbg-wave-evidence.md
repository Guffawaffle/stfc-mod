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

## Prior-build wave closeout and deployment boundary

- Human closeout: aside from one small Enterprise-D stall, the prior-build wave run was flawless.
- A second Enterprise-D stall was reported as auto-recovered approximately ten seconds before the cycle request
  completed.
- Evidence quality for that second stall: human observation only. The cycle had already begun when the report arrived,
  and it reset both the old file-log rotations and recent-event ring before a marker could be written. Do not correlate
  it with the post-cycle diagnostic build.
- Commit `b032d89` was deployed as releasedbg at 20:17:39. The build and deployed DLL shared SHA-256
  `DF7BA5A4F4072016E921A8C97C81F429C4DC047A395B9EA24010D2B5BA9A1C0D`; the restarted game PID was 30916.
- The first retained post-cycle exact-suffix recovery occurred at 20:27:52.991 on Enterprise-D:
  `2 -> 1`, old head `2774485297147542805`, surviving head `2774485306182072373`, `engage_result=0`.
- Dogfood finding: the new detailed transition hooks generated roughly 922 KB across 711 retained ActionQueueGuard
  records in under two minutes. `OnFleetStateChange` accounted for 357 records. The next instrumentation revision
  should compact or transition-filter these logs; do not cycle during the active wave merely to address verbosity.

## Post-cycle Enterprise-D exact-prune stall

- Marker: `94bfb692-2790-4791-ae77-9165f17d74c3`
- Marker time: 20:30:23.552
- Human observation: queue stalled and then self-recovered, but the visible delay remained unacceptable.
- Player fleet: `2644013931949275840` (`USS Enterprise-D`)
- Evidence quality: high. The new diagnostics captured the exact prune and delayed resume.

```text
[20:30:03.287] OnFleetStateChange.before queue={count=2 targets=[2774486369018708914,2774486369790460851] engaging=0 last=0 pending=2774486369018708914}
[20:30:03.287] DoPlanPathAndEngageTarget.before player_state=1 prev=512 queue={count=2 head=2774486369018708914 engaging=0 pending=2774486369018708914}
[20:30:03.291] DoPlanPathAndEngageTarget.after result=false queue={count=1 head=2774486369790460851 engaging=0 last=0 pending=2774486369018708914}
[20:30:03.291] OnFleetStateChange.after queue={count=1 head=2774486369790460851 engaging=0 last=0 pending=2774486369018708914}
[20:30:03.303] OnPlayerFleetStateChanged.before/after queue unchanged
[20:30:08.320] HandleStall -> DoPlanPathAndEngageTarget.before queue={count=2 head=2774486369790460851 engaging=0 last=0 pending=2774486369018708914}
[20:30:08.321] DoPlanPathAndEngageTarget.after result=true queue={count=2 head=2774486369790460851 engaging=1 last=0 pending=2774486369018708914}
```

Native `OnFleetStateChange` initiated plan/engage, removed the invalid first target, returned `false`, and left the
exact surviving suffix idle. The surviving head did not begin engagement until the watchdog retried 5.030 seconds
later. This is the user-visible stall.

Two implementation assumptions are now disproven:

1. Exact-prefix pruning is not limited to the `HandleStall` call boundary. It can occur inside
   `DoPlanPathAndEngageTarget` invoked by `OnFleetStateChange`.
2. A nonzero `PendingEngageTargetId` does not always block safe progress. The watchdog later engaged the surviving
   head successfully while `pending` still referenced the removed prefix target.

The next repair should evaluate the exact-suffix postcondition at the native plan/engage boundary as well as
`HandleStall`. A positive `pending` or `last` latch may be treated as stale only when it exactly names an ID in the
verified removed prefix; it must still block recovery when it names the surviving head or an unrelated target. Do not
change or cycle during the active wave.

## Incremental native-plan recovery prepared

Prepared locally after the wave ended; not yet deployed:

- `DoPlanPathAndEngageTarget` now applies the same exact-surviving-suffix recovery used by `HandleStall` when the
  original native call returns `false` and leaves the player fleet idle.
- Recovery still requires a strict queue shrink, a changed head, the complete post-native queue to equal the exact
  suffix of the pre-native queue, and an unchanged second live snapshot before replay.
- `LastEngagedTargetId` and `PendingEngageTargetId` may be `0`, a negative no-target sentinel, or an exact ID from the
  verified removed prefix. A latch naming a surviving target or any unrelated positive ID fails closed.
- A successful native replay is returned as successful from the guarded plan call. The watchdog remains installed as
  a fallback, but will not replay after the plan-boundary repair because the queue is already engaging.
- `OnFleetStateChange` and `OnPlayerFleetStateChanged` detailed logs are now emitted only when the callback's fleet
  list intersects an active player action queue. `DoPlanPathAndEngageTarget` logs only its relevant queue, and
  `HandleStall` no longer duplicates the full eight-slot queue dump.

Static validation:

- `stfc-mod-tests`: 299/299 test cases, 3026/3026 assertions.
- releasedbg `mods` target: pass.
- releasedbg `stfc-community-mod` target: pass.
- `git diff --check`: pass.

Expected next-wave success signature:

```text
DoPlanPathAndEngageTarget native_result=false
candidate=resume-after-native-prune source=DoPlan
action=resume-after-native-prune source=DoPlan engage_result=0
DoPlanPathAndEngageTarget ... replay_result=0 ... queue_after_guard={engaging=1 ...}
```

The target runtime check is the previously observed multi-target invalid-prefix case. Ordinary queue completion,
unchanged same-head callbacks, empty queues, reordered queues, fleet replacement, and active survivor latches must
remain no-ops.

## Incremental recovery deployment boundary

- Commit `ade600a` was deployed as releasedbg at 20:42:36.
- The build and deployed DLL shared SHA-256
  `DDD140BD2BCD01C6E2701BDF2C9DABE7883ED183AB4E13BE18A7D81283743637`.
- The prior game PID 30916 stopped cleanly and the restarted game PID is 32760.
- AX cycle completed with a healthy boot. The ActionQueueGuard module produced its expected hook-registry and patch
  audit records, and the post-cycle status reported a clean worktree, matching deploy hash, running game, and no
  current log errors.
- The local sidecar endpoint at `127.0.0.1:43127` was unavailable after boot. This does not block native file-log
  evidence or the queue guard itself, but recent-event ingestion through that endpoint should not be assumed available
  until the sidecar is restarted separately.
