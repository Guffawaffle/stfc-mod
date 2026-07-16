# Native Seam Ledger

Purpose: maintain living confidence records for native seams and generated hook surfaces. Policy lives in [Native Probe Safety](NATIVE_PROBE_SAFETY.md); this file records seam-specific evidence.

Rules for this ledger:

- Static source or IL2CPP dump evidence is not runtime confidence.
- A generated family can be tracked as a risk surface, but it does not make any member product-safe.
- One callback's result does not transfer to siblings with similar signatures.
- Add or update a ledger entry before runtime work that goes beyond static review.
- Keep failed seams in the ledger so future work does not rediscover the same crash path.

## Entry Template

Use this shape for future seams:

- Seam:
- Owner / file:
- Intended question:
- Static evidence:
- Risk class:
- Confidence rung:
- Runtime evidence:
- Payload confidence:
- Original/trampoline confidence:
- Flag / rollback path:
- Status:
- Next action:

Confidence rungs are defined in [Native Probe Safety](NATIVE_PROBE_SAFETY.md): `symbol exists -> static relationship -> runtime observed -> state-correlated -> payload understood -> product-safe`.

Risk classes are also defined there: R0 static, R1 passive runtime, R2 managed log-only, R3 native canary, R4 native interpretation, and R5 behavioral or broad.

## Entries

### Static Inventory Snapshot - 2026-05-23

This inventory records source-level seams from a static-only review. It does not claim new runtime observation, payload confidence, original/trampoline confidence, or product-safe status. "Operationally relied on" below means the seam is part of current product code paths; it is not a new confidence rung and is not evidence from this pass.

| Seam | Owner / file | Functionality supported | Risk class | Confidence / status |
| --- | --- | --- | --- | --- |
| `ScreenManager.Update` frame tick | `mods/src/patches/frame_tick.cc`, installed from `mods/src/patches/patches.cc` | Central frame fan-out for hotkeys, activity-triggered fleet-state follow-through, optional live-debug tick, and original frame policy | R5 behavioral | Operationally relied on; hook installation runtime-verified on 2026-07-25. |
| `ShortcutsManager.InitializeActions` | `mods/src/patches/parts/hotkeys.cc` | Controls whether Scopely shortcut initialization runs according to shortcut policy | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `ShortcutsManager.LateUpdate` | `mods/src/patches/parts/hotkeys.cc` | Suppresses native shortcut update when dispatcher-owned input should win | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `ShortcutsManager.SelectShip(int)` | `mods/src/patches/parts/hotkeys.cc` | Native fleet-selection suppression/fallthrough around numeric ship selection | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `RewardsButtonWidget.OnDidBindContext` | `mods/src/patches/parts/hotkeys.cc` | Reward/cargo context capture for hotkey action state | R4/R5 native interpretation / product behavior | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `PreScanTargetWidget.ShowWithFleet` | `mods/src/patches/parts/hotkeys.cc` | Pre-scan target and fleet context capture for scan/cargo actions | R4/R5 native interpretation / product behavior | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `FleetBarViewController.RequestSelect(int)`, `RequestSelect(Component)`, `ElementAction(int)` | `mods/src/patches/parts/hotkeys.cc` | Defensive suppression around native fleet selection and fleet-bar click/select paths | R5 behavioral | Grouped because the seams support one fleet-bar selection boundary; operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `SectionManager.BackButtonPressed` | `mods/src/patches/parts/hotkeys.cc` | Escape/back duplicate suppression | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `NavigationInteractionUIViewController.OnSetCourseButtonClick` | `mods/src/patches/parts/hotkeys.cc` | Duplicate set-course suppression | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `NavigationZoom.Update` | `mods/src/patches/parts/zoom.cc` | Zoom in/out, preset, min/max, and reset dispatch | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `FleetEvents.TriggerPlayerFleetsChangedEvent(List<FleetPlayerData>)` | Removed release canary; historical evidence below | Rejected opportunistic player fleet-state observation for notifications | R4 native interpretation | Hook installed successfully on 2026-07-25 but produced zero observations during the relevant hidden-Fleet-Bar arrival and was removed before release. |
| `FleetStateWidget.SetWidgetData` | `mods/src/patches/parts/fleet_arrival.cc`, owned by `FleetArrivalHooks` | Opportunistic Fleet Bar fallback into the shared fleet-state notification machine | R4/R5 native interpretation / product behavior | Operationally relied on; static relationship mapped; retained as a deduped fallback rather than the authoritative source. |
| `FleetsManager` scan from the shared frame tick | `mods/src/patches/fleet_notifications.cc`, called from `mods/src/patches/frame_tick.cc` | Fleet Bar-independent follow-through at a bounded 250 ms cadence while an observed fleet remains transitional | R5 behavioral | Runtime-validated twice on 2026-07-25: hidden-Fleet-Bar arrivals produced the expected transition, audio playback, and WinRT request; continuous post-login polling removed before publication. |
| `DeploymentEvents.Trigger*` live-debug/runtime-sync hooks | `mods/src/patches/parts/live_debug.cc` and `mods/src/patches/parts/deployment_runtime_observers.cc` | Historical fleet runtime observation, live-debug events, notifications, and sync triggers | R4/R5 native interpretation / product behavior | Dormant/disabled after unattended action-queue stall evidence; do not reactivate as the fleet-notification source. |
| `live_debug_tick(ScreenManager*)` | `mods/src/patches/parts/live_debug.cc`, reached through the frame tick subscriber when live query is enabled | File-backed live-debug request polling and read-only response generation | R4 native interpretation | Static relationship mapped; gated by `LiveDebugChannelEnabled()`; not newly runtime-verified by this pass. |
| `probe::dump_*` / `probe::search_methods` | `mods/src/probe/probe.h` | Header-only IL2CPP runtime introspection toolkit | R0 while unused; R3/R4 if invoked in-process | Static toolkit only in this inventory. No active call site was found in the reviewed patch surface; do not treat it as a safe runtime probe without a separate ledger row and approval. |

