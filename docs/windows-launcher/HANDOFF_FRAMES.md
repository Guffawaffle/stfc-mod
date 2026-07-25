# Windows Launcher Handoff Frames

This is the branch-visible index for durable Lex frames created while planning
and orchestrating `Guffawaffle/stfc-mod#182`. Lex remains the frame store; this
file records stable IDs so a fresh session can recover the intended sequence.

| Sequence | Role | Frame | Outcome | Next action |
|---:|---|---|---|---|
| 1 | Foreground orchestrator | `frame-1785006342138-93252cdc-5507-4c7b-9440-a53c657bdbd8` | Codex/LexRunner setup validated, dogfood issues filed, and delivery-base conflict isolated. | Collect independent plan and base audits. |
| 2 | Plan-audit worker | `frame-1785006556980-05886fbd-e6c7-48e2-8e16-8790c8e9f884` | All 10 issue mappings and DAG edges agree; five waves are exact. Governance and gate-enforcement gaps block dispatch. | Resolve base, worker isolation, readiness labels, and acceptance enforcement. |
| 3 | Base-audit worker | `frame-1785006561899-3bf06cc9-b6a4-4a5d-a4a6-279cf36273e2` | `origin/main` is 33 commits newer with the same tree; `upstream/dev` omits 335 fork commits. | Record fork-main policy and fast-forward the planning branch. |
| 4 | Foreground orchestrator | `frame-1785007005652-7dbb403d-5241-4a0a-85a9-fad2c9dbc6fe` | Planning committed at `64d2c0f` and pushed; fork-main policy recorded; all launcher items remain pending and undispatched. | Pivot to user feedback; authorize isolation and dispatch WL-001 only when the sprint resumes. |

## Current handoff contract

Each worker handoff must report:

- objective and non-goals;
- branch, base commit, and dirty-state snapshot;
- work-item and GitHub issue IDs;
- files touched and artifacts produced;
- commands, tests, gates, and outcomes;
- decisions, assumptions, blockers, and risks;
- recommended next action;
- stable frame ID and idempotency key.

`workspace/unscoped` is a temporary module sentinel because this repository has
no loaded Lex policy. The policy and automatic unscoped behavior are tracked in
`Guffawaffle/lex#800`.
