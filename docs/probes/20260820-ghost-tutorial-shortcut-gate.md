# Probe: Ghost tutorial shortcut gate

- Status: approved
- Owner: Codex / Guffawaffle
- Date: 2026-08-20
- Related patch label: M94-game-255
- Related timeline refresh ID: 20260820-102946Z
- Related diff report: private M94 research diff
- Native seam ledger entry: `ShortcutsManager.LateUpdate` in `docs/NATIVE_SEAM_LEDGER.md`

## Question

Which concrete tutorial mission, data object, and action-flow step keep `TutorialManager.IsActive` true after a cold
start while no tutorial UI or component exists?

## Static Evidence

- Symbol: `Digit.Prime.Tutorial.TutorialManager`
- Method signature: existing sampled seam `ShortcutsManager.LateUpdate()`; no new detour
- String/script/config evidence: M94 game-255 contains `tutorial_seen_modifyKeybindingsOneStep`,
  `CheckForKeybindingTutorial()`, and `ClearKeybindingOneStepTutorialFlag()`.
- Old/new diff context: the canonical verified M94 game-255 corpus was refreshed on 2026-08-20. The two keybinding
  tutorial methods currently resolve to the shared empty-return stub, so the literal alone does not identify the
  stranded runtime state.
- Why this target is the narrowest candidate: the existing read-only sampler already observes the exact frame where
  `CanUseShortcuts` changes. Expanding that sample avoids another hook and can identify the objects responsible.

Runtime evidence before this probe: after cold-start section entry, the native Q/E actions are enabled but
`CanUseShortcuts` remains false. The only persistent blocking predicate is `TutorialManager.IsActive=true`; input is
not blocked, tutorial UI is closed, and `_currentComponent` is null while mission, data, and step pointers remain set.

## Risk

- Risk class: R0
- Confidence rung: state-correlated
- Payload confidence: class identity and bounded scalar/string fields only
- Original/trampoline confidence: existing `ShortcutsManager.LateUpdate` seam is operationally relied on
- Behavior change expected: no

## Implementation Plan

- Module/file: `mods/src/patches/parts/hotkeys.cc`
- Config or compile guard: `advanced_diagnostics.hotkey_suppression_logging`
- Hook descriptor name: existing `kShortcutLateUpdateHook`
- Target assembly: `Assembly-CSharp`
- Target namespace: `Digit.Prime.GameInput`
- Target class: `ShortcutsManager`
- Target method: `LateUpdate`
- Install path: existing `InstallHotkeyHooks()` registration
- Log tag or event kind: `[Hotkeys] ghost tutorial object`

Registry requirements:

- Reuse the existing `HookDescriptor` and `HookModuleHealth` registration.
- Do not install a new detour.
- Do not interpret pointer payloads beyond verified IL2CPP object identity and bounded known fields.

## Disable Path

- Flag or code path to disable: set `advanced_diagnostics.hotkey_suppression_logging=false` or remove the bounded
  object dump from the existing sampler.
- File/entry to delete if it crashes: bounded diagnostic additions in `mods/src/patches/parts/hotkeys.cc`
- Expected boot log when disabled: no `native shortcut state` or `ghost tutorial object` entries

## Human Smoke Test

Goal: identify the tutorial objects that strand the native shortcut gate on a cold start.

Steps:

1. Launch normally without clearing cache.
2. Wait for station/system section entry and the native shortcut actions to become enabled.
3. Press Q and E once each and report whether the native view changes occur.

Expected log marker/event: one bounded object dump when `tutorial_active=true`, `tutorial_component=null`, and
`can_use_shortcuts=false`.

Stop immediately if: the game crashes before the installed-mod banner, hangs, or emits the object dump repeatedly.

Report back: whether Q/E worked and whether any tutorial UI was visible.

## Result

- Build/deploy command:
- Runtime command:
- Human action performed:
- Observed log/event evidence:
- Crash/hang/recovery notes: a separate direct `get_CanUseShortcuts` detour crashed during hook installation and was
  removed; this probe deliberately reuses the stable read-only seam.
- Answer to the question:

## Exit Decision

Revise

Next action: use the identified concrete tutorial state to select Digit's narrowest existing cleanup path, then test
that cleanup as a separate behavior-changing probe.
