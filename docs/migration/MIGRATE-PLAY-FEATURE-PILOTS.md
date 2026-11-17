# migrate-play-feature — first two pilots

2026-10-26 (America/Chicago). Bounded work, not resumption of whole-product parity.
The implementation checkpoint was offline. The subsequently authorized Windows
pilot deployment is recorded below; no push or promotion occurred.

## Subsequent authorized pilot launch

The operator requested the exact pilot DLL in the main ordinary installation.
Verified artifact `66074968bcc1c5718d9ec1c3a5f089a70e7c9de7d1b6468b96afb1320d79d1cc`
replaced the installed DLL using existing AX deployment helpers under the runtime
lease, after rechecking PID 2968/creation identity. No build ran during downtime.
New process: PID 42964, launched 2026-10-27 01:32:53 UTC, installation
`C:/Games/Star Trek Fleet Command/default/game`, client 273. The expected local
module path loaded, the window responded, and FrameTick reported ready. Primary
TOML SHA-256 remained `78c248ce73fad72394405765db17fb95b5ee454c8be51fc8eddcf482b7f4f6f1`
before deployment and after launch. No settings edits or store migration.

Cycle/deploy/boot receipts and startup log:
`D:/dev/stfc-workspace/artifacts/migrate-play-feature-20261026/main-runtime/`.
This is boot evidence, not player confirmation of assignment, picker, or Settings
behavior. Earlier offline receipt claims remain tied to their original checkpoint.

macOS qualification direction confirmed by the operator: builds will run in CI;
Mcohall will perform native testing when ready. Do not create a local SDK project
or treat Windows success as macOS qualification. No CI changes are requested here.

### Subsequent player confirmation and procedure acceptance

Guff reported that both pilot workflows worked in the installed Windows build
("both work") and approved using the procedure for further migrations. This is
basic player confirmation tied to the artifact above, not itemized verification
of every capture, reset, native-object custody or failure case. Keep those
limitations below visible. No overall parity, release or macOS claim follows.
The next bounded application is [cohort 20261026-01](FEATURE-COHORT-20261026-01.md).

## Source and recovery boundary

- Frozen source: `frozen/play-dev-20261026` =
  `1baa55fedf1758b19f843cc59707b8901a0b4998`, local and never advanced.
- Target archive: `study/ripper-typed-bindings-20261026` =
  `076c85a84ee5d126b02582f55936ccfc4a066a29`, preserved.
- Work branch: `feature/migrate-play-feature-pilots-20261026`, existing
  `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream` worktree.
- Implementation checkpoint: `241372c3efb2e3b1c5f0c9fa5ee336387044cfc6`.
  Subsequent inventory/recovery edits are documentation only. Production inputs
  were checked unchanged from that commit after all final checks.
- Canonical product checkout remains on its separate Alliance Games study branch.
  The game remains on its existing installed build.

## What the skill does

[SKILL.md](../../.agents/skills/migrate-play-feature/SKILL.md) is a short procedure
plus [one contract template](../../.agents/skills/migrate-play-feature/assets/feature-contract.md)
and native Codex UI metadata. It directs discovery from pinned source, mapping to
existing target code, narrow adaptation, verification by obligation, and additive
inventory recording. Architecture remains in AGENTS.md, IL2CPP.md, and the typed
study index. No custom orchestrator, installer, validator, or global skill copy.

The repo-local `.agents/skills` location and relative references were checked.
The bundled skill-creator validator passes. This chat invoked the file explicitly;
automatic discovery in a fresh target-rooted Codex session has not been observed.

## Pilot 1 — reconcile the existing assignment port

Contract: [PLAY-DOCK-ASSIGN](features/PLAY-DOCK-ASSIGN.md).

Most source logic already existed. Preserve it. Complete-source comparison and an
independent read-only forward test found that raw Enter in a feature frame callback
bypassed Settings capture, and the original hotkey-patch relationship had been lost.

