# Downstream releases

`main` remains the default landing branch and tracks upstream `dev`. `play` is
the downstream integration and release source. Merging into `play` runs CI but
does not publish a release. Keep feature changes and fork release tooling out
of the upstream mirror.

## Publish a tested play commit

1. Merge the intended changes into `play`, wait for its **push** Build workflow
   to succeed, and play-test that exact commit. PR CI alone is not sufficient.
2. Create a new immutable tag on that commit, using `vX.Y.Z-guffa.N` (or
   `vX.Y.Z.W-guffa.N`). For a preview, append `-rc.N`.
3. Push the tag. The release workflow validates that it belongs to `play` and
   selects a successful push build for the exact commit. It reuses those
   artifacts; it does not rebuild or use the latest unrelated build.
4. Approve the existing `windows-release` environment deployment. Azure signs
   the Windows DLL. Publisher, public-trust certificate and timestamp checks
   must pass before any release is published.
5. Approve the `macos-release` environment deployment. The workflow signs the
   exact build's macOS library, loader and launcher with Developer ID, notarizes
   the app and installer, and staples both tickets. Signing, notarization and
   Gatekeeper verification must succeed before publication.
6. The workflow packages the signed DLL and the signed macOS artifacts.
   A regular downstream release is explicitly marked Latest; an `-rc.N` tag
   creates a prerelease and leaves Latest unchanged.

Use a new tag for each release. Do not move published tags. If the tag workflow
started before CI completed, rerun it after the exact push build succeeds.
If build artifacts have expired, rerun that exact build first.

Windows signing uses the existing `windows-release` environment variables:
`AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, `AZURE_SUBSCRIPTION_ID`,
`AZURE_TRUSTED_SIGNING_ENDPOINT`, `AZURE_CODE_SIGNING_ACCOUNT_NAME`,
`AZURE_CERTIFICATE_PROFILE_NAME`, and `WIN_PUBLISHER_NAME`. The environment must
allow the release tags (`v*`) and retain its approval requirement.

## macOS signing setup

Normal Build/PR jobs remain ad-hoc signed and have no signing credentials.
`sign-macos.yaml` reuses the installer from the exact successful `play` push
build, without rebuilding. Release publication requires both signing jobs.

Configure `macos-release` with the release reviewer and custom deployment
policies allowing tags `v*-guffa.*` and the commissioning branch
`ci/macos-signing-play`. Keep the reviewer approval requirement enabled.

Environment secrets:

- `MACOS_CERTIFICATE_P12_BASE64`: base64 of the password-protected Developer ID
  Application certificate and private key (PKCS12).
- `MACOS_CERTIFICATE_PASSWORD`: the PKCS12 password.
- `APPLE_APP_SPECIFIC_PASSWORD`: a dedicated Apple app-specific password for
  notarization, not the account login password.

Use a Keychain-compatible PKCS12 export. The macOS runner rejected the initial
AES256/PBES2 container; the same identity imports successfully when exported
with PKCS12 3DES/SHA1 protection. This changes the encrypted container format,
not the certificate or code-signing algorithm. Retain a strong password and
the original protected backup.

Environment variables:

- `MACOS_SIGNING_IDENTITY`: SHA-1 fingerprint of the Developer ID Application
  certificate, identifying the exact certificate to use.
- `APPLE_TEAM_ID`: the certificate's ten-character Team ID.
- `APPLE_ID`: the Apple account login used for notarization; it can differ from
  the email address on the original certificate request.

The job imports the identity into a temporary keychain, cleans it on success
or failure, and retains only signed artifacts and notarization evidence.
`macos-provenance.json` records the build source, signing workflow source,
input/output hashes and Apple's submission IDs. The standalone dylib archive
contains the same signed library accepted with the app; libraries cannot have
a stapled ticket and rely on Apple's online ticket lookup when needed.

`Validate macOS signing` checks the existing commissioning submission on branch
pushes. Manually dispatch with `operation=sign` to sign and submit the current
successful `play` build. This is an explicit new upload, not a status retry.
The signing operation produces downloadable CI
artifacts only: it does not tag, publish or change Latest. Inspect its evidence
and test the signed app on macOS before publishing the first notarized release.
The launcher keeps its existing entitlements and game-launch behavior; a
notarization success does not prove game loading or runtime hooks work.

If Apple leaves a submission pending beyond the bounded wait, publication
fails closed. Submission and waiting are separate steps: the ID is saved and
printed before waiting. Retrieve it from `macos-notarization-evidence` and
check its status before deciding to retry; a retry starts a new submission.
The `macos-notarization-payloads` artifact retains the exact signed uploads,
their SHA-256 hashes, source/build identity and submission receipts for 30 days,
including on failure. These are recovery inputs, not verified release assets.
Recover the accepted bytes and verify their provenance before completing
stapling/packaging; do not substitute a rebuilt or re-signed app. There is no
automatic resume or resubmission. Existing releases remain unchanged.

### Inspecting a pending submission

Use `Validate macOS signing` with `operation=status` and the existing Apple
`submission_id` to retrieve history, current status and the completed analysis
log when available. It uses a separate `macos-notary-status` environment without
required reviewers and does not sign or upload another payload. Restrict that
environment to the `ci/macos-signing-play` branch. Configure only `APPLE_ID`,
`APPLE_TEAM_ID` and the `APPLE_APP_SPECIFIC_PASSWORD` secret there; never copy
the Developer ID private key or PKCS12 password. The Apple credential itself
is not limited to read-only queries, so keep the branch restriction. Actual
signing remains behind the `macos-release` approval gate.
A successful status job means the query succeeded; read Apple's status in its
output or `macos-notarization-status` artifact for the actual verdict.

The same check can run on a Mac with Xcode command-line tools and `jq`:

```sh
# Follow the interactive prompts, including the app-specific password.
xcrun notarytool store-credentials stfc-notary
bash scripts/check-macos-notarization.sh <submission-id>
```

Set `NOTARY_PROFILE` or `NOTARY_KEYCHAIN` if using a different local Keychain
profile. This check does not require the signing private key. `notarytool` is
not available on Windows; Apple's separate Notary REST API supports clients
on other platforms but requires API-key authentication.

## Stable links

- [Latest release](https://github.com/Guffawaffle/stfc-mod/releases/latest)
- [Windows ZIP](https://github.com/Guffawaffle/stfc-mod/releases/latest/download/stfc-community-mod.zip)
- [macOS installer](https://github.com/Guffawaffle/stfc-mod/releases/latest/download/stfc-community-mod-installer.dmg)

The default branch does not determine these links. Stable asset names are kept
between releases. Verify updater repository/channel settings separately before
promising that an installed downstream build will update from this fork.

## Updating the landing branch

Fetch upstream `dev`, inspect the incoming changes, then fast-forward fork
`main` to that exact commit through the normal protected-branch process. Avoid
merge or squash commits if `main` is to remain an exact upstream mirror.
