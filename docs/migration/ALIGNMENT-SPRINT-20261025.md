# Remaining PLAY alignment sprint

This is an execution/evidence record. [PLAY-PARITY.md](PLAY-PARITY.md) and
[play-functional-parity.json](play-functional-parity.json) remain the preservation authority.
Overall functional parity is not established.

## Stopped by operator — SPUD-only alternative study

Guff stopped further Ripper-foundation parity implementation. The source checkpoint
is `c9c03ac21999b2e9cd056f64ecbed54c96c501fb` on
`review/ripper-play-migration-checkpoint-20261025`. Do not continue restoring or
polishing this migration without a new instruction. The next assignment is a study
of updating SPUD directly in pinned PLAY, preserving PLAY's existing implementation.
No changes to PLAY, deployment, game cycles, Runner orchestration or fanout are part
of that study.

The player confirmed that Mod Settings now loads. This is not complete UI parity:
the redundant Search settings command remains beneath the inline search bar;
subsection header backgrounds are missing; selected choice rows use the heavy
native white treatment; the chord-entry workflow was reported not to work.
Requested appearance is dark headers, a subtler intermediate selected treatment,
and unchanged unselected rows. These are regressions or unresolved behaviors from
PLAY, not accepted redesigns. Their fixes have **not** been implemented.

The installed candidate is the Profiles-enabled Windows DLL from `c9c03ac2`, SHA256
`1867d0fe6e37accc1955f353a82f258b183591da1ae40e90ee902937d9f87be9`.
It booted with 67/67 groups, and its log confirmed publication of 175 Settings pages.
Local regression was 239/239 steps; that does not qualify the reported UI regressions.
No primary TOML changes were made by the agent; cycles preserved its prior bytes.

Unfinished implementation slices remain recoverable in stash
`0fa6d6edd6f359187ac6957b705034e4a108a19a`; do not apply it as part of the SPUD study.
Unrelated development-install tooling edits in build.zig, scripts/dev-install*, their
tests and zig/dev_install_guard.zig remain outside this checkpoint. No branch push,
merge, release or active-branch replacement was performed at this stopping point.

Current methodology: LexRunner orchestration, implementation fanout and autonomous
task decomposition are paused at Guff's request. Continue directly in this checkout
from the complete pinned PLAY source; adapt necessary integration boundaries rather
than reconstructing behavior from this inventory. Earlier Runner receipts remain
historical evidence only. No additional infrastructure is needed for this change.

## Inputs and isolation

- PLAY behavior oracle: `cad98ef7990dd66167eb64ea5dda525c1fe2c296`.
- Sprint starting commit: `7b6e1d109171350f274a7f7ac8efbda08eb3f065`.
- Approved checkout: `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`,
  branch `review/ripper-play-migration-checkpoint-20261025`.
- Existing development-install changes in `build.zig`, `scripts/dev-install*`,
  their fixtures and `zig/dev_install_guard.zig` are unrelated and preserved.
- No CI/PR conflict work, branch promotion, system orbit, AAI/Kit expansion or store migration.

## Runner use

LexRunner 2.5.0 created the seven-node dependency plan at
`D:/dev/stfc-workspace/.ax/play-alignment-sprint-20261025/plan/plan.json`.
`merge_order` returned reconciliation; independent gameplay and input ownership;
native Settings adapters; catalog/tools; lifecycle/platform closure; final reconciliation.
This is a local implementation plan, not permission to execute merges.

The existing-checkout Attempt preflight returned `windows_native_boundary_unavailable`,
`broker_required`, with physical containment unavailable. No Attempt was started and
no checkout/security changes were made to force it. Implementation and parallel review
are direct collaboration; only planning and durable verification are Runner work.

The first durable gate rejected a changing worktree with `GATE_CANDIDATE_CHANGED`.
This is useful protection, not a passing test or an AAI/product failure. Intermediate
direct builds guide corrections; publication-quality gates require a frozen candidate.
Gate operations, logs and manual execution state live under the same `.ax` sprint directory.

## Restored boundaries under qualification

- Keyboard diagnostics publish immutable layout/status/reason generations to the
  existing accepted runtime-vars owner. Primary TOML remains untouched. Parsing and
  key queries do no diagnostic I/O. Failed writes retain the generation for retry.
  The generated TOML generation is a decimal string, preserving the unsigned value.
- Shortcut capture has one process-lifetime owner, replacement tokens, focus/release
  gating and normal native Update delivery. Per-action editing reuses the existing
  parser, action catalog, layout resolution and dispatcher; reloads invalidate drafts.
- Discovery Shift+configured-ship selection uses the exact seven-argument PlanCourse,
  native coroutine scheduling, immutable destination custody and fresh-course checks.
  Real managed callback construction replaces the unsafe legacy callback layout.
- Native Settings uses the existing models and registered-key writer. Pages are
  staged off-screen and publish an exact, query-owned root. Boolean, selection,
  slider and command adapters keep native context and callback ownership distinct.
  Canonical exact-number editors remain separate from convenience slider ranges.
- The officer-preset handoff experiment is deliberately inactive. Its deterministic
  tests do not qualify native canonical writes or renderer repair. P15 remains open.

## Camera finding

