# PLAY-DOCK-ASSIGN — Swap Ship assignment

Contract version: 2, 2026-11-03. Source/consumer reconciliation; native edge
qualification remains separate.

## Inputs and scope

- Source: Downstream `frozen/play-dev-20261026`,
  `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target base: `076c85a84ee5d126b02582f55936ccfc4a066a29`.
- Working branch: `feature/migrate-play-feature-pilots-20261026`.
- Inventory: PLAY-DOCK-ASSIGN. This reassesses the old Partial entry against the
  frozen play-dev source; it does not redefine the inventory's historical baseline.
- Scope: reconcile the existing dock port and repair supported gaps. No pin-state
  redesign, global tracker replacement, new assignment UX, deployment, or promotion.

The pilot target/base/branch above are historical. The current continuation uses
the existing `play-dev-ripper` worktree, starting at
`5d7613d0383c91b6c051fecdc6605f707224ae0e`. It retains the accepted basic Windows
pilot behavior and reassesses the actual assignment consumer without repeating
live assignment or pin gestures.

## Source workflow and ownership

Pinned source paths below are relative to that exact source revision:

- `mods/src/patches/parts/double_click_assign_ship.cc` owns the tile-click hook,
  400 ms pairing, pin-input precedence, first shown assignment button, and Enter.
- `mods/src/patches/parts/hotkeys.cc:ScreenManager_Update_Hook` calls Enter after
  settings-capture and installHotkeyHooks gates, but before hotkeys-enabled/native
  hotkey-mode gates. Disabling ordinary shortcuts does not disable assignment.
- `mods/src/patches/key.cc` suppresses `Key::Pressed` during Settings input
  ownership; its underlying GetKeyInt observation is held-state, not an edge.
- Settings capture suppresses keyboard assignment. The source's physical tile
  double-click route has no capture or text-focus gate; preserve that distinction.
- `mods/src/patches/swap_ship_pin_input.cc:HandleTileClick` consumes badge/drag
  gestures before ordinary tile handling. Its durable pin writer remains separate.
- `mods/src/prime/AssignShipsWidget.h`, `ShipTileWidget.h`,
  `ButtonAndContextWrapper.h`, `GenericButtonWidget.h`, `CanvasController.h`, and
  `mods/src/il2cpp/il2cpp_helper.h:ObjectFinder` supply native access/lifetime.
- `mods/src/config.cc`, `defaultconfig.h`, `settings/upstream_features.cc`,
  `settings/upstream_settings.cc`, `settings/shortcut_settings.cc`, and
  `patches/parts/runtime_config_keys.h` supply the live UI setting, availability,
  registered persistence, and shortcut conflict explanation.

## Behavioral obligations

| ID | Pinned source behavior | Target mapping / adaptation | Verification |
| --- | --- | --- | --- |
| O1 | `[patches].doubleclickassignshiphooks` controls installation; `[ui].double_click_to_assign_ship` defaults false and is live/persisted. | Existing feature descriptor, nested Config, catalog owner/readiness and registered writer. | Source comparison; production catalog regression; exact-client toggle pending. |
| O2 | Badge/drag consumption clears the pending double-click and bypasses ordinary tile handling; ordinary clicks call original first. | Shared `InstallShipTileClick` owner and `ship_pin_input::ConsumeTileClick`. | Static call-order comparison; badge/drag interaction trial pending. |
| O3 | Pair the same FleetPlayerData identity within 400 ms; a third click does not immediately retrigger; different/late clicks restart the pair. | Existing `AssignOnDoubleClick`; compare-only pointer, never dereferenced later. | Controlled production pairing/late/different modes; timed native gestures pending. |
| O4 | Press only the first active/enabled dock on a visible canvas; stop even if its button cannot be read. Traverse wrapper → widget → listener → Button. | Qualified active/parent/visible queries and rooted raw button chain; first-known-shown return. | Controlled production consumer modes cover ordering, unavailable observations and local reentry; native dock trial pending. |
| O5 | Main/keypad Enter, no focused text input, no Settings-owned input; Enter follows hotkey patch activation, not ordinary shortcut mode. | Retain capture guard and hotkey-group activation, including current capture checks after managed queries; preserve frame-owner registration. | Input-boundary capture fixture plus actual Enter frame consumer reentry mode; native editor/Enter edge trial pending. |
| O6 | Installed/live flags affect action availability; a disabled double-click group may still share a tile hook with pins without enabling assignment. | Existing independent success flag, shared terminal HookSet, live config checks. | Static installation/owner comparison; hook installation regression; failure/patch-off native checks pending. |
| O7 | Native enumeration roots the snapshot under tracker exclusion and excludes destroyed Unity objects. | Target still acquires a borrowed pointer snapshot; roots each candidate and the selected canvas/button chain during synchronous inspection. This is a narrower custody boundary, not whole-snapshot equivalence. | Controlled first-visible, liveness and rebind cases; actual collection/lifetime and whole-snapshot ordering remain unqualified. |

The source's active/visible wrappers have an additional implementation defect:
`AssignShipsWidget::__get_isActiveAndEnabled` and `CanvasController::Visible`
return `IL2CppPropertyHelper::Get<bool>()` directly, although it returns a boxed
value pointer. Successful boxed false can therefore convert to true. The table's
active/visible rule states the intended workflow; do not restore that conversion
defect. Typed bool-value gating is an existing correction.

## Current consumer audit

`PressAssignButton` in the starting typed source roots each candidate, then reads
Unity liveness and `CanvasController::OnScreen`. The latter performs managed
Behaviour active-state, GetComponentInParent(Type) and Canvas.Visible calls. It
then traverses the assignment wrapper, widget, semaphore listener and Unity Button
through fields, stopping after the first candidate judged shown, even if its
button is absent. Those field reads do not themselves supply a managed reentry
point.

The preserved Windows before-repair consumer fixture demonstrates eight bounded
failures in the starting source:

- An unavailable liveness/active/visible observation reads as false and
  can redirect the pending action to another dock. A known hidden/dead candidate
  may be skipped; an unknown candidate must not silently establish another as the
  first eligible dock.
- A managed visibility/liveness query can disable current assignment policy,
  hide the selected canvas, invalidate its owner, or start shortcut capture during
  the Enter route. Admission performed before that query does not establish that
  the later Press is still authorized. Capture suppression remains Enter-only;
  physical double-click retains its native mouse policy.

The bounded correction uses qualified liveness, Behaviour active-state,
GetComponentInParent and Canvas.Visible results. Known false or a successfully
observed absent canvas may skip a candidate; unavailable observations abort the
pending action. Stop at the first known shown dock even if its button is absent
or its state later becomes invalid. Retain roots for the selected owner, canvas
and acquired raw field-chain objects; validate their identities after the managed
observations and make the final decision from current assignment policy. Do not
redirect an invalidated selected dock into another one. These roots and same-parent/
field-chain identity rechecks provide local custody for the synchronous call;
they do not lock the Unity hierarchy or make all managed observations atomic.
The final same-parent check follows the second Visible and active-state queries;
a controlled late parent replacement during that added validation boundary
failed before this order correction. That self-review delta is recorded
separately from the original eight failures. All 27 final dock modes pass on both
Windows and Linux controlled executables; exact inputs, build results and binary
identities remain attached to the cohort record.

These are consumer guard obligations, not evidence that a current-client player
has reproduced the race. The late-policy risk also exists in the frozen flow;
classify a demonstrated correction honestly rather than calling every failure a
newly omitted migration feature. No full-registry/snapshot redesign is implied.
The [donation/dock cohort](../FEATURE-COHORT-20261103-DONATION-DOCK.md) records the
preserved before-repair cases, final controlled results and executable identities.
Signed source and review results remain separate from fixture qualification.

## Decisions and limits

1. **Preserve the existing port.** Its pairing algorithm and button workflow do
   not need another implementation. Typed bindings replace access, not behavior.
2. **Repair Enter capture suppression at the consumer.** `FrameServices().Dispatch`
   precedes `HandleShortcuts`. The latter's capture check does not protect a
   separate frame callback reading raw `unity::Key::Down`. The editor intentionally
   needs those raw keys. Changing global raw-input behavior would break recording.
   Recheck current capture policy after managed queries for Enter. Do not add that
   restriction to physical mouse assignment. `unity::Key::IsInputFocused()` is
   cached within the frame; repeating it does not establish a fresh text-focus
   observation after reentry. This correction does not change that cache policy.
3. **Retain edge-triggered Enter.** This is an already documented target safety
   decision: one press per physical key edge instead of source held-key repeats.
   It is an intentional difference, not demonstrated equivalence. Native acceptance
   remains pending; do not silently revert it or claim source-identical behavior.
4. The target requires successful shared tile-hook installation before Enter can
   assign; the source checks the configured gate even when hook installation failed.
   Retain fail-unavailable target behavior as an explicit safety difference.
5. A managed root protects GC custody, not native Unity lifetime. Check liveness
   separately. Per-candidate roots do not reproduce acquisition of all source
   roots under tracker exclusion. Retain this as an unresolved structural boundary,
   not proof of a reproduced UAF. Source enumeration also bounds its snapshot
   (default 128, latest cohort), while target enumerates all tracked widgets in
   construction order. Many simultaneously shown docks could therefore differ.
   No global tracker redesign or persistent dock cache is introduced in this pilot.
6. Source tests cover configuration persistence, not full double-click/native
   assignment. Offline fixture success cannot establish exact-client behavior.

## Validation and recovery

The original pilot run, checkpoints, artifacts, and pending trial are recorded in
[MIGRATE-PLAY-FEATURE-PILOTS.md](../MIGRATE-PLAY-FEATURE-PILOTS.md).
The current [donation/dock continuation](../FEATURE-COHORT-20261103-DONATION-DOCK.md)
records controlled consumer checks separately. Classification remains Partial;
do not mark overall functional parity complete.

Guff subsequently confirmed both pilot workflows worked on Windows using the
installed pilot artifact: source `241372c3efb2e3b1c5f0c9fa5ee336387044cfc6`,
SHA-256 `66074968bcc1c5718d9ec1c3a5f089a70e7c9de7d1b6468b96afb1320d79d1cc`.
That basic acceptance does not qualify this continuation's changed bytes, resolve
full-snapshot custody or establish all native edge cases listed here. See the
linked pilot record.

The extracted production `dock_assignment::EnterRequested` input boundary is
exercised with the real Shortcut capture state by `shortcut_capture_guard_test`.
It covers capture/drain, main/keypad edges, held continuation, focused text,
installed/live/patch gates, and ordinary shortcut-mode independence. It does not
invoke the native button chain or full exported feature frame callback. The
original pair has no episode reset across view closure, setting toggles, or reused
ship pointers; preserve that inherited limitation separately from capture repair.
