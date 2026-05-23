# Fleet Notification Discovery Spike

Purpose: understand why fleet arrival/change notifications can be missed when other menus are open, and choose a narrow proof before building a fuller notification model.

This is intentionally an investigation note, not an implementation plan for a full notification system.

## Working Hypothesis

The current fleet arrival path is not purely tied to `fleet_actions`, but it is still heavily UI-triggered.

The strongest current signal is `FleetStateWidget.SetWidgetData`. That hook receives a `FleetPlayerData*`, updates the fleet notification state machine, and may trigger fleet runtime sync. When the fleet bar/widget surface does not refresh because another menu is active, the notification state machine may never observe the state transition.

The runtime sync layer looks more promising because it can read fleet slots from `FleetsManager`, but today its snapshot helper still gates slot observation on a tracked `FleetBarViewController`. That may turn a runtime-accessible signal into a UI-dependent one.

## Guardrails From Hotkey Triage

The recent `Shift+Space` crash is a concrete warning that some native action/callback seams are toxic even when the surrounding hotkey logic is otherwise correct.

- `ShortcutsManager.OnShipLocateAction` / callback-guard style hooks should be treated as suspect by default.
- This spike should prefer read-only state observation and reconciliation over action/callback interception.
- If a future branch needs to revisit native callback hooks, it should do so as a separate, isolated experiment with explicit crash validation.

## Current Observation Paths

### Fleet arrival notifications

Files:

- `mods/src/patches/parts/fleet_arrival.cc`
- `mods/src/patches/fleet_notifications.cc`
- `mods/src/testable_functions.cc`

Current flow:

1. `FleetStateWidget_SetWidgetData_Hook` resolves the widget context as `FleetPlayerData*`.
2. It calls `fleet_notifications_observe_fleet_bar(fleet)` before the original method.
3. The notification layer caches state by fleet id.
4. If the cached state changes, `fleet_bar_transition_notification_decision(...)` derives a notification.
5. `Warping -> Impulsing` means `ARRIVED_IN_SYSTEM`.
6. `Impulsing -> IdleInSpace` or similar deployed states means `ARRIVED_AT_DESTINATION`.
7. The same transition classifier returns a `fleet_runtime_sync_trigger(...)` source for runtime sync.

Implication: the state machine is transition-based and can be accurate when it sees consecutive states, but missed observations mean missed transitions.

### Fleet runtime sync

Files:

- `mods/src/patches/fleet_runtime_sync.cc`
- `mods/src/patches/live_debug_fleet_runtime_observers.cc`
- `mods/src/patches/live_debug_fleet_change_events.cc`
- `mods/src/patches/parts/live_debug.cc`

Current flow:

1. `fleet_runtime_sync_trigger(source)` is enabled when sync patches and `sync.fleet_runtime` are enabled.
2. It captures `observe_fleet_runtime_snapshot()`.
3. `FleetStateKey` compares selected index, current fleet, all slots, cargo buckets, and hull names.
4. Unchanged snapshots and initial non-meaningful snapshots are suppressed.
5. Changed snapshots are queued as `fleet.runtime` payloads.

Additional hooks already trigger runtime sync from non-fleet-bar events:

- `DeploymentEvents.TriggerFleetStateChangeEvent`
- `TriggerPlayerFleetsUpdatedEvent`
- `TriggerCoursePlannedEvent`
- `TriggerCourseStartEvent`
- `TriggerCourseChangeEvent`
- `TriggerCourseEndEvent`
- `TriggerSetCourseResponseEvent`
- `TriggerBattleStartEvent`
- `TriggerBattleEndEvent`
- `TriggerStaleFleetDataDetected`

Important weakness: `observe_fleet_slots(FleetBarViewController*)` currently returns empty slots immediately when `fleet_bar` is null, even though per-slot observation can read `FleetsManager::Instance()->GetFleetPlayerData(slot_index)`. This is the clearest candidate for a UI dependency that may not be necessary.

### Incoming attack, mining, and node depleted signals

Files:

- `mods/src/patches/parts/fleet_arrival.cc`
- `mods/src/patches/fleet_notifications.cc`

Signals:

- `ToastFleetObserver.QueueNotifications` is targeted for incoming fleet/attack materialization.
- `ToastFleetObserver.HandleMiningDepleted` drives node depleted notifications.
- `MiningObjectViewerWidget.UpdateTimerWidget` captures mining ETA for notification body formatting.

