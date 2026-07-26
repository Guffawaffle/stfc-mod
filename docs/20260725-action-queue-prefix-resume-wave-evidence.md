# 2026-07-25 Action Queue Prefix-Resume Wave Evidence

This is the bounded evidence snapshot from the first wave run with commit `46a6744` (`Resume exact surviving queue
suffixes`) deployed. It preserves the high-signal queue transitions and tester markers before the rolling logs
overwrite them.

## Provenance and retention boundary

- Branch: `fix/issue-186-thin-queue-protection`
- Deployed commit: `46a6744`
- Local timezone: America/Chicago (UTC-05:00)
- Capture time: 2026-07-25 19:34:44.898
- Runtime sources:
  - `community_patch.2.log`: 19:23:17.026-19:25:50.688, 1,362 lines,
    SHA-256 `FC3A17CAFFD10131D8CF1DBF79BD8EC57576CF97A008351E64F2998F09501C75`
  - `community_patch.1.log`: 19:25:50.688-19:28:06.088, 1,277 lines,
    SHA-256 `930F0F1BEADF27CC548926A525CDD44DD682FC172133DDC0885777C7680C24A0`
  - `community_patch.log`: retained from 19:28:06.088 and was still being appended during capture
- Detailed tracing exhausted both 512 KiB rotations. The wave's first two human markers, at approximately 19:19:45
  and 19:21:18, had already rotated out of the file logs. They were also older than the 256-entry recent-event ring,
  whose first retained sequence was 493 with 492 entries evicted. No raw correlation claim is made for those markers.
- The game and sidecar were not cycled or reconfigured during this evidence pass.

## Human markers

| Local time | Marker ID | Observation | Retention |
|---|---|---|---|
| 19:19:45.641 | `9cc449e8-af2f-4c01-bb55-5227a1603796` | Enterprise stalled for about five seconds | Raw interval rotated out |
| 19:21:18.904 | `c6333cda-c707-4303-a083-f28445686260` | Probable successful bad-state recovery | Raw interval rotated out |
| 19:24:58.438 | `5b0b58f6-ffe9-419c-875b-4af246f0c89d` | Second probable successful recovery | Retained as recent-event sequence 508 |
| 19:25:19.485 | `64a3b952-536b-4f98-910c-f042d456457c` | Uncertain cluster, possibly about two skips | Retained as recent-event sequence 517 |
| 19:29:02.120 | `fdeb31e4-96fc-492e-8ae4-dd01a0d4e4e6` | Immediate stall or skip | Retained as recent-event sequence 734 |

## Aggregate guard result

The retained logs contain 156 `HandleStall` after-samples:

- 153 native no-ops: `count_delta_native=0`, no resume candidate.
- 2 exact one-item prefix prunes: both became resume candidates and both returned `engage_result=0`.
- 1 one-item prune to an empty Enterprise queue: correctly not a candidate because no surviving head existed.
- 0 suppressed postconditions.
- 0 missing-method or missing-fleet skips.

There is no retained instance where the guard itself removed a second queue item.

## Proven recovery at 19:24:56

This is strongly correlated with the 19:24:58.438 human marker. Native `HandleStall` removed stale Junker head
`2774453366003465168`, leaving the exact one-item suffix headed by `2774453359334521800`. The guard verified the
stable suffix and asked the native engage path to resume it.

```text
[19:24:53.508] HandleStall.before Junker count=1 head=2774453366003465168 engaging=0
[19:24:53.508] HandleStall.after  delta=0 candidate=false result=-1 count=1 head=2774453366003465168 engaging=0
[19:24:56.505] HandleStall.before Junker count=2 head=2774453366003465168 engaging=0
[19:24:56.508] candidate=resume-after-native-prune action=verify-and-resume fleet=2644013931932498622 old_head=2774453366003465168 new_head=2774453359334521800 count_before=2 count_after_native=1
[19:24:56.510] action=resume-after-native-prune fleet=2644013931932498622 head=2774453359334521800 count=1 engage_result=0
[19:24:56.510] HandleStall.after  delta=-1 candidate=true result=0 native={count=1 head=2774453359334521800 engaging=0} guard={count=1 head=2774453359334521800 engaging=1}
```

The queue count remained one across the guard call. At 19:24:56.738, 228 ms later, the resumed target was delivered
to `OnFleetsDisposed` with `state=1`, `destroyed=0`, and `removal_reason=3`; native `ProcessQueue` then removed that
one remaining item. This is a target-disposal transition, not evidence of a guard double-drop.