No new speed/momentum control was invented. PLAY's `system_pan_momentum` has no
gameplay consumer; its own settings catalog calls it a legacy input. Active glide
already uses `system_pan_momentum_falloff` in the migration. PLAY can apply a second
movement during keyboard/coasting frames. The existing matched-runtime feel comparison
remains necessary before deciding whether that difference should change.

## Qualification discipline

The frozen Windows candidate passed `mods test-mod test-il2cpp test-sync
test-settings-models` with ReleaseFast product code and Debug/assertion-enabled
fixtures: **239/239 build/run steps**. This is a graph count, not 239 features.
Evidence: `.ax/play-alignment-sprint-20261025/candidate-build-7.log` in the shared
workspace. Earlier failures are retained: unsigned TOML generation, incomplete
shortcut-editor ownership, native template syntax and fixture include wiring
were corrected before this pass. This command builds the native static mod;
it does not establish a deployed DLL or gameplay compatibility.

The independent final source audit also restored cancellation of pending native
screen actions when capture begins, and live fleet-label refresh/Native restoration.
Preset native integration, custom media pickers and debug surfaces remain open.

Local fixture execution, native compilation, boot smoke and player-confirmed behavior
are separate claims. New Settings source is not complete Settings parity merely because
its model fixture passes. Search, editors, custom media, native lifecycle and platform
coverage remain separately tracked. Exact final source, gate receipts, installed bytes
and any trial results must be added after they actually exist.

## Native Settings trial corrections

The first packaged candidate, `7a16ca0f`, booted but Actions/Pages/Search declined
because GenericButtonWidget exposes `get_Interactable`, not Unity's lower-case
`get_interactable`. Correction `0291d40f` installed all 67 feature groups. The
player then confirmed that Mod Settings still did not appear. Opening the native
Settings screen exposed the next construction failure: SliderOptionContext's
LabelType accessors were invoked through an OptionContext facade. The off-screen
transaction correctly refused to publish the incomplete menu.

The slider adapter now invokes a concrete, receiver-checked ConfigureLabel binding
and verifies the setter with a successful matching readback. Controlled-runtime
tests exercise both label values (including valid zero), mismatched/failed readback,
an incorrect receiver and an unsupported label kind. Page construction failures
also identify the page and control without publishing a partial tree.

The correction passed Windows `mods test-il2cpp` (84/84 build/run steps), with
Debug fixture assertions; see `slider-label-correction-build.log` in the sprint
evidence directory. This is not yet a player-verified menu. Neither cycle changed
the primary TOML (SHA256 `78c248ce73fad72394405765db17fb95b5ee454c8be51fc8eddcf482b7f4f6f1`).
An independent read-only audit found no further confirmed base-facade/native-member
mismatch across the current Settings adapters. Native layout and interactions still
require qualification. Unfinished next slices remain in scoped stash
`0fa6d6edd6f359187ac6957b705034e4a108a19a`; unrelated development-install work was
excluded. Restore with `apply`, retain the stash, after the current menu trial.

The `11dcf83e` DLL passed its Windows build/PE checks and booted with 67/67 groups.
Direct opening of native Settings then exposed a distinct construction refusal at
Travel's `community_mod.ui.auto_confirm_instant_warp` choice. The game was left
running; this is not a successful Mod Settings trial. Computer Use was stopped on
physical Escape, with no further UI input in that trial.

## Source-preserving Settings correction

The complete pinned PLAY `native/value_widgets.cc` and its metadata/lifetime headers,
`native/interop.h/.cc`, relevant page-bind call sites and existing models were read
directly. The five files `choice_setting.h`, `slider_setting.h`, `native_view_state.h`,
`value_settings.h` and `value_view.h` are unchanged from PLAY (line-ending normalized).
The port's missed operations are at the native integration boundary, not in those models.

| PLAY operation | Adaptation / remaining qualification |
| --- | --- |
| AddChoiceRows checks native Index, then rewrites existing LabelContext.Identifier to item_id | Restored in concrete ConfigureIdentity, after exact callback custody; checked readback preserves valid Index zero and unavailable observations. Retained273 AddSelection formats supplied value strings, so the input is not already the final identifier. |
| Track detaches an owned multi-select from its native exclusive ToggleGroup | Restored with the existing weak view ownership and checked native setters. |
| Clear restores the native group and interactability before pooling | Restored only for the same owned component; a replacement owner's state is left alone, and failed observations retain cleanup responsibility. |
| ChangeValue rejects multi-select writes during native panel refresh | Restored before the settings mutation path. The existing outer OnDidBindContext render also synchronizes the toggle after native binding. |
| Slider uses concrete LabelType | Corrected in 11dcf83e; retained in this candidate. |

Windows `mods test-mod test-il2cpp test-sync test-settings-models` passed **239/239**
build/run steps, with ReleaseFast product code and Debug/assertion-enabled fixtures:
`settings-source-preservation-build2.log`. Earlier fixture failures are preserved;
the controlled LocaleTextContext lacked PropertyInfo entries used by HasLabel, and
was corrected without weakening production checks. The fixture main now reports
failed requirements instead of terminating without a useful diagnostic.

These checks do not prove native group restoration, rendered control appearance,
search, modal editing or persistence. The next trial must open the actual menu,
visit a choice/slider page, verify independent selections, leave/re-enter it, use
search and cancel an exact numeric edit. PLAY's inline Edit presentation, choice
styling, custom text/media controls and the remaining catalog workflows are still
part of reconciliation, not silently counted as equivalent.