These are not fleet movement/arrival truth sources. The toast hooks are event-like and useful for their narrow domains. The mining viewer timer is explicitly UI-dependent.

### Fleet action surface

Files:

- `mods/src/patches/fleet_actions.cc`
- `mods/src/prime/FleetBarViewController.h`
- `mods/src/prime/FleetsManager.h`

`fleet_actions` depends on `FleetBarViewController`, visible object viewers, and input state to execute user actions. It is relevant because it shares the fleet bar and object tracker surfaces, but it should not be treated as the long-term observation source for movement changes.

`HandleShipSelection` / locate is also in this bucket. The double-tap locate path resolves the selected fleet from the fleet bar and calls `RequestViewFleet(...)`; that is useful for user-driven recenter behavior, but it is not a passive truth source for movement or arrival state.

## UI-Dependent Versus Runtime-Dependent Signals

| Signal | Current dependency | Notes |
| --- | --- | --- |
| `FleetStateWidget.SetWidgetData` | UI-dependent | Primary arrival notification path. Starves when widget does not refresh. |
| `FleetBarViewController` selected/current fleet | UI-dependent | Useful for selected index and active fleet panel context, not a complete source of truth. |
| `HandleShipSelection` / `RequestViewFleet` locate | UI/action-dependent | Useful for user-initiated recentering, not for passive movement truth. |
| `MiningObjectViewerWidget.UpdateTimerWidget` | UI-dependent | Good for displayed ETA hints only. |
| Object viewer visibility in `fleet_actions` | UI-dependent | Action context, not movement state. |
| `DeploymentEvents.*` hooks | likely runtime/game-event dependent | Already fire runtime sync captures for course and fleet state changes. Need live validation across menu states. |
| `FleetsManager::GetFleetPlayerData(slot)` | likely runtime/game-state dependent | Promising slot source. Current bulk observer unnecessarily requires a fleet bar pointer. |
| Sidecar `fleet.snapshot` projection | producer-dependent | Durable state projector, not a source of truth. It can only project what the mod emits. |
| Sidecar timer/projection logic | not currently implemented for arrivals | Candidate fallback only if mod emits enough course timing and invalidation signals. |

## What The Current Code Actually Gives Us

### 1. Where `FleetBarViewController` gets its fleet facts

From the current wrappers, `FleetBarViewController` exposes selection and currently bound fleet context, not an authoritative slot-state store:

- `FleetBarViewController::IsIndexSelected(index)` gives UI selection state.
- `FleetBarViewController::_fleetPanelController->fleet` gives the currently bound `FleetPlayerData*`.
- `FleetBarContext.CurrentFleet` is another UI-bound current-fleet surface.
- `FleetStateWidget_SetWidgetData_Hook` also receives `FleetPlayerData*` as widget context.

In other words, the fleet bar appears to consume `FleetPlayerData`; it is not the only place fleet facts exist.

The strongest code-level clue is the runtime observer itself: `observe_fleet_slot(slot_index, fleet_bar)` already reads slot facts from `FleetsManager::GetFleetPlayerData(slot_index)` and uses the fleet bar only for `selected` state. The current bulk helper becomes UI-dependent only because `observe_fleet_slots(FleetBarViewController*)` returns early when `fleet_bar` is null.

### 2. What we can read directly without the fleet bar

Passive runtime reads already available through current wrappers:

- `FleetsManager::GetFleetPlayerData(slot)`
- `FleetPlayerData::Id`
- `FleetPlayerData::CurrentState`
- `FleetPlayerData::PreviousState`
- `FleetPlayerData::Hull`
- `FleetPlayerData::MiningData`
- `FleetPlayerData::CargoResourceFillLevel`
- `FleetPlayerData::Address`
- `DeploymentEvents.Trigger*` lifecycle hooks already wired in `parts/live_debug.cc`
- `DeploymentEvents.TriggerSetCourseResponseEvent(fleet_id, success, is_recall_course, planned_course_data)`
- `DeploymentEvents.TriggerStaleFleetDataDetected`

Nearby runtime objects that look promising but are not currently exposed by a safe wrapper in this codebase:

- deployed/course event objects passed through `TriggerCoursePlanned/Start/Change/End`
- `planned_course_data` from `TriggerSetCourseResponseEvent`
- protobuf model evidence for `DeployedFleet` fields such as `courseId`, `nodeId`, `nodeAddress`, `warpData.destinationNodeId`, and `latestCourseVectorX/Y`

