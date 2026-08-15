# Probe: fleet notification scan lifecycle

- Status: approved
- Owner: runtime-observability
- Date: 2026-08-15
- Tracking issue: `Guffawaffle/stfc-mod#255`
- Concern ID: `fleet-notification-scan`
- Sunset boundary: `2.2.0`
- Native seam ledger entry: none; this probe adds no hook

## Question

During a long play session, does fleet-notification follow-through scanning remain active, stall inside a native read,
or retain notification cache entries after the corresponding fleets are no longer in the current slots?

## Static Evidence

- `FleetNotificationScanPolicy` scans every 250 ms for 30 seconds, then every 5 seconds, and permits a followed
  fleet state to keep the request alive for up to 24 hours.
- `fleet_notifications.cc` retains state, ship-name, resource-name, cargo-level, and mining-ETA maps keyed by fleet ID.
- The maps expose no eviction path for fleet IDs which disappear from current slots.
- Existing main-log rows report lifecycle transitions but not phase progress, elapsed-time windows, cache cardinality,
  or stale-entry counts.

## Risk

- Risk class: R0
- Confidence rung: runtime observed
- Payload confidence: typed, allowlisted, aggregate-only records
- Original/trampoline confidence: not applicable; existing owners only
- Behavior change expected: no

## Implementation Plan

- Module/file: `mods/src/patches/fleet_notification_diagnostics.*`
- Activation: `[advanced.diagnostics.concerns].enabled = ["fleet-notification-scan"]`
- Output: `community_patch_target_fleet-notification-scan.jsonl`
- Existing owners: fleet state widget observation and the fleet-notification frame subscriber
- New hooks: none
- Search markers: `TARGET_DIAGNOSTIC_ENABLED`, `TARGET_DIAGNOSTIC_WRITE`, `TARGET_DIAGNOSTIC_REGISTER`

The concern records paired begin/end events around manager acquisition and slot enumeration. It emits a bounded
10-second summary for scan cost, followed-state counts, cache cardinality/high-water/staleness, and the full producer
path through `scan-completed` admission. Diagnostic overhead is the measured producer-path time minus the timed game
work for manager acquisition and slot enumeration.
Stale cardinality is exact and uses membership checks for at most the eight current fleet slots rather than walking
historical cache entries.

The shared substrate uses a preallocated bounded MPSC ring with fixed inline event storage. Queue limits account for
the complete ring slot, so producers perform no event heap allocation and ordinary producer contention cannot create
lock-busy gaps. The writer drains ready records in bursts and keeps each concern file open for the burst, making file
preparation, size checks, and close/flush batch work rather than per-record work.

Shutdown stops admission immediately and gives queued records 750 ms to drain. Records still queued at the deadline
are counted as shutdown drops. A synchronous filesystem call already in flight is joined for memory and module-lifetime
safety and may exceed that drain target; such an overrun is counted and warned. The blocked-writer test injects this
condition and verifies both halves of the contract.

## Disable Path

- Remove `fleet-notification-scan` from the generic concern allowlist and restart the game.
- An unknown or removed ID is reported once as unsupported and ignored.
- An expired registration is reported once as expired and cannot start its writer channel.
- Delete the concern module, instrumentation calls, registry entries, tests, and this record when the exit decision is
  `delete`; do not retain a tombstone.

## Human Smoke Test

Goal:

Confirm that a fleet entering a followed state produces an isolated, parseable file without input, focus, window,
notification, or rich-presence regressions.

Steps:

1. Enable only `fleet-notification-scan` in the generic allowlist and restart through the normal AX cycle.
2. Trigger an impulse or battle transition and continue ordinary play for at least one 10-second window.
3. Confirm paired `scan-started`/`scan-completed` and phase records, then inspect one `scan-summary`.
4. Leave the session running long enough to cross the 30-second cadence boundary when practical.

Expected evidence:

- The targeted file contains only `concern_id=fleet-notification-scan` JSONL rows.
- Sequence numbers increase, common build identity is present, and queue/writer failure counters remain zero.
- Cache size/high-water/stale fields and diagnostic collection cost are visible without fleet IDs or names.
- A settled or suspended session emits `scan-ended`; a missing phase completion identifies a stalled boundary.

Stop immediately if:

- input latency, focus behavior, black-window behavior, notification delivery, or rich-presence timing regresses;
- queue drops or writer failures rise during ordinary scanning;
- cache collection time becomes material on the frame thread.

## Result

- Build/deploy command: `axf run global.stfc-mod-private.cycle --build-mode releasedbg --tail 40`
- Runtime command: structured PowerShell query over the concern-isolated JSONL file
- Human action performed: normal fleet battle and impulse transitions while the concern was enabled
- Observed evidence: 2,969 contiguous records across 20 sessions; one session remained active for 59,888 ms, crossed
  from the 250 ms cadence to the 5,000 ms cadence, and ended `settled`; zero lock, queue, shutdown, record-size, or
  writer failures; maximum scan time 988 us; cache sizes reached 7 state/name entries and 4 resource entries with zero
  stale entries and no truncated inspection
- A follow-up process exposed two `dropped_lock_busy` admissions and orphaned `phase-completed` rows at session 6,
  scan 114 and session 7, scan 232. That ordinary-contention failure triggered the MPSC and batching sweep.
- Post-sweep evidence (`62e9d72`): 27 envelope-v2 records from a battle transition contained four complete scan pairs
  and eight complete phase pairs with contiguous sequences, zero queue drops, zero writer failures, and zero shutdown
  overruns. The first four-scan summary measured 626 us total producer time, 178 us maximum producer time, 40 us total
  diagnostic overhead, 13 us maximum diagnostic overhead, and a 1 us exact current-slot cache snapshot.
- Crash/hang/recovery notes: AXF's boot parser timed out during initial process startup, but the client reached a healthy,
  responsive state with the releasedbg build/deployed hashes equal and no native errors
- Answer to the question: the scanner remains active while followed states recur, backs off to the intended 5-second
  cadence after 30 seconds, and settles when follow-through stops. The observed one-minute boundary showed no
  accumulating scan cost, cache cardinality, stale entries, or capture pressure. Multi-hour behavior remains unproven.

## Exit Decision

Pending: delete | revise | promote

Promotion requires a demonstrated support consumer, measured low overhead, a supported schema, and explicit
retention/compatibility ownership. Extending the sunset is not promotion.
