# STFC AXF Provider

Agent-facing validation should go through the AXF MCP router, not through raw
`axf` shell commands. In this repo, the primary AXF surface is now declared
locally from `axf.workspace.json` and imported from `.ax/ax.ps1`. Use MCP
`operation=list`, then `operation=inspect`, then `operation=run` against the
`global.stfc-mod.*` capability IDs.

Primary capability IDs include:

```text
global.stfc-mod.doctor
global.stfc-mod.windows-interop
global.stfc-mod.status
global.stfc-mod.pure-tests
global.stfc-mod.battle-log
global.stfc-mod.cycle
```

Raw CLI execution is for provider development and manual debugging only.

For AXF 1.0.0 and later, this repo owns its STFC family manifests locally.
Run `axf scout --write` from the repo root to regenerate
`manifests/families/stfc-mod.family.json` and any standalone capabilities from
the `.ax` inventory. If you want to share the same family across multiple repos,
place that shared pack on `AXF_MACHINE_ROOT`; project-local manifests still win.

`build`, `deploy`, and `cycle` run through Windows interop. The provider
syncs this WSL checkout to `/mnt/d/dev/stfc-mod-interop`, launches
Windows PowerShell from WSL, sets `AX_REPO_ROOT` to that Windows mirror,
then calls the reference `.ax` dispatcher at
`/mnt/d/dev/stfc-mod/.ax/ax.ps1`. The existing Windows worktree at
`/mnt/d/dev/stfc-mod` is not used as the build root.

The dump query commands currently run the existing Python dump tools from
`/mnt/d/dev/stfc-mod/.ax` and use that reference cache. The pure decoder
validation path is native to `/srv/stfc-mod`.
