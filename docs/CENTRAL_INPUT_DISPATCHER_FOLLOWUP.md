# Central Input Dispatcher Follow-Up

Source folder: `D:\dev\stfc-mod`

Status: handoff plan after the native ship-selection fallthrough fix. This doc is intentionally scoped to input/keybind seams and should be read with:

- `docs/KEYBIND_ACTION_SYSTEM_AUDIT.md`
- `docs/UNIFIED_INPUT_BIND_IMPLEMENTATION_PLAN.md`
- `docs/EVENT_DRIVEN_INPUT_SPIKE.md`
- `docs/SCOPELY_NATIVE_SHORTCUT_CALLBACK_AUDIT.md`

## Current Landing Point

The unified runtime binding dispatcher now decides modified numeric chords before Scopely native ship selection can act on the bare digit.

The important fixed case is:

- `1` still selects ship 1 through the normal fleet binding.
- Double-tap ship locate behavior is preserved.
- `ALT-1` can toggle the default cargo display.
- `ALT-1` no longer falls through to native ship 1 selection.
- Rebinding still matters: if a user intentionally binds `ALT-1` to `select_ship1`, the dispatcher-owned ship action is not treated as a conflicting cargo/default action.

The implementation does this by refreshing native shortcut suppression from a physical key snapshot at the native shortcut seam, then suppressing `ShortcutsManager.SelectShip` when the active chord has already been consumed by the dispatcher.

## Native Seams Now In Play

These hooks are part of the current input routing surface:

- `ScreenManager.Update`: primary game-thread dispatcher pump.
- `ShortcutsManager.InitializeActions`: Scopely shortcut startup policy only.
- `ShortcutsManager.LateUpdate`: native shortcut update suppression for consumed chords.
- `ShortcutsManager.SelectShip`: confirmed upstream seam for native number-key ship selection.
- `FleetBarViewController.RequestSelect(int)`: downstream fleet-selection guard.
- `FleetBarViewController.RequestSelect(Component)`: downstream component-based fleet-selection guard.
- `FleetBarViewController.ElementAction`: downstream fleet element-action guard.
- `RewardsButtonWidget.OnDidBindContext`: cargo display context seam.
- `PreScanTargetWidget.ShowWithFleet`: cargo display context seam.
- `SectionManager.BackButtonPressed`: Escape exit suppression seam.
- `NavigationInteractionUIViewController.OnSetCourseButtonClick`: duplicate set-course suppression seam.

The downstream `FleetBarViewController` guards should be treated as defensive until the upstream native shortcut coverage is audited. Do not add more downstream fleet guards without first proving which native method is escaping the dispatcher.

## Additional Work Needed

1. Audit remaining Scopely native shortcut callbacks.
   - Initial dump-backed audit is in `docs/SCOPELY_NATIVE_SHORTCUT_CALLBACK_AUDIT.md`.
   - Find native methods equivalent to `SelectShip` for chat, panels, zoom, and other shortcut actions.
   - Classify each as `dispatcher-owned`, `native-owned`, or `passthrough`.
   - Add targeted seam hooks only when a consumed dispatcher chord can otherwise trigger native behavior.

2. Move physical-key suppression out of `hotkey_router.cc`.
   - The current helper belongs in a focused module, for example `native_shortcut_guard.h/.cc`.
   - Keep it pure/testable where possible: physical snapshot collection should be platform-specific; suppression decisions should stay in `testable_functions`.

3. Define action ownership metadata.
   - Extend action schema metadata with original-call/native ownership intent.
   - Make `ConsumesOriginalKeyEvent` and native shortcut suppression derive from action metadata instead of one-off guard rules.

4. Finish event-driven input spine spike design.
   - Keep game mutation on safe game-thread seams.
   - Use event collection only to produce chord events and action requests.
   - Do not mutate IL2CPP objects from OS/window input callbacks.

5. Bring zoom into the dispatcher plan.
   - `parts/zoom.cc` still reads `MapKey` directly.
   - The target is a zoom phase executor fed by dispatcher winners, preserving repeat behavior and input-focus gating.

6. Bring right-click/space policy under pure decision tests.
   - `ExecuteSpaceAction` still owns multiple meanings for `SPACE|MOUSE1`.
   - Extract the priority decision before changing live behavior.

7. Retire or formalize downstream fleet guards.
   - If `ShortcutsManager.SelectShip` fully covers native number selection, remove redundant `FleetBarViewController` guards in a later cleanup.
   - If they cover mouse/UI selection too, document the exact scenario and add tests for the guard semantics.

8. Keep instrumentation deliberate.
   - Trace-level dispatcher/native-guard logs are acceptable for live debugging.
   - Avoid recurring info-level keypress logs in normal sessions.

## Test Matrix To Preserve

Pure tests should keep covering:

- Bare `1` selects ship 1.
- `ALT-1` bound to cargo/default consumes the original digit and suppresses native ship selection.
- `ALT-1` rebound to `select_ship1` remains a valid dispatcher-owned ship selection.
- Modified consumed chords persist native suppression while the chord remains held.
- Suppression clears after release.
- Hotkeys disabled clears suppression.
- Modifier-only binds are rejected.
- Exact modifier matching prevents bare and modified actions from both winning.

Manual smoke should cover:

- `ALT-1`, `ALT-2` cargo toggles do not select ships.
- `1`, `2` still select ships.
- Double-tap ship locate still works.
- Cargo display changes are visible immediately on the currently selected target.

## Validation Commands

Narrow checks:

```powershell
git diff --check
xmake build stfc-mod-tests
xmake run stfc-mod-tests
xmake build mods
```

Live smoke when touching native seams:

```powershell
.\.ax\ax.ps1 cycle
```

Use trace/debug logs only when investigating a live key sequence.

## Do Not Change Casually

- Do not re-enable Scopely shortcuts globally to fix one missing native seam.
- Do not hard-code `ALT-1` or any specific user binding in hook logic.
- Do not treat modified ship selection as a conflict when the user explicitly binds it to `select_shipN`.
- Do not move game-object mutation into lower-level OS/window input callbacks.
- Do not collapse zoom, fleet, chat, and cargo executors into a generic junk drawer.