That split matters:

- slot presence and movement state are already reachable passively through `FleetsManager`
- destination-change identity is not yet proven from the current `FleetPlayerData` wrapper alone

### 3. Field-level classification map

| Question | Enough with current passive reads? | Candidate fields / evidence | Notes |
| --- | --- | --- | --- |
| arrived in system | yes, if snapshots capture both sides | `CurrentState`, `PreviousState`, previous snapshot state | Current classifier already uses `Warping -> Impulsing`. |
| docked | yes | `CurrentState`, previous snapshot state | Current classifier already uses `newState == Docked` with space-state guard. |
| recalled | partial | `CurrentState`, `TriggerSetCourseResponseEvent.is_recall_course`, course-end timing | State alone can show docked/end state, but not reliably distinguish recall cause from other dock outcomes. |
| started impulse | yes | previous snapshot state, `CurrentState`, `PreviousState` | Existing helper uses `newState == Impulsing` and excludes warp-arrival. |
| started warp | yes | previous snapshot state, `CurrentState`, `PreviousState` | Existing helper distinguishes `WarpCharging` and `Warping`. |
| changed destination | not with confidence yet | likely course/deployed-fleet fields such as `courseId`, `nodeAddress`, `destinationNodeId`; maybe `Address` | `FleetPlayerData::Address` is exposed, but its destination semantics are not yet proven in this branch. |
| ship disappeared | yes | slot `present` false, slot `fleetId` changed | Existing fleet-change events already emit `fleet-slot-cleared` and `fleet-slot-fleet-changed`. |
| stale state | yes | `TriggerStaleFleetDataDetected`, unchanged snapshot suppression, all-slots-empty snapshots | We can observe stale-data events now. |
| session reset | partial | stale-data event, all-slots-cleared transition, fleet id turnover | A real session id is still missing from the current runtime snapshot model. |

### 4. Passive and low-risk reads

Safest current reads:

- `FleetsManager::Instance()` existence
- `GetFleetPlayerData(slot)` per slot
- `FleetPlayerData` scalar/model properties already wrapped (`Id`, states, hull, mining, cargo, address)
- deployment lifecycle hooks that are already installed for live debug / runtime sync
- existing change-driven snapshot machinery in `fleet_runtime_sync`

These are passive because they do not issue game actions, do not depend on widget refresh, and do not require callback-style input interception.

### 5. Reads that still depend on UI or menu state

UI-dependent or action-surface reads:

- `FleetStateWidget.SetWidgetData`
- tracked `FleetBarViewController`
- `FleetBarViewController::IsIndexSelected`
- `_fleetPanelController->fleet`
- `MiningObjectViewerWidget.UpdateTimerWidget`
- object viewer visibility used by `fleet_actions`
- `HandleShipSelection` / `RequestViewFleet` locate path

These are useful for selection context, visible-viewer context, or user actions, but they should not be treated as the root observation model for reliable fleet notifications.

## Recommended Proof Plan

### Smallest read-only proof surface

The smallest credible proof is still the runtime slot observer, but now the implementation surface can be stated more concretely:

1. Keep the existing deployment-event hooks as the trigger surface.
2. Change only `observe_fleet_slots(FleetBarViewController*)` so it does not early-return when `fleet_bar` is null.
3. Continue reading slot facts from `FleetsManager::GetFleetPlayerData(slot_index)` for every slot.
4. When `fleet_bar` is null, leave `selected=false` for slots and keep selected-index metadata at `-1`.
5. Keep `observe_fleetbar(fleet_bar)` and `fleetBarTracked` metadata unchanged so the payload still tells us whether UI context was present.
6. Reuse existing `fleet_runtime_sync_trigger(...)` calls from deployment events.
7. Validate by comparing runtime snapshots and recent fleet-change events while the shop/faction screens are open.

### Tiny instrumentation patch this proof would require

This proof is not purely observational in the current codebase because one small read-only patch is still needed:

- file: `mods/src/patches/live_debug_fleet_runtime_observers.cc`
- change: remove the `if (!fleet_bar) return empty slots;` early return from `observe_fleet_slots(FleetBarViewController*)`
- behavior: still populate slot observations from `FleetsManager`; only selection metadata remains UI-dependent

That patch does not add a new hook family, does not issue any actions, and does not touch hotkey/callback seams.

### What this proof can and cannot prove

This proof can prove:

- whether `FleetsManager` remains readable while menus are open
- whether slot presence and movement-state transitions continue to advance without the fleet bar
- whether arrival/docked/warp/impulse classification can be driven from runtime slot state instead of widget refresh
- whether stale-fleet-data events line up with snapshot changes

This proof cannot yet prove:

- destination change identity with confidence
- recall-vs-other-dock cause without using course-response data
- session identity / reconnect boundaries beyond stale-data and all-slots-cleared heuristics

### If the proof succeeds

If this proof works, the next narrow step should be:

- keep arrival/docked/warp/impulse classification mod-side from runtime slot observations
- add one separate destination/course discovery pass around deployment course objects or `planned_course_data`
- defer any sidecar timer fallback until after runtime truth is proven

### If the proof fails

If this proof fails, the next investigation target should be the course/deployed-fleet objects carried by deployment events, not more widget hooks and not callback-style action guards.

## Architecture Candidates

### Candidate A: Mod-side runtime transition detection

Use runtime/game-event triggers plus `FleetsManager` slot snapshots to detect transitions in the mod. Fleet-bar observations become one trigger among several, not the owner of truth.

Pros:

- Closest to the authoritative runtime state the mod can see.
- Can confirm real `Warping -> Impulsing` and `Impulsing -> IdleInSpace` transitions instead of predicting them.
- Keeps notification decisions near the current transition classifier.
- Existing deployment hooks and runtime snapshot machinery already provide much of the shape.

Cons:

- More hooks increase maintenance and crash risk if broadened carelessly.
- Still needs proof that `FleetsManager` is valid during shop/faction/menu states and reconnects.
- If snapshots are only event-triggered, missed runtime events can still cause missed transitions.
- If snapshots are polled too often, it becomes over-instrumentation.

Best fit:

- Confirmed arrival/change notifications.
- Low-latency runtime truth.
- A narrow first proof that removes accidental UI gating from slot snapshots.

### Candidate B: Sidecar-side timing/projection

Let the sidecar maintain expected fleet arrivals from course start, duration, ETA, or arrival time data emitted by the mod. The sidecar would schedule or project an expected arrival even if the game UI never surfaces a transition.

Pros:

- Durable across UI route changes inside the sidecar.
- Natural place for timers, expiry, coalescing, and user-facing notification preferences.
- Can still show useful expected-arrival state when the mod is quiet.
- Avoids adding heavy timer ownership to game hooks.

Cons:

- Predictions are not confirmations.
- Course changes, recalls, speedups, hostile interruptions, failed course starts, reconnects, and server corrections can make timers stale.
- Sidecar cannot know if the game session reset unless the mod sends session and invalidation events.
- A timer-only notification can fire after logout, reconnect, ship death, recall, or target change unless aggressively invalidated.

Best fit:

- Expected arrival UI.
- Fallback reminders with explicit `predicted` confidence.
- Durable projection after the mod provides course timing and invalidation evidence.

### Candidate C: Hybrid confirmed-state plus predicted fallback

The mod emits compact runtime snapshots and course/timing observations. The sidecar stores projection state, derives expected arrivals, and marks predictions as pending until a later mod snapshot confirms or invalidates them.

Pros:

- Separates truth from projection.
- Lets the sidecar handle durable timing and stale-session cleanup.
- Lets the mod stay focused on cheap observation.
- Can support richer UX later: expected, overdue, confirmed, invalidated.

Cons:

- Requires a stricter event model and confidence vocabulary.
- More moving pieces than the current notification path.
- Needs clear dedupe rules so a predicted fallback and later confirmed transition do not double-notify.

Best fit:

- Long-term reliable notifications and sidecar UI.
- Later phase after the basic runtime signal is proven.

### Candidate D: Fleet-bar plus fallback timer

Keep the existing fleet-bar transition path and add an arrival-time fallback that fires if no transition is seen.

Pros:

- Smallest conceptual change.
- Could improve obvious misses quickly if arrival times are easy to capture.

Cons:

- Easy to create false positives.
- Does not solve stale state by itself.
- Leaves the root observation model UI-dependent.
- Needs the same invalidation rules as sidecar timing, but with less durable context.

Best fit:

- Temporary experiment only, behind explicit diagnostics or default-off behavior.

## Timing Module Candidate

A small dispatchable timing module is worth considering, but only after the source model is clear.

Minimum inputs needed:

- fleet key and slot index
- course or movement id if available
- observed state at scheduling time
- expected arrival time or duration
- source event and session id
- invalidation events: course changed, course ended, recall, docked, battle, stale data, session ended, reconnect

