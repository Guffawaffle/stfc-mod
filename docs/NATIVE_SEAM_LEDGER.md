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

### `GameServerModelRegistry.ProcessResultInternal(IParsingContext, ServiceResponse)` for client-253 sync

- Owner / file: `SyncHooks` in `mods/src/patches/parts/sync.cc`; payload routing in
  `mods/src/patches/sync_payload_builders.cc`; probe record in
  `docs/probes/20260810-client-253-sync-process-result.md`.
- Intended question: can one surviving central response seam replace the removed binary-container hook family while
  preserving the original entity-group type and protobuf bytes before typed client dispatch?
- Static evidence: verified client-252 and client-253 corpus snapshots retain the logical
  `ProcessResultInternal(IParsingContext, ServiceResponse)` relationship. Client-253 disassembly of the concrete
  generic instantiation receives the service response, enumerates its entity groups, parses them, and later
  completes typed dispatch. The old `ParseBinaryObject` / `ParseBinaryObjectsHelper` family is absent.
- Risk class: R4 native interpretation on an established production seam.
- Confidence rung: runtime observed in both the Netniv validation checkout and this private integration.
- Runtime evidence: the private client-253 release build booted cleanly with six active sync hooks installed,
  eleven removed/changed seams recorded as replaced, and zero failed. The configured receiver accepted officer,
  mission, trait, forbidden-tech, research, module, and resource uploads with HTTP 200/204. One existing ship upload
  received HTTP 400; the ship builder was unchanged by this repair and remains a separate receiver-contract
  uncertainty.
- Payload confidence: the hook reads only the `ServiceResponse.EntityGroups` wrapper, copies selected protobuf bytes,
  and retains no IL2CPP object pointers across asynchronous work.
- Original/trampoline confidence: validated in both builds; the private hook keeps the established ABI and calls the
  original exactly once with unchanged arguments.
- Flag / rollback path: disable `SyncPatches` through the existing sync patch configuration or remove the single
  `model-registry-process-result` install entry. Removed container hooks remain absent.
- Status: runtime-observed and promoted for client 253 in the private release build.
- Next action: revalidate after client updates and investigate the isolated ship HTTP 400 only if it reproduces.

### Removed client-253 binary sync seams and changed slot parser

- Owner / file: replaced descriptors in `mods/src/patches/parts/sync.cc`.
- Intended question: which pre-253 hooks must be prohibited rather than treated as ordinary missing methods?
- Static evidence: client 253 contains no `ParseBinaryObject` container family or
  `GameServerModelRegistry.ParseBinaryObjectsHelper`. `SlotDataContainer.ParseEntitySlotsData` changed from
  `EntityGroup` to `EntitySlotsData` under the same method name.
- Risk class: R4 native interpretation with one confirmed incompatible payload type.
- Confidence rung: static relationship for removal/change; the unsafe same-name slot detour failed the signature
  gate by construction and is not installed.
- Runtime evidence: both the Netniv validation and private release builds booted without the changed slot-parser
  detour; the private `SyncHooks` audit recorded it as replaced and installed the central owner with zero failures.
- Payload confidence: the old `EntityGroup*` slot parser declaration is invalid for client 253 and must never be
  reinstated. `EntitySlotsData` bytes are handled from their original central `EntityGroup` wrapper instead.
- Original/trampoline confidence: not applicable because these seams are not detoured.
- Flag / rollback path: replaced descriptors are diagnostic-only; rollback does not restore removed hooks.
- Status: replaced / prohibited for client 253.
- Next action: keep source guards and regression tests preventing lookup or installation from returning.

### Generic battle-result toast classification for Armada notifications

- Owner / file: `mods/src/patches/notification_service.cc` and
  `mods/src/patches/battle_notify_parser.cc`, reached through the existing
  `ToastObserver` hooks in `mods/src/patches/parts/disable_banners.cc`.
- Intended question: can `armada_battle_won` and `armada_battle_lost` follow
  current armada result production without adding another native hook or
  breaking the established generic `victory`/`defeat` fallbacks?
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
- Confidence rung: state-correlated for both Armada victory and defeat.
- Runtime evidence: retained 2026-07-30 logs show the existing
  `BattleResultHeader` parser and `Defeat` delivery path completing
  successfully. On 2026-07-31, an Armada victory on the release build logged
  `source=battle.armada_battle_won title='Armada Victory!'`, played the
  configured Armada audio policy, flushed and displayed the parsed battle
  summary, and requested the WinRT notification. The human confirmed that the
  notification appeared. A separate Armada defeat smoke on the same release
  build also produced the configured `armada_battle_lost` notification and
  warning audio.
