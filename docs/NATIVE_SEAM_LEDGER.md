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

### Generic battle-result toast classification for Armada notifications

- Owner / file: `mods/src/patches/notification_service.cc` and
  `mods/src/patches/battle_notify_parser.cc`, reached through the existing
  `ToastObserver` hooks in `mods/src/patches/parts/disable_banners.cc`.
- Intended question: can `armada_battle_won` follow current armada result
  production without adding another native hook or breaking the established
  generic `victory` fallback?
- Static evidence: the canonical 2026-07-30 corpus (`dump.cs` SHA-256
  `0e3ab23ea6c0b7697485e45b4aaa5c8002597ed377a56854a8478c8aca89f205`)
  retains `ToastState.ArmadaBattleWon = 18` and
  `ToastState.ArmadaBattleLost = 19`, but current-client disassembly of
  `ToastBattleResultObserver.CreateToastForBattle(string,
  IBattleResultHeader)` at RVA `0x140F120` routes `BattleType.ArmadaMarauder`
  (`8`) and `BattleType.ArmadaMta` (`11`) through the ordinary
  `Victory`/`Defeat` (`10`/`11`) creation branch. `BattleResultHeader` exposes
  the authoritative `IsArmadaBattle` property at RVA `0x1775FC0`.
- Risk class: R4 native interpretation within an existing product hook.
- Confidence rung: static relationship plus generic battle-payload runtime
  evidence; armada-specific state correlation awaits artifact smoke.
- Runtime evidence: retained 2026-07-30 logs show the existing
  `BattleResultHeader` parser and `Defeat` delivery path completing
  successfully. VIP feedback independently reports that `victory` delivers
  while `armada_battle_won` does not, consistent with the static producer
  relationship.
- Payload confidence: the existing generic battle parser already reads this
  toast payload. The new classification reads only the current
  `BattleResultHeader.IsArmadaBattle` property under the same SEH boundary and
  fails closed to generic routing.
- Original/trampoline confidence: unchanged; no new hook or original call was
  introduced.
- Flag / rollback path: disabling `armada_battle_won` preserves the prior
  `victory`/`defeat` selection. Removing the contextual classifier fully
  restores the former exact-toast-state routing.
- Status: release candidate. When the armada-specific policy is enabled it
  specializes a generic armada result; otherwise the established generic
  policy remains the fallback.
- Next action: smoke an Armada victory with a production artifact and retain
  the resulting `battle.armada_battle_won` queue/delivery evidence.

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

### `Digit.Prime.Officers.OfficerSortGenerators.InitializeAssignmentSorters()`

- Owner / file: `mods/src/patches/parts/officer_assignment_sort.cc`
- Intended question: can the removed Below Deck Ability sort in Manage Ship's Assign Officers view be restored through
  the game's existing assignment-sort generator without intercepting dropdown selection or officer payloads?
- Static evidence: the 2026-07-30 corpus retains
  `SortingPredicates.OfficerSortByBelowDeckAbilityAscending(object, object)`, while the prior corpus contained
  `_assignBelowDeckAbilityId`. The current `OfficerSortGenerators` still exposes private
  `StandardAssignmentSortFunction` and `AddAssignmentSorter` methods.
- Risk class: R5 behavioral.
- Confidence rung: runtime-observed managed mutation plus human-confirmed dropdown behavior.
- Runtime evidence: 2026-07-30 releasedbg and release cycles installed the assignment hook once, borrowed the concrete
  comparator delegate type from assignment option zero, and appended option nine. The release audit reported the
  promoted `OfficerAssignmentSort` module consistent with one installed hook. The game remained responsive and the
  user confirmed the sort works.
- Payload confidence: no callback payload; the hook receives only the `OfficerSortGenerators` instance and operates
  after the original returns.
- Original/trampoline confidence: observed returning successfully during client initialization.
- Flag / rollback path: `[ui].restore_below_decks_assignment_sort`, default true; set it to false or remove the single
  patch-table entry and module if the seam is unstable.
- Status: promoted to release-supported, default-on behavior after runtime validation. A bounded runtime
  dump established that assignment option display keys are leading-underscore suffixes; using
  `_below_deck_ability` resolves the retained assignment localization and renders `BELOW DECK ABILITY`.
- Next action: monitor normal release artifacts for future client dependency drift.

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

## Static Enforcement

The quarantine patch adds a source-level static guardrail for product hook code:

- `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS` must stay absent.
- `OnShipLocateAction` must stay absent from `mods/src/patches/parts/hotkeys.cc` unless it is explicitly re-promoted through a new one-callback ledger entry.
- Generated pointer callback guard install machinery must stay absent unless a future change carries an explicit allowlist/promotion marker tied to a ledger entry.
