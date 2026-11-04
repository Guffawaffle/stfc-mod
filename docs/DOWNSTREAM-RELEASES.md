# Downstream releases

`play` is the fork's integration and release source. Promote the reviewed typed
snapshot through a pull request, then use the successful **push** Build for that
exact merged commit. A pull-request build or an older branch build does not
qualify the merged source. Normal core CI excludes optional AAI/UI Lab.

## Signed publication

Signing stays on the existing trusted `ci/macos-signing-play` controller. The
generic upstream tag publisher is disabled for `Guffawaffle/stfc-mod`; pushing
a version tag must not publish the ordinary unsigned/ad-hoc CI payloads.

1. Verify the intended `play` commit, successful platform jobs, run attempt and
   retained Windows and universal Mac installer artifacts.
2. Dispatch `validate-macos-signing.yaml` on the trusted controller with
   `operation=sign-run`, `platform=both`, the exact `build_run_id`, and
   `expected_head_sha`. Verify the signing run's controller commit before
   approving the existing `windows-release` and `macos-release` environments.
   Keep their reviewers and deployment policies intact. The commissioning
   controller at this change is `735c6149d2b59cbc8b216e623de4c3f246096bd8`.
3. The controller produces signed test-download artifacts without publishing.
   Keep those original input/signature receipts, including their purpose labels.
   Promotion to a release is a separate explicit operation after the remaining
   checks below; signing alone does not establish release readiness.
4. Verify the downloaded Windows publisher, timestamp and embedded build
   identity. Verify the Mac signing/notarization result, source/input identity,
   both architectures, final installer and matching signed library archive.
   Use the signed Windows core payload for the assigned player's exact-artifact
   smoke check. Do not substitute a locally built host-enabled DLL.
5. Package those same verified signed bytes under the stable Windows ZIP and
   native archive names, with checksums and original provenance. Preserve the
   controller's final notarized DMG; do not repack or re-sign it.
6. Create a new immutable `vX.Y.Z[.W]-guffa.N` tag on the tested `play` commit.
   Upload the complete signed assets to a draft release, verify the asset set,
   then publish with `prerelease=false` and explicitly mark it Latest. An
   `-rc.N` publication remains a prerelease and leaves Latest unchanged.

Never move an existing published tag. Recheck source, run attempt and artifact
identity after any CI retry or approval delay. The controller refuses artifacts
from an earlier attempt; a successful job reused by a partial rerun is not a
newly produced artifact for the current attempt. Rebuild the required platform
payloads when needed rather than weakening that check.

Apple pending submissions retain their existing evidence/recovery path. Inspect
the saved submission before any new upload; do not automatically resubmit.
Mac gameplay testing remains separate from signing and notarization.
