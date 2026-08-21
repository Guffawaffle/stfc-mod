# Probe: Ghost tutorial active-flag repair

- Status: approved
- Owner: Codex / Guffawaffle
- Date: 2026-08-20
- Related patch label: M94-game-255
- Related timeline refresh ID: 20260820-102946Z
- Related diff report: private M94 research diff
- Native seam ledger entry: `ShortcutsManager.LateUpdate` in `docs/NATIVE_SEAM_LEDGER.md`

## Question

Does clearing only `TutorialManager._isActive` restore native shortcuts when the exact orphaned button-step state persists?

## Static Evidence

- Symbol: `Digit.Prime.Tutorial.TutorialManager._isActive`
- Method signature: existing sampled seam `ShortcutsManager.LateUpdate()`; no new detour
- String/script/config evidence: `ShortcutsManager.get_CanUseShortcuts()` rejects all native shortcuts while
  `TutorialManager.IsActive` is true.
- Old/new diff context: M94 game-255 introduced keybinding-tutorial scaffolding, but its two named methods currently
  resolve to the shared empty-return stub. Runtime instead identifies an ordinary active mission and an orphaned
  `ActionFlow` button step.
- Why this target is the narrowest candidate: the static flag is the sole persistent false predicate in
  `CanUseShortcuts`; clearing it avoids invoking tutorial completion delegates or mutating mission/objective data.

Runtime evidence: the orphan signature persists across cold starts with mission `1463528981`, button action
`-1401001831`, no objective/items/component/UI/end-step/next-step, step index 0, and target section -1. Disabling the
game's mission-reminder-on-start option did not alter the state. Static disassembly shows `FinishTutorialStep()`
increments the index and dereferences `_currentObjectiveTutorialItems`, so it is not valid while that field is null.

## Risk

- Risk class: R5
- Confidence rung: state-correlated
- Payload confidence: exact fields and scalar values are runtime-observed
- Original/trampoline confidence: existing `ShortcutsManager.LateUpdate` seam is operationally relied on
- Behavior change expected: yes

## Implementation Plan

- Module/file: `mods/src/patches/parts/hotkeys.cc`
- Config or compile guard: diagnostic compile constant plus `advanced_diagnostics.hotkey_suppression_logging`
- Hook descriptor name: existing `kShortcutLateUpdateHook`
- Target assembly: `Assembly-CSharp`
- Target namespace: `Digit.Prime.GameInput`
- Target class: `ShortcutsManager`
- Target method: `LateUpdate`
- Install path: existing `InstallHotkeyHooks()` registration
- Log tag or event kind: `[Hotkeys] ghost tutorial active-flag repair`

Registry requirements:

- Reuse the existing `HookDescriptor` and `HookModuleHealth` registration.
- Do not install a new detour.
- Require the orphan signature to persist across two diagnostic samples.

## Disable Path

- Flag or code path to disable: set `kEnableGhostTutorialActiveFlagRepairProbe=false` or
  `advanced_diagnostics.hotkey_suppression_logging=false`.
- File/entry to delete if it crashes: repair function and call in `mods/src/patches/parts/hotkeys.cc`
- Expected boot log when disabled: no `ghost tutorial active-flag repair` entry

## Human Smoke Test

Goal: prove that the stranded `_isActive` flag alone causes the native shortcut failure.

Steps:

1. Cold-launch without clearing cache.
2. Wait for the repair log to report `can_use_shortcuts_after=true`.
3. Press Q and E once each.

Expected log marker/event: one repair entry with `tutorial_active_after=false` and `can_use_shortcuts_after=true`.

Stop immediately if: the game crashes, a real tutorial UI is visible, or a dialog/input field is active.

Report back: whether both native view-change hotkeys work and whether any visible tutorial or mission UI changed.

## Result

- Build/deploy command:
- Runtime command:
- Human action performed:
- Observed log/event evidence:
- Crash/hang/recovery notes:
- Answer to the question:

## Exit Decision

Revise

Next action: if causal, replace the compile canary with an explicit temporary workaround setting and review the exact
structural guard before promotion.
