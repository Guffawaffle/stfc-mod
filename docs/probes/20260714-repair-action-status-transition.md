# Probe: Repair action-status transition

- Status: implemented; reachability passed, repair sequence pending
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
- Config or compile guard: science-only `[advanced.diagnostics].ship_state_probe = "repair_action_status"`; default `"off"`; no stack capture is implemented in the first airlock.
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

- Flag or code path to disable: set probe mode to `"off"` and restart the client; stack budget remains independently zero by default.
- File/entry to delete if it crashes: remove `client_ship_state_probe.cc` and its single `patches.cc` install entry.
- Expected boot log when disabled: the science probe module is skipped and owns no hook target.

## Human Smoke Test

Goal:

Observe one repair action-status sequence without altering the repair request or UI.

Steps:

1. Enable only `repair_action_status`; leave stack budget zero.
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
- Human action performed: two docked fleets were selected during the observation window; no fleet entered `Repairing`, so a Repair click was not established.
- Observed log/event evidence: hook registry reported exactly one installed science seam. Two deduplicated events returned `actionType=-1850668115`, `actionStatus=0`, `originalReturn=0`, `currentState=2`, on the same thread for two distinct docked fleets. No later repair status appeared.
- Crash/hang/recovery notes: none. Final cycle was healthy with the probe explicitly restored to `off` and skipped at boot.
- Answer to the question: inconclusive for the repair sequence. The hook ABI, receiver reads, original return, filter, dedupe, and bounded event path are runtime-reachable; the required damaged-fleet repair transition was not reproduced.

## Exit Decision

Reachability passed, but the stack-capture airlock remains closed because no repair transition was observed. Re-enable this same probe only for one known damaged docked fleet; do not add stack capture until that sequence is captured cleanly.

Next action: reproduce one real Repair action, record the complete distinct status sequence and visible button outcome, then decide whether one-shot caller capture is justified.