Minimum outputs:

- `expected_arrival_scheduled`
- `expected_arrival_due`
- `expected_arrival_confirmed`
- `expected_arrival_invalidated`
- confidence: `confirmed`, `predicted`, `stale`, `invalidated`

This module belongs more naturally in the sidecar or shared sidecar core than directly in hot mod hooks. The mod can emit observations; the sidecar can own durable clocks.

## Key Risks

### False positives

Timer-based detection can fire when a fleet was recalled, interrupted, destroyed, moved again, or corrected by server state. Any predicted notification needs explicit confidence and invalidation.

### Missed transitions

Transition detection requires seeing both old and new state. If captures are sparse, the system may see `Warping -> IdleInSpace` and miss the intermediate `Impulsing` arrival-in-system transition.

### Stale timers

Timers must expire or invalidate on course changes, recall, docking, battle state, session end, reconnect, and stale-fleet-data events. A timer without invalidation is worse than no timer.

### Session changes and reconnects

The sidecar already handles same-session stale projection differently from changed-session snapshots. Arrival timing needs the same discipline. A new session should not inherit live timers from an old game connection unless a fresh mod snapshot confirms them.

### Menu state

The specific bug is menu-state starvation. The proof must test shop/faction/other full-screen menus while fleet state changes. A fix that only works when the nav HUD is visible is not enough.

### Over-instrumentation

Hooking every movement or steering method is tempting but risky. Existing `DeploymentEvents` hooks are a better first surface because they already represent fleet/course lifecycle changes and are already installed for live debug or fleet runtime sync.

### Toxic callback seams

The recent `ShipLocate` callback-guard failure is a concrete example of why action/callback interception should not be the first proof surface here. A discovery branch for reliable notifications should avoid introducing new pointer-style native callback guards unless that branch is explicitly about validating the hook seam itself.

### Duplicate notifications

A long-term hybrid model must dedupe across fleet-bar confirmation, deployment-event confirmation, sidecar predicted due events, reconnect snapshots, and repeated unchanged snapshots.

## Recommended Smallest Safe Proof

Do not build a notification system yet.

Build a trace-only proof that answers one question:

Can `FleetsManager` slot snapshots observe fleet state changes while the fleet bar or navigation HUD is not active?

Suggested proof shape:

1. Keep current deployment-event hooks as the trigger surface.
2. Change only the runtime snapshot observer so slot observation uses `FleetsManager` even when `FleetBarViewController` is unavailable.
3. Keep `FleetBarViewController` only for selected-index metadata.
4. Emit no new user notification.
5. Use existing `fleet.runtime`, live-debug recent events, or `fleet-slots-state` diagnostics to compare captures with shop/faction menus open.
6. Treat success as seeing non-empty slots and state transitions from deployment-event-triggered captures while `fleetBarTracked` is false or while the bar is not actively refreshing.

Why this proof first:

- It tests the likely root UI gate directly.
- It reuses existing hooks instead of adding a new hook family.
- It avoids the `ShipLocate`-style callback seam that just proved unstable.
- It is read-only observation.
- It improves the quality of current runtime sync without deciding the final notification architecture.
- It gives sidecar projection better evidence without asking the sidecar to guess.

Success criteria:

- With another menu open, a fleet movement or arrival causes a deployment-event-triggered capture.
- The capture includes present fleet slots from `FleetsManager`.
- The observed slot state advances through useful movement states, ideally `Warping`, `Impulsing`, and destination state.
- Sidecar projection advances from the runtime snapshot when fleet runtime sync is enabled.
- No user-facing notification behavior changes during the proof.

Failure criteria:

- `FleetsManager` is null or stale while menus are open.
- Deployment events do not fire for the missed arrival cases.
- The slot states skip too much detail to distinguish in-system arrival from final destination.

If the proof fails, the next investigation target should be course/timing data on the deployment/course objects rather than adding more UI hooks.

## Tentative Long-Term Direction

The most promising model is hybrid:

- Mod-side confirmed observation from runtime/game-event surfaces.
- Sidecar-side durable projection and optional timing for expected arrivals.
- Fleet-bar UI observations as opportunistic extra evidence, not the source of truth.

The proof above should come first because it tells us whether we can get confirmed runtime state while menus are open. If yes, timing becomes a fallback and UX feature. If no, timing becomes a larger part of the detection model and needs stricter invalidation from day one.