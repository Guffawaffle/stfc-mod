# Fleet Runtime Dispatch Plan

Purpose: turn the next unified-input runtime slice into small, testable steps after the table-driven dispatcher bridge is complete.

Scope for this plan:

- `fleet_primary`
- `fleet_secondary`
- `fleet_service`
- `fleet_view_info`
- `fleet_queue_clear`
- adjacent space-action behavior that currently lives next to them: reward dismissal, warp cancel suppression, viewer toggles, deferred retries, and queue add semantics

## Current Live Ownership

The remaining runtime gap is no longer the schema. It is the execution path.

Current path:

1. `mods/src/patches/hotkey_router.cc`
2. `hotkey_router_screen_update(...)`
3. legacy `MapKey::IsDown(...)` checks for `ActionPrimary`, `ActionSecondary`, `ActionRecall`, `ActionRepair`, `ActionQueue`, `ActionQueueClear`
4. `ExecuteSpaceAction(...)` in `mods/src/patches/fleet_actions.cc`
5. direct IL2CPP mutations based on pre-scan widgets, mining viewer, star node viewer, armada widget, navigation interaction, queue state, and fleet state

Adjacent but separate:

- `TryDismissRewardsScreen()` in `mods/src/patches/viewer_mgmt.cc`
- `HandleActionView()` / `TickInfoPending()` in `mods/src/patches/viewer_mgmt.cc`
- deferred retry globals and identity checks in `mods/src/patches/fleet_actions.cc`

## What Is Already Covered

Canonical input actions already exist for the core fleet slice:

- `FleetPrimary`
- `FleetSecondary`
- `FleetService`
- `FleetViewInfo`
- `FleetQueueClear`

Pure policy coverage already exists in `mods/src/patches/fleet_input_policy.cc` and `tests/src/test_pure_logic.cc` for:

- primary outcome priority
- queue add vs queue full vs defer
- mining / engage / armada / warp / set-course outcomes
- secondary scan/view outcomes
- service recall/repair outcomes

What is still bypassing the dispatcher/runtime binding model:

- `ExecuteSpaceAction(...)` reads `MapKey` directly
- `HandleActionView()` is still triggered from legacy `GameFunction::ActionView`
- queue clear still enters through the monolithic runtime path instead of a dispatcher-owned action

## Efficiency Guardrails

These are non-negotiable for the next slice:

1. Reuse the single frame dispatcher plan already built in `hotkey_router.cc`.
2. Do not add a second per-frame watched-key scan for fleet actions.
3. Gather pre-scan/viewer context once per attempted space action, not once per branch.
4. Keep deferred retry identity checks O(1) with fleet/widget/context identity, not rescans.
5. Avoid long-lived dual paths where both dispatcher winners and `MapKey` are queried every frame for the same steady-state action.

## Key Discovery Findings

### 1. `FleetViewInfo` and `FleetQueueClear` are the next safest live slices

They are canonical already, and their runtime behavior is simpler than primary/secondary/service.

- `FleetViewInfo` maps to `HandleActionView()` in `mods/src/patches/viewer_mgmt.cc`.
- `FleetQueueClear` maps to `ActionQueueManager::ClearQueue(fleet)` at the top of `ExecuteSpaceAction(...)`.

Neither requires the full pre-scan decision tree to become dispatcher-owned.

### 2. The monolithic risk is input sampling, not just action execution

`ExecuteSpaceAction(...)` currently owns both:

- input detection
- effect execution

That means moving fleet runtime bindings cleanly requires an explicit input seam such as `SpaceActionInputs`, otherwise the function still depends on legacy `MapKey` no matter how the router evolves.

### 3. The pure fleet policy is useful but not wired in

`DecideFleetPrimary(...)`, `DecideFleetSecondary(...)`, and `DecideFleetService(...)` already express most of the intended behavior, but runtime still uses an inline imperative tree in `ExecuteSpaceAction(...)`.

### 4. Reward dismissal and viewer toggles are adjacent but not the same layer

- reward dismissal is a first-priority primary-action outcome concern
- `ActionView` is an independent viewer-info toggle concern

They should stay separate in the runtime bridge even though they touch the same viewer surfaces.

## Recommended Slice Order

### Slice 2.1: Bridge `FleetViewInfo` and `FleetQueueClear`

Goal: move the two simplest remaining fleet-adjacent actions into the existing frame dispatcher plan.

Changes:

- extend the frame watched-action set with `FleetViewInfo` and `FleetQueueClear`
- dispatch `FleetViewInfo` directly to `HandleActionView()`
- dispatch `FleetQueueClear` directly to a small helper that clears the active fleet queue
- keep `ExecuteSpaceAction(...)` untouched for primary/secondary/service in this slice

Why first:

- smallest live bridge with immediate value
- no new pre-scan context model required
- proves the fleet layer can ride the current one-plan-per-frame runtime without reopening the large space-action tree

Required tests:

- dispatcher snapshot winner test for `FleetViewInfo`
- dispatcher snapshot winner test for `FleetQueueClear`
- runtime binding override test for both actions

