# Repair Instant-Button Context Probe

Date: 2026-07-17
Status: archived evidence; accepted behavior promoted through PR #248
Support tier: historical science probe; production successor is `RepairActionInterlockHooks`

> Final disposition (2026-08-11): the `ClientShipStateProbe` runtime/config path was removed. This document preserves
> the evidence that informed the production interlock and is not an active science-build procedure.

## Question

What final `GenericButtonContext` does `ActionElementWidget` project for Repair while the job/fleet model is
transitioning, especially when the UI briefly re-exposes `Repair [time] / Instant [amount]` instead of the expected
active-repair action?

The existing `FleetPlayerData.GetActionStatus(ActionType)` canary cannot answer this. The widget reads action status
and instant cost independently, and a reproduced invalid `Ready` window occurred while the fleet briefly reported
`Docked` with previous state `Repairing`, outside the status guard predicate.

## Evidence for the Seam

- The one-shot caller sample resolved
  `JobService.UpdateJobList → ActionElementWidget.HandleReactiveInt → ActionElementWidget.GetInstantButtonContext`.
- Disassembly showed `GetInstantButtonContext` reading status and instant cost separately before
  `HandleReactiveInt` copies the result into the live widget context.
- The 2026-07-17 human smoke reproduced `Repair [time] / Instant 0` during the repair flow.
- The current dump resolves `Digit.Prime.Actions.ActionElementWidget.GetInstantButtonContext()` at RVA `0x11E6CD0`
  and returns `Digit.Client.UI.GenericButtonContext`.

## Probe Contract

- Config mode: `[advanced.diagnostics].ship_state_probe = "repair_instant_context"`.
- Requires `live_query = true`; default remains `off`.
- Mutually exclusive with `repair_action_status` and `repair_action_status_guard`.
- Sole target: `ActionElementWidget.GetInstantButtonContext()`.
- Call the original exactly once and return the exact original context pointer unchanged.
- Filter to Repair widgets whose bound `IActionData` is exactly `FleetPlayerData`.
- Record only the first snapshot and changes per fleet, with a fixed 16-fleet cache.
- Record numeric fleet state, previous state, context presence, interactability, amount presence, and amount.
- Retain no managed pointer, label text, resource inventory, coordinates, or request payload.
- Emit the deduplicated transition to both the bounded recent-event ring and the bounded native debug log so an
  uncommon reproduction survives event-ring eviction.

Event kind: `ship-state-probe.repair-instant-button-context`.

## Smoke

1. Enable `live_query` and `repair_instant_context`; keep the stack budget at zero.
2. Restart with the science build and confirm the `ClientShipStateProbe` hook installs only
   `ActionElementWidget.GetInstantButtonContext()`.
3. From the ship card, click `Repair [time]`, then follow the normal `Ask for Help` → `Speed Up` flow at normal player
   cadence. Smart Speed-Up may be used after opening the Speed Up panel.
4. If `Repair [time] / Instant [amount]` reappears, do not intentionally click Instant. Capture recent events and the
   native `[ShipStateProbe]` lines immediately after the flow.
5. Confirm that fleet state, previous state, context interactability, and projected amount change together or locate
   the exact incoherent tuple.

## Stop Conditions

Disable the mode immediately for a crash, hang, input loss, changed button behavior, duplicate hook ownership,
unbounded output, or any request that was not initiated by the player. This probe is observational; it is not a fix
and must not mutate the returned context.

## Decision Gate

Do not widen the current status guard. After one captured incorrect context, choose between:

- a projection-boundary canary that prevents committing only the proven incoherent tuple; or
- a click-boundary interlock if the UI cannot be made coherent without changing unrelated action presentation.

Any behavior canary must remain Repair-only, default-off, preserve Ask for Help and Speed Up, and prove that it does
not send or duplicate an Instant request.

## Installation Result

The 2026-07-17 released-debug cycle built and deployed successfully. Fresh boot evidence reported exactly one
`ClientShipStateProbe` hook installed, `ActionElementWidget.GetInstantButtonContext()`, with zero failures or skips.
The prior `FleetPlayerData.GetActionStatus(ActionType)` guard was not installed. This proves mode selection, method
resolution, and detour installation.

## Runtime Result

The user reproduced the incorrect base Repair/Instant layout during a normal repair flow while this observer was
active. The native log retained the following deduplicated context transitions for the same fleet:

- `04:56:18.659`: `Repairing`, previous `Docked`, interactable, amount `75`.
- `04:56:29.534`: `Repairing`, previous `Docked`, interactable, amount `74`.
- `04:56:29.705`: `Repairing`, previous `Docked`, interactable, amount `0`.
- `04:56:31.050`: the independent fleet-bar log reported `REPAIR_COMPLETE`.

The projected Instant context therefore changed from a nonzero amount to zero while the fleet was still
`Repairing`, approximately 1.345 seconds before the repair-complete boundary. The hook returned the original context
unchanged, and the run showed no crash, hang, duplicate hook, unintended Instant request, or spend.

This rules out a post-completion-only explanation. It does not justify suppressing every zero-cost context: the
normal completed-repair flow intentionally presents `Finish Ship Repair — FREE`. The safer behavior-canary shape is
to preserve the last coherent Repair action status across a transient `Ready`, leaving the original instant context
and request path untouched.

Follow-up runtime evidence narrowed the unsafe post-completion tuple to `Docked + previous Repairing + native Ready`.
The passive `repair_instant_context` mode remains unchanged. The explicit `repair_action_status_hold` mode layers a
bounded presentation hold at this getter and retains an independent click interlock; see
`20260718-repair-human-click-correlation.md`.
