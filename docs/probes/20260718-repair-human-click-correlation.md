# Repair Human-Click and Help-Request Correlation

Date: 2026-07-18
Status: archived evidence; accepted behavior promoted through PR #248
Support tier: historical science probe; production successor is `RepairActionInterlockHooks`

> Final disposition (2026-08-11): the investigation hooks and `ship_state_probe` config were removed. The accepted
> stale-click predicate now ships through the production interlock; the procedure below remains only as evidence.

## Question

When Repair visibly re-enters Ask for Help, what did the human actually click, and did that click dispatch another
`RequestHelpJob`? Fleet state and action status alone cannot answer either question.

## Triggering Evidence

Quv'Sompek reproduced native `202 → 100 → 202` while still `Repairing`. The coherent-status hold projected the
transient `100` back to `202`, yet Ask for Help visibly appeared again. One sequence later emitted a second
`REPAIR_COMPLETE`. The missing distinction is widget rebinding versus a repeated human Ask-for-Help click/request.

## Probe Contract

With `live_query = true` and `ship_state_probe = "repair_action_status_hold"`, install these passive correlation
seams alongside the existing status hold:

- `ActionElementWidget.GetInstantButtonContext()` after its original return.
- `ActionElementWidget.OnActionButtonClickCallback()` before its original call.
- `ActionElementWidget.OnInstantButtonClickCallback()` before its original call.
- `JobService.RequestHelpJob(IJob, CallbackContainer<string>)` before its original call.

The click trace is Repair-only and records the physical control, raw `_actionBehaviours` or `_instantBehaviours`,
fleet state, last native/projected Repair status, and last final Instant context.
The request trace records job ID, target ID, repair-job flag, and whether help was already requested. It records no
label text, request body, auth/session data, retained managed pointer, coordinate, or inventory.

Event kinds:

- `ship-state-probe.repair-human-click`
- `ship-state-probe.job-help-request`
- `ship-state-probe.repair-instant-button-context`

Every observational detour calls its original exactly once. In hold mode, the Instant-click detour suppresses only
the proven tuple `Docked + previous Repairing + native Ready + present/interactable amount context`; every other
click calls the original exactly once. The probe does not synthesize, delay, or duplicate input or requests.

## Interpretation

- Reappearing Ask for Help with no new human-click event: widget presentation/rebinding only.
- A click with `askHelpBehaviour=true` and one matching request: the player clicked the reappeared Ask for Help.
- A click with no matching request: the UI handler rejected or rerouted it before `JobService`.
- One click followed by multiple request events: downstream duplication.
- A request without a preceding Repair click: another caller initiated help and requires a bounded caller sample.

## Installation Result

The 2026-07-18 released-debug boot reported all five `ClientShipStateProbe` detours installed with zero failures:
status hold, final Instant context, action click, instant click, and the exact `IJob` help-request overload. The full
unit suite passed 294 test cases and 3009 assertions.

## Junker Runtime Result

The rare bad path was captured on Junker (`fleetId 2647877601347320583`). At `01:50:41.023`, the independent fleet
boundary emitted `REPAIR_COMPLETE`. At `01:50:41.024`, native Repair status was `Ready (100)`, fleet state was
`Docked` with previous `Repairing`, and the final Instant context was interactable with amount `101558`. The hold
returned stale `202` because its then-current predicate accepted previous `Repairing`. At `01:50:41.164`, the human
clicked the Instant control; the trace recorded native `100`, returned `202`, and amount `101558`. No help-request
event followed, and the game opened the photographed `101.55K` Lat confirmation.

At `01:52:36`, the same fleet recovered through a normal sequence: one Ask-for-Help click produced exactly one
repair-job help request, followed by `200`, a Speed-Up click, `201`, a Finish click, `0`, and one repair-complete
boundary.

This proves the previous-state extension can create a misleading label/action split. It is removed: coherent status
may now be held only while current fleet state is `Repairing`.

The exact same tuple gates a narrow click interlock: when the human hits the Instant control during that stale
post-completion race, the click is recorded with `suppressed=true` and the game's callback is not invoked. Normal
Repair, Ask-for-Help, Speed-Up, and free-Finish clicks remain untouched.

A second runtime sample on Monaveen (`fleetId 2647877601347320583`) established the zero-amount half of the same
boundary. At `02:24:17.985`, Repair completed; the fleet immediately became `Docked/previous Repairing` with native
`Ready`. At `02:24:18.313`, the human clicked the stale Instant control with a present, interactable amount context
of zero. The paid-only interlock did not fire, the native callback ran, and the game displayed `SHIP ERROR`. The
fleet re-entered `Repairing` at `02:24:19.967`. The interlock therefore covers both proven stale context outcomes:
positive amount would open Latinum confirmation, while zero amount produces Ship Error.

The final interlock released-debug build passed 296 test cases and 3017 assertions, deployed with matching build/game DLL
hashes, restarted successfully, and installed all five probe hooks with zero failures.

The broadened interlock then passed its runtime acceptance sample on USS Reliant (`fleetId 2647877601347320583`).
At `03:11:45.073`, an Instant click landed in the zero-amount `Docked/previous Repairing/native Ready` window and
was recorded with `suppressed=true`; no help request or native error/Latinum action followed. The fleet returned to
`Repairing`, and at `03:11:46.796` the next Instant click was not suppressed, emitted exactly one repair-job
`RequestHelpJob`, and advanced to native status `200`. This demonstrates both halves of the boundary in one flow:
the stale action is blocked and the subsequent genuine Ask-for-Help action remains functional.

Runtime also established that `_instantBehaviours` remains zero on both known Ask-for-Help and Speed-Up clicks. It is
retained only as raw evidence; status plus actual `RequestHelpJob` dispatch determine the click meaning.

## Layered Presentation Canary

The accepted click interlock remains the safety floor. In `repair_action_status_hold` mode,
`GetInstantButtonContext` now also rejects the same stale presentation proposal for at most 2.5 seconds by returning
the widget's already-rooted live `_instantButtonContext`. `HandleReactiveInt` consequently value-copies that context
onto itself and leaves the visible Instant action unchanged. No managed pointer is retained, no context is fabricated,
and no click or request is queued or replayed. A coherent current fleet state releases the hold immediately; the
fixed bound releases a persistently stale tuple so normal Docked presentation cannot freeze indefinitely.

The implementation passes 298 test cases and 3040 assertions and the full released-debug build. Runtime acceptance
requires one ordinary complete Repair flow plus one recurrence showing `presentationHeld=true`, no bad native action,
and exactly one help request after coherent Repairing re-entry.

## Layered Canary Runtime Acceptance

The 2026-07-18 `17:14` Quv'Sompek flow passed the layered acceptance contract. At `17:14:19.638`, native Repair
regressed to `Ready` while current state was still `Repairing`; status hold returned `202`, immediately followed by
`REPAIR_COMPLETE`. At `17:14:19.639`, fleet state became `Docked/previous Repairing` and the zero-amount stale
proposal logged `presentationHeld=true`. A human Instant click at `17:14:19.676` logged `suppressed=true` and emitted
no help request or native error/Latinum action. The fleet re-entered `Repairing` at `17:14:20.712`; the presentation
hold released, and the next human click at `17:14:21.389` emitted exactly one repair-job `RequestHelpJob` before
status advanced to `200`.

The same fleet produced a stronger presentation-only sample at `17:14:29.644`: after another completion, native
proposed paid amount `251434` in the exact stale tuple, while the layered getter returned the existing amount-zero
live context with `presentationHeld=true`. No click or bad native action followed. USS Crozier independently produced
an amount-zero held completion sample at `17:14:16.572`. These traces runtime-prove that the presentation layer
rejects both zero and paid stale proposals while the click interlock remains an independent safety boundary.

A later no-click Quv'Sompek sample at `20:53` isolated presentation behavior from the click interlock. The fleet
cycled through three completion boundaries:

- `20:53:27.126`: native `Ready`, `Docked/previous Repairing`, amount-zero proposal held; Repairing re-entry followed
  at `20:53:28.430` without a stale-window click.
- `20:53:28.883`: native proposed paid amount `59914`, while the getter returned the existing amount-zero live
  context with `presentationHeld=true`; Repairing re-entry followed at `20:53:29.334`, again without a stale click.
- `20:53:30.656`: native again proposed paid amount `59914`, while the getter returned amount zero with
  `presentationHeld=true`; no stale click or bad native action followed in the captured window.

The sole nearby Instant click occurred at `20:53:30.035` while current state was `Repairing` and status was
`InProgress_Free (201)`. It was correctly unsuppressed and represents the valid Finish action, not a stale-window
interaction. This sample independently demonstrates that the presentation layer rejects repeated stale proposals
without relying on a human click, while the valid completion control remains usable.
