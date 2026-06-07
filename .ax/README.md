# Public `.ax` Facade

This tracked `.ax` folder is a small public facade for the STFC AXF surface in
this repo.

What is public here:

- `.ax/ax.ps1`: stable entrypoint used by [`axf.workspace.json`](/d:/dev/stfc-mod/axf.workspace.json)
- this README
- the repo-owned AXF manifests under [`manifests/`](/d:/dev/stfc-mod/manifests)

What stays local/private:

- the working AX tool repo in `.ax-priv/`
- local caches, dump indexes, tokens, machine-specific scripts, and scratch data

How it works:

1. AXF imports `.ax/ax.ps1` from the repo root.
2. The tracked wrapper delegates to `.ax-priv/ax.ps1` when that private repo is
   present.
3. Repo-owned command metadata lives in `manifests/` and the provider notes live
   in [`scripts/axf/README.md`](/d:/dev/stfc-mod/scripts/axf/README.md).

This split keeps the MCP-visible entrypoint stable without checking private
automation or data into the repo.
