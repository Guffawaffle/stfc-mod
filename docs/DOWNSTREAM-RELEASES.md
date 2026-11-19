# Downstream releases

| Channel | Source | Package |
| --- | --- | --- |
| Release | `main` | Signed full release marked Latest |
| Pre | `play` | Signed pre-release |
| DEV | `play-dev` | Development package with optional AAI/UI Lab |

Integrate changes through `play`. Before merging a pull request, have the
assigned player smoke the exact candidate core CI payload. For a named Pre,
select the successful **push** Build for the intended `play` commit. For a
full Release, fast-forward `main` to the reviewed `play` commit and select the
successful **push** Build from `main` for that exact commit. Build eligibility
includes the producing branch, even when both branches point to the same SHA.
Normal core CI excludes optional AAI/UI Lab. Follow the separate package
workflow for [DEV builds](development-releases.md).
The downstream Windows and Mac jobs enable the pinned Profiles host and matching
helper. Upstream builds retain their ordinary defaults. Profiles does not change
ordinary launch: account isolation starts only when a named profile is selected.

The Mac installer includes the helper beside its library, plus source and license
notices. The standalone Mac archive must retain that same adjacent helper and
`notices/` directory. Windows retains its DLL-only updater download; the separate
`stfc-community-mod-profiles-windows-x64.zip` supplies the matching CLI and notices.
Do not replace the canonical Windows ZIP with the larger Profiles package.

## Signed publication

Signing stays on the existing trusted `ci/macos-signing-play` controller. The
generic upstream tag publisher is disabled for `Guffawaffle/stfc-mod`; pushing
a version tag must not publish the ordinary unsigned/ad-hoc CI payloads.

1. Verify the intended `main` or `play` push commit, successful platform jobs, run attempt and
   retained Windows and universal Mac installer artifacts.
   Retain the `stfc-community-mod-profiles-windows` artifact from that same run
   and attempt. Named Pre and Release signing require it; the controller verifies
   its manifest, pinned Profiles revision and exact match to the selected DLL.
2. Dispatch `validate-macos-signing.yaml` on the trusted controller with
   `operation=sign-run`, `platform=both`, the exact `build_run_id`, and
   `expected_head_sha`. Verify the signing run's controller commit before
   approving the existing `windows-release` and `macos-release` environments.
   Keep their reviewers and deployment policies intact. The controller checks the
   named channel's source branch again before signing. Use a planned tag with
   `prerelease=false` for Release and `prerelease=true` for Pre. An untagged
   `sign-run` remains available for signed CI test downloads.
3. The controller produces signed test-download artifacts without publishing.
   Keep those original input/signature receipts, including their purpose labels.
   Promotion to a release is a separate explicit operation after the remaining
   checks below; signing alone does not establish release readiness.
4. Verify the downloaded Windows publisher, timestamp and embedded build
   identity. Verify the Mac signing/notarization result, source/input identity,
   both architectures, final installer and matching signed library archive.
   Verify the Profiles helper's timestamped publisher signature on Windows and
   both helper slices' Developer ID signatures and Apple ticket coverage on Mac.
   Every helper must match the signed package's source receipt. Never execute a
   producer-supplied helper or script inside a signing job.
   Use the signed Windows core payload for the assigned player's exact-artifact
   smoke check. Do not substitute a locally built host-enabled DLL.
5. Package those same verified signed bytes under the stable Windows ZIP and
   native archive names, with checksums and original provenance. Preserve the
   controller's final notarized DMG; do not repack or re-sign it.
   Include the controller's additional signed Profiles ZIP as a separate download;
   its refreshed manifest records signed hashes and retains the unsigned input
   manifest and signing provenance. Do not reconstruct it from another build.
6. Create a new immutable `vX.Y.Z.W-guffa.N` tag on the tested source commit
   from the matching channel's push Build.
   Upload the complete signed assets to a draft release, verify the asset set,
   then publish with `prerelease=false` and explicitly mark it Latest. For a
   pre-release, use a fresh tag in the same format with `prerelease=true` and
   `make_latest=false`. Use those same identity inputs when signing. A later
   Release needs a new tag if its identity or signed bytes change.

Never move an existing published tag. Recheck source, run attempt and artifact
identity after any CI retry or approval delay. The controller refuses artifacts
from an earlier attempt; a successful job reused by a partial rerun is not a
newly produced artifact for the current attempt. Rebuild the required platform
payloads when needed rather than weakening that check.

Apple pending submissions retain their existing evidence/recovery path. Inspect
the saved submission before any new upload; do not automatically resubmit.
Mac gameplay testing remains separate from signing and notarization.

## Advance the in-game update index

Once the release is public and its Windows ZIP and Mac DMG are uploaded, run this
from a checkout based on the current `play` branch:

```sh
python3 scripts/update-release-index.py release v1.1.9.5-guffa.9
```

Use `pre` for a signed Pre release and `dev` for a tagged DEV release. The script
checks the public GitHub release and manifest, including both asset digests,
then updates the corresponding entry in `releases/update-index.json`. Merge that
index change into `play` to publish it. Advance the DEV entry from `play` after
the DEV workflow publishes the exact `-dev.N` tag.

Players can check from **Settings → Mod Settings → Updates**. Manual and
optional daily checks use the installed package's channel. **View release**
opens the release notes in the browser. An automatic check can show an in-game
notice once per newer version. **Options** opens the Updates page, where players
can turn automatic checks off or set **Ignore this version**. The ignore toggle
applies to the current available version on that channel. It starts off for a
newer version. Manual checks and **View release** remain available.
On macOS the app supplies the installed
package identity when launching the game. A standalone dylib without that
identity has no published update channel.