### `Digit.PrimeServer.Events.FleetEvents.TriggerPlayerFleetsChangedEvent(List<FleetPlayerData>)`

- Former owner / file: release canary in `FleetArrivalHooks`, removed from `mods/src/patches/parts/fleet_arrival.cc`.
- Intended question: observe player fleet-state changes while full-screen UI prevents `FleetStateWidget` refreshes.
- Static evidence: the 2026-07-25 private research corpus contains `FleetEvents.TriggerPlayerFleetsChangedEvent` at `0x1565180` with the static signature `void(List<FleetPlayerData>)`; multiple game systems subscribe handlers with the same payload.
- Risk class: R4 native interpretation. The hook calls the original first, then reads only the supplied list and existing typed `FleetPlayerData` fields.
- Confidence rung: static relationship.
- Runtime evidence: the hook installed successfully on 2026-07-25, but a one-system arrival with the Fleet Bar hidden emitted neither hook traffic nor a notification. A visible-Fleet-Bar recall immediately afterward produced sound, isolating the failure to observation rather than delivery.
- Payload confidence: list shape and element type are statically corroborated; firing order, coverage, and transition timing remain unverified.
- Original/trampoline confidence: unverified for this exact seam.
- Flag / rollback path: the canary was removed without changing the validated widget/scanner path.
- Status: rejected and removed before release because it added hook surface without demonstrated coverage.
- Next action: keep absent unless new authoritative runtime evidence justifies a separately reviewed seam.

### `Digit.Prime.GameInput.ShortcutsManager.OnShipLocateAction(InputAction.CallbackContext)`

