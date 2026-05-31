# Action Queue Skip Investigation Log

## Goal

Track queue-skip experiments in one place so we can correlate code changes with live behavior and avoid repeating dead ends.

## Baseline

- Baseline before this session: skips happened occasionally.
- Current repo branch for experiments: `feat/manual-nav-refresh-hotkey-guffa`.
- Queue repair strategy to preserve:
  - keep Guffa's narrow postcondition-based queue repair in `mods/src/patches/parts/misc.cc`
  - do not migrate NetNiv queue behavior
  - do not introduce `BlockFirstArrival` / `ReleaseHeldArrival`

## High-Confidence Findings

- `fleet-slot-combat-ended` and `deployment-battle-end-event` can arrive before the queue state is actually resolved.
- `RemoveActionFromQueue` is often where the queue is finally proven to have removed a target.
- `fleet-slot-arrived-at-destination` is also being used by the game in head-turnover sequences that still represent a real queue advance.
- The queue can temporarily lose `LastEngagedTargetId` even while `IsEngaging == true`.
- In some runs, combat starts while the queue instance is still `is_engaging=false` and `last_target=0`, so there is no authoritative exact target id latched yet.

## Experiments

### 1. Preserve orphaned engaging during early combat-end window

- Change:
  - `HandleStall` stopped clearing orphaned engaging immediately after `fleet-slot-combat-ended` / `deployment-battle-end-event`.
- Outcome:
  - helped avoid one duplicate-reengage path
  - still exposed real stalls

### 2. Restore `LastEngagedTargetId` from live head when already engaging

- Change:
  - if `IsEngaging == true`, `LastEngagedTargetId == 0`, and a live queue head exists, restore the native last-target field from the live head
- Outcome:
  - kept some stalled queues coherent
  - still not enough when the game never latched an exact engaged target before combat ended

### 3. Infer engaging state from recent combat-start

- Change:
  - temporarily restored both `IsEngaging=true` and `LastEngagedTargetId=head` from recent `fleet-slot-combat-started`
- Outcome:
  - regressed skip frequency
  - rolled back
- Conclusion:
  - combat-start alone is not authoritative enough to infer the exact queue head safely

### 4. Sticky engaged-target latch

- Change:
  - add a mod-side sticky engaged target per action-queue instance
  - set it only from queue-authoritative points:
    - successful `TryPlanPathAndEngageTarget`
    - native nonzero `LastEngagedTargetId`
    - stall-time repair when native state is already engaging and we restore the last target
  - clear it only from queue-authoritative points:
    - confirmed remove of that target
    - clear-queue / clear-queue-and-move
    - queue empty after remove
- Intended effect:
  - do not let the exact engaged id drop just because the game sends an early combat-end style signal
  - prefer restoring native `LastEngagedTargetId` from sticky exact id when it still matches live queue contents

## Important Live Signatures

### Early end signal before authoritative queue removal

- Sequence seen in logs:
  - `fleet-slot-combat-started`
  - queue still unstable or unlatching target
  - `fleet-slot-combat-ended`
  - actual `RemoveActionFromQueue(target_id)` happens later

This means combat-end is not sufficient proof that the queue head should be forgotten.

### Arrival-turnover remove

- Sequence seen in logs:
  - queue is engaging target A
  - `fleet-slot-arrived-at-destination`
  - `RemoveActionFromQueue(target A)` runs
  - queue advances to target B

This path is real and should not be treated the same as a stale combat-end cleanup.

## Current Risks

- If sticky target latching helps the exact-id problem but skips remain, the remaining issue is likely that the game sometimes reaches combat without any authoritative queue-target latch at all.
- If sticky target latching regresses behavior, the likely failure mode is holding a target id across a genuine head transition longer than intended.

## Useful Data Points To Capture

- exact target ids at:
  - `TryPlanPathAndEngageTarget`
  - `RemoveActionFromQueue`
  - `HandleStall`
- whether native `LastEngagedTargetId` dropped to `0`
- whether sticky target matched the live queue head
- whether remove happened on:
  - `fleet-slot-combat-ended`
  - `deployment-battle-end-event`
  - `fleet-slot-arrived-at-destination`
- whether queue count changed only after manual repair

## Next Questions

- Does sticky-target preservation reduce skips back toward the old occasional baseline?
- When a skip still happens, did we ever have an authoritative exact target id latched before the skip?
- Are the worst skips concentrated in the `arrived-at-destination` turnover path or in the pre-remove early-combat-end path?
