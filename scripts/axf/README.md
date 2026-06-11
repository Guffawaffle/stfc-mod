# STFC AXF Provider

Agent-facing validation should go through the AXF MCP router, not through raw
`axf` shell commands. In this repo, the primary AXF surface is now declared
locally from `axf.workspace.json` and imported from `.ax/ax.ps1`. Use MCP
`operation=list`, then `operation=inspect`, then `operation=run` against the
`global.stfc-mod.*` capability IDs.

The tracked `.ax/` folder is a small public facade only. The working private AX
repo can live in `.ax-priv/`, and the tracked wrapper delegates there when it
is present.

Primary capability IDs include:

```text
global.stfc-mod.review-contract
global.stfc-mod.doctor
global.stfc-mod.windows-interop
global.stfc-mod.status
global.stfc-mod.pure-tests
global.stfc-mod.battle-log
global.stfc-mod.cycle
```

Raw CLI execution is for provider development and manual debugging only.

## Review Contract

Use `global.stfc-mod.review-contract` as the normal blocking agent review gate
for code and hook-adjacent branches. It runs:

- `scripts/scan_gameplay_seams.py` against the unmanaged gameplay seam baseline
- `git diff --check`
- `git diff --cached --check`
- AX `pure-tests`

The gameplay seam scanner is blocking in this contract. The standalone scanner
command remains useful for focused local checks, but review-ready branches should
use the AXF review contract so the scanner is not skipped.

## Collaboration Contract

Use `docs/AXF_COLLABORATION_CONTRACT.md` as the lightweight shared vocabulary for
STFC missions, reports, and reviews. It defines gameplay seams, seam owners,
evidence surfaces, subscribers, provenance, drift, classifications, and parked
follow-ups.

This is not a second blocking gate and should not turn every report into
ceremony. It is the preferred place to route information when the concept
applies: live game work through gameplay seams, copied game facts through
evidence surfaces, old probe paths through explicit classification, changed
mission shape through drift, and useful out-of-scope ideas through parked
follow-ups.

For AXF 1.0.0 and later, this repo owns its STFC family manifests locally.
Run `axf scout --write` from the repo root to regenerate
`manifests/families/stfc-mod.family.json` and any standalone capabilities
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