- Owner / file: `mods/src/patches/parts/hotkeys.cc`, formerly in the removed generated pointer callback guard family.
- Intended question: suppress native locate fallthrough when dispatcher-owned input should win.
- Static evidence: source/dump evidence showed the callback exists and has `InputAction.CallbackContext` shape; this is static only.
- Risk class: R5 as part of the generated guard family; R3/R4 only if revisited as a one-callback private canary.
- Confidence rung: failed before product-safe; runtime-observed does not imply safe.
- Runtime evidence: plain `Space` worked; `Shift+Space` crashed; live config secondary was `TAB`; removing logging did not stop the crash; opaque payload treatment did not stop the crash; removing `OnShipLocateAction` from the generated guard list stopped the crash.
- Payload confidence: failed/unsafe. Do not dereference payload for this seam without a new isolated proof.
- Original/trampoline confidence: failed/unsafe. Do not rely on original/trampoline behavior for this seam without a new isolated proof.
- Flag / rollback path: keep absent from product hook code and any generated guard list. Any revisit must be one-callback, default-off, and removable without touching siblings.
- Status: failed/unsafe/superseded.
- Next action: keep deleted from the product guard family; only revisit from R0/R1 with explicit approval.

### `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS`

- Owner / file: formerly `mods/src/patches/parts/hotkeys.cc`; removed from product hook code.
- Intended question: shared suppression for native `ShortcutsManager.On*` pointer-shaped shortcut callbacks when dispatcher-owned input should win.
- Static evidence: the former macro expanded one source list into descriptors, hook functions, and install calls for `ShortcutsManager.On*` callback guards. After the quarantine patch, the macro list, shared handler, generated hook functions, and generated install machinery are absent from product hook code.
- Risk class: R5 when installed or changed as a broad generated family; individual callbacks must start lower as separate ledger entries.
- Confidence rung: static relationship for the historical family shape only. No runtime confidence for unclassified members.
- Runtime evidence: the former `OnShipLocateAction` member failed/unsafe; that failure does not classify the remaining callbacks as safe or unsafe.
- Payload confidence: none for the family. The former shared handler cast `void*` to `InputActionCallbackContext`; each callback needs its own payload confidence before payload interpretation is trusted.
- Original/trampoline confidence: unclassified for the family. The former shared handler could call `original(_this, context)` when not suppressed, so original/trampoline confidence must be tracked per callback.
- Flag / rollback path: removal/quarantine is the rollback path. Do not use a single generated-family flag as a discovery mechanism. Any future probe should enable one callback/action at a time with an explicit disable path.
- Status: removed/quarantined; not product-safe.
- Next action: keep absent from product hook code; create individual ledger entries before touching any member; do not reintroduce without per-callback ledger promotion.

### `Digit.PrimeServer.Models.FleetPlayerData.GetActionStatus(ActionType)`