- Payload confidence: the existing generic battle parser already reads this
  toast payload. The new classification reads only the current
  `BattleResultHeader.IsArmadaBattle` property under the same SEH boundary and
  fails closed to generic routing.
- Original/trampoline confidence: unchanged; no new hook or original call was
  introduced.
- Flag / rollback path: disabling the matching `armada_battle_won` or
  `armada_battle_lost` policy preserves the prior `victory`/`defeat` selection.
  Removing the contextual classifier fully restores the former exact-toast-state
  routing.
- Status: Armada victory and defeat are release-promoted and runtime-validated.
  When the matching Armada policy is enabled it specializes a generic Armada
  result; otherwise the established generic policy remains the fallback.
- Next action: revalidate the classifier after relevant battle-result producer
  or `BattleResultHeader` changes.

### `Digit.Prime.HUD.MissionsHudViewController` current-button visibility lifecycle

- Owner / file: release-supported patch in `mods/src/patches/parts/mission_hud_tweaks.cc`; probe record in
  `docs/probes/20260731-mission-hud-current-buttons.md`.
- Intended question: can the four buttons retained by the current mission HUD continue to honor the configured
  `always`/`auto`/`never` policy after the former common refresh method and Daily Goals button were removed?
- Static evidence: the 2026-07-30 current-client corpus no longer contains `_dailyGoalsButton` or `UpdateButtons()`.
  It retains `_missionsButton`, `_achievementsButton`, `_outpostsButton`, and `_challengesButton`, plus
  `OnEnable()` and the narrower `HandleOutpostsAndChallengesHUD()` refresh method.
- Risk class: R5 behavioral.
- Confidence rung: state-correlated for `OnEnable()`; runtime observed for the dedicated refresh seam.
- Runtime evidence: a Windows release experiment installed the replacement `OnEnable()` detour without failure.
  With `q_trials = "never"`, the human confirmed Q Trials was hidden; with `q_trials = "auto"` and
  `outposts = "always"`, the human confirmed all three buttons in that HUD state were visible. The final two-hook
  release cycle resolved and installed both current methods (`installed=2 failed=0 skipped=0 total=2`), and
  `PatchAudit` reported the module consistent.
- Payload confidence: typed managed controller only; both hooks have no extra arguments.
- Original/trampoline confidence: `OnEnable()` returned successfully and the UI remained interactive. The original
  is called exactly once before the mod reapplies configured visibility.
- Config / rollback path: `[ui.mission_hud]` retains `q_trials`, `field_training`, `outposts`, and `missions`.
  Set all four to `auto` to skip the module and install no hooks. The obsolete `daily_goals` key is ignored with a
  warning and omitted from rewritten/example configuration.
- Status: release-promoted for the current four-button surface. Daily Goals remains navigable elsewhere but no
  longer has a HUD component that this visibility feature can control.
- Next action: recheck visibility after an outpost or challenge state refresh when practical, and revalidate the
  explicit field/method dependency set after relevant client updates.

### Standard Recruit custom-quantity submission and failure callbacks

- Owner / file: completed temporary discovery record in
  `docs/probes/20260730-standard-recruit-transaction-limit-discovery.md`.
- Intended question: is the observed Standard Recruit inclusive ceiling of 160 present in client-visible bundle,
  feature-config, request, or failure data?
- Static evidence: the client-side feature-config method returns a generic/faction custom-quantity cap or fallback
  50. Disassembly of the showcase and shop-manager request path forwards quantity without comparing it to 160. The
  named generated `Bundle` fields expose purchase state and custom-quantity eligibility but no maximum custom
  quantity.
- Risk class: R2 managed log-only.
- Confidence rung: payload understood.
- Runtime evidence: the submission detour logged quantity 161 and Standard Recruit bundle ID `145512548`; its named
  metadata exposed no maximum. Neither premium-purchase failure callback executed; the common loot-chest failure
  handler captured both 161 and 520 as platform error 24, `InvalidBundleQuantity`, with message
  `Can not purchase chosen quantity`. HTTP code, category, transaction ID, and request URL were empty, and no accepted
  maximum was present. Human tests remain authoritative for the observed 159/160 success and 161 failure boundary.
- Payload confidence: typed managed submission and loot-chest failure parameters; code 24 maps directly to the
  current-client `PlatformError.ReponseCodes.InvalidBundleQuantity` enum.
- Original/trampoline confidence: both retained probe seams executed and called their originals once with unchanged
  arguments.