### Slice 2.2: Introduce a Runtime Input Seam for Space Actions

Goal: separate input ownership from effect ownership.

Changes:

- add a small `SpaceActionInputs` struct carrying:
  - primary
  - secondary
  - service
  - queue
  - queue_clear
  - recall_cancel
  - deferred_primary_for_fleet
- change `ExecuteSpaceAction(...)` to accept explicit inputs instead of reading `MapKey` internally
- keep a thin adapter in the router while the migration is incomplete

Why second:

- without this seam, dispatcher migration remains partial no matter what the router does
- it creates a clean place to test alias and precedence behavior

Required tests:

- pure adapter tests for translating dispatcher winners into `SpaceActionInputs`
- regression tests around which combinations cause `ExecuteSpaceAction(...)` to run

### Slice 2.3: Extract Deferred Retry State

Goal: isolate the retry mechanism before deeper policy rewiring.

Changes:

- move `force_space_action_next_frame`
- move deferred fleet/widget/context identity state
- move clear/arm/match/generation helpers into a dedicated module

Why here:

- it is a small but high-risk state machine
- ship selection already depends on clearing it correctly
- it should be test-covered before primary/queue defer behavior changes

Required tests:

- arm/clear/generation bump
- same fleet same widget same target match
- mismatch by fleet
- mismatch by widget
- stale deferred state cleared on ship selection

### Slice 2.4: Bridge `FleetPrimary`, `FleetSecondary`, and `FleetService`

Goal: make the router read those actions from the runtime binding model without adding another frame scan.

Changes:

- extend the existing frame dispatcher plan with:
  - `FleetPrimary`
  - `FleetSecondary`
  - `FleetService`
- populate `SpaceActionInputs` from dispatcher winners plus deferred state
- call `ExecuteSpaceAction(...)` once when any fleet action or deferred retry is active

Why here:

- by this point the router owns input and the deferred state is isolated
- the remaining work is runtime effect routing, not key polling

Required tests:

- runtime binding override tests for all three actions
- regression tests confirming the old default binds still trigger the same runtime path

### Slice 2.5: Rewire `ExecuteSpaceAction(...)` to the Existing Pure Policy

Goal: replace inline branch ordering with the already-tested policy functions.

Changes:

- gather runtime context once
- build primitive policy inputs once
- call:
  - `DecideFleetPrimary(...)`
  - `DecideFleetSecondary(...)`
  - `DecideFleetService(...)`
- branch from outcomes to small effect handlers

Important separation:

- reward dismissal remains first-priority primary behavior
- warp cancel suppression for mouse primary stays explicit and tested
- queue defer behavior remains bounded and identity-checked
- `ActionView` does not get folded into primary/secondary/service

Required tests:

- outcome-to-handler tests for every primary outcome
- warp cancel suppression tests
- queue full / defer / deferred retry tests
- recall vs repair state matrix tests

### Slice 2.6: Remove Steady-State Legacy Input Reads

Goal: finish the migration instead of leaving a permanent dual-dispatch system.

Changes:

- remove direct `MapKey` reads for migrated fleet actions from `ExecuteSpaceAction(...)`
- keep compatibility at the config bridge and runtime binding layer, not at the per-frame polling layer

Why last:

- this should only happen after the new dispatcher-owned path is fully covered and live-stable

## Risks To Watch Closely

### Reward dismissal ordering

`TryDismissRewardsScreen()` runs in the router today before the space-action block. The migration must preserve the rule that rewards dismissal wins before normal primary action resolution.

### Warp cancel suppression

Mouse primary while warping is intentionally suppressed when visible target-consuming context exists. That exact behavior must remain explicit.

### Queue semantics and aliases

The config bridge currently preserves historical aliases:

- `action_primary`
- `action_queue`
- `action_recall_cancel`

Those all feed canonical fleet actions during migration. Runtime changes must not accidentally make queue semantics diverge from those legacy binds.

### Viewer toggle independence

`FleetViewInfo` should stay independent from primary/secondary/service. It shares viewer surfaces, but it is not the same dispatch layer.

### Preview conflict warnings

As more fleet-adjacent actions become canonical, runtime binding preview warnings can surface existing user-config collisions. Those warnings are config evidence, not automatically a code defect.

## Recommended Validation For Each Live Slice

Every live slice should finish with:

1. `xmake build -y stfc-mod-tests`
2. `xmake run -y stfc-mod-tests`
3. `xmake build -y mods`
4. `xmake build -y stfc-community-mod`
5. `ax cycle`

If a slice changes runtime dispatch precedence, add a focused dispatcher test before expanding scope.

## Immediate Next Step

Implement Slice 2.1 first:

- `FleetViewInfo`
- `FleetQueueClear`

That gives the next live dispatcher bridge with minimal risk, preserves efficiency by reusing the existing frame plan, and keeps the monolithic `ExecuteSpaceAction(...)` out of scope until the input seam and deferred state are ready.