# CI cache retention

GitHub's default cache allowance is 10 GB per repository. Compiler snapshots use
commit keys so new builds can refresh their contents, but retaining every snapshot
can evict dependencies that take much longer to rebuild.

Windows sccache is capped at 512 MiB. Its v2 schema avoids restoring the previous
2 GiB snapshots. macOS protobuf sccache is capped at 256 MiB per architecture;
the existing snapshots were below that size. Dependency and tool cache keys are
unchanged, including SPUD's package inputs.

PR builds restore available compiler snapshots but do not upload new ones. They
can still populate missing dependency and tool caches within their own PR scope.
GitHub does not let `play` restore PR-scoped or sibling `play-dev` caches.

After successful Windows and macOS builds on a fork branch push, a separate job
keeps the newest snapshot in each of five compiler-cache families on that branch.
It never selects dependency, tool, Swift module, another branch, PR or tag caches.
Only this maintenance job receives `actions: write`; build and PR jobs keep their
existing permissions. A maintenance failure does not invalidate build artifacts.

Inspect the plan locally with an authenticated GitHub CLI:

```powershell
python scripts/prune-compiler-caches.py --repo Guffawaffle/stfc-mod --ref refs/heads/play
```

Add `--apply` to remove the listed obsolete compiler snapshots. Cleanup requires
Actions write access. An initial small compiler cache may cause compilation misses;
it does not require changing or rebuilding the dependency cache deliberately.

This policy reduces storage pressure but does not guarantee cache hits. GitHub
may still evict caches, and package or toolchain changes can require new entries.

## Measuring effectiveness

Each platform job writes a **Cache effectiveness** section to the Actions summary:
Setup XMake, Configure, Build and individual test timings; exact/fallback hits,
no restore, failure or skipped outcomes with requested and matched keys;
repository inventory by ref; and local
compiler cache sizes after building. These read-only reports are best effort and
cannot fail the build.

Compare Configure time and dependency restores across equivalent warm runs before
judging the policy by compiler hits alone. Check requested keys against past runs
for dependency/key changes, and look at the inventory scopes for inaccessible
sibling or PR caches. To establish eviction, compare a prior cache inventory with
a later one; no restore alone does not identify its cause. Check restore logs to
distinguish missing entries, download errors and access restrictions. Cache versions and scopes
also restrict access even when an identical text key exists.