- Flag / rollback path: temporary descriptors, detours, install blocks, and manifest module removed after capture.
- Status: completed discovery evidence, not a feature or release surface.
- Next action: revalidate the empirical boundary after relevant game updates or test a materially different
  non-recruit chest category; do not infer a universal maximum from recruit chests.

### Runtime Unity UI composition from `InventoryUseRowWidget.SetWidgetData()`

- Owner / file: completed temporary capability proof recorded in
  `docs/probes/20260730-runtime-unity-ui-injection.md`.
- Intended question: can the injected mod compose a visible, arbitrarily positioned text object on an active game
  canvas using the current client's Unity UI runtime?
- Static evidence: the current corpus exposes `UnityEngine.Object.Instantiate(Object, Transform, bool)`,
  `GameObject.GetComponent(Type)`, `Transform` parenting/sibling methods, `RectTransform` layout setters,
  `TextLocalizer.OverrideLocalizedText(string)`, and `TMP_Text.set_fontSize(float)`. An existing popup-owned text
  object supplies a compatible font, material, and renderer.
- Risk class: R5 behavioral.
- Confidence rung: runtime observed.
- Runtime evidence: on 2026-07-30, a Windows release build cloned the Standard Recruit popup's amount text object,
  reparented and centered it on the active root canvas, and displayed mod-authored purple-backed text. The human
  supplied a screenshot confirming the visible result; the runtime logged
  `[ChestPurchaseSlider] displayed experimental-limit in-game notice`.
- Payload confidence: understood only for the tested popup-owned `TextLocalizer`, widget, and canvas relationship.
- Original/trampoline confidence: the already registered chest-row detour returned successfully before composition;
  the proof added no new detour.
- Flag / rollback path: the prototype composition path was removed after its visual proof; only the evidence record
  remains.
- Status: ability proven possible in the current client, explicitly not product-safe. Object lifecycle, managed
  rooting, scene/canvas replacement, deduplication, scaling, clipping, layering, raycast/input behavior, copied
  component side effects, UI-thread assumptions, and future symbol drift remain unproven.
- Next action: define and validate a bounded lifecycle contract before generalizing this into a reusable mod UI
  service or shipping any screen element based on it.

### `Digit.Prime.Inventories.InventoryUseRowWidget.SetWidgetData()`

- Owner / file: release-supported bounded patch in `mods/src/patches/parts/misc.cc`; probe record in
  `docs/probes/20260730-chest-purchase-slider-extension.md`.
- Intended question: can the client-side quantity ceiling for explicitly tagged chest-purchase rows be raised
  without changing donation, artifact-conversion, or ordinary inventory-use sliders?
- Static evidence: the 2026-07-30 corpus retains `InventoryUseRowWidget.SetWidgetData()` at RVA `0x11A2890`,
  `InventoryForPopup.IsChestPurchase` at offset `0x90`, and `InventoryForPopup.MaxItemsToUse` at offset `0x20`.
  `InventoryManager.GetChestsMaxPurchaseCustomQuantity()` selects a generic or faction-store feature-config ceiling
  and falls back to 50. Current-client disassembly shows the row renderer checking the chest tag after the context's
  ceiling has been assigned, making this narrower than any global slider mutation.
- Risk class: R5 behavioral.
- Confidence rung: state-correlated.
- Runtime evidence: a 2026-07-30 Windows release cycle resolved and installed the single hook
  (`installed=1 failed=0 skipped=0 total=1`) and the client completed boot without a seam-related crash. An eligible
  custom-quantity chest then logged `[ChestPurchaseSlider] extended quantity ceiling from 50 to 123`, and the human
  confirmed that the rendered slider reached 123 without confirming a purchase. A later release cycle at the final
  `0 = disabled` contract reported `installed=0 failed=0 skipped=1 total=1`, after which restoring 999 installed the
  hook again and the same tagged row logged `[ChestPurchaseSlider] extended quantity ceiling from 50 to 999`.
- Payload confidence: typed managed widget and typed `InventoryForPopup` context only; no opaque callback payload.
- Original/trampoline confidence: observed returning successfully for the tested tagged chest row after the guarded
  context adjustment.
- Config / rollback path: `[ui].extend_chest_purchase_max`, Windows-only and default 160. Set it to 0 to skip the
  hook and retain native behavior. Values from 1 through 160 are exposed and larger configured values clamp to 160.
  A future native ceiling above the configured value wins and emits an explicit diagnostic.
- End-to-end boundary: a Standard Recruit Chest purchase at 999 was rejected with the game's generic
  purchase-failure dialog, while follow-up tests succeeded at 159 and 160 and failed at 161 despite sufficient claim
  resources. Discovery logging captured the loot-chest service rejection as platform error 24,
  `InvalidBundleQuantity`, with no maximum in its payload. A different recruit chest subsequently accepted 160 and
  rejected 161 as well. This establishes a repeated empirical recruit-chest boundary, not a universal or
  client-discoverable limit.
