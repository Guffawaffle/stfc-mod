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
global.stfc-mod-private.review-contract
global.stfc-mod-private.release-preflight
global.stfc-mod-private.doctor
global.stfc-mod-private.windows-interop
global.stfc-mod-private.status
global.stfc-mod-private.pure-tests
global.stfc-mod-private.battle-log
global.stfc-mod-private.cycle
```

Raw CLI execution is for provider development and manual debugging only.

## Review Contract

Use `global.stfc-mod-private.review-contract` as the normal blocking agent review gate
for code and hook-adjacent branches. It runs:

- `scripts/scan_gameplay_seams.py` against the unmanaged gameplay seam baseline
- `git diff --check`
- `git diff --cached --check`
- AX `pure-tests`

The gameplay seam scanner is blocking in this contract. The standalone scanner
command remains useful for focused local checks, but review-ready branches should
use the AXF review contract so the scanner is not skipped.

## Release Preflight

Use `global.stfc-mod-private.release-preflight` before pushing a fork release
tag. The default mode is a dry run. It verifies the local GitHub repo target,
the freshly fetched `origin/main` target SHA, the proposed `vX.Y.Z-guffa.N`
tag, a successful matching `Build` workflow run from a push to `main`, expected
release artifacts, and explicit smoke-test acknowledgement.

Only pass `-PushTag` after the dry run is clean and the exact production
artifact has been smoked. The release GitHub Actions workflow remains
responsible for publishing release assets after the tag is pushed.

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

The dump query commands currently run the existing Python dump tools from
`/mnt/d/dev/stfc-mod/.ax-priv` and use that reference cache. The pure decoder
validation path is native to `/srv/stfc-mod`.
