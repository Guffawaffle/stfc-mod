# Publish a DEV package

The fork's `play-dev` branch builds the explicit AAI development product. Every
push runs the **Development package** workflow and retains a preview package.
A tag such as `v1.1.9.5-guffa.6-dev.1` publishes the same product as a GitHub
prerelease. DEV uses the violet launcher icon and the complete tag in its display
version. Core PR builds retain their CI identity and require no private source.

## Configure source access once

As an administrator of both fork repositories, review and run
`scripts/setup-dev-source-access.ps1` in PowerShell 7. It creates a dedicated
read-only deploy key on `Guffawaffle/stfc-aai`, and stores its private half as
`AAI_SOURCE_READ_KEY` in the fork's `stfc-dev-source` Actions environment. That
environment permits only the `play-dev` branch and `v*-dev.*` tags. The script
deletes its temporary key files and refuses to overwrite an existing environment.
Do not put a personal access token or private key in the source tree.

The source pin in `zig/aai.zig` must be a full commit available from the private
remote. Publish a signed AAI commit to a retained branch before changing the pin.
The DEV pin is `d7713b6ccb24c85713ec5c7abb097ef3dbfe210b`, retained by
`feat/client275-damage-channels`. It retains the portable-fixture baseline and
adds client-275 Polaron/Infusion source channels, exact incoming timeline views
and separately qualified shield-rule catalog context. Effective shield
mitigation/floor percentages remain unavailable when the report does not supply
them. The workflow checks out that exact commit outside the
mod checkout. Builds explicitly select `aai-host`, `aai-ui-lab` and diagnostic
patch toggles; dirty-source overrides and automatic local source discovery are
not used. Neither private sources nor build caches are uploaded as artifacts.

## Publish

1. Merge reviewed changes into `play-dev`. Its **Build** run checks the core
   product; **Development package** also offers an untagged AAI preview for testing.
   Every tagged run repeats the full native builds and tests before publication,
   including private source access, DEV metadata, icons, and packaging.
2. Choose an unused `v<four-part-base>-guffa.<fork-revision>-dev.<sequence>` tag.
   Its base and fork revision must match `src/mod/version.h`; the sequence must
   be a positive canonical integer through 4294967295.
3. Create and push a signed annotated tag on that exact `play-dev` tip:

   ```sh
   git tag -s v1.1.9.5-guffa.6-dev.1 -m "DEV package 1.1.9.5-guffa.6-dev.1" <commit>
   git push origin refs/tags/v1.1.9.5-guffa.6-dev.1
   ```

4. Wait for the tag's **Development package** run. It builds from the tag,
   assembles the universal Mac app, packages an explicit asset allowlist, and
   checks SHA-256 digests before publishing. The release remains a prerelease
   and never becomes Latest. Its `dev-release.json` records source, AAI pin,
   build run, mode and payload hashes; `SHA256SUMS` also covers that manifest.

The full Mac installer includes the DEV app and icon. The standalone Mac archive
contains only the dylib. Windows packages contain only `version.dll`. Windows
DEV binaries are unsigned; Mac apps have only the ad hoc signature required for
local execution and are not notarized. This workflow has no Apple or Azure
release-signing environment or credentials.

A failed publication can be rerun while its release is still a matching draft.
Already published releases are never overwritten. Fix a published package by
using a new DEV sequence. Keep the source branch at the selected commit until
tag selection completes; a stale tag fails before private credentials are used.
