# Probe: Repair action-status transition

- Status: implemented; repair flip reproduced and captured
- Owner: ship-state synchronization investigation
- Date: 2026-07-14
- Related patch label: none
- Related timeline refresh ID: current local dump corpus
- Related diff report: none
- Native seam ledger entry: `Digit.PrimeServer.Models.FleetPlayerData.GetActionStatus(ActionType)`

## Question

Which repair `ActionStatus` transitions occur between one Repair click and the stable Ask for Help button?

## Static Evidence

- Symbol: `Digit.PrimeServer.Models.FleetPlayerData.GetActionStatus`
- Method signature: `ActionStatus GetActionStatus(ActionType actionType)`; current script RVA `0x17BE0B0`.
- String/script/config evidence: `ActionStatus` contains `InProgress=200`, `InProgress_Free=201`, and `InProgress_AskForHelp=202`; `ActionLocaleSetting` owns a distinct `_inProgressAskForHelpSettings` presentation.
- Old/new diff context: none; this investigates a chronic runtime symptom.
- Why this target is the narrowest candidate: its return value directly feeds action presentation while avoiding global action-widget hooks and repair request callback interception.

## Risk

- Risk class: R4
- Confidence rung: static relationship
- Payload confidence: `ActionType` and `ActionStatus` are statically confirmed 32-bit enums; `FleetPlayerData*` lifetime at this seam and fleet-ID reads are not yet runtime-proven for this detour.
- Original/trampoline confidence: unproven for a detour; exact script ABI is recorded, but a clean reachability run is required before parameter or stack escalation.
- Behavior change expected: no

## Implementation Plan

- Module/file: `mods/src/patches/parts/client_ship_state_probe.cc`
- Config or compile guard: science-only `[advanced.diagnostics].ship_state_probe = "repair_action_status"`; default `"off"`. Caller capture uses a separate `ship_state_probe_stack_budget` integer clamped to 0-1 and defaulting to 0.
- Hook descriptor name: `kRepairActionStatusHook`
- Target assembly: `Digit.Client.PrimeLib.Runtime`
- Target namespace: `Digit.PrimeServer.Models`
- Target class: `FleetPlayerData`
- Target method: `GetActionStatus(ActionType)`
- Install path: dedicated science module entry in `patches.cc`; install only when the exact mode is selected.
- Log tag or event kind: `ship-state-probe.repair-action-status-transition`

Emit only when `actionType == ActionType::Repair` and the returned numeric status changes for that fleet. Record sequence, monotonic timestamp, thread ID, numeric fleet/current/previous/action status, and original return. Do not retain the object pointer.

Registry requirements:

- Use `HookDescriptor`.
- Use `HookModuleHealth`.
- Use `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- Do not use raw `SPUD_STATIC_DETOUR`.

## Disable Path

- Flag or code path to disable: set probe mode to `"off"`, set `ship_state_probe_stack_budget = 0`, and restart the client.
- File/entry to delete if it crashes: remove `client_ship_state_probe.cc` and its single `patches.cc` install entry.
- Expected boot log when disabled: the science probe module is skipped and owns no hook target.

## Human Smoke Test

Goal:

Observe one repair action-status sequence without altering the repair request or UI.

Steps:

1. Enable only `repair_action_status`; leave stack budget zero for normal transition capture. Set it to one only for an explicitly approved caller-sample run.
2. Build/deploy releasedbg and cycle the client.
3. Confirm the hook registry reports exactly one installed probe seam.
4. Mark the observation window.
5. Click Repair once on one damaged docked fleet and wait until Ask for Help appears or the state stops converging.
6. Stop the observation window and disable the probe before another experiment.

Expected log marker/event:

`ship-state-probe.repair-action-status-transition` with a small ordered set of distinct numeric statuses.

Stop immediately if:

The game crashes, hangs, loses input, repair behavior changes, more than one server request is observed, duplicate hook ownership is reported, or events are emitted continuously without status changes.

Report back:

Build commit, config snapshot, marker/sequence range, status sequence, final visible button, any server error, and whether the game needed recovery.

## Result

- Build/deploy command: `axf run global.stfc-mod-private.cycle --build-mode releasedbg`; run once with the probe off, once with `repair_action_status`, and once more after restoring `off`.
- Runtime command: serial `live-state --view fleet-slots`, an AX marker, and bounded exact-kind `recent-events` follow calls.
- Human action performed: during a later free-play window, Repair was clicked on one docked damaged fleet and the visible Free action was clicked from a different screen than the usual reproduction. The button then flipped back to the cost-to-repair presentation and a popup appeared.
- Observed log/event evidence: exact-kind event-store sequence 297-305 and native log lines 1643-1652 captured the same fleet and thread. The original return was preserved at every call.

| Local time | ActionStatus | Fleet state | Correlated event |
| --- | --- | --- | --- |
| 03:54:46.702 | `Ready` (100) | `Docked`, previous `IdleInSpace` | Pre-repair presentation |
| 03:54:55.129 | `InProgress_Free` (201) | `Repairing`, previous `Docked` | `fleet-slot-repair-started` |
| 03:54:56.702 | `Complete` (300) | `Repairing` | Job/action reports complete |
| 03:54:57.452 | `Ready` (100) | `Repairing` | Invalid cost-to-repair presentation window begins |
| 03:54:57.453 | unchanged | `Repairing` | Empty-title state-0 toast; native `REPAIR_COMPLETE`; repair-complete notification queued |
| 03:54:59.000 | `Disabled` (0) | `Docked`, previous `Repairing` | Invalid presentation window ends after about 1.55 seconds |
| 03:54:59.964 | unchanged | `Docked` | Debounced `fleet-slot-repair-completed` runtime capture executes |

- Crash/hang/recovery notes: none. The fleet converged to Docked without manual recovery. The probe remains enabled for the current investigation window.
- Answer to the question: confirmed. The observed sequence was `Ready → InProgress_Free → Complete → Ready → Disabled`. `GetActionStatus(Repair)` returned `Ready` for about 1.55 seconds while `CurrentState` was still `Repairing`, directly explaining the transient cost-to-repair button. The popup coincided with repair completion and an empty-title toast; its exact text is still needed to attribute it conclusively.

## Exit Decision

The repair transition and failure window are captured cleanly. A one-shot caller-stack airlock is now implemented but remains default zero. It triggers only on `Complete → Ready` while `CurrentState == Repairing`, consumes its one-event budget atomically, and records module basenames plus relative offsets for offline symbolization.

Next action: run the one-event caller sample from Ship Manage, identify the UI caller for the `Complete → Ready` transition, and correlate it with repair job/fleet-state completion ordering. Do not install the broader reconciliation seam concurrently.