- Status: runtime-observed, state-correlated, and release-promoted with a configurable, enforced 0-160 UI ceiling.
- Next action: revalidate after relevant game updates or test a materially different non-recruit chest category.

### Repair action coherence and stale-click interlock - 2026-08-11

- Seams: `FleetPlayerData.GetActionStatus(ActionType)`,
  `ActionElementWidget.GetInstantButtonContext()`, and
  `ActionElementWidget.OnInstantButtonClickCallback()`.
- Owner / file: `RepairActionInterlock` in `mods/src/patches/parts/repair_action_interlock.cc`; pure policy in
  `mods/src/patches/repair_action_interlock_policy.h`.
- Intended behavior: preserve the last coherent in-progress Repair status while the fleet is still `Repairing`, hold
  the current instant-button presentation for at most 2.5 seconds across the proven `Docked` / previous `Repairing`
  / native `Ready` race, and suppress an actionable stale Instant click only inside that same bounded window.
- Static evidence: PR #168 mapped the caller chain through `JobService.UpdateJobList`,
  `ActionElementWidget.HandleReactiveInt`, and `GetInstantButtonContext`; current dump lookup reports one exact
  overload for each promoted seam.
- Risk class: R5 behavioral. The interlock changes a returned status/context or omits one native click only for the
  runtime-proven stale tuple.
- Confidence rung: repeated in-game interception plus pure policy coverage. The science canary suppressed stale paid
  and zero-amount clicks without blocking the following genuine Ask-for-Help request; promotion removes stack
  capture, passive action-click observation, help-request observation, and live-debug event emission.
- Payload confidence: `FleetPlayerData`, `ActionType`, `GenericButtonContext`, and the widget context/property shapes
  were runtime-proven by PR #168. No managed pointer is retained.
- Original/trampoline confidence: all three seams installed and returned normally during the investigation smokes;
  the instant-click original is called exactly once unless the bounded stale predicate suppresses it.
- Performance boundary: one early action-type branch for non-Repair status queries; Repair-only paths use a single
  mutex over a fixed 16-entry array. There are no heap allocations, frame-tick work, polling, or normal-path logs;
  only an actual suppression writes one informational line.
- Performance smoke: canonical 60-second idle-system captures from clean commit `0ba8de7` used the same public
  release DLL with this module enabled and disabled. Average process CPU was 3.422% enabled versus 3.379% disabled;
  GPU-time p50 was 16.654 ms versus 16.666 ms and p95 was 17.742 ms versus 17.667 ms. The small, mixed deltas did not
  reveal a regression signal in this bounded A/B sweep; capture IDs are
  `20260811T094519Z-repair-interlock-enabled-idle-system-0ba8de7` and
  `20260811T094720Z-repair-interlock-disabled-idle-system-0ba8de7`.
- Flag / rollback path: `[patches].repairactioninterlock`, default `true`; set it to `false` and restart to remove all
  three hooks.
- Status: promoted from PR #168 science evidence to release-supported production behavior for issue #166.
- Next action: revalidate the three exact seams and the bounded overhead comparison after relevant client updates.

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

### `Digit.PrimeServer.Models.FleetPlayerData.GetActionStatus(ActionType)`

