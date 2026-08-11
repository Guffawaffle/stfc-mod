# Probe: Current mission HUD button visibility

- Status: release-promoted
- Owner: Guffawaffle / stfc-mod
- Date: 2026-07-31
- Related patch label: mission-hud-current-buttons
- Related timeline refresh ID: 2026-07-30 current-client corpus
- Related diff report: N/A
- Native seam ledger entry: `Digit.Prime.HUD.MissionsHudViewController` current-button visibility lifecycle

## Question

Can the mission HUD visibility feature be repaired for the current client without retaining a dead Daily Goals
target or depending on the removed common `UpdateButtons()` method?

## Static Evidence

- Symbol: `Digit.Prime.HUD.MissionsHudViewController`
- Current fields:
  - `_missionsButton` at offset `0x38`
  - `_achievementsButton` at offset `0x40`
  - `_outpostsButton` at offset `0x58`
  - `_challengesButton` at offset `0x60`
- Current lifecycle/refresh methods:
  - `void OnEnable()` at RVA `0x13EB680`
  - `void HandleOutpostsAndChallengesHUD()` at RVA `0x13ED8F0`
- Removed surface: the current corpus contains neither `_dailyGoalsButton` nor `UpdateButtons()` on this controller.
  Daily Goals navigation still exists elsewhere, but it is not a current mission HUD button.
- Why these targets are bounded: `OnEnable()` establishes visibility after the controller becomes active. The
  dedicated outpost/challenge refresh is the only current controller method named for recalculating those two
  buttons, so reapplying after it prevents that native refresh from undoing configured Q Trials or Outposts policy.

## Risk

- Risk class: R5
- Confidence rung: state-correlated for `OnEnable()`; runtime observed for the dedicated refresh hook
- Payload confidence: typed managed controller with no additional hook parameters
- Original/trampoline confidence: observed returning successfully for `OnEnable()`; each hook calls its original
  exactly once before applying the configured state
- Behavior change expected: yes, only for buttons configured away from `auto`

## Implementation

- Module/file: `mods/src/patches/parts/mission_hud_tweaks.cc`
- Config: `[ui.mission_hud]` with `q_trials`, `field_training`, `outposts`, and `missions`
- Defaults: all four are `auto`; when all remain `auto`, the patch module is skipped and no detour is installed
- Hook descriptors:
  - `MissionsHudViewController.OnEnable`
  - `MissionsHudViewController.HandleOutpostsAndChallengesHUD`
- Refresh scope: the second hook installs only when Q Trials or Outposts has a non-`auto` override
- Removed config: `daily_goals` is ignored with a warning because no current HUD component exists to control
- Registry path: both hooks use `HookDescriptor`, `HookModuleHealth`, and `HOOK_REGISTRY_SPUD_STATIC_DETOUR`

## Runtime Evidence

- Build/deploy mode: Windows release
- Initial experiment: replacing the missing `UpdateButtons()` detour with `OnEnable()` installed cleanly.
- Human smoke evidence:
  - `q_trials = "never"` hid Q Trials.
  - `q_trials = "auto"` with `outposts = "always"` left all three buttons visible in the observed HUD state.
- Disable path: all current keys at `auto` leaves native visibility behavior unchanged and skips both hooks.
- Final enabled-path evidence: the exact dirty-source Windows release cycle resolved and installed both current
  methods (`installed=2 failed=0 skipped=0 total=2`), and `PatchAudit` reported `MissionHudTweaks` consistent.
- Retired-key evidence: the same cycle logged that `ui.mission_hud.daily_goals` is unsupported and ignored. After
  removing that key and returning all four current keys to `auto`, a second clean cycle skipped the module, installed
  no mission HUD hooks, emitted no Daily Goals warning, and again reported the module consistent.
- Artifact evidence: the built and deployed release DLLs had matching SHA-256
  `3336A94265097AC63E588A693ADE0FD9FCFE669D1081287D193E5BF52FA7316E`.

## Exit Decision

Promote the current four-button repair. Retire Daily Goals from the supported schema rather than pretending its
removed HUD object can still be controlled. The dedicated refresh seam is runtime-observed and has a bounded install
condition; a later human state-change smoke can raise its behavioral confidence without blocking this repair.
