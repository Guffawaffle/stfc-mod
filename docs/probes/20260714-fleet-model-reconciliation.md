# Probe: Fleet model reconciliation

- Status: proposed
- Owner: ship-state synchronization investigation
- Date: 2026-07-14
- Related patch label: none
- Related timeline refresh ID: current local dump corpus
- Related diff report: none
- Native seam ledger entry: `Digit.PrimeServer.Services.FleetService.UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData)`

## Question

When deployed-fleet data arrives, does `FleetService` update the matching client `FleetPlayerData` to a coherent state?

## Static Evidence

- Symbol: `Digit.PrimeServer.Services.FleetService.UpdateFleetWithDeploymentData`
- Method signature: `bool UpdateFleetWithDeploymentData(FleetPlayerData fleet, FleetDeployedData deploymentData)`; current script RVA `0x1613380`.
- String/script/config evidence: `FleetService` owns player-fleet update, deployed-fleet update, job lifecycle, state evaluation, repair cleanup, and explicit stuck-fleet recovery methods.
- Old/new diff context: none; this investigates chronic repair/warp/general stuck-state symptoms.
- Why this target is the narrowest candidate: it uniquely pairs one incoming deployed model with the exact player-fleet model used by all UI action gating.

## Risk

- Risk class: R4
- Confidence rung: static relationship
- Payload confidence: the two pointer types and boolean return are statically confirmed; nested payload fields and lifetimes are not yet proven for this detour. The first implementation may read only already-established scalar IDs/base states.
- Original/trampoline confidence: unproven for a detour; exact script ABI is recorded, but a clean reachability run is required before stack or payload escalation.
- Behavior change expected: no

## Implementation Plan

- Module/file: `mods/src/patches/parts/client_ship_state_probe.cc`
- Config or compile guard: proposed science-only `[advanced.diagnostics].ship_state_probe = "fleet_reconciliation"`; not accepted by the parser or implemented yet. The current parser fails this value closed to `"off"`.
- Hook descriptor name: `kFleetModelReconciliationHook`
- Target assembly: `Digit.Client.PrimeLib.Runtime`
- Target namespace: `Digit.PrimeServer.Services`
- Target class: `FleetService`
- Target method: `UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData)`
- Install path: dedicated science module entry in `patches.cc`; install only when the exact mode is selected.
- Log tag or event kind: `ship-state-probe.fleet-model-reconciliation`

Record a bounded pre/post pair with one correlation sequence, monotonic timestamp, thread ID, stable fleet IDs, numeric incoming/client current and previous states, and the original boolean result. Do not retain pointers, serialize whole objects, or call recovery/actions.

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

Observe one incoming fleet update and its client-model result without altering requests or reconciliation.

Steps:

1. Enable only `fleet_reconciliation`; leave stack budget zero.
2. Build/deploy releasedbg and cycle the client.
3. Confirm the hook registry reports exactly one installed probe seam.
4. Capture a serial `fleet-slots-state` baseline and mark the observation window.
5. Perform one selected action: repair one damaged docked fleet, or reproduce one known stuck repair/warp transition. Do not combine actions in one window.
6. Capture the serial post-action fleet snapshot, stop the window, and disable the probe.

Expected log marker/event:

`ship-state-probe.fleet-model-reconciliation` with paired pre/post events for the affected fleet.

Stop immediately if:

The game crashes, hangs, loses input, action behavior changes, output becomes unbounded, a duplicate hook owner is reported, or a server request is emitted by the probe.

Report back:

Build commit, config snapshot, marker/sequence range, pre/post numeric states, passive fleet snapshots, selected human action, and any recovery required.

## Result

- Build/deploy command: not run
- Runtime command: not run
- Human action performed: none
- Observed log/event evidence: none
- Crash/hang/recovery notes: none
- Answer to the question: pending approval and implementation

## Exit Decision

Revise after the first reachability run. If updates arrive but projection remains wrong, retire this seam and propose `FleetService.EvaluateFleetState(FleetPlayerData)` as a separate canary. If updates never arrive, move upstream without keeping both hooks installed.

Next action: keep unimplemented until the repair action-status lane produces a real repair transition or issue #167 supplies a bounded stuck-fleet reproduction.
