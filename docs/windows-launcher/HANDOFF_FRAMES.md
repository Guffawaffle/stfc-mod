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
| 5 | Foreground orchestrator | `frame-1785125569518-2de46ced-7c86-44f7-b2e5-720650555601` | WL-001 dispatched in the foreground workspace; self-contained WPF shell, core boundaries, tests, package, 125% render, and Windows CI job are implemented. Codex `WINDIR` friction is filed as `openai/codex#35545`. | Direct gates passed after LexRunner could not discover the explicit plan; publish the branch/PR, then collect clean-runtime and 100%/150%/200% manual evidence. |
| 6 | Foreground orchestrator | `frame-1785126039923-c7a31132-fed3-4ab5-a344-56cf1f1371e5` | WL-001 is committed at `955fc83` and published as draft PR #198 with launcher, package, native, and policy gates green. | Accept the CI artifact and collect clean-runtime plus 100%/150%/200% DPI, keyboard, and screen-reader evidence. |
| 7 | Foreground orchestrator | `frame-1785129383575-884820b0-a639-43bb-b81c-6e834535e91e` | WL-001 architecture direction accepted; Wave 2 promoted; WL-002 implements bounded official-settings/conventional/manual discovery, exact `prime.exe` validation, confirmed selection, and composable health. The real custom game path passed the visible UI smoke. | Complete gates and diff review, then publish WL-002 as a draft PR stacked on #198. |
| 8 | Foreground orchestrator | `frame-1785131509443-591c7f02-808c-4e51-8179-37facec9dadf` | Product UX direction accepted: compact outcome-first Home, adaptive schema-driven Settings workspace, first-class notification configuration, progressive redacted diagnostics, streamer-safe paths, and System/Light/Dark themes. LCARS is no longer a requirement. | Resume WL-002 using `UX_DIRECTION.md` as the UI contract; retain the internal health model while replacing the diagnostic-first presentation. |

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
