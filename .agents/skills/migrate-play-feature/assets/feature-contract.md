# <capability-id> — <feature name>

Contract version: 1. Status: <discovery / implementation / verification>.

## Inputs and scope

- Source repository/ref/revision:
- Target repository/branch/base:
- Assessed candidate or input manifest:
- Existing inventory ID:
- Authorized work and exclusions:

| Inspected branch | Exact revision | Presence: control / consumer / absent / not assessed | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | | | |
| play | | | |
| play-dev / frozen source | | | |
| refactor | | | |

Record activation/settings location, platforms, parent/dependencies, source scan
date, migration checkpoint and verification scope. Add undocumented discoveries
to the inventory immediately; keep out-of-scope discoveries out of implementation.

## Source workflow and ownership

Describe activation → producers/callbacks → consumers → cleanup. Link pinned
implementation, supporting headers/call sites, configuration/defaults, persistence,
platform paths, and tests. Identify dormant paths and source defects separately.
Verify paths at the pinned revision. Record fallback prerequisites and who owns
individual objects versus the entire acquired collection.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| O1 | <pinned path + symbol/operation> | <reuse / adapter / rewrite reason / gap / decision> | <actual check; pending is valid> |

## Design and behavior decisions

Record the actual typed/native integration boundaries and ownership. Explain
substantial rewrites. Separate preserved behavior, intentional changes, source
defects, migration regressions, and unresolved interpretation. Do not infer
equivalence from similarly named APIs.

## Validation evidence

Record exact commands, candidate identities, artifacts/receipts, and limitations.
Separate source comparison, automated tests, compilation, boot, in-game behavior,
and each platform. Describe what production path a fixture exercises.
Identify its layer: model, production adapter, native host, or final consumer.
Synthetic replacements qualify only the behavior they actually exercise.

## Disposition and recovery

- Migration coverage:
- Feature correctness / known defects:
- Pending workflow/platform checks with reproducible steps:
- Dependencies or blocker and alternatives:
- Checkpoints and next action:
