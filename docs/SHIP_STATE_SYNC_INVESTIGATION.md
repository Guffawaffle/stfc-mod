# Ship State Synchronization Investigation

Date: 2026-07-14
Branch: `investigation/ship-state-sync`
Status: repair action-status reachability passed; probe restored to off; real repair sequence pending

## Problem Register

### 1. Repair action churn

Priority: highest.
Observed: after Repair is clicked, the action area cycles through several clickable but incorrect options before settling on **Ask for Help**. Sometimes it never converges; Repair becomes clickable again, but another request returns an error.
Expected: one repair request produces a coherent repairing state and the correct next action without exposing transient invalid choices.

### 2. Fleet stuck in an action state

Priority: second.
Observed: a ship can remain stuck in a displayed or underlying repair, warp, or other action state. No action succeeds through multiple UI entry points, so this is not assumed to be a fleet-bar-only defect.
Expected: authoritative fleet state converges and at least the valid recovery/action path remains usable.

### 3. Placeholder

Priority: reserved.
The symptom was forgotten during the initial report. Preserve this ordinal and do not infer a description. Replace this section when the symptom is remembered.

### 4. Ghost fleet entities

Priority: fourth.
Observed: ghost ships are now uncommon, most often seen during wave defense, but occasionally occur elsewhere.
Expected: departed/disposed fleets do not remain visible or interactive.

This is probably an entity lifetime or view ownership problem, not the same player-fleet action-state problem as issues 1 and 2. Earlier abandoned investigation evidence remains in [Action Queue Skip Investigation Log](ACTION_QUEUE_SKIP_INVESTIGATION_LOG.md); do not revive its manual-refresh behavior or old broad probes without a fresh question.

## Issue Boundaries

No older GitHub issue duplicates these symptoms. Closed issues #46 and #56 concern mod-owned input/action dispatch and are adjacent history only.

Current tickets:

1. [#166 Repair action UI cycles through invalid states before Ask for Help](https://github.com/Guffawaffle/stfc-mod/issues/166)
   - Scope the user-visible repair sequence and the occasional non-converging/error case together until evidence proves two causes.
   - Link the repair action-status probe and one bounded reproduction timeline.
2. [#167 Fleet can remain stuck in stale repair/warp/action state and reject commands](https://github.com/Guffawaffle/stfc-mod/issues/167)
   - Keep separate because it spans multiple actions and UI entry points.
   - Link the model-reconciliation probe and passive fleet-slot captures.
3. **Reserved placeholder**
   - Do not create a GitHub issue until the symptom is remembered.
4. **Ghost fleet entity persists, usually during wave defense**
   - Create only when this lower-priority lane is resumed with a current reproduction.

## Current State Ownership Model

The fleet bar is a consumer, not the authoritative owner. Existing live snapshots read each slot from `FleetsManager::GetFleetPlayerData()` in `mods/src/patches/live_debug_fleet_runtime_observers.cc`.

The current static map has four layers:

| Layer | Candidate owners | Evidence and role |
| --- | --- | --- |
| User/UI action | `FleetLocalViewController`, `DynamicActionGroupWidget`, `ActionElementWidget` | Binds action buttons and chooses locale/button context from derived action status. |
| Client player-fleet model | `FleetPlayerData`, `ActionDataContainer` | Owns computed fleet state, current job, repair action data, enabled actions, and action status. This model gates actions outside the fleet bar too. |
| Incoming reconciliation | `FleetService`, `JobService`, deployment/job events | Applies deployed-fleet and job updates, evaluates the player model, and fires client events. |
| Server request | `FleetService.RepairFleet`, deployment/course requests, `JobService.RequestHelpJob` | Sends repair, movement, help, and recovery requests. Async completion is distinct from request initiation. |

Static adjacency is not a proven call graph. Real callers and ordering require runtime sequence evidence and a separately gated stack sample. Direct callees require offline disassembly of the exact game build, mapped back to current script RVAs; the next candidate is validated by moving the probe rather than keeping both hooks installed.

## Repair Findings

The strongest current hypothesis is an ordering race between:

1. `FleetPlayerData.CurrentState` and its state container.
2. The attached repair `Job` and its lifecycle state.
3. The derived repair `ActionStatus`.
4. Job help flags: allowed, requested, and progress.

Relevant exact static evidence:

- `ActionStatus` includes `InProgress=200`, `InProgress_Free=201`, and `InProgress_AskForHelp=202`.
- `ActionLocaleSetting` has dedicated ask-for-help settings and selects button presentation from action status.
- `FleetPlayerData.GetActionStatus(ActionType) -> ActionStatus` is therefore the narrowest direct observer of the button-driving state.
- `FleetService.RepairFleet(long fleetId, CallbackContainer<int>)` posts the `fleet/repair` route.
- `JobService.RequestHelpJob(IJob, CallbackContainer<string>)` owns the help request.
- `FleetService` receives player-fleet updates and repair job created/discarded events.
- The current binary contains: `[FleetService] HandlePlayerFleetsUpdate() - Failed to add Repair Job {0} to fleet {1} because job state was 'Collected'. Cleaning up this job!`

That literal supports an existing repair-job/fleet-update ordering edge. It does not prove that this exact edge causes the reported symptom.

## Stuck-Fleet Findings

`FleetPlayerData` combines incoming deployed state, job state, course and warp data, action masks, and state evaluation into the model consumed by action gating. `FleetService` is the strongest shared repair/warp reconciliation neighborhood.

Relevant static candidates:

- `FleetService.UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData) -> bool`
- `FleetService.EvaluateFleetState(FleetPlayerData)`
- `FleetPlayerData.UpdateDeployedFleetState(Nullable<DeployedFleetState>) -> bool`
- `FleetPlayerData.UpdateJobData(IJob) -> bool`
- `FleetPlayerData.EvaluateState() -> bool`
- `FleetPlayerData.EvaluateEnabledActions()`
- `FleetService.ClearOutFleetRepairState(long fleetId)`
- `FleetService.RecoverStuckFleets()`

`RecoverStuckFleets()` appears to dispatch a server request with a boolean JSON-response callback. It is relevant evidence that an explicit recovery contract exists, but it must not be invoked during discovery.

## Prerequisite: FleetState Drift

The repository's `FleetState` wrapper has stale composite values. The current dump reports:

- `CannotMove=2552`, repo `504`
- `CanManage=2947`, repo `899`
- `CanEngage=3591`, repo `1543`
- `CanRecall=5637`, repo `1541`
- `Deployed=8133`, repo `1989`
- `CanLocate=8135`, repo `1991`

The dump also contains newer flags such as `AutoHunting=2048` and `Outposting=4096`. Base states involved in the reported symptoms remain stable: `Repairing=32`, `WarpCharging=128`, and `Warping=256`.

Before trusting diagnostic labels or composite-state policy, update the wrapper/serializer vocabulary and add a static validation test. Treat this as a prerequisite correctness repair, not as the root-cause fix for issues 1 or 2.

## Investigation Sequence

### Phase 0: make observations trustworthy

- Correct current `FleetState` composite values and friendly-name serialization.
- Add a dump/version evidence note and pure tests for known numeric mappings.
- Confirm no behavior change is smuggled into this patch.

### Phase 1: passive baseline

- Run `live-state --view fleet-slots` serially before, during, and after one repair reproduction.
- Capture recent fleet/deployment events without clearing the event ring.
- Record numeric state, previous state, fleet ID, ship ID, selected slot, and timestamps.
- If the UI churns while the underlying player model stays stable, move toward action-status/UI projection.
- If the player model churns or remains stale, move toward incoming reconciliation.

### Phase 2A: repair action-status canary

Use the proposed [Repair Action Status Transition Probe](probes/20260714-repair-action-status-transition.md).

- One target: `FleetPlayerData.GetActionStatus(ActionType)`.
- Filter to Repair before emitting.
- Record only status transitions per fleet.
- First clean run proves reachability and return-value stability.
- A later, separately enabled one-shot stack budget records module-relative callers for the transition; symbolize offline.

### Phase 2B: shared model-reconciliation canary

Use the proposed [Fleet Model Reconciliation Probe](probes/20260714-fleet-model-reconciliation.md).

- One target: `FleetService.UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData) -> bool`.
- Record bounded before/after numeric client and incoming state, IDs, thread, sequence, and return value.
- Do not retain either pointer or inspect unproven nested fields.
- Run separately from the repair action-status canary.

### Phase 3: map call traffic, then move

- Symbolize the bounded module-relative entry stack offline to identify real callers.
- Disassemble only the exact target method from the exact tested game build and map direct call RVAs to current script/dump symbols.
- Label indirect/interface/delegate calls as unresolved unless runtime evidence identifies them.
- Select one next seam from this evidence, then retire or disable the current hook before installing it.

Move, do not widen:

Based on Phase 2 evidence, retire or move the canary to exactly one next seam:

- Repair request initiation: `FleetService.RepairFleet`.
- Repair job ordering: `FleetService.HandlePlayerFleetsUpdate` or one repair-job lifecycle handler.
- Client projection: `FleetService.EvaluateFleetState`.
- Repair-only cleanup: `FleetPlayerData.UpdateJobData` or `FleetService.ClearOutFleetRepairState`.

Do not hook generated repair closures, replace callbacks, invoke recovery, add a deployment-event detour duplicate, or install a family.

### Phase 4: resolution

Implement behavior only after the trace locates the divergence. Keep the repair fix and general stuck-fleet fix separate unless the same failed invariant is demonstrated in both reproductions.

## Evidence Schema

Every emitted event should contain:

- Probe version and build commit.
- Monotonic sequence and timestamp.
- Thread ID.
- Seam and phase (`pre`, `post`, or `transition`).
- Stable fleet/ship IDs when already proven safe.
- Numeric incoming, current, previous, action, job, and help states only when their payload rung is approved.
- Original return value.
- Stack sample ID, not an unbounded inline symbol trace.

Logs must be bounded, deduplicated, and free of coordinates, auth/session data, request bodies, and retained managed pointers.

## Stop Conditions

Disable the probe immediately if the game crashes, hangs, loses input, changes action behavior, repeats server requests, produces unbounded output, or reports duplicate hook ownership. A failed seam is recorded and removed; it is not tuned in place by adding more hooks.

## Process Findings From This Pass

- Static corpus lanes can run in parallel safely.
- File-backed live queries are single-flight and must run serially.
- `dump-method` works with the short class name; the fully qualified class plus `--fts` path currently fails in the private adapter.
- Existing `fleet-slots-state` is a useful client-model baseline but lacks jobs, deployed state, action masks/status, callbacks, and callers.
- The dump provides exact symbols and ABI hints, not actual callers. Real caller evidence requires the stack-capture airlock.
- The default-off repair probe installed as the sole owner of `GetActionStatus`, returned the original value unchanged, and emitted two bounded status-0 observations for docked fleets without a crash or hang.
- This was a reachability result, not a repair reproduction: no fleet entered `Repairing`, no Ask for Help transition was captured, and the stack-capture airlock therefore remains closed.
- The game config is back on `ship_state_probe = "off"`, and the final releasedbg cycle logged the module as skipped.
