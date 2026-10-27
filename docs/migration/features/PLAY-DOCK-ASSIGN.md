# PLAY-DOCK-ASSIGN — Swap Ship assignment

Contract version: 1. Status: implementation and offline qualification.

## Inputs and scope

- Source: Downstream `frozen/play-dev-20261026`,
  `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target base: `076c85a84ee5d126b02582f55936ccfc4a066a29`.
- Working branch: `feature/migrate-play-feature-pilots-20261026`.
- Inventory: PLAY-DOCK-ASSIGN. This reassesses the old Partial entry against the
  frozen play-dev source; it does not redefine the inventory's historical baseline.
- Scope: reconcile the existing dock port and repair supported gaps. No pin-state
  redesign, global tracker replacement, new assignment UX, deployment, or promotion.

## Source workflow and ownership

Pinned source paths below are relative to that exact source revision:

- `mods/src/patches/parts/double_click_assign_ship.cc` owns the tile-click hook,
  400 ms pairing, pin-input precedence, first shown assignment button, and Enter.
- `mods/src/patches/parts/hotkeys.cc:ScreenManager_Update_Hook` calls Enter after
  settings-capture and installHotkeyHooks gates, but before hotkeys-enabled/native
  hotkey-mode gates. Disabling ordinary shortcuts does not disable assignment.
- `mods/src/patches/key.cc` suppresses `Key::Pressed` during Settings input
  ownership; its underlying GetKeyInt observation is held-state, not an edge.
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
| O3 | Pair the same FleetPlayerData identity within 400 ms; a third click does not immediately retrigger; different/late clicks restart the pair. | Existing `AssignOnDoubleClick`; compare-only pointer, never dereferenced later. | Static algorithm comparison; timed native gestures pending. |
| O4 | Press only the first active/enabled dock on a visible canvas; stop even if its button cannot be read. Traverse wrapper → widget → listener → Button. | `CanvasController::OnScreen`, wrapper `UnityButton`, first-match return. | Binding-chain comparison; native shown/hidden/multiple dock trial pending. |
| O5 | Main/keypad Enter, no focused text input, no Settings-owned input; Enter follows hotkey patch activation, not ordinary shortcut mode. | Repair missing capture guard and hotkey-group activation; preserve frame-owner registration. | Existing production capture fixture demonstrates raw Enter remains readable during capture; repaired feature call path inspected; native editor/Enter trial pending. |
| O6 | Installed/live flags affect action availability; a disabled double-click group may still share a tile hook with pins without enabling assignment. | Existing independent success flag, shared terminal HookSet, live config checks. | Static installation/owner comparison; hook installation regression; failure/patch-off native checks pending. |
| O7 | Native enumeration roots the snapshot under tracker exclusion and excludes destroyed Unity objects. | Target still acquires a borrowed pointer snapshot; now roots each candidate and checks native liveness during synchronous inspection. This is a narrower custody boundary, not whole-snapshot equivalence. | Existing GC/tracker fixtures; later-item GC/destruction/rebind and first-visible ordering remain unqualified. |

## Decisions and limits

1. **Preserve the existing port.** Its pairing algorithm and button workflow do
   not need another implementation. Typed bindings replace access, not behavior.
2. **Repair capture suppression at the consumer.** `FrameServices().Dispatch`
   precedes `HandleShortcuts`. The latter's capture check does not protect a
   separate frame callback reading raw `unity::Key::Down`. The editor intentionally
   needs those raw keys. Changing global raw-input behavior would break recording.
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

Current run, checkpoints, artifacts, and pending trial are recorded in
[MIGRATE-PLAY-FEATURE-PILOTS.md](../MIGRATE-PLAY-FEATURE-PILOTS.md).
Classification remains implementation/qualification work until that evidence is
attached. Do not mark overall functional parity complete.

The extracted production `dock_assignment::EnterRequested` input boundary is
exercised with the real Shortcut capture state by `shortcut_capture_guard_test`.
It covers capture/drain, main/keypad edges, held continuation, focused text,
installed/live/patch gates, and ordinary shortcut-mode independence. It does not
invoke the native button chain or full exported feature frame callback. The
original pair has no episode reset across view closure, setting toggles, or reused
ship pointers; preserve that inherited limitation separately from capture repair.