## Second proven recovery at 19:26:02

This recovery did not receive a dedicated human marker, but it repeated the same safe transition:

```text
[19:25:59.701] HandleStall.before Junker count=2 head=2774454131505888217 engaging=0
[19:25:59.701] HandleStall.after  delta=0 candidate=false result=-1 count=2 head=2774454131505888217 engaging=0
[19:26:02.709] HandleStall.before Junker count=2 head=2774454131505888217 engaging=0
[19:26:02.714] candidate=resume-after-native-prune action=verify-and-resume fleet=2644013931932498622 old_head=2774454131505888217 new_head=2774454132604795867 count_before=2 count_after_native=1
[19:26:02.715] action=resume-after-native-prune fleet=2644013931932498622 head=2774454132604795867 count=1 engage_result=0
[19:26:02.715] HandleStall.after  delta=-1 candidate=true result=0 native={count=1 head=2774454132604795867 engaging=0} guard={count=1 head=2774454132604795867 engaging=1}
[19:26:09.125] OnFleetsDisposed id=2774454132604795867 state=3 prev=6 destroyed=1 battling=1 removal_reason=1 queue_count=0
```

Again, the guard preserved the native one-item suffix rather than dequeuing it. The resumed head was later reported
destroyed while battling and the queue reached zero through native disposal.

## Uncertain skip cluster at 19:25:19

The marker is useful as a symptom timestamp but does not correlate with an exact-prefix-prune recovery:

```text
[19:25:20.563] HandleStall.before Junker count=2 head=2774454144013304225 engaging=0
[19:25:20.563] HandleStall.after  delta=0 candidate=false result=-1 count=2 head=2774454144013304225 engaging=0
[19:25:23.570] HandleStall.before Junker count=3 head=2774454144013304225 engaging=0
[19:25:23.570] HandleStall.after  delta=0 candidate=false result=-1 count=3 head=2774454144013304225 engaging=0
[19:25:26.583] HandleStall.before Junker count=4 head=2774454144013304225 engaging=0
[19:25:26.583] HandleStall.after  delta=0 candidate=false result=-1 count=4 head=2774454144013304225 engaging=0
```

This is a real idle-head span while the tester continued adding targets. Native `HandleStall` did not prune the head,
so the strict guard correctly remained inactive. The observation cannot distinguish a visual skip from a transient
native stall.

## Stall-or-skip marker at 19:29:02

The final marker landed between two native no-op watchdog passes:

```text
[19:28:57.273] OnFleetsDisposed id=2774455520466434241 destroyed=1 queue={count=2 head=2774455520902641858 engaging=1}
[19:29:00.256] HandleStall.before Junker count=2 head=2774455520902641858 engaging=0
[19:29:00.256] HandleStall.after  delta=0 candidate=false result=-1 count=2 head=2774455520902641858 engaging=0
[19:29:03.271] HandleStall.before Junker count=2 head=2774455520902641858 engaging=0
[19:29:03.271] HandleStall.after  delta=0 candidate=false result=-1 count=2 head=2774455520902641858 engaging=0
[19:29:04.326] OnFleetsDisposed id=2774455520902641858 destroyed=1 queue={count=1 head=2774455521322073293 engaging=1}
[19:29:07.411] OnFleetsDisposed queue={count=0 head=0 engaging=0 last=2774455521322073293}
[19:29:11.278] OnFleetsDisposed id=2774455521322073293 destroyed=1 queue={count=0 head=0 engaging=0}
```

This confirms a visible approximately four-second idle interval, followed by native disposal of the stale head and
automatic engagement of the surviving head. The queue then reached zero while retaining that surviving target in its
`last` latch, and the same target was reported destroyed four seconds later. That is consistent with normal engagement,
not a skipped surviving target. It is another example where elapsed idleness alone is not a safe replay predicate.

## Conclusion

- The new exact-suffix recovery path fired twice and preserved queue count across both native engage calls.
- The 19:24:58 human report is strong runtime confirmation of the intended recovery behavior.
- No double-drop signature appears in the retained data.
- The remaining visible stalls occur when native `HandleStall` leaves the queue unchanged. Expanding the guard to act
  on elapsed idleness would risk replaying ordinary transient states already observed to self-heal.
- The current strict predicate should remain unchanged until a future marker captures a persistent no-op state that
  does not recover through native disposal.
- The two-rotation retention window is too short for detailed wave tracing. Future wave runs should begin with a
  bounded live export or copy evidence immediately after each marker so early intervals are not lost.
