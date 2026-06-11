# AXF Collaboration Contract

Date: 2026-06-11
Branch: `docs/axf-collaboration-contract`

## Purpose

This is the lightweight collaboration contract for STFC agent work through AXF/Lex/Codex.

It is not a new runtime framework, checklist engine, or required ceremony. It defines shared concepts so missions, reports, reviews, and parked ideas use the same vocabulary instead of ad hoc prose.

Use the concept when the work naturally belongs to it. Omit the section when it adds no useful signal.

## Operating Rule

Structure should preserve reasoning and prevent unsafe drift. It should not constrain useful engineering judgment.

When a mission touches live game behavior, explain the gameplay seam. When work consumes copied game facts, explain the evidence surface and subscribers. When old probe code appears, classify it. When the mission changes shape, report drift. When a good idea is out of scope, park it explicitly.

## Shared Concepts

| Concept | Meaning | Use when |
| --- | --- | --- |
| Gameplay seam | Native/game surface where the mod touches live game behavior or state. | Hooks, detours, game-thread reads, native callbacks, frame/update seams, repair/action paths. |
| Seam owner | Module responsible for raw hook contact and hook-scoped extraction. | A seam publishes evidence or takes action. |
| Evidence surface | Copied, mod-owned facts emitted from a seam. | Downstream code should consume facts without touching live Unity/STFC objects. |
| Subscriber | Feature that consumes an evidence surface. | Notifications, sidecar Fleet Watch, live debug, analytics, sync/export. |
| Delivery feature | User-visible or external-output behavior built from evidence. | Sidecar transport, notifications, battle events, logs, config-facing capability. |
| Provenance | Source, owner, seam, reason, and intended effect carried with dispatch/capture/export. | Data crosses module, async, diagnostic, or external boundaries. |
| Classification | Explicit posture for old/disabled/probe/legacy paths. | Code is not part of the happy path but still exists or was encountered. |
| Drift | A change in mission shape, assumption, or solution direction. | Work should deviate from the stated branch scope. |
| Review contract | Mechanical validation gate owned by AXF. | Branch is ready for review or touches code/hook-adjacent behavior. |
| Scanner tripwire | Static unmanaged-hook detector inside the review contract. | Raw hook-like behavior may have changed. |
| Parked follow-up | Useful idea intentionally not absorbed into current scope. | Idea is valid but would broaden blast radius or sequence poorly. |

## Concept Routing

- If work touches live game behavior, route it through `gameplay seam`.
- If code consumes copied game facts, route it through `evidence surface` and `subscriber`.
- If data crosses worker, sidecar, file, cloud, log, or diagnostic boundaries, carry `provenance`.
- If old probe code appears, classify it instead of silently skipping it.
- If implementation direction changes, report `drift`.
- If a useful idea is out of scope, record it as a `parked follow-up`.

## Probe And Legacy Classification

Old probes, disabled paths, and legacy diagnostic surfaces must be classified when encountered:

| Classification | Meaning |
| --- | --- |
| migrate | Bring the path into the current seam/evidence/provenance model. |
| retire/remove | Remove or disable because the path is obsolete, unsafe, or misleading. |
| quarantine/probe-only | Keep disabled or limited to tests/tools with comments and guard tests. |
| temporary exception | Keep temporarily with owner, reason, expiry or trigger, and validation plan. |

Do not leave an old paradigm in place merely because it is disabled. If cleanup would expand the branch too far, document the path and park it as a named follow-up.

## Mission Guidance

Useful missions should state:

- the problem and current evidence
- the desired outcome
- hard boundaries
- autonomy level
- validation expectations
- required final report fields

Avoid over-prescribing the implementation unless the exact implementation is the requirement. Prefer "here is the problem and boundary" over "execute this checklist mechanically."

## Report Template

Required facts:

```text
Branch:
Commit(s):
Status:
What changed:
What did not change:
Runtime behavior changed:
Validation:
Scanner/review-contract:
Remaining risks:
Recommended next step:
Push / merge result or recommendation:
```

Conditional sections:

```text
Old probes / legacy paths encountered:
Classifications:
Drift:
Parked ideas / follow-ups:
```

Use `none` when a conditional section was considered and does not apply. Do not include empty ceremony.

## Drift Reporting

Use this shape when drift occurs:

```text
Drift:
- Assumption changed:
- Value of alternate path:
- Decision: absorbed / parked / rejected
```

If there was no drift:

```text
Drift:
- none
```

## Validation Posture

`global.stfc-mod.review-contract` remains the normal blocking review gate for code and hook-adjacent branches. It owns the gameplay seam scanner, whitespace checks, cached diff checks, and pure tests.

Docs-only branches should still run the review contract when practical, because the contract is cheap enough and catches scanner/baseline drift. If a docs-only branch intentionally skips pure tests, report that explicitly.

Runtime validation remains separate. Use it when behavior, deployment, live game config, sidecar behavior, or hook timing changes.

## Non-Goals

- No runtime `GameplaySeamRegistry` is introduced here.
- No new hook migration is implied.
- No config behavior changes are implied.
- No report section is mandatory when it adds no information.
- No agent is required to accept a flawed mission plan without challenge.

## Preferred Working Style

This contract should help agents reason with the operator, not reduce agents to checklist executors.

Good agent behavior:

- challenge weak assumptions before broadening scope
- keep branches narrow
- preserve source/evidence/provenance
- classify old paths explicitly
- report drift instead of hiding it
- park good ideas instead of absorbing them silently
- keep final reports concise and factual
