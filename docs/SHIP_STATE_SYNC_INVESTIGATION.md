# Ship State Synchronization Investigation

Date: 2026-07-14
Branch: `investigation/ship-state-sync`
Status: archived evidence; production successor merged as PR #248

> Final disposition (2026-08-11): the investigation-only `ClientShipStateProbe` runtime and its science-build config
> were retired. The accepted behavior now ships through the production-tier, default-on `RepairActionInterlockHooks`
> owner with `[patches].repairactioninterlock = false` as the restart rollback. Config names and procedures below are
> retained as a historical record of the investigation, not as active runtime controls.

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

### Captured repair timeline

A free-play reproduction from a different screen captured the reported flip on one fleet:

`Ready → InProgress_Free → Complete → Ready → Disabled`

The critical mismatch is the second `Ready`: `GetActionStatus(Repair)` returned the cost-to-repair-ready status while `FleetPlayerData.CurrentState` still reported `Repairing`. The mismatch lasted about 1.55 seconds before the fleet became `Docked` and Repair became `Disabled`.

Native fleet-bar logging reported `REPAIR_COMPLETE` at the same millisecond that `Ready` reappeared. On Ship Manage, the rebound action opened the instant lat-cost confirmation as though the cost button had been clicked. This makes the mismatch a spend-path interaction hazard, not merely visual churn. An empty-title state-0 toast and the mod's repair-complete notification were also queued at that boundary.

A second Ship Manage reproduction on 2026-07-15 showed that the invalid projection is not limited to a
`Complete → Ready` transition. For one fleet, the observed sequence included
`InProgress_AskForHelp → Ready → InProgress_AskForHelp` while `CurrentState` remained `Repairing`. The invalid
`Ready` result lasted about 334 ms, and native fleet-bar logging again reported `REPAIR_COMPLETE` at the same
boundary. The original one-shot caller predicate deliberately required `Complete → Ready`, so it did not consume
its budget. The predicate is now keyed to the invariant violation itself: any distinct transition to `Ready` while
the fleet is still `Repairing`.

### Caller sample and post-completion Repair reappearance

The revised one-event airlock fired at 01:27:44.278 on an `InProgress_Free → Ready` transition while the fleet
remained `Repairing`. Offline symbolization against the exact deployed DLL and current game dump resolved the
relevant frames:

| Frame | Module-relative RVA | Symbol |
| --- | --- | --- |
| 0 | `VERSION.dll+0xF2D3A` | `CaptureModuleRelativeStack` |
| 1 | `VERSION.dll+0xF25E0` | `FleetPlayerData_GetActionStatus_Hook` |
| 2 | `GameAssembly.dll+0x11E6C12` | `ActionElementWidget.GetInstantButtonContext+0x72` |
| 3 | `GameAssembly.dll+0x11E63E6` | `ActionElementWidget.HandleReactiveInt+0x216` |
| 6 | `GameAssembly.dll+0x16517BA` | `JobService.UpdateJobList+0x31A` |
| 7 | `GameAssembly.dll+0x1651368` | `JobService.Tick+0x4E8` |
| 8 | `GameAssembly.dll+0x1530E7C` | `GameServer.TickServer+0x13C` |

The exact disassembly shows `GetInstantButtonContext` querying `IActionData.GetActionStatus` and the instant cost
separately, then `HandleReactiveInt` copying the returned context into `_instantButtonContext` through
`GenericButtonContext.ValueCopy`. This is a real caller chain: a job-list tick refreshes the action widget while
the repair model is between coherent states.

At 01:39:02.195, after Free had finished the repair, the client momentarily re-exposed Repair with a displayed cost
of zero. The transition evidence was again `InProgress_Free → Ready` while `CurrentState == Repairing`, and
`REPAIR_COMPLETE` plus an empty-title state-0 toast occurred at the same millisecond. In user-visible terms, the
action layer briefly considered the already-repaired ship repairable again while the fleet model had not yet
converged from `Repairing` to `Docked`. The zero amount is consistent with the instant-button context being rebuilt
from independently observed status and instant-cost values during the non-atomic job update; the current probe does
not record the cost payload, so that final payload relationship remains an evidence-backed inference rather than a
direct measurement.

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

