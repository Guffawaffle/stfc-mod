# 2026-07-25 Action Queue Wave Follow-up Snapshot

This preserves the runtime evidence captured after the interrupted notification-build cycle and during the following
wave run.

## Provenance and retention limits

- Original sources:
  - `C:\Games\Star Trek Fleet Command\default\game\community_patch.2.log`
  - `C:\Games\Star Trek Fleet Command\default\game\community_patch.1.log`
  - `C:\Games\Star Trek Fleet Command\default\game\community_patch.log`
- Local timezone: America/Chicago (UTC-05:00 at capture).
- The first cycle attempt restarted `prime.exe` at 18:03:29.445. The second attempt was interrupted at the user's
  request before another cycle could occur.
- Detailed runtime tracing filled the two retained 512 KiB rotations. At capture, the surviving continuous span was
  18:12:04.394 through 18:20:54.591 (3,885 timestamped records). The earlier 18:03:29-18:12:04 portion had already
  rotated out and is not reconstructed here.
- The game was still running as PID 5952 when captured. No cycle, deploy, config edit, or process change was made
  during this evidence pass.
- The new targeted fleet-arrival observer was built locally but was not active in this runtime:
  `FleetEvents.TriggerPlayerFleetsChangedEvent` and the new `[FleetState]` log prefix both had zero retained hits.
  Runtime fleet transitions continued to identify `FleetStateWidget.SetWidgetData` as their seam.

## Aggregate queue-guard results

- `HandleStall` before/after pairs: 157.
- Hull samples: 75 Junker, 82 USS Enterprise-D.
- Every after-sample retained the same head and reported `count_delta_native=0`.
- Thin-guard decisions: 0 resume candidates, 0 successful replays, 157 `replay_result=false`.
- Forty-six watchdog samples occurred while the ship was not battling and the queue reported `engaging=0`.
- Disposal callbacks: 157 before/after pairs.
- Error or critical records: 0.
- Warning records: 68 total; 65 were `SpaceActionDiag` timing warnings and 3 were `SidecarLocal` transport warnings.

These counts show that the thin guard remained observe-only throughout the retained wave span. It neither advanced nor
double-dropped a queue head.

## Junker: idle head self-recovered without guard replay

Head `2774417179435991919` remained idle through three watchdog passes. Native state changed to engaging at the third
pass and the target was later delivered as destroyed. The thin guard did not replay during the sequence.

```text
[2026-07-25 18:12:31.459] [ActionQueueGuard] before HandleStall hull='Junker' deployed_state=0 battling=false count=1 head=2774417179435991919 engaging=0 pending=0
[2026-07-25 18:12:31.459] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false engaging=0
[2026-07-25 18:12:34.459] [ActionQueueGuard] before HandleStall hull='Junker' deployed_state=0 battling=false count=1 head=2774417179435991919 engaging=0 pending=0
[2026-07-25 18:12:34.459] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false engaging=0
[2026-07-25 18:12:37.459] [ActionQueueGuard] before HandleStall hull='Junker' deployed_state=0 battling=false count=1 head=2774417179435991919 engaging=0 pending=0
[2026-07-25 18:12:37.461] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false engaging=1
[2026-07-25 18:12:45.090] [ActionQueueGuard] OnFleetsDisposed id=2774417179435991919 state=3 prev=6 destroyed=1 battling=1 removal_reason=1
```

This is a retained example of the roughly five-second self-repair behavior reported earlier: a queue can look idle
across multiple watchdog passes and then resume through native behavior without a thin-guard action.

## USS Enterprise-D: longer idle span, then combat and disposal

Head `2774419221441284344` stayed at the front of a four-item queue for 18.044 seconds. Four consecutive watchdog
passes observed the ship out of battle with `engaging=0`; the fleet then entered battle and the target was disposed.
Again, no replay was attempted.

```text
[2026-07-25 18:16:17.048] [ActionQueueGuard] before HandleStall hull='USS Enterprise-D' deployed_state=0 battling=false count=4 head=2774419221441284344 engaging=1
[2026-07-25 18:16:20.047] [ActionQueueGuard] before HandleStall hull='USS Enterprise-D' deployed_state=1 battling=false count=4 head=2774419221441284344 engaging=0 pending=2774419221441284344
[2026-07-25 18:16:20.048] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false
[2026-07-25 18:16:23.061] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false
[2026-07-25 18:16:26.073] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false
[2026-07-25 18:16:29.080] [ActionQueueGuard] after  HandleStall same_head=true count_delta_native=0 resume_candidate=false replay_result=false
[2026-07-25 18:16:32.088] [ActionQueueGuard] before HandleStall hull='USS Enterprise-D' deployed_state=3 battling=true count=4 head=2774419221441284344
[2026-07-25 18:16:36.388] [ActionQueueGuard] OnFleetsDisposed id=2774419221441284344 state=3 prev=6 destroyed=1 battling=1 removal_reason=1
```

## Evidence implications

- The retained wave is a clean no-double-drop control: the guard made no replay decisions and native queue processing
  continued.
- A same-head/no-op watchdog result is not sufficient evidence of a broken queue. Both preserved examples eventually
  progressed through native engagement and disposal.
- The stronger protection predicate should continue to require exact stale-head evidence rather than elapsed idle time
  alone.
- This snapshot does not validate the new fleet-arrival observer because that DLL was not deployed before the wave.

## Human observation: possible remaining multi-target failure

Evidence quality: low / anecdotal. This is a tester recollection recorded after the wave, not a timestamp-correlated
claim proven by the retained log.

- Confidence is roughly 90% that single-target cases behaved correctly: when the only queued target disappeared, the
  queue recovered gracefully and moved on.
- The remaining break appeared to require a larger queue, such as five ordered targets, followed by two or more
  nontrivial removals (for example, middle targets 2 and 3 dying, or another multi-target combination).
- The suspected failure is therefore not simply "the current target died." It may involve native list compaction,
  index/head bookkeeping, or several disposal updates arriving while other queued targets remain.

Working hypothesis only: the thin exact-head protection covers the single-target stale-head case, while the earlier
multi-target shrinking-loop behavior may remain. A future wave should capture the full queue before and after each
multi-item disposal and correlate the removed IDs with their original positions before changing the guard.

## Next-wave implementation

The follow-up build promotes only the existing exact-prefix-prune predicate:

- Native `HandleStall` must reduce the queue to an exact surviving suffix.
- The player fleet must be idle in space and the surviving queue must be fully captured.
- The new head must remain unengaged with no last-target or pending-target latch.
- A second snapshot must exactly match the surviving suffix before action.
- Recovery calls native `TryPlanPathAndEngageTarget` on the compacted queue instance. It does not call
  `ProcessQueue` for the new head, so the guard does not dequeue an additional target.

For the next wave, watch for `action=resume-after-native-prune` and confirm that the reported surviving `head` enters
engagement without the queue count dropping again. Ordinary same-head idle watchdog passes should continue to report
`resume_candidate=false`.
