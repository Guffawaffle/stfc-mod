# STFC AXF Provider

Linux-native command entrypoint for this repo:

```sh
axf run stfc-mod doctor --json
axf run stfc-mod windows-interop --json
axf run stfc-mod status --summary --json
axf run stfc-mod pure-tests --json
axf run stfc-mod battle-log --summary-only --no-build --json
axf run stfc-mod cycle --json
```

The AXF family and provider adapter live in `/srv/axf`:

- `/srv/axf/manifests/families/stfc-mod.family.json`
- `/srv/axf/adapters/stfc/`

`build`, `deploy`, and `cycle` run through Windows interop. The provider
syncs this WSL checkout to `/mnt/d/dev/stfc-mod-interop`, launches
Windows PowerShell from WSL, sets `AX_REPO_ROOT` to that Windows mirror,
then calls the reference `.ax` dispatcher at
`/mnt/d/dev/stfc-mod/.ax/ax.ps1`. The existing Windows worktree at
`/mnt/d/dev/stfc-mod` is not used as the build root.

The dump query commands currently run the existing Python dump tools from
`/mnt/d/dev/stfc-mod/.ax` and use that reference cache. The pure decoder
validation path is native to `/srv/stfc-mod`.
