# Signed upstream Windows test releases

The fork-only `signed-upstream-test.yaml` workflow adds Azure Authenticode signing to an
existing upstream `main` DLL. It does not build fork code or change the normal release channel.

1. Select a successful `netniV/stfc-mod` main push build with an unexpired
   `stfc-community-mod` artifact and GitHub provenance attestation.
2. On a signing branch, update the workflow's default commit and run ID (both dispatch
   defaults and tag-trigger fallbacks). Review the workflow changes before tagging.
3. Push a new `vX.Y.Z.W-sac-test.N` tag at the reviewed signing-workflow commit.
   `X.Y.Z.W` must match the upstream DLL file version. Do not reuse a published tag.
4. Review the verified upstream commit/hash in the preparation job, then approve the
   existing `windows-release` environment gate.
5. The workflow signs with the existing Azure OIDC identity, verifies RSA/Public Trust,
   publisher, timestamp and Windows signature validity, then publishes a prerelease
   without changing GitHub's latest release.

The environment needs `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, `AZURE_SUBSCRIPTION_ID`,
`AZURE_TRUSTED_SIGNING_ENDPOINT`, `AZURE_CODE_SIGNING_ACCOUNT_NAME`,
`AZURE_CERTIFICATE_PROFILE_NAME`, and `WIN_PUBLISHER_NAME`. The federated identity uses
the existing `windows-release` environment; no new credentials or branch exceptions are needed.

Manual dispatch is available once the workflow exists on the default branch. Dispatch
must select an existing test tag, with an exact upstream commit and run ID. Re-running
publication against an existing release fails rather than replacing its assets.

The release includes the signed `version.dll`, a ZIP containing that DLL, provenance
with unsigned/signed SHA-256 values, and asset checksums. Its tag identifies the signing
workflow source; `provenance.json` identifies the actual upstream mod source.

For the player: close the game, back up the existing DLL, install the test DLL, and
launch with Smart App Control enabled. Record the result and any CodeIntegrity block.
Restoring the backup undoes the test; a mod updater may overwrite the test DLL.
