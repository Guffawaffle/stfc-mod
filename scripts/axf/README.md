# STFC AXF Provider

Agent-facing validation should go through the AXF MCP router, not through raw
`axf` shell commands. In this repo, the primary AXF surface is now declared
locally from `axf.workspace.json` and imported from `.ax/ax.ps1`. Use MCP
`operation=list`, then `operation=inspect`, then `operation=run` against the
`global.stfc-mod-private.*` capability IDs.

The tracked `.ax/` folder is a small public facade only. The working private AX
repo can live in `.ax-priv/`, and the tracked wrapper delegates there when it
is present.

Primary capability IDs include:

```text
global.stfc-mod-private.agent-brief
global.stfc-mod-private.agent-worktree
global.stfc-mod-private.review-contract
global.stfc-mod-private.release-preflight
global.stfc-mod-private.release-withdrawal
global.stfc-mod-private.doctor
global.stfc-mod-private.windows-interop
global.stfc-mod-private.status
global.stfc-mod-private.pure-tests
global.stfc-mod-private.battle-log
global.stfc-mod-private.cycle
global.stfc-mod-private.corpus-status
```

Raw CLI execution is for provider development and manual debugging only.

## Agent Briefs

Use `global.stfc-mod-private.agent-brief` to create scoped task briefs for
background agents. Briefs default to read-only scout/review work and state the
repo context, objective, allowed actions, forbidden actions, and expected
handoff shape.

The bridge/orchestrator remains responsible for mutations: edits, branch moves,
GitHub writes, releases/tags, runtime actions, final validation, and Lex-frame
logging. See `docs/AGENT_ORCHESTRATION.md` for the full workflow.

Use `global.stfc-mod-private.agent-worktree` when a background agent needs an
isolated mutation lane. The bridge creates the lease, the agent works inside the
leased worktree, and the bridge collects the handoff before cleanup. See
`docs/AGENT_WORKTREE_BROKER.md`.

## Review Contract

Use `global.stfc-mod-private.review-contract` as the normal blocking agent review gate
for code and hook-adjacent branches. It runs:

- `scripts/validate_hook_support_tiers.py` against the hook support-tier manifest
- `scripts/scan_gameplay_seams.py` against the unmanaged gameplay seam baseline
- `git diff --check`
- `git diff --cached --check`
- AX `pure-tests`

The hook support-tier validator and gameplay seam scanner are blocking in this
contract. The standalone scanner commands remain useful for focused local
checks, but review-ready branches should use the AXF review contract so these
gates are not skipped.

## Release Preflight

Use `global.stfc-mod-private.release-preflight` before pushing a fork release
tag. The default mode is a dry run. It verifies the local GitHub repo target,
the freshly fetched `origin/main` target SHA, the proposed `vX.Y.Z-guffa.N`
tag, a successful matching `Build` workflow run from a push to `main`, expected
release artifacts, and explicit smoke-test acknowledgement.

Only pass `-PushTag` after the dry run is clean and the exact production
artifact has been smoked. The release GitHub Actions workflow remains
responsible for publishing release assets after the tag is pushed.

## Release Withdrawal

Use `global.stfc-mod-private.release-withdrawal` when a published fork release
needs to be superseded, marked known-bad, or yanked. The default mode is a dry
run. Dry runs and executed actions require a reason. Executed actions append a
repo-local JSONL record under `docs/release-withdrawals/`.

The `yanked` state deletes the GitHub release and remote tag. Use it only after
reviewing the dry-run output and confirming that preserving the artifact is worse
than losing public release/tag history.

For AXF 1.0.0 and later, this repo owns its STFC family manifests locally.
Run `axf scout --write` from the repo root to regenerate
`manifests/families/stfc-mod-private.family.json` and any standalone capabilities
through the `.ax/ax.ps1` facade. If you want to share the same family across
multiple repos, place that shared pack on `AXF_MACHINE_ROOT`; project-local
manifests still win.

`build`, `deploy`, and `cycle` run through Windows interop. The provider
syncs this WSL checkout to `/mnt/d/dev/stfc-mod-interop`, launches
Windows PowerShell from WSL, sets `AX_REPO_ROOT` to that Windows mirror,
then calls the private dispatcher at `/mnt/d/dev/stfc-mod/.ax-priv/ax.ps1`
through the tracked `.ax/ax.ps1` facade. The existing Windows worktree at
`/mnt/d/dev/stfc-mod` is not used as the build root.

Successful build/deploy/cycle receipts include `sourceProvenance`. The provider
uses one Git tracked-plus-untracked-nonignored NUL-delimited manifest for both
source identity and Windows synchronization. It materializes that manifest in a
fresh mirror with a new Git checkout, verifies the staged fingerprint, and
fails if the source changes during synchronization. Ignored/private files are
neither synchronized nor disclosed, and dirty initialized submodules fail
closed. The Windows-only mirror explicitly omits the clean, commit-verified
`macos-launcher/deps/PLzmaSDK` submodule because its test corpus contains
Unicode-normalization-equivalent names that cannot enter the Windows manifest;
the exclusion is disclosed in the receipt.

Clean receipts identify a reproducible commit. Dirty receipts distinguish the
HEAD base commit from a deterministic `sourceStateId`, include a
privacy-bounded ordered path summary with truncation metadata, and retain the
private dispatcher’s distinct build/deployed artifact hashes. Legacy
dispatcher commit fields must agree with the canonical base commit. Diffs and
source bodies are never returned.

Each Windows build/deploy/cycle also receives one `ax:<uuid>` build invocation
ID. XMake embeds it with the canonical source state in the DLL version resource.
AX reads the built DLL metadata back—and the deployed DLL for deploy/cycle—and
rejects a receipt when either identity differs. The mod logs that immutable
build ID plus a separate runtime launch UUID on each game-process initialization.
See `docs/WINDOWS_DLL_IDENTITY.md` for the consumer contract.

The dump query commands currently run the existing Python dump tools from
`/mnt/d/dev/stfc-mod/.ax-priv` and use that reference cache. The pure decoder
validation path is native to `/srv/stfc-mod`.

## Canonical IL2CPP Corpus

Raw human and agent searches must use:

```text
tools/il2cpp-dump/dump.cs
tools/il2cpp-dump/script.json
```

The matching query index is `.ax-priv/cache/stfc.db`. Files under
`.ax-priv/tools/Il2CppDumper/` are private tool artifacts, not research inputs,
even when their names look authoritative.

Run `global.stfc-mod-private.corpus-status` before raw dump research. It reports
the canonical paths and index metadata, fingerprints plausible legacy copies,
and classifies each copy as identical, stale, divergent-newer, or legacy-only.
The command never returns dump contents. Use `global.stfc-mod-private.dump-refresh`
to regenerate the canonical corpus.
