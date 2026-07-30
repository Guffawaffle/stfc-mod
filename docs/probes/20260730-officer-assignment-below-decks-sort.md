# Probe: Restore Below Deck Ability officer-assignment sort

- Status: observed
- Owner: `experiment/officer-sorting-dropdown`
- Date: 2026-07-30
- Related patch label: `OfficerAssignmentSortExperiment`
- Related timeline refresh ID: 2026-07-30 client corpus refresh
- Related diff report: local research diff for the 2026-07-30 client
- Native seam ledger entry: `Digit.Prime.Officers.OfficerSortGenerators.InitializeAssignmentSorters()`

## Question

Can the removed Below Deck Ability sort in Manage Ship's Assign Officers view be restored by adding its still-present
comparator through the game's existing assignment-sort generator?

## Static Evidence

- Symbol: `Digit.Prime.Officers.OfficerSortGenerators.InitializeAssignmentSorters`
- Method signature: `void()`
- String/script/config evidence: the current corpus still contains
  `SortingPredicates.OfficerSortByBelowDeckAbilityAscending(object, object)` and the `Below Deck Ability` label.
- Old/new diff context: the prior corpus had `_assignBelowDeckAbilityId`; the current corpus removed that serialized
  field but retained both comparator methods and the assignment-sort construction helpers.
- Why this target is the narrowest candidate: one post-initialization hook can call the existing private
  `StandardAssignmentSortFunction` and `AddAssignmentSorter` methods without intercepting dropdown selection, officer
  payloads, or the new filter popup.

## Risk

- Risk class: R5
- Confidence rung: runtime-observed managed mutation plus human-confirmed dropdown behavior
- Payload confidence: no callback payload; the hook only receives the `OfficerSortGenerators` instance.
- Original/trampoline confidence: observed returning successfully during client initialization.
- Behavior change expected: yes

## Implementation Plan

- Module/file: `mods/src/patches/parts/officer_assignment_sort_experiment.cc`
- Config or compile guard: `[ui].restore_below_decks_assignment_sort`, default `false`
- Hook descriptor name: `OfficerSortGenerators.InitializeAssignmentSorters`
- Target assembly: `Assembly-CSharp`
- Target namespace: `Digit.Prime.Officers`
- Target class: `OfficerSortGenerators`
- Target method: `InitializeAssignmentSorters`
- Install path: `InstallOfficerAssignmentSortExperimentHooks`
- Log tag or event kind: `[OfficerAssignmentSortExperiment]`

Registry requirements:

- Use `HookDescriptor`.
- Use `HookModuleHealth`.
- Use `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- Do not use raw `SPUD_STATIC_DETOUR`.

## Disable Path

- Flag or code path to disable: set `ui.restore_below_decks_assignment_sort = false`.
- File/entry to delete if it crashes: remove the `OfficerAssignmentSortExperiment` patch entry and
  `officer_assignment_sort_experiment.cc`.
- Expected boot log when disabled: patch audit reports `OfficerAssignmentSortExperiment` as not requested; no hook is
  installed.

## Human Smoke Test

Goal: verify that the option appears in Manage Ship's Assign Officers view and performs the original below-decks
ordering.

Steps:

1. Enable `ui.restore_below_decks_assignment_sort`.
2. Deploy and start the game.
3. Open Manage Ship, choose Assign Officers, and open that view's sort dropdown.
4. Select `BELOW DECK ABILITY`, then toggle ascending/descending.
5. Compare officers with and without below-decks abilities.

Expected log marker/event:
`[OfficerAssignmentSortExperiment] restored 'Below Deck Ability' assignment sort`.

Stop immediately if: the game crashes or hangs during boot/officer-screen entry, the dropdown stops opening, or
selecting another ordinary sort stops working.

Report back: option visibility, label rendering, selected ordering, direction behavior, and any crash/hang.

## Result

- Build/deploy command: AXF `global.stfc-mod-private.cycle` with `build-mode=releasedbg`.
- Runtime command: normal game launch through the AX cycle with local
  `[ui].restore_below_decks_assignment_sort = true`.
- Human action performed: the user opened Manage Ship's Assign Officers sort dropdown and reported the restored option
  works.
- Observed log/event evidence: the hook installed once, borrowed the concrete comparator delegate type from assignment
  option zero, and appended option nine.
- Label evidence: the option initially rendered the generated localization key beginning with
  `officer_assignment_s...`. A bounded runtime dump showed every existing assignment option uses a leading-underscore
  display suffix (`_strength`, `_class`, `_ship`, and so on), while its nested `SortFunction` display key is empty.
  Supplying `_below_deck_ability` lets the assignment widget resolve its retained
  `officer_assignment_sort_below_deck_ability` label normally. The user confirmed the dropdown now renders
  `BELOW DECK ABILITY`; no localization hook or post-construction field mutation is required.
- Crash/hang/recovery notes: the game remained responsive after the hook and managed assignment-list mutation.
- Answer to the question: yes. The retained comparator successfully produced an assignment-specific `SortFunction`,
  the game appended it to the intended options list, and the user confirmed the option works.

## Exit Decision

Retain as a default-off science experiment with the working localized label.

Next action: collect wider artifact feedback before considering promotion beyond a default-off science experiment.