- Owner / file: science-only `mods/src/patches/parts/client_ship_state_probe.cc`; experiment contract in `docs/probes/20260714-repair-action-status-transition.md`.
- Intended question: which repair `ActionStatus` transitions occur between one Repair click and the stable Ask for Help button?
- Static evidence: current dump and script metadata report `ActionStatus GetActionStatus(ActionType)` at RVA `0x17BE0B0`; repair presentation has distinct `InProgress`, `InProgress_Free`, and `InProgress_AskForHelp` states and ask-for-help locale settings.
- Risk class: R4 native interpretation in passive mode; R5 behavioral in explicit guard mode.
- Confidence rung: state-correlated for the exact passive detour and established scalar/property reads; the guard is runtime-observed, showed partial mitigation, and is restored for an isolated original-symptom smoke.
- Runtime evidence: one free-play reproduction produced `Ready → InProgress_Free → Complete → Ready → Disabled`. A second Ship Manage reproduction produced `InProgress_AskForHelp → Ready → InProgress_AskForHelp` while the fleet remained `Repairing`; the invalid `Ready` window lasted about 334 ms. After another Free repair finished, Repair momentarily reappeared with displayed cost zero while the model still reported `Repairing`, producing the same `InProgress_Free → Ready` transition. In the guard smoke, two transitions recorded `guardApplied=true`, but the user still saw pay-to-repair choices before Ask for Help; one additional invalid `Ready` occurred while the model briefly reported `Docked` with previous state `Repairing`, outside the guard predicate. Native fleet-bar evidence reported repair complete at the boundary, and both fleets later converged to `Docked` without further input. No crash, hang, input loss, duplicate owner, or probe-induced request was observed.
- Payload confidence: runtime confidence for the receiver, fleet ID, current/previous state, 32-bit `ActionType`, and `ActionStatus`/original returns 0, 100, 200, 201, 202, and 300 on observed repair paths.
- Original/trampoline confidence: passed across the reproduced passive repair lifecycles with every original result returned unchanged. The retained passive mode calls the original exactly once and returns it unchanged.
- Flag / rollback path: science modes `repair_action_status` and `repair_action_status_guard`, default `off`, require `live_query`; caller budget separately defaults to zero and is clamped to one. Disable the probe and budget in TOML and restart, or remove one module/install entry.
- Status: passive science canary runtime-proven; the original behavior guard is restored unchanged for a pre-Ask-for-Help-only smoke after a later mixed-symptom smoke showed incomplete coverage. The one-event caller airlock was consumed successfully and its persistent budget restored to zero. The separate final post-completion `Instant 0` button remains possible, but has not been accidentally activated and produced no observed unintended request or spend. Symbolized caller chain: `JobService.UpdateJobList → ActionElementWidget.HandleReactiveInt → ActionElementWidget.GetInstantButtonContext → FleetPlayerData.GetActionStatus`. Static disassembly also confirms the instant click path forwards to `IActionHandler.RequestAction`.
- Next action: retain the deployed `repair_action_status_hold` canary until the uncommon Ask-for-Help → Speed-Up
  regression recurs. Two primary-transition flows passed as `202 → 200`; completion-only projections to `201` are
  accepted for this pass. On the next visual regression, correlate whether `202 → 100` occurred and whether the hold
  returned `202`. Keep the broader reconciliation hook disabled.

### `Digit.Prime.Actions.ActionElementWidget.GetInstantButtonContext()`

- Owner / file: science-only `mods/src/patches/parts/client_ship_state_probe.cc`; experiment contract in
  `docs/probes/20260717-repair-instant-button-context.md`.
- Intended question: which final interactability and amount tuple is projected when Repair/Instant briefly reappears
  during an active or completing repair?
- Static evidence: the current dump reports `GenericButtonContext GetInstantButtonContext()` at RVA `0x11E6CD0`.
  The prior one-shot stack and exact disassembly place it between `JobService.UpdateJobList` and the live widget's
  `_instantButtonContext`, with status and instant cost obtained independently.
- Risk class: R4 native interpretation in passive mode; R5 for the explicit bounded presentation hold in
  `repair_action_status_hold` mode.
- Confidence rung: runtime-proven passive seam, scalar/property reads, and bounded presentation canary.
- Runtime evidence: the released-debug cycle installed exactly this hook with zero failures or skips. During a user
  reproduction it captured an interactable amount transition `75 → 74 → 0` while the fleet remained `Repairing`; the
  zero transition preceded the independent `REPAIR_COMPLETE` boundary by approximately 1.345 seconds.
- Payload confidence: static confidence for the Repair `ActionType`, exact `FleetPlayerData` receiver filter, numeric
  current/previous fleet state, `GenericButtonContext.Interactable`, and `ResourceData.Amount`. No pointer is retained.
- Original/trampoline confidence: runtime-proven. The hook calls the original exactly once. Passive mode returns the
  exact original context pointer unchanged; hold mode may instead return the widget's already-rooted live Instant
  context for at most 2.5 seconds during the exact `Docked/previous Repairing/native Ready` race.
- Flag / rollback path: science modes `repair_instant_context` and `repair_action_status_hold`, default `off`, require
  `live_query`. Set the mode to `off` and restart, or revert the layered presentation experiment to checkpoint
  `085bfb6e1652b03e8a7a397bb899e7a48ad86a8c`.
- Status: passive observer runtime-proven; bounded presentation canary runtime-accepted on zero and paid stale
  proposals. Quv'Sompek proposed amount `251434` while the getter returned the existing amount-zero live context;
  an earlier held stale click was independently suppressed, followed by one valid help request after re-entry.
- Next action: retain the layered canary and accepted click interlock. Additional ordinary repairs are soak evidence;
  any regression should be compared with checkpoint `085bfb6e1652b03e8a7a397bb899e7a48ad86a8c`.

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
