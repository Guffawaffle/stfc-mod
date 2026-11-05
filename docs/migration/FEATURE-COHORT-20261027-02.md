# Travel and cargo workflow cohort â€” 2026-10-27

Status: implementation and basic Windows player qualification complete. Eight existing workflows are being reassessed,
not eight presumed missing implementations. The accepted fleet/queue build remained
the runtime baseline until this candidate passed its own checks.

## Source and execution boundary

- Frozen play-dev: `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target: `play-dev-ripper`, existing worktree at
  `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`, starting at
  `a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`.
- Source-preserving procedure: `.agents/skills/migrate-play-feature/SKILL.md`.
- Three directly coordinated workers plus one integrating coordinator; each reads
  complete pinned implementation, supporting calls, headers, lifecycle and tests.
- No new checkouts, branch rewrites, pushes or releases. AAI and the prior queue
  fixes remain intact. Settings redesign and the historical repair interlock are
  separate follow-ups.

## Dependencies and ownership

| Lane | Existing feature IDs | Exclusive implementation ownership |
| --- | --- | --- |
| Warp choice and labels | PLAY-WARP-AUTO, PLAY-WARP-MODIFIER, PLAY-WARP-LABEL | instant_warp.cc and lane fixture/contracts |
| Course and preview | PLAY-WARP-COURSE, PLAY-CARGO-PREVIEW | fleet_actions.cc and lane fixture/contracts |
| Mining and cargo display | PLAY-MINING-CYCLE, PLAY-CARGO-FORMAT, PLAY-CARGO-INSTANT | mining_preview.cc/h, cargo.cc and lane fixtures/contracts |
| Integration | All eight | shared game bindings, test registration, inventory, serial builds, qualification and checkpoints |

The coordinator reviews shared dependencies, integrates the lane changes in the
same authorized checkout and verifies the combined candidate. A worker's source
inspection or passing policy test is not native runtime qualification.

## LexRunner use and limitations

LexRunner CLI is 2.5.0. The authored dependency plan is stored at
`D:/dev/stfc-workspace/.ax/travel-cargo-cohort-20261027/dependency-plan.json`.
Schema validation passed (7 nodes, 8 edges), and Runner merge-order returned three
parallel implementation lanes, then integration, candidate validation and runtime
qualification. Plan identity:
`sha256:4d83810ead2cfd5e9d8d649d8995793a10deb218fb4bad6a990cba53c4eba3c5`.
These are local task nodes, not invented GitHub PRs.

Governed Attempt containment preflight reports
`windows_native_boundary_unavailable`, `broker_required`, and
`physicalContainmentAvailable=false`. No boundary override, alternate checkout or
security change is being used to force it. Actual worker fanout and same-checkout
integration are direct collaboration, not Runner-governed execution or a
Runner-applied Git merge. Repo-bound durable gates qualified the actual frozen
candidate, as recorded below. Automatic Runner recovery-frame emission remains off.

The generic workflow guide has no repo-root parameter and inspected the shared
container as though it were a Git repository, reporting missing profile files.
That advisory is not a reason to initialize a false shared project. Explicitly
repo-bound task plans and gate interfaces remain the useful lane.

## Validation and remaining qualification

The combined source was frozen for source/obligation review, focused
production-adapter and policy tests, Settings/queue/AAI host regressions, native
Windows build, exact artifact identity, and normal-use in-game trial. Results
and remaining limitations are recorded below.
macOS remains a separate CI build and Mcohall native qualification.

## Integrated offline checkpoint

The combined Windows build/run/install suite passed **347/347 steps** with
ReleaseFast production dependencies and Debug fixture assertions. This is a step
count, not 347 independent tests. The current passing log is
`D:/dev/stfc-workspace/.ax/travel-cargo-cohort-20261027/tests-4.log`.

New production-adapter modes cover warp prompt/intent/label ownership, missing
override metadata, delayed cargo source/cancellation/expiry, and cargo
format/snap metadata and restoration. The original mining polling helper and
fixture were preserved, including 100 ms loading, 250 ms presence checks, a
five-second loading timeout and immediate input validation. Native frame cost
and complete mining-cycle behavior are not proven by the pure poll fixture.
Retained course policy and typed-boundary checks passed; the actual hidden-card
Galaxy consumer still requires a native trial. Existing queue, Settings, preset
and pinned AAI host regressions passed.

Cross-review corrected cancellation after reentrant getters and next-frame
automatic retry timing before final qualification. A fixture initially inherited
a focused text field; production correctly cancelled cargo work, so the fixture
now establishes gameplay focus explicitly rather than weakening cancellation.
A duplicate test string API and C++ dependent-template syntax were integration
fixes, not game behavior changes. Earlier failed logs are retained, not counted
as passing evidence. The restricted execution environment returned a generic
Zig `Unexpected` before compilation; the established authorized native build
environment ran the actual suite. No execution guards or tooling were changed.

The complete frozen-source review also found mining cycling is absent upstream
at the inspected revision; upstream's existing PreScan cargo toggle is a
different workflow. The inventory records that distinction instead of inferring
equivalence from shared shortcut names.

## Integrated offline checkpoint

The combined Windows build/run/install suite passed **347/347 steps** with
ReleaseFast production dependencies and Debug fixture assertions. This is a step
count, not 347 independent tests. The current passing log is
`D:/dev/stfc-workspace/.ax/travel-cargo-cohort-20261027/tests-4.log`.

New production-adapter modes cover warp prompt/intent/label ownership, missing
override metadata, delayed cargo source/cancellation/expiry, and cargo
format/snap metadata and restoration. The original mining polling helper and
fixture were preserved, including 100 ms loading, 250 ms presence checks, a
five-second loading timeout and immediate input validation. Native frame cost
and complete mining-cycle behavior are not proven by the pure poll fixture.
Retained course policy and typed-boundary checks passed; the actual hidden-card
Galaxy consumer still requires a native trial. Existing queue, Settings, preset
and pinned AAI host regressions passed.

Cross-review corrected cancellation after reentrant getters and next-frame
automatic retry timing before final qualification. A fixture initially inherited
a focused text field; production correctly cancelled cargo work, so the fixture
now establishes gameplay focus explicitly rather than weakening cancellation.
A duplicate test string API and C++ dependent-template syntax were integration
fixes, not game behavior changes. Earlier failed logs are retained, not counted
as passing evidence. The restricted execution environment returned a generic
Zig `Unexpected` before compilation; the established authorized native build
environment ran the actual suite. No execution guards or tooling were changed.

The complete frozen-source review found mining cycling is absent upstream
at the inspected revision; upstream's existing PreScan cargo toggle is a
different workflow. The inventory records that distinction instead of inferring
equivalence from shared shortcut names.

## Final native and player checkpoint

Implementation: `10477fd1a05df0c06ea3921dc56843fb8296fdbd`. The exact clean
source passed Runner's three sequential repository-bound gates: regression
suite, native DLL build and catalog. Artifact readback is complete across nine
retained references. Repeating the same idempotency key returned the same
operation rather than starting another execution. Native construction passed
41/41 steps. These are execution/artifact results, not merge or release authority.

Gate manifest SHA256:
`032a91d4602eb43ccdfa084f1f0f95e265adeae2f7c23b213f7a759513b4e605`.
Terminal SHA256:
`6728c870ab07373521ceee9388c346db4ae2159978070dbe8f81e95c017a3c97`.
Native gate DLL SHA256:
`70194446b97f6ae47f84174a7e0e32d875a388f0859596a35afb52039847c335`.

The normal AX Zig cycle rebuilt before stopping PID 3352, then copied and
launched only the main ordinary installation. The source stayed clean/stable.
PID 14228, creation FILETIME `134376226725289302`, runs client 273 with build
`ax-zig-20261028T005458-ff6ea696`. Final installed DLL SHA256:
`c0f1d6077aecedad8481c435792bda552f92508e26c395e1671a5227d98d4a18`.
The fresh invocation explains why its hash differs from the retained gate DLL;
both name the same source. AAI stays pinned to 4bece2e.

Actual fresh logs confirmed all 71 groups installed, including CargoFormat,
InstantCargo, MiningPreview, InstantWarp and both AAI native hooks, followed by
FrameTick readiness. The window responded. Five tracked TOML/state files were
byte-identical at the post-cycle comparison; no config or store migration was
performed. Guff answered **“all work”** to the requested Warp/Jump choice/label,
occupied mining-node cycle, cargo totals and reopened-card checks. This is basic
combined Windows acceptance, not itemized proof of every failure/rebind case.

The previously observed contained GameObject.GetComponent NullReference was
logged again after startup. Caller and cause remain unresolved. Frequency is
unknown because the binding suppresses further reports from that method. The
player's successful trial does not establish an error-free runtime. Keep this
as a separate diagnostic follow-up; no unrelated fix is bundled here.

As in the earlier cohort, the generic boot parser returned after two groups;
the full 71 and frame readiness were independently read afterward. The raw
nested deploy.commit names canonical a815944d, while explicit Zig worktree,
prepared build, embedded marker and hash bind the actual source to 10477fd1.
Raw receipts are preserved. The cycle performs a second native invocation after
the gate build; that adds compile/link time while the game remains up. A future
approved prebuilt-artifact cycle could avoid duplication; no lifecycle tooling
was changed during this batch.

## Recovery and remaining work

Qualification receipt:
`D:/dev/stfc-workspace/artifacts/travel-cargo-cohort-20261027/qualification.json`
(SHA256 `18e58adb52b52290872c956951ba293fdb16d138ca872daebcd19bd941121520`).
Execution artifacts and raw cycle/gate receipts:
`D:/dev/stfc-workspace/.ax/travel-cargo-cohort-20261027/`.

The subsequent qualification checkpoint is documentation only. The game remains
on source 10477fd1; no push, merge, release, new checkout, or frozen-branch change.
Native card/scene edge cases and actual frame-cost measurement remain separate.
macOS needs both architecture builds in CI and Mcohall's trial; instant cargo
remains Windows-only as in frozen source. The historical repair interlock,
Settings visual follow-ups, UI toolkit expansion and persistence remain parked.
