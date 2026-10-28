# Travel and cargo workflow cohort â€” 2026-10-27

Status: implementation in progress. Eight existing workflows are being reassessed,
not eight presumed missing implementations. The accepted fleet/queue build stays
the runtime baseline until this candidate passes its own checks.

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
Runner-applied Git merge. Repo-bound durable gates will qualify the actual frozen
candidate where supported. Automatic Runner recovery-frame emission remains off.

The generic workflow guide has no repo-root parameter and inspected the shared
container as though it were a Git repository, reporting missing profile files.
That advisory is not a reason to initialize a false shared project. Explicitly
repo-bound task plans and gate interfaces remain the useful lane.

## Validation and remaining qualification

Pending until the combined source is frozen: source/obligation review, focused
production-adapter and policy tests, Settings/queue/AAI host regressions, native
Windows build, exact artifact identity, and normal-use in-game trial.
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