The trace located the repair divergence and mapped the instant-button click path. The first canary reused the
sole-owner `GetActionStatus` seam and changed only Repair `Ready` while the fleet was still `Repairing` to `Disabled`.
It avoided changing job reconciliation or server requests, but its human smoke still displayed pay-to-repair choices
before Ask for Help. One invalid `Ready` arrived while the model briefly reported `Docked` with previous state
`Repairing`, outside the predicate; another matched the predicate and was projected to `Disabled`, yet the visible
churn remained. That mixed-symptom smoke proves the status seam is not a complete solution, but does not erase the
initial improvement observed for the pre-Ask-for-Help path. The original guard has therefore been restored unchanged
as a science canary. Its next acceptance test covers only the original churn; it must not be widened to absorb the
post-completion `Instant 0` case.

### Known merge-boundary limitation

After a repair completes, the Repair action can still briefly reappear as an `Instant 0` button before the client
model converges to `Docked`. The observer has not accidentally activated this button, and this investigation captured
no unintended repair request or spend from it. The behavior remains unresolved: this branch documents and safely
probes the projection race, but does not claim to fix it. Any follow-up for this separate symptom should begin at the
mapped instant-button projection boundary rather than widening the original-issue guard.

### Verified reproduction (2026-07-17)

A human smoke reproduced the pre-repair action-card layout while the repair flow was still transitioning: the card
briefly showed `Repair [time]` above `Instant 0` instead of the expected active-repair action. This is the same unsafe
visual replacement reported during the rapid `Ask for Help` → `Speed Up` cadence, where a muscle-memory second click
can land on the reappearing Instant control.

The first run's native log corroborated only its `REPAIR_COMPLETE` boundary. A second user reproduction while the
passive Instant-context observer was active captured the incorrect tuple directly. For the same fleet, the projected
Instant amount changed `75 → 74 → 0` while the state remained `Repairing` and previous state remained `Docked`; the
zero transition occurred at `04:56:29.705`, while `REPAIR_COMPLETE` followed at `04:56:31.050`. The zero-cost context
was still interactable and preceded completion by approximately 1.345 seconds. No unintended request or spend was
observed.

### Active next canary

The passive `repair_instant_context` mode answered the projection question and remains documented in
[Repair Instant-Button Context Probe](probes/20260717-repair-instant-button-context.md). A blanket
`amount == 0 && Repairing` interlock would also suppress the legitimate `Finish Ship Repair — FREE` state, so the
captured tuple does not support changing the instant context or click boundary directly.

The narrower behavior canary is `repair_action_status_hold`. At the already runtime-proven
`FleetPlayerData.GetActionStatus(Repair)` seam, it remembers only coherent in-progress statuses (`InProgress`,
`InProgress_Free`, `InProgress_AskForHelp`, or `Complete`) and returns that last status when the original briefly
regresses to `Ready` while current fleet state is still `Repairing`. A settling non-Ready status clears the
held lifecycle. The mode is Repair-only, mutually exclusive, default-off, and leaves the Instant context and request
path unchanged. Pure policy tests cover both Ask-for-Help and zero-cost completion sequences. The 2026-07-17
released-debug deployment installed exactly the `FleetPlayerData.GetActionStatus(ActionType)` hook with zero failures
or skips. Two repair flows then passed the primary Ask-for-Help → Speed-Up transition as `202 → 200`, without an
intervening `Ready`. At each completion, the hold projected `Ready` back to `InProgress_Free`, matching the observed
`FREE` finish presentation.

Treat those as two primary-transition passes, not proof that the intermittent bug is fixed. For the current canary,
the acceptance focus is only the unsafe Ask-for-Help → Speed-Up replacement; the coherent `FREE` completion state is
not a failure and needs no further work in this pass.

Two 2026-07-18 Quv'Sompek reproductions exposed a different repeated prefix: native status
`InProgress_AskForHelp (202) → Ready (100) → InProgress_AskForHelp (202)` while the fleet still reported
`Repairing`. The hold returned `202` for the transient native `100`, but the visible action still re-entered/rebound
and Ask for Help appeared again. One flow continued through `202 → 200 → 201 → 0` and emitted a second independent
`REPAIR_COMPLETE`. This proves the status projection did not create the native regression and also proves status
return coherence alone does not guarantee a stable widget/request sequence.

The active 2026-07-18 trace therefore keeps the same status hold but adds passive evidence at three downstream
boundaries: final `GetInstantButtonContext`, both human button callbacks, and
`JobService.RequestHelpJob(IJob, CallbackContainer<string>)`. Human-click events record the physical control,
raw button behavior bits, last native/projected status, fleet state, and last final
Instant context before calling the original exactly once. The help-request hook records only pre-dispatch job
identity/repair/help flags and then calls the original exactly once. The click seam is observational except for the
exact stale post-completion interlock documented below; no click or request is synthesized or duplicated.

Runtime showed `_instantBehaviours == 0` for both Ask-for-Help and Speed-Up clicks, so that field is not treated as a
semantic label. The physical click seam plus returned status and the downstream request seam are authoritative.

The Junker reproduction at `01:50:41` established a safety boundary for that predicate. The fleet emitted
`REPAIR_COMPLETE` and became `Docked/previous Repairing`; native Repair status and final Instant context had already
settled to `Ready` plus paid amount `101558`. The original hold still returned `202` because it accepted previous
Repairing, so the human's next Instant-control click saw Ask for Help but invoked the paid context and opened a
`101.55K` Lat confirmation. No `RequestHelpJob` followed the click. At `01:52:36`, the same fleet then completed a
normal `202 + help request → 200 → 201 → 0` sequence. The hold is therefore narrowed to current `Repairing` only;
previous state remains evidence but cannot authorize presentation projection.

The same tuple now defines an exact Instant-click interlock in hold mode: suppress only when current state is
`Docked`, previous state is `Repairing`, native status is `Ready`, and the last final Instant context is present,
interactable, and has an amount field. All ordinary Repair, Ask-for-Help, Speed-Up, and free-Finish tuples fall
outside that predicate. A suppressed click is logged explicitly and does not call the game's instant callback.

The Monaveen reproduction at `02:24:18` proved that the amount-zero variant is also stale and unsafe. Repair had
completed at `02:24:17.985`; the fleet was `Docked/previous Repairing`, native status was `Ready`, and the final
Instant amount was zero. The human click was not suppressed by the original paid-only predicate, the native callback
ran, and the game displayed `SHIP ERROR`; the fleet re-entered `Repairing` at `02:24:19.967`. The predicate therefore
does not require a positive amount: positive and zero are two outcomes of the same exact post-completion race.

Runtime acceptance followed on USS Reliant at `03:11:45.073`: the same zero-amount post-completion click logged
`suppressed=true` and produced neither a help dispatch nor a native error/Latinum action. After re-entry to
`Repairing`, the next click at `03:11:46.796` remained unsuppressed, emitted exactly one repair-job help request, and
advanced to status `200`. The interlock therefore blocks the stale action without blocking the next genuine
Ask-for-Help action.

### Validated checkpoint and next experiment

The runtime-accepted implementation checkpoint is commit
`085bfb6e1652b03e8a7a397bb899e7a48ad86a8c` (`fix: interlock stale repair actions`). Use that commit as the known-good
rollback boundary if a later presentation experiment regresses Repair behavior; prefer reverting the later
experiment first, or revert this checkpoint explicitly when the intent is to remove the complete probe/interlock.

The next canary combines presentation coherence with the accepted safety floor:

- Retain the exact stale-click interlock unchanged so a mismatched widget can never invoke the native paid/error path.
- At the final Repair button-context/presentation boundary, reject or defer only a proven incoherent button change
  until current fleet state and native Repair status agree on the action.
- Never allow previous `Repairing` state alone to authorize a projected label or action.
- Never synthesize, replay, delay, or duplicate a click or `RequestHelpJob`; a human click is either accepted against
  a coherent current action or explicitly suppressed.
- Require runtime acceptance to show no Latinum confirmation or Ship Error in the stale window and exactly one help
  request when the subsequent genuine Ask-for-Help action is clicked.

