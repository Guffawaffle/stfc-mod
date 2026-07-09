# Background Agent Orchestration

This workspace can use background agents as bounded scouts while one main agent
acts as the bridge/orchestrator. The model is fan out, gather evidence, then
merge the findings through one accountable operator.

This is local workflow tooling only. It does not belong in mod runtime code, the
game client, the sidecar, or release artifacts.

## Roles

### Bridge

The bridge is the main agent in the active session. The bridge owns:

- branch selection and branch changes
- file edits
- conflict resolution
- validation
- commits and pushes
- PR creation, updates, and merges
- tags, releases, and release withdrawal actions
- game/client/sidecar cycling
- Lex-frame logging and durable handoff notes

The bridge may delegate investigation, but the bridge remains accountable for
every mutation and final claim.

### Background Agent

A background agent is an assigned scout or reviewer. It defaults to read-only.
It should return evidence and recommendations, not hidden state changes.

Background agents are useful for:

- PR review comment triage
- branch and feature archaeology
- public/private reconciliation planning
- hook-manifest inventory
- release/readiness audits
- risk review of a small code area before the bridge edits it

## Default Boundaries

Unless a task brief explicitly says otherwise, a background agent must not:

- create, edit, move, or delete files
- run `git switch`, `git checkout`, `git rebase`, `git merge`, `git reset`, `git commit`, `git tag`, or `git push`
- create or mutate GitHub issues, PRs, comments, labels, releases, or tags
- cycle STFC, the sidecar, or runtime processes
- edit TOML or game files
- create sibling clones or worktrees
- inspect secrets, tokens, or unrelated private files

The bridge can authorize a narrower exception, but the exception must appear in
the brief.

## Task Brief

Every background-agent task should include:

- objective
- issue or PR reference
- current repo, branch, and head SHA
- allowed scope
- questions to answer
- allowed actions
- forbidden actions
- expected handoff shape

Use `global.stfc-mod-private.agent-brief` to generate a consistent brief:

```powershell
.\scripts\axf\agent-brief.ps1 `
  -Objective "Audit PR #148 for release-policy review risks" `
  -Mode review `
  -Issue "#148" `
  -Scope "docs/RELEASE_WITHDRAWAL_POLICY.md; scripts/axf/release-withdrawal.ps1" `
  -Questions "Are destructive actions clearly gated?; Are docs and manifest consistent?"
```

The command emits JSON and a ready-to-paste markdown brief. It can optionally
write the markdown to a repo-local path with `-OutputPath`, but stdout-only use
is preferred during normal sessions.

## Handoff Shape

Background-agent handoffs should be concise and structured:

- `summary`: what was inspected and learned
- `findings`: actionable issues with file/line evidence
- `risks`: behavior, release, validation, or branch risks
- `openQuestions`: only material blockers
- `commandsRun`: commands or API reads used
- `filesRead`: important files inspected
- `recommendedNextSteps`: concrete next moves for the bridge
- `mutationConfirmation`: whether any mutations were made

If a background agent cannot answer without mutation, it should stop and report
the needed permission instead of guessing.

## Bridge Workflow

1. Confirm the repo and branch are clean enough to delegate from.
2. Generate a brief with `global.stfc-mod-private.agent-brief`.
3. Assign one bounded question per background agent when possible.
4. Keep background agents read-only by default.
5. Collect handoffs and reconcile conflicts in the main session.
6. Make any edits directly as the bridge.
7. Run the relevant AXF validation.
8. Record the outcome in GitHub, PR notes, or Lex-frame entries as appropriate.

This keeps fanout useful without letting parallel work fragment branch state.

## STFC Examples

### PR Review

Assign one agent to inspect review threads and another to inspect the affected
files. The bridge decides which comments are actionable and pushes the fix.

### Public/Private Reconciliation

Assign agents to compare branch histories, changed files, and upstream deltas.
The bridge decides merge order and performs the actual rebase or merge.

### Hook Manifest Inventory

Assign agents by hook family, such as hotkeys, fleet runtime, notifications, or
Kir'Shara/action queue. Each agent returns tier suggestions and evidence. The
bridge owns the manifest edit and release validation.

### Release Work

Background agents may audit release workflow state, artifact names, and docs.
The bridge owns release-preflight, tag pushes, release edits, and withdrawal
actions.
