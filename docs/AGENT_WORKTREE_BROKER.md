# Background Agent Worktree Broker

Worktrees are the mutation lane for background agents. The bridge creates a
lease, hands the worktree path to a background agent, collects the handoff, and
then cleans the lease up.

Background agents should not create their own worktrees. They should work inside
a lease created by the bridge.

## What Worktrees Isolate

Git worktrees isolate:

- working files
- the Git index
- untracked files inside that worktree
- branch checkout state

They do not isolate:

- GitHub writes
- tags and releases
- shared remotes and refs
- the game install
- STFC or sidecar processes
- TOML runtime configuration
- global build caches or external tools

Runtime, release, and GitHub mutation lanes remain bridge-owned unless the lease
explicitly says otherwise.

## Command

Use `global.stfc-mod-private.agent-worktree`, or run:

```powershell
.\scripts\axf\agent-worktree.ps1 -Action list
```

The default worktree root is a sibling directory:

```text
D:\dev\stfc-mod-agent-worktrees
```

Lease state is local-only and ignored by Git:

```text
.ax/agent-worktrees/leases.jsonl
```

## Create A Lease

Dry-run first:

```powershell
.\scripts\axf\agent-worktree.ps1 `
  -Action create `
  -LeaseId hook-tier-scout `
  -AgentName HookTierScout `
  -Objective "Prototype hook support-tier manifest changes" `
  -Issue "#145" `
  -BaseRef HEAD `
  -AllowedScope "manifests; docs; scripts/axf" `
  -AllowEdits
```

Execute only after the planned path, branch, and scope look right:

```powershell
.\scripts\axf\agent-worktree.ps1 `
  -Action create `
  -LeaseId hook-tier-scout `
  -AgentName HookTierScout `
  -Objective "Prototype hook support-tier manifest changes" `
  -Issue "#145" `
  -BaseRef HEAD `
  -AllowedScope "manifests; docs; scripts/axf" `
  -AllowEdits `
  -Execute
```

The broker creates the worktree and writes `.agent-worktree/AGENT_LEASE.md`
inside it.

## Inspect A Lease

```powershell
.\scripts\axf\agent-worktree.ps1 -Action status -LeaseId hook-tier-scout
```

Use status before cleanup. If the worktree is dirty, collect the handoff or diff
before removing it.

## Cleanup

Dry-run cleanup first:

```powershell
.\scripts\axf\agent-worktree.ps1 -Action cleanup -LeaseId hook-tier-scout
```

Execute cleanup after collecting the handoff:

```powershell
.\scripts\axf\agent-worktree.ps1 `
  -Action cleanup `
  -LeaseId hook-tier-scout `
  -Execute
```

Dirty worktrees block cleanup, including tracked, untracked, and ignored files
outside the broker-owned lease brief. Use `-Force` only after the bridge has
collected or intentionally discarded the worktree handoff:

```powershell
.\scripts\axf\agent-worktree.ps1 `
  -Action cleanup `
  -LeaseId hook-tier-scout `
  -Force `
  -Execute
```

Cleanup first verifies the recorded path is still a registered worktree for this
repository and still on the recorded lease branch. It then removes only
broker-owned lease files, removes the worktree, and records a cleanup event. It
does not delete the local branch by default; the bridge keeps that branch
available for review, cherry-pick, or manual deletion after integration.

## Bridge Rule

The bridge owns final integration. A background agent may leave a diff or a
commit in a leased worktree only when the lease allows it. The bridge decides
whether to copy, cherry-pick, reimplement, or discard that work.