- Owner / file: science-only `mods/src/patches/parts/client_ship_state_probe.cc`; experiment contract in `docs/probes/20260714-repair-action-status-transition.md`.
- Intended question: which repair `ActionStatus` transitions occur between one Repair click and the stable Ask for Help button?
- Static evidence: current dump and script metadata report `ActionStatus GetActionStatus(ActionType)` at RVA `0x17BE0B0`; repair presentation has distinct `InProgress`, `InProgress_Free`, and `InProgress_AskForHelp` states and ask-for-help locale settings.
- Risk class: R4 native interpretation in passive mode; R5 behavioral in explicit guard mode.
- Confidence rung: state-correlated for the exact passive detour and established scalar/property reads; the first guard is runtime-observed but failed its visible-outcome smoke.
- Runtime evidence: one free-play reproduction produced `Ready → InProgress_Free → Complete → Ready → Disabled`. A second Ship Manage reproduction produced `InProgress_AskForHelp → Ready → InProgress_AskForHelp` while the fleet remained `Repairing`; the invalid `Ready` window lasted about 334 ms. After another Free repair finished, Repair momentarily reappeared with displayed cost zero while the model still reported `Repairing`, producing the same `InProgress_Free → Ready` transition. In the guard smoke, two transitions recorded `guardApplied=true`, but the user still saw pay-to-repair choices before Ask for Help; one additional invalid `Ready` occurred while the model briefly reported `Docked` with previous state `Repairing`, outside the guard predicate. Native fleet-bar evidence reported repair complete at the boundary, and both fleets later converged to `Docked` without further input. No crash, hang, input loss, duplicate owner, or probe-induced request was observed.
- Payload confidence: runtime confidence for the receiver, fleet ID, current/previous state, 32-bit `ActionType`, and `ActionStatus`/original returns 0, 100, 200, 201, 202, and 300 on observed repair paths.
- Original/trampoline confidence: passed across the reproduced passive repair lifecycles with every original result returned unchanged. The retained passive mode calls the original exactly once and returns it unchanged.
- Flag / rollback path: science mode `repair_action_status`, default `off`, requires `live_query`; caller budget separately defaults to zero and is clamped to one. The failed `repair_action_status_guard` mode was removed. Disable the passive probe and budget in TOML and restart, or remove one module/install entry.
- Status: passive science canary runtime-proven; first behavior guard failed its visible-outcome goal and was removed. The one-event caller airlock was consumed successfully and its persistent budget restored to zero. The final post-completion `Instant 0` button remains possible, but has not been accidentally activated and produced no observed unintended request or spend. Symbolized caller chain: `JobService.UpdateJobList → ActionElementWidget.HandleReactiveInt → ActionElementWidget.GetInstantButtonContext → FleetPlayerData.GetActionStatus`. Static disassembly also confirms the instant click path forwards to `IActionHandler.RequestAction`.
- Next action: review one bounded probe/canary at the mapped instant-button projection boundary; do not widen or promote the failed status-only guard, and keep the broader reconciliation hook disabled.

### `Digit.PrimeServer.Services.FleetService.UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData)`

- Owner / file: proposed science-only `mods/src/patches/parts/client_ship_state_probe.cc`; experiment contract in `docs/probes/20260714-fleet-model-reconciliation.md`.
- Intended question: when deployed-fleet data arrives, does `FleetService` update the matching client `FleetPlayerData` to a coherent state?
- Static evidence: current dump and script metadata report `bool UpdateFleetWithDeploymentData(FleetPlayerData, FleetDeployedData)` at RVA `0x1613380`; `FleetService` owns the adjacent player-fleet update, state evaluation, job lifecycle, repair cleanup, and stuck-fleet recovery neighborhood.
- Risk class: R4 native interpretation because a passive detour must use the original/trampoline and correlate two managed object pointers with a boolean return.
- Confidence rung: static relationship.
- Runtime evidence: none for this detour. Existing serial `fleet-slots-state` queries prove only that the current player-fleet model can be read passively.
- Payload confidence: static type confidence only. Initial reads are limited to already-established scalar IDs/base states; pointer lifetimes and nested payloads remain unproven.
- Original/trampoline confidence: unproven. Exact script ABI is recorded; require a single-seam reachability run before stack or payload escalation.
- Flag / rollback path: proposed mutually exclusive science mode `fleet_reconciliation`, default `off`; stack budget independently defaults to zero; disable in TOML and restart, or remove one module/install entry.
- Status: proposed; not implemented or approved for runtime.
- Next action: review the probe contract and ABI, correct stale fleet-state diagnostic mappings, then explicitly approve or reject one bounded reachability run.

## Static Enforcement

The quarantine patch adds a source-level static guardrail for product hook code:

- `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS` must stay absent.
- `OnShipLocateAction` must stay absent from `mods/src/patches/parts/hotkeys.cc` unless it is explicitly re-promoted through a new one-callback ledger entry.
- Generated pointer callback guard install machinery must stay absent unless a future change carries an explicit allowlist/promotion marker tied to a ledger entry.
