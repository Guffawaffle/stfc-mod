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
5. The workflow packages the signed DLL and the same build's macOS artifacts.
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

macOS packaging currently has an ad-hoc signature. Apple Developer ID signing
and notarization are not configured by this workflow.

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