The production input boundary now honors capture/drain and both patch gates.
The frame descriptor again follows the hotkey group. Existing production capture
fixtures exercise that same input helper, main/keypad edges, held continuation,
text focus, unavailable owner, disabled setting, and ordinary hotkey-mode independence.
Per-candidate managed custody and native liveness checks protect button inspection.

**Remaining distinction:** source enumeration roots an entire bounded snapshot
under tracker exclusion. Target still acquires borrowed snapshot pointers and then
roots individual candidates. This pilot does not establish full source-snapshot
equivalence, nor native button/teardown behavior. Source and target also lack an
ordinary-click episode reset across same-address ship reuse. The existing safer
one-press-per-edge behavior and successful-hook gate remain explicit differences.

Disposition: Partial, with the concrete input omissions repaired and independently
tested. Do not turn this into a claim of full runtime parity.

## Pilot 2 — restore loading image selection

Contract: [SET-IMAGE-PICKER](features/SET-IMAGE-PICKER.md).

Reuse unchanged source `ImageFileSetting`, `ImagePathValidator`, and picker API.
Reuse source tests, adding dialog-open failure and oversized-result cases. Preserve
the Windows COM/image-dialog operations, UTF-8 result and cancellation/RAII. Preserve
the macOS main-queue NSOpenPanel/promise. Keep the image branch separate from the
unrelated audio-picker UI. The packaged-task helper retains source's nonjoining
future policy; a dedicated fixture verifies pending-future disposal and exception delivery.

Adapt only the existing catalog/value writer, shared frame admission, page custody,
and missing native validation-completion notification. Catalog registration stays
pure; an explicit invocation lazily admits polling, without installing another hook.
Restart intent persists through the registered preserving writer while the active
image remains unchanged. The renderer and image decode/fallback path are untouched.

The real catalog adapter is exercised with synthetic frame, dialog, persistence and
editor boundaries. Tests cover unavailable renderer, failed polling admission,
trim/empty fallback, async validation, stale-page commit, reset, one pending picker,
and requested-versus-active values. Source-model tests cover stale edit/reset,
in-flight validation reset, cancellation, missing files, exceptions and path limits.

**Fallback qualification:** a synthetic editor can demonstrate routing with no
shared frame delivery, but the real native editor also requires that delivery.
The meaningful native fallback is rejected picker-callback admission while the
editor remains ready. Native completion/status refresh without typing remains a
specific in-game check; tests do not exercise the actual TMP/native host.

Closing Settings intentionally does not cancel an external picker's process-level
intent; it can apply an unchanged observed preference after the page closes. It
owns no borrowed Unity view. Exact-entry is page-bound and rejects delayed commits.
Regular-file validation does not establish that the game can decode the image.

Disposition: Implemented, unverified in game. No current-game behavior is replaced.

## Verification record

Windows toolchain: Zig 0.16.0, VS 18 MSVC 14.51.36231 / Windows SDK 10.0.26100.0.
Libraries ReleaseFast; fixtures Debug with assertions. Commands run in the target
worktree, through the existing local gate helper or VS developer shell.

| Check | Result / limit |
| --- | --- |
| Base `test-mod test-il2cpp` | 202/202 build/run steps passed before feature edits. Not 202 independent behavior tests. |
| Candidate `test-mod test-il2cpp` | 208/208 steps passed, including preserved/new picker and production capture/catalog fixtures. |
| Windows `zig build mods` | 23/23 steps passed. Static native library only. |
| Windows default DLL build | Final candidate 37/37 steps passed; x86_64 proxy/16 exports/static CRT/full resource provenance marker verified. |
| Skill metadata, relative references, `git diff --check` | Passed. These do not establish workflow or runtime correctness. |
| macOS aarch64 ReleaseFast build | Could not configure: `.cache/toolchain/macos-sdk` absent, no SDK path/URL configured. No macOS compile/runtime claim; x86_64 also unqualified. |
| Native dialogs, TMP completion state, button chain, game/client ABI | Pending. No boot smoke or player verification performed. |

