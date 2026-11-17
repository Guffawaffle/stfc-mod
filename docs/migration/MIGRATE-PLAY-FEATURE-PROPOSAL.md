# Proposed migrate-play-feature workflow

Status: accepted for bounded skill creation and two offline pilots, 2026-10-26
(America/Chicago). The operator approved proceeding after freezing play-dev.
The historical proposal below explains the design; execution evidence belongs in
the linked feature contracts and MIGRATE-PLAY-FEATURE-PILOTS.md, not in its initial
estimates. Implementation uses feature/migrate-play-feature-pilots-20261026 in the
existing target worktree. The archived study branch is preserved. Deployment,
publishing, and the old whole-product restoration sprint remain separate work.

## Exact inspected inputs

- Source repository: Downstream, `D:/dev/stfc-mod`.
- Frozen feature baseline: `frozen/play-dev-20261026`, created from `play-dev` at
  `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Current canonical checkout: `study/alliance-tournament-store-20261026` at
  `a815944d26047c6a7d768a7ade1b52b57d06d6ab`, clean when inspected. Its additional
  Alliance Games study is not part of the pinned play-dev baseline.
- Parked target/reference: `study/ripper-typed-bindings-20261026` at
  `076c85a84ee5d126b02582f55936ccfc4a066a29`, in the existing
  `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream` checkout.
  It was clean before adding this proposal. No branches or worktrees were created
  or switched. The archive is a technical reference, not permission to resume the
  old whole-product restoration sprint.
- Older inventory baseline: `play` at
  `cad98ef7990dd66167eb64ea5dda525c1fe2c296`.

Re-resolve and record revisions at the start of an authorized migration. Branch
names alone are not immutable specifications; a later source advance needs its
own delta assessment rather than silently changing an active contract.
The frozen reference must continue to resolve to the exact source revision above;
if it moves, report the mismatch rather than accepting a newer baseline. Ordinary
play-dev development may continue independently. Later source additions require
an explicit new baseline/delta decision, not advancement of the frozen branch.

## What can be reused

| Existing resource | Useful role | Limit |
| --- | --- | --- |
| `AGENTS.md`, `CONTRIBUTING.md`, `docs/IL2CPP.md` | Target architecture, ownership, builds and binding conventions. | Read current instructions; old recovery prompts are historical evidence, not fresh authority. |
| `docs/migration/TYPED-BINDINGS-STUDY.md` | Retrieval index for checked calls, managed custody, arrays, ABI, callback ownership and hook outcomes. | Includes downstream improvements beyond Ripper's original implementation. Requalify the actual helper and consumer. |
| `docs/migration/play-functional-parity.json` and `PLAY-PARITY.md` | Stable capability IDs, source/target locations, known gaps, exclusions and workflow procedures. | 101 entries target older PLAY: 83 Implemented/unverified, 12 Partial, 4 Missing, 2 intentionally excluded. These are recorded classifications, not newly verified facts. |
| `play-parity-work.json`, `play-parity-order.json` | Historical dependency discovery. | Neither is an execution or verification authority; do not restart the entire old plan. |
| `settings-preservation-baseline.json` | Explicit counting units, required settings and known baseline failures. | It is tied to its own source revision; do not reuse totals as a current play-dev preservation claim. |
| `tests/il2cpp_bindings.cc`, `il2cpp_gc.cc`, `hook_installation.cc`, `installation.cc`, `frame_callbacks.cc` | Existing controlled-runtime and lifecycle verification seams. | Fake metadata and native build success do not qualify exact-client widget behavior or platform ABI. |
| Existing feature-policy and Settings fixtures | Reusable test cases and failure models. | Trace which production path is exercised; pure policy success cannot stand in for a missing native adapter. |
| `scripts/verify-windows-binary.py`, target Zig build/test targets | Native artifact checks and platform-specific build evidence. | Select relevant documented targets; build-step counts are not feature/test counts. |
| Source `tests/run-settings.ps1` and narrow Xmake targets | Source behavior characterization when needed. | Canonical source rebuild currently has a cross-account dependency-cache problem. That does not establish source behavior is broken. |
| AXF context, scoped Lex, explicit runtime-context and existing lifecycle helpers | Continuity and target/artifact observation. | Inspect capability root/arguments first. Legacy Downstream Xmake helpers are not automatically the correct Zig-target execution route. |

Concrete inventory-drift example: DIAG-SCIENCE-ERROR is recorded Missing against
old PLAY, while `game_error_probe.cc` is deleted in pinned play-dev. Discovery must
investigate removal/replacement and intended activation before restoring anything.
No classification is changed by this proposal.

## Packaging recommendation

Put the eventual skill in the target repository:

    .agents/skills/migrate-play-feature/
      SKILL.md
      assets/feature-contract.md

Keep the skill roughly one short procedure, not a duplicate architecture manual.
The template is copied into `docs/migration/features/<stable-id>.md` for a real
migration. Its references point into authoritative target documentation and the
pinned source implementation. Add no plugin, npm package, new orchestration
system, custom installer, or execution script initially.

Repository `.agents/skills` is a supported Codex discovery location. A shared
container chat does not automatically select a sibling repository's skill;
invoke the exact skill path or work from the intended target repository. Do not
maintain independent global and repository copies. Local host discovery still
needs a real test before claiming it works in every client.
Official reference: https://developers.openai.com/codex/skills

Normal skill discovery can remain enabled. Invocation selects a procedure, not
authority to migrate, deploy, switch branches, create worktrees, push or release.
The task's current scope and workspace instructions still govern actions.

Suggested description:

> Investigate, adapt and verify a named play-dev feature in the typed IL2CPP/SPUD
> refactor using a pinned behavioral contract and the repository migration inventory.

## Proposed instruction body, in substance

1. Establish the selected feature, source and target revisions, owning checkout,
   current work, and authorized mode. Inspect repository guidance and existing
   records. In discovery/proposal mode, stop before implementation. Existing
   authorization can cover successive stages; do not invent approval gates after
   each ordinary milestone.
2. Read the complete relevant source implementation, headers, registrations,
   producers, callbacks, consumers, tests and cleanup. Trace configuration,
   availability, persistence, platform paths and shared interactions. Record
   actual reachable behavior separately from dormant code and known defects.
3. Write a versioned feature contract whose numbered obligations have source
   evidence and verification methods. Reconcile any existing target implementation
   before designing changes. A fully covered feature may need verification rather
   than another port.
4. Map each obligation to unchanged reusable logic, a narrow typed/native adapter,
   a missing architectural capability, or an explicit behavior decision. Read the
   applicable target architecture docs and existing helpers. Justify substantial
   rewrites with concrete incompatibilities. Do not add a parallel framework or
   silently replace unavailable observations with defaults.
5. When implementation is authorized, preserve source logic where practical and
   adapt the smallest necessary boundaries. Complete activation, configuration,
   UI, dependencies, lifecycle, disabled and failure paths. Keep shared hook and
   runtime ownership with their established owners. Resolve bounded gaps, or report
   an architectural blocker with evidence and alternatives.
6. Verify every obligation against the actual candidate, including shared-feature
   interactions. Bind checks to source/target identities, current changes, built
   artifacts, toolchain/options and client/platform where relevant. Distinguish
   source inspection, automated tests, build, boot and player-observed behavior.
7. Update the existing inventory and the feature record with changes, evidence,
   unresolved obligations, defects and next action. Completion is an accounting of
   obligations, not a claim that the feature has no bugs. Keep unverifiable behavior
   explicitly unverified. Record the exact resume point without treating a memory
   frame as a substitute for the repository record.

The skill should route to `docs/IL2CPP.md` and feature-owner guidance rather than
embed an API tutorial, guessed member names, architecture snapshots or fixed
platform commands. Tool availability is discovered; LexRunner is optional and
must not be claimed as execution authority when it was not used.

## Feature contract: minimum useful record

Use one compact Markdown record per stable capability or tightly bounded workflow.
Record these fields, expanding only when they matter:

- Identity and scope: stable ID, user workflow, exact source revision, target base
  and candidate, included dependencies, exclusions and current authorization mode.
- Source evidence: relevant complete source units and symbols; activation/defaults,
  config key/type/aliases/save/reload behavior; UI/input semantics; producers and
  consumers; original-call order; resource ownership, reset, teardown and failures.
  Include platform differences and runtime evidence if available. Mark code-derived
  expectations as such when no source runtime trial was performed.
- Obligations table: `ID | expected behavior | source evidence | target mapping |
  implementation disposition | verification method/result | remaining gap`.
- Behavioral findings: known source defect, suspected source defect, target defect,
  intentional change or unresolved expectation. Give evidence, impact and decision
  authority separately from migration coverage.
- Architecture decision: reuse/adapt/rewrite reason, shared-owner integration,
  required binding/helper capabilities and bounded prerequisites.
- Validation identity/results: exact commands and exits, meaningful assertions,
  platform/toolchain/client/input identity, artifact hashes and limitations.
  Preserve baseline failures separately from migration-introduced failures.
- Recovery: current candidate, completed obligations, pending work and next action.

Examples of obligations include gesture precedence over assignment, canonical row
identifiers consumed by native UI, selected-state appearance, source-preserving
settings writes, cancellation and post-close callbacks. A hook/config checklist
alone cannot discover these obligations.

## Inventory evolution: retain one product accounting

Do not create another competing migration inventory. Preserve the existing 101
IDs, old source baseline, exclusions and historical evidence. For the pilot's
selected entry, add a source/candidate-bound assessment and a contract link.
Do not bulk relabel all old entries or silently claim they were reviewed against
play-dev. The Markdown per-feature record holds detailed evidence; the JSON is
its searchable index and current summary. PLAY-PARITY.md links to that record
rather than duplicating its entire evidence history.

Suggested assessment fields (proposal, not an implemented schema):

    source_ref, source_revision, target_base, target_candidate, contract_path
    migration_status: not-started | in-progress | implementation-complete | blocked
    obligation_coverage: total / implemented / intentionally-changed / unresolved
    behavior_findings: [defect-or-change records with evidence and decision]
    verification: static / automated / Windows / macOS-arm64 / macOS-x64 / runtime
    evidence_refs, dependency_blockers, next_action

Each verification entry needs its own status, candidate identity and limitations.
Keep established human-facing classifications as a compatibility summary until an
explicit record-schema revision is approved; do not manufacture equivalence by
mapping one old label automatically into these independent dimensions.

A source bug may be preserved with complete migration coverage and an open defect.
An approved fix may intentionally change behavior while satisfying the agreed
contract. An accidental omission is a regression. An unverified path is neither
a demonstrated regression nor evidence of equivalence. Do not reintroduce a known
unsafe implementation merely to reproduce a bug; identify the explicit change
or blocker and its justification.

No schema migration framework is needed. If the first two pilots demonstrate a
repeatable need, add a small record validator for IDs, required identities, links
and incompatible completion claims. Such a validator cannot prove behavioral
completeness and should not grade wording or counts as semantics.

## Proposed pilots, subject to contract discovery

### 1. Swap Ship double-click and Enter assignment

This pilot is provisional: use it to finish a bounded existing migration and
validate the process, not to promise an undiscovered implementation gap.

Source: `mods/src/patches/parts/double_click_assign_ship.cc`, its header,
`patches/swap_ship_pin_input.h` and its implementation, ShipTile/AssignShips bindings,
settings availability and input consumers. Target: existing
`src/mod/features/dock.cc`, `features/ship_pin_input.*`, game/dock bindings,
shortcut/frame and installation owners.

Why: compact player workflow with native calls, shared-hook ownership, input
precedence and lifecycle behavior; it tests more than a pure helper. Existing
record PLAY-DOCK-ASSIGN is Partial, so this is a completion/reconciliation pilot,
not permission to rewrite its already-present typed implementation.

Qualify pin-badge gestures before native tile/assignment handling; native original
behavior when disabled; same-ship and different-ship clicks; 400 ms window;
triple-click consumption; visible/active assignment control; Enter/keypad Enter,
held-key behavior, text focus; recycled/replaced rows; install availability and
missing native members. The target currently describes one assignment per key
press as an intentional change pending acceptance. Resolve that explicitly rather
than counting it as either accidental loss or automatic improvement.

Do not assume a missing feature merely from the Partial label. If the full source
contract is already covered, the pilot should report that result and concentrate
on verification. Before selecting a second implementation candidate solely to
obtain a diff, choose another independently confirmed bounded gap with the user.

### 2. Loading-image file selection and exact-entry fallback

Source: the loading-image section and PollLoadingImagePicker in
`mods/src/settings/upstream_settings.cc`, `settings/image_file_setting.h`,
`settings/image_path_validator.h`, `settings/image_file_picker.h`, the existing
Windows/macOS implementations in `audio_file_picker_windows.cc` and
`audio_file_picker_mac.mm`, native text-entry callbacks and the settings writer. Target: existing Settings page/value owners, typed callbacks, configured
loading-image consumer and persistence owner. Inventory: SET-IMAGE-PICKER, recorded
Partial; its current target description is image consumption only.

Why: a different integration shape—async platform interaction, native UI,
validation, cancel/close/stale completion, Unicode paths, defaults and persisted
configuration—with a concrete, bounded user outcome. It prevents overfitting the
skill to synchronous hook-only features.

Preserve cancellation and rejected/invalid paths, no unwanted file copying, exact
entry fallback, UI closure/replacement while selection or validation runs, source
key/default/save semantics and truthful restart messaging. Follow both Windows
and macOS implementations; do not label an unavailable Mac trial as a pass.
If missing general input/picker capabilities make this larger than a bounded
pilot, document the specific prerequisite first and use a narrower confirmed
interactive gap; do not silently build an entire Settings framework.

Not first pilots: officer preset ordering/canonical save (larger unresolved native
handoff), Profiles isolation, sync, AAI/Kit wholesale integration or the entire
Settings subsystem. The Alliance Games study is not in pinned play-dev and would
need an explicit alternate source boundary before this workflow could use it.

## Workflow validation after approval

1. Create the small skill/template and validate frontmatter, routing, links and
   invocation behavior with the skill-creator validator and actual host discovery.
   Metadata validation is not evidence that migrations will be complete.
2. Perform the first authorized pilot from source. Record which omissions the
   procedure caught, what it failed to prompt investigation of, and actual overhead.
3. Make only corrections supported by that experience, then use the second,
   different-shaped migration to validate the revised procedure.
4. Test failure cases in the workflow: stale inventory, feature already present,
   removed source probe, known source defect, unavailable binding and successful
   build without native UI evidence. These should produce honest dispositions,
   not fabricated implementation work or success.
5. Consider an independent read-only evaluation when appropriate and authorized.
   Do not introduce orchestration or fanout as a required part of porting a feature.

Skill acceptance: complete source reading remains necessary; every obligation has
a target disposition and verification plan; missing information remains visible;
existing behaviors are not replaced merely for architectural style; unrelated
work stays untouched; state survives a handoff through repository records; a second
migration demonstrates the process works beyond the first example.

## Boundaries for the next assignment

Before actual migration, confirm the chosen feature and intended writable target
branch/checkpoint in the existing worktree. Do not auto-create another worktree or
switch the daily source checkout. Separate a task's implementation authority from
its runtime/deployment/publishing authority. Current assignment ends at this
proposal: source branches, product implementation, inventory and game are unchanged.
