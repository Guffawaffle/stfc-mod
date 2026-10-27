---
name: migrate-play-feature
description: Investigate, adapt, and verify a named PLAY/play-dev feature in the typed IL2CPP/SPUD refactor using a pinned source contract and the existing migration inventory. Use for bounded feature ports or migration reconciliation, not unrelated feature development.
---

# Migrate a PLAY feature

Preserve working source behavior through the narrowest necessary adapters. A feature
inventory is a discovery index, not a substitute for reading the implementation.

## Establish the task

Identify the feature, authorized mode (discovery, implementation, or verification),
owning checkout, source revision, target base, and current changes. Follow the
repository's [AGENTS.md](../../../AGENTS.md) and nested instructions. Invocation
does not authorize a new worktree, deployment, publishing, or unrelated migration.
Existing authorization may cover successive stages; do not invent milestone approvals.

For this migration series, resolve `frozen/play-dev-20261026` and verify it is
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Do not advance it. A different source
requires an explicit baseline/delta decision. Read source through Git at that
revision; the canonical checkout may be on unrelated work.

Find the existing capability ID in
[play-functional-parity.json](../../../docs/migration/play-functional-parity.json).
Historical PLAY dispositions and recovery prompts are evidence, not current authority.
If an old entry disappeared or became dormant in the selected source, investigate
that change before restoring it.

## Discover and contract

Read the complete relevant source implementation, headers, registrations, call
sites, producers, consumers, callbacks, and tests. Trace configuration/defaults,
availability, UI, shortcut precedence, persistence, platform paths, and cleanup.
Separate reachable behavior from dormant declarations and known defects.
Validate evidence paths at the pinned revision, including supporting headers.

Use [feature-contract.md](assets/feature-contract.md) to record the result in
`docs/migration/features/<capability-id>.md`. Give meaningful operations numbered
obligations with pinned source evidence and verification methods. Include disabled,
failed, cancelled, replaced-object, and reset paths where the feature has them.
In discovery-only mode, report this contract and gaps without implementing a port.
For a read-only request, deliver the contract inline instead of writing repository files.

## Design and adapt

Read the existing target implementation before changing it. Map each obligation
to unchanged reuse, a narrow adapter, a justified rewrite, a capability gap, or an
explicit behavior decision. Already covered behavior needs qualification, not
another implementation. Explain concrete incompatibilities behind rewrites.

Consult [IL2CPP.md](../../../docs/IL2CPP.md) for actual binding/ABI contracts and
[TYPED-BINDINGS-STUDY.md](../../../docs/migration/TYPED-BINDINGS-STUDY.md) as a
retrieval index. Inspect the actual helper and consumer before using them. Keep
game bindings, feature behavior, shared hook ownership, and UI lifetime ownership
with their existing owners. Missing observations must not silently become action-
authorizing defaults. Do not introduce a parallel decoder, hook registry, or UI
framework to avoid a bounded integration problem.

When authorized, reuse source logic and adapt only incompatible boundaries.
Account for the whole workflow, including activation, settings, callbacks,
consumers, teardown, and failure behavior. Do not reintroduce known unsafe behavior
for parity; record intentional changes and their justification. Improvements and
preserved source defects are separate from accidental migration regressions.

## Verify and record

Compare the finished implementation to every obligation, not just the entrypoint.
Use relevant existing fixtures and add focused tests for meaningful new risk.
Record what each test actually exercises and what needs an exact-client trial.
Name the exercised layer (model, production adapter, native host, or final consumer)
and the prerequisites that make a fallback reachable. A synthetic host does not
establish availability of the real host when its shared services have failed.
Bind evidence to the source revision, target candidate/changed inputs, toolchain,
build options, and executable identity where applicable. Re-run affected checks
after later changes; historical binaries and passing gates do not qualify new code.

Keep these claims distinct: source inspection, automated behavior, build success,
boot smoke, player verification, and platform qualification. Migration coverage and
feature correctness are independent. An intentional change is not equivalence
until its decision and verification are explicit.

Update the selected inventory entries additively with contract/evidence links and
source-bound assessments. Preserve old baselines and exclusions. Link from
[PLAY-PARITY.md](../../../docs/migration/PLAY-PARITY.md) rather than maintaining a
competing inventory. Record exact checkpoints, known gaps, and the next action.
Keep unresolved obligations visible; do not mark overall parity complete.

For a genuine blocker, retain completed independent work and report the specific
incompatibility, evidence, alternatives, and decision needed. Otherwise continue
within the authorized feature scope.