Local receipts/logs are under `D:/dev/stfc-workspace/.ax/` with
`migrate-play-feature-*-20261026` names. Receipt:
`D:/dev/stfc-workspace/.ax/migrate-play-feature-241372c3-receipt.json`, SHA-256
`8081ac3f64ee871c8ea34756f7e3c1793cd18e319897d9191bcb7c38ac444ecd`.
It records changed input hashes, five fixture executable identities and successful
post-commit re-executions, log hashes, platform limits, and byte-identical source
reuse of both models and the picker API. Earlier builds do not qualify later code.

Final DLL (built, **not installed**):
`zig-out/migrate-play-feature-241372c3/bin/version.dll`, SHA-256
`66074968bcc1c5718d9ec1c3a5f089a70e7c9de7d1b6468b96afb1320d79d1cc`.
Its resource marker carries implementation commit `241372c3...` as source/base,
build `migrate-play-feature-20261026`, mode `release`, channel `study`.

Reproduce the tests with the existing
`D:/dev/stfc-workspace/.ax/play-alignment-sprint-20261025/Run-LocalGate.ps1`
and `-Targets 'test-mod test-il2cpp'`. In a VS amd64 developer shell, the DLL command
is Zig 0.16.0 `build -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -j4
--summary all -p zig-out/migrate-play-feature-241372c3`, with
`-Dstfc_source_state_id=241372c3efb2e3b1c5f0c9fa5ee336387044cfc6`, the same
`-Dstfc_base_commit`, `-Dstfc_build_invocation_id=migrate-play-feature-20261026`,
and `-Dstfc_build_channel=study`. Verify it through
`scripts/verify-windows-binary.py` with the complete resource marker, not a branch label.

## Workflow dogfood findings

1. **Discovery first paid off.** It found behavioral relationships outside the
   named feature and identified logic already ported. No replacement assignment
   implementation or duplicate native Settings system was justified.
2. **Two shapes exercise different boundaries.** Assignment exposed raw-input and
   callback-order ownership. Image selection exercised pure model reuse, async
   results, platform adapters, restart intent, and fallback reachability.
3. **Refined after actual use.** Skill/template now permit inline contracts for
   read-only evaluation, require checking pinned paths, identify fixture layer,
   and record fallback prerequisites and collection-versus-item custody.
4. **Independent evaluation was read-only.** A fresh agent used the skill for source
   discovery, then reviewed adapters and evidence claims. Implementation remained
   one continuous foreground workflow. No Runner-managed fanout or execution is
   claimed; LexRunner was not needed or used for these pilots.
5. **Avoid false completion.** Passing model/adapter fixtures cannot establish a
   real host exists when its frame anchor is unavailable. Similarly, per-item GC
   custody is not a fully rooted snapshot. Both remain visible in the contracts.

## Focused next qualification

Before promotion, use an explicitly selected candidate and client for:

1. Swap Ship: enable live assignment; main/keypad Enter, hold/release/repress;
   record a chord and close the editor with Enter while a dock is visible; verify
   no assignment during capture/drain. Check hotkey patch disabled, UI setting
   disabled, native shortcut mode, and mod hotkeys disabled separately.
2. Double/triple/four clicks, different ships, expired window, pin badge/drag,
   recycled tiles, hidden/multiple docks, missing/destroyed native objects. Resolve
   the snapshot-acquisition custody/order difference before claiming source parity.
3. Loading Screens: pick a valid Unicode PNG/JPEG path; cancel; reset while picker
   or validation is pending; leave/reopen Settings; inspect restart status and the
   preserving TOML delta. Verify missing/malformed file fallback independently.
4. With the exact-entry host ready, reject picker polling admission in a fixture
   or controlled diagnostic and check unchanged-draft validation completion,
   cancelled/replaced editor, page departure, Apply and native Back/Escape.
5. Supply a qualified macOS SDK, build both architectures, then qualify Cocoa
   dialog lifecycle and native UI independently. Windows success is not Mac evidence.

The existing machine-readable inventory gains source-bound assessments only for
these two IDs. Historical baselines/dispositions remain traceable; no claim of
complete product parity, release readiness, or broad migration authorization.