Static disassembly resolves the commit mechanism without another detour. `HandleReactiveInt` loads the live
`_instantButtonContext`, calls `GetInstantButtonContext`, and value-copies the returned context into that live object.
In hold mode, the getter now recognizes only `Docked + previous Repairing + native Ready` and, for a maximum of 2.5
seconds, returns the widget's already-rooted live context instead of the stale proposal. The resulting self-copy is a
no-op, so the proposed button change is rejected without retaining or fabricating a managed object. Re-entry to any
coherent current state releases the hold immediately; an unchanged stale tuple is released at the bound so the UI
cannot freeze indefinitely. The independent click interlock remains active throughout and no input is replayed.

The layered canary passes 298 policy/integration test cases and 3040 assertions plus the full released-debug build;
runtime smoke passed on Quv'Sompek and USS Crozier. Quv'Sompek's first recurrence held an amount-zero stale proposal
at `17:14:19.639`, suppressed the human click at `17:14:19.676`, released on Repairing re-entry at `17:14:20.712`,
and allowed exactly one genuine help request at `17:14:21.389`. Its second recurrence at `17:14:29.644` proposed
paid amount `251434`, but returned the existing amount-zero live context with `presentationHeld=true`; no bad action
followed. USS Crozier independently held an amount-zero post-completion proposal at `17:14:16.572`. This satisfies
the layered presentation and click-safety acceptance contract.

At `20:53`, Quv'Sompek supplied a no-stale-click negative control across three further completion/re-entry cycles.
The presentation layer held stale proposals at `20:53:27.126` (amount zero), `20:53:28.883` (native `59914`, returned
zero), and `20:53:30.656` (native `59914`, returned zero). No human click occurred in any stale Docked window and no
bad native action followed. The only nearby Instant click, at `20:53:30.035`, occurred while currently `Repairing`
with status `201` and was correctly unsuppressed as the valid Finish control.

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
- The default-off passive repair probe installed as the sole owner of `GetActionStatus`, returned every original value unchanged, and captured multiple repair variants without a crash or hang.
- The confirmed mismatch is `ActionStatus::Ready` while the fleet remains `Repairing`; the client converged to `Docked` about 1.55 seconds later.
- The one-event caller airlock resolved the real UI refresh chain as `JobService.UpdateJobList → ActionElementWidget.HandleReactiveInt → ActionElementWidget.GetInstantButtonContext → IActionData.GetActionStatus/GetInstantCost`.
- A later post-completion Repair reappearance displayed cost zero and shared the same `InProgress_Free → Ready` while `Repairing` invariant and repair-complete boundary.
- The first `Ready + Repairing → Disabled` guard canary fired twice in a later smoke, but pay-to-repair still appeared before Ask for Help; a separate invalid `Ready` also occurred during a transient `Docked/previous Repairing` state.
- The same status-only behavior canary is restored unchanged to re-test only the original pre-Ask-for-Help symptom; the post-completion `Instant 0` symptom is excluded from its acceptance criteria.
- The mutually exclusive `repair_instant_context` passive mode captured an interactable `74 → 0` projection while
  the fleet remained `Repairing`; the zero transition preceded `REPAIR_COMPLETE` by approximately 1.345 seconds.
- A default-off `repair_action_status_hold` behavior canary now preserves the last coherent Repair status across only
  the proven transient `Ready` regression. It is built, unit-tested, deployed, and install-proven; behavior smoke is
  active. Two Ask-for-Help → Speed-Up flows passed without a `Ready` regression; completion projections produced the
  observed coherent `FREE` state.
- Two Quv'Sompek bad paths regressed natively `202 → 100 → 202` despite the hold returning `202` continuously; one
  continued to a second repair-complete boundary. The currently deployed trace adds passive final-context,
  human-click, and actual help-request correlation so the next rare repro distinguishes widget rebinding from a
  repeated player request.
- The persistent stack budget has been restored to zero after the successful sample.
- Defaults remain `ship_state_probe = "off"`; the local investigation runtime explicitly enables
  `repair_action_status_hold` with stack budget zero for the bounded smoke.
