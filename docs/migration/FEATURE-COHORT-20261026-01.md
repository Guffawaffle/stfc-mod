# Frozen PLAY feature cohort 20261026-01

2026-10-26, America/Chicago. Four bounded source-preserving migrations following
the accepted `migrate-play-feature` procedure. Implementation remains in the
existing `feature/migrate-play-feature-pilots-20261026` worktree. This does not
resume the entire parity sprint or authorize branch promotion.

Subsequent operator direction promotes this accepted development checkpoint to
`play-dev-ripper`, by renaming the active branch in that same checkout. Preserve
the distinction: launch accepted, player testing partial, Settings improvements
still deferred. See [active branch routing](PLAY-PARITY.md#active-ripper-migration-branch--2026-10-26).

## Exact inputs and catalog

- Source: `frozen/play-dev-20261026` = `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target base: `05ee3d7f66eec4d9cb39f8924d012a7ca1ec5952`.
- Inspected local play: `cad98ef7990dd66167eb64ea5dda525c1fe2c296`.
- Inspected local upstream dev: `6ceb892baade31d907071db7bf72374902c8e668`.
- Preserved study archive: `076c85a84ee5d126b02582f55936ccfc4a066a29`.

These are local revision observations, not assertions about current remote heads.
No source ref was advanced, other checkout switched, or new worktree created.

The [JSON inventory](play-functional-parity.json) is the sole status source. Its
generated [feature catalog](FEATURE-CATALOG.md) shows branch presence, entry
points and current qualification. Full metadata includes source paths/symbols,
parent capability, canonical keys, platforms, contracts and validation limits.
Generate with `python scripts/render-feature-catalog.py`; `--check` checks freshness,
unique IDs, pinned revisions and contract existence. It does not establish complete
discovery or functional equivalence.

The existing 101 entries were retained. Three newly distinguished Settings
subfeatures were registered on discovery. The incidental
`SET-AAI-ANALYSIS-ENTRY` was also recorded immediately, but remains deferred and
unassessed: it is not a fifth migration. Total inventory: 105 entries. Only the
two pilots and these four have the new branch-presence assessment; absent metadata
elsewhere means not assessed, not absent from a branch.

## Four completed source scans and adaptations

Each scan read the complete selected producer, supporting headers, registration,
models, configuration, consumers, lifecycle and tests at the frozen revision.
The common list-editor source block was read once and traced into each consumer.
Read-only branch provenance and correction review were delegated separately;
implementation stayed in one continuous foreground session. No LexRunner Run,
Attempt or orchestration is claimed.

| Contract | Preserved source and narrow target adaptation | Existing consumer retained |
| --- | --- | --- |
| [SET-WARP-FILTERS](features/SET-WARP-FILTERS.md) | Source ExactEntry behavior and strict comma-list validation; adapt normalized strings to existing typed ShipFilter word vectors, shared page-visit guard and preserving writer. Three canonical keys, wildcard and precedence retained. | `src/mod/features/instant_warp.cc` and its Ctrl/Command/default routing. |
| [SET-DAILY-FACTIONS](features/SET-DAILY-FACTIONS.md) | Existing multiple-choice control reconciled with source: empty/all displays all three; all persists blank, none persists explicit `none`. No replacement claims implementation. | `src/mod/features/missions.cc`, including non-main-faction eligibility. |
| [SET-BANNER-FILTERS](features/SET-BANNER-FILTERS.md) | Source exact editors, strict validation, defaults/save/reset; use the single existing banner catalog. Keep hidden-banner and desktop-notification lists independent, and source platform scope. | Existing typed Toasts delivery/audio/suppression order. |
| [SET-AUDIO-PICKER](features/SET-AUDIO-PICKER.md) | Preserve source alert/page state machine, async owned tasks, persistence, custom reuse and preview. Adapt nested Config, lazy shared frame delivery and narrow readiness/refresh exports. Share the existing image/audio platform dialog providers. | Existing bounded file decoder, prepared playback and fleet activation. |

### Integration decisions

- No new game hook, frame owner, decoder, playback engine, watcher or Settings
  framework. Pure models and preserving registered-key writer remain their owners.
- Strict editor validation is distinct from tolerant manual TOML startup. Route
  reading shares its parser but ignores a false result as before; faction/banner
  startup warning/default behavior is not replaced by the strict editor parser.
- Source banner `All` has a sentinel plus expanded types. Target already expands
  concrete known types. Preserve the existing target representation and test its
  startup equivalence; do not invent future-state wildcard behavior.
- File chooser/loader workers own paths, futures and prepared audio. Config,
  settings models and native page refresh remain on the game thread. Only one
  pending file operation is admitted. Preview reuses prepared audio without decode.
- An external chooser is process-level explicit intent, as in source; page departure
  alone does not cancel it. No borrowed Unity object crosses the worker boundary.
- A completed sound cannot publish over an observed newer cue, explicit reset/retry,
  or unavailable owner. Pending Custom admission is not falsely shown as an applied
  cue. Computation may finish after publication is revoked. These are explicit
  safety improvements, not a claim of byte-identical legacy behavior.
- The generation guard covers observed replacement and explicit editor intent,
  not unobserved external A→B→A changes or authenticated account epochs.
- No configuration keys/defaults were invented, stores migrated, TOML edited or
  reward claims executed. No AAI/Kit feature expansion, UI redesign or CI work.

## Local verification

Toolchain: Zig 0.16.0, VS 18 amd64 environment, Windows x86_64 MSVC ABI. Existing
`D:/dev/stfc-workspace/.ax/play-alignment-sprint-20261025/Run-LocalGate.ps1`
with `-Targets 'test-mod test-il2cpp'`: **210/210 build/run steps passed**, production
libraries ReleaseFast and assertion fixtures Debug. This is a step count, not a
claim of 210 tests. Log: `D:/dev/stfc-workspace/.ax/feature-cohort-01-tests-20261026.log`.

- `tests/config_settings_test.cc` exercises production catalog/model/writer
  adapters: route normalization/wildcards, stale/page-departed commits, reset,
  canonical keys, all eight faction masks and actual startup parse round-trips,
  banner canonicalization/All/invalid input and independent allowlist values.
- `tests/audio_settings_test.cc` uses the production alert producer and source
  positives plus delayed/cancelled/failed dialog/decoder completion, observed ABA,
  unchanged-default reset, reset during blocked decoding, lazy frame admission,
  owner gates, prepared custom reuse and preview without a new decode. The blocked
  test worker automatically releases within three seconds; no main-thread wait.
- Registration stays inert: fixtures check that constructing the catalog does
  not acquire configuration, game objects, frame service, files or audio devices.
- Read-only correction review found missing Toasts readiness composition and
  observed ABA publication risk; both were fixed and covered. Correction-delta
  review reported no residual findings. This is scoped review, not a general PR gate.
- Native pre-check: `zig build mods -Dtarget=x86_64-windows-msvc
  -Doptimize=ReleaseFast -j4 --summary all`, **23/23 steps passed**. A final source-bound
  DLL/receipt is recorded below after the implementation checkpoint; this initial
  success alone does not identify a deployable candidate.
- Skill validator, generated-catalog check and `git diff --check` passed.

No local macOS SDK work: operator-directed qualification is CI for both supported
architectures, then Mcohall for native testing. No macOS compile/runtime success
is claimed. Synthetic host adapters cannot prove COM/Cocoa dialogs, audible output,
native focus/Back, actual course execution, rewards or notification delivery.

## Focused in-game qualification

Use the exact candidate/artifact below. First open Mod Settings normally; test
without changing unrelated preferences. Restore deliberate test edits afterward.

1. Map & Travel → Ship route filters: open each editor, enter a ship name or `*`,
   inspect normalization; reject mixed wildcard/unknown syntax; cancel/back; check
   source precedence separately on an intended trip. Browse must not write TOML.
2. Claims & Confirmations: independently toggle faction choices, verify reopening
   and none/all semantics. No real reward collection is necessary for UI acceptance.
3. Notifications: open hidden banners and (Windows) desktop allowlist; validate,
   cancel, edit and reset one list without changing the other. Actual banner/audio/
   desktop delivery needs a separately observed triggering event.
4. Audio Alerts: preview a built-in, choose/cancel a local supported sound file,
   load/preview Custom, return to built-in/custom without reloading, and reset while
   selection is pending. Check native dialog and Unicode path behavior. Test fleet
   alert settings only when their real owner is available. Do not trigger external
   network messages or gameplay actions merely to satisfy a fixture.

Whole-product parity remains false. Native interaction, failure paths and platform
qualification must not be inferred from compilation or the previous pilot trial.

## Checkpoint and recovery

Implementation commit: `37e298a397fe4f56f0026e1c9dc6e05612ea69fe`. Subsequent
inventory and qualification notes are documentation only; production/build/test
inputs were checked unchanged from that commit.

Final Windows default DLL build: **37/37 steps passed**. This is the default install
target, separate from the 23-step `mods` static-library build. Candidate:
`zig-out/feature-cohort-37e298a3/bin/version.dll`, SHA-256
`916850e10de7fb13ac86e5faf0af9c7db57e8a64a8a8402a61ec3b0fcb183939`.
`scripts/verify-windows-binary.py` verified x86_64, exactly 16 proxy exports, static
CRT and the complete embedded identity: source/base `37e298a3...`, build
`feature-cohort-20261026-01`, mode `release`, channel `study`.

Post-commit `test-mod test-il2cpp` gate again passed 210/210 steps. The three
current audio, config-settings and owned-task fixture executables were explicitly
re-executed successfully; their paths, selection method, hashes and outputs are
retained in `D:/dev/stfc-workspace/.ax/feature-cohort-37e298a3-receipt.json`, SHA-256
`319efa15715dc6afbe5e2ef3042d9f0a3f55286b711f6c9abe36e424be6afdb1`.
The receipt records toolchain/input hashes and individual build/test log identities.
One initial identity-option invocation was rejected during configure; no artifact
was produced. The successful final invocation used explicit literal values and
verified the resulting resource marker.

Reproduce in the existing VS amd64 shell with Zig 0.16.0:

```powershell
zig build -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -j4 --summary all `
  -p zig-out/feature-cohort-37e298a3 `
  -Dstfc_source_state_id=37e298a397fe4f56f0026e1c9dc6e05612ea69fe `
  -Dstfc_base_commit=37e298a397fe4f56f0026e1c9dc6e05612ea69fe `
  -Dstfc_build_invocation_id=feature-cohort-20261026-01 -Dstfc_build_channel=study
```

### Authorized main-game boot smoke

The candidate was built before stopping the game. Existing canonical AX lifecycle
helpers then stopped the explicitly checked ordinary main PID 7464 / file time
`134375393035625138`, deployed the exact DLL above and launched PID 51248 / file
time `134375424875951356`. Installation:
`C:/Games/Star Trek Fleet Command/default/game`, client 273. The local VERSION.dll
module path was observed and the current installed disk hash matched; that does
not independently hash in-memory module bytes. The system version.dll also appears
normally in the module inventory.

The window responded; startup logged FrameTick ready and zero error/critical
lines. All four installation TOML files were byte-identical before deployment
and after boot. No settings edits, store changes or build during game downtime.
Receipts and retained startup log:
`D:/dev/stfc-workspace/artifacts/feature-cohort-20261026-01/main-runtime/`
(`cycle.json`, `deploy.json`, `boot.json`, `startup-community_patch.log`). The
canonical deploy helper's branch fields describe its own checkout; the explicit
Zig source commit/marker above is the candidate identity.

This boot smoke alone does not establish the four workflows. Guff subsequently
reported "Seems good" in response to the four-workflow trial request. Record this
as basic Windows acceptance of the installed candidate, not itemized completion
of every native dialog/control/consumer edge case. macOS remains unqualified.
Original source and study archive stay pinned. The next discovery should start
from an unported capability, not another scan of these four contracts. Recovery
uses the scoped Downstream handoff with this worktree/branch and implementation
commit explicitly named; no LexRunner continuity attribution is made.

## Deferred Settings follow-ups

Operator feedback, 2026-10-26: these are inherited usability/design improvements,
not newly discovered migration regressions. Medium priority ("semi-priority"),
explicitly deferred; no UI work or additional cycle is requested now.

- **SET-UX-STYLING:** Visual sweep across Mod Settings, especially subsection
  header backgrounds and selected-row styling. Preserve readable hierarchy and
  avoid the previously rejected heavy white selected fill.
- **SET-UX-BANNER-LISTS:** Revisit the hidden-banner and desktop allowlist editing
  experience. The current source-preserved text editors remain the accepted baseline.
- **SET-UX-SHIP-ROUTES:** Revisit ship-specific jump/warp/always-ask controls. Keep
  existing matching, precedence and persistence semantics unless deliberately changed.

These follow-ups do not block continuing feature migration and do not change the
current analytical, Settings or gameplay contracts.

## Search-row correction before branch consolidation

The operator explicitly requested removing the redundant root "Search settings"
action because the native header already contains the search field. Only that
permanent row is removed. The header's automatic page-visit/frame creation,
typing/capture lifecycle, query/result routing, contextual Back and Clear remain
their existing owners.

A narrow read-only review identified that the removed action also presented
search startup errors. Preserve that feedback as a conditional "Search unavailable"
row only when an error exists; Dismiss clears it and refreshes even with an already
empty query. This is not a replacement always-visible Search control. The revised
`maintenance_pages_test` checks the absent redundant action, unchanged metadata
search/result navigation/Clear, visible failure feedback and dismissal. Correction
review reported no residual defect. No new hook, input service or settings write.

The earlier accepted DLL remains identified above; it does not include this
later correction. This delta needs its own source-bound build and native trial.
