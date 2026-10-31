# Optional signing of a CI build

Use the existing trusted signing branch, ci/macos-signing-play, to sign an
explicit Build run with available artifacts. Choose macOS, Windows or both. This produces
test downloads without a release tag, release publication or latest-channel
change. Normal pushes do not start signing.

```powershell
gh workflow run validate-macos-signing.yaml --repo Guffawaffle/stfc-mod `
  --ref ci/macos-signing-play `
  -f operation=sign-run -f build_run_id=<run-id> -f platform=both `
  -f expected_head_sha=<full-run-head-sha>
```

expected_head_sha is an optional cross-check; use the Build run's HEAD, not a
moving branch name. The workflow form offers the same inputs. Environment
approval remains required independently for windows-release and macos-release.
The existing sign operation still selects a successful play push for Mac
release qualification. status and push-triggered Apple status checks remain.
The obsolete hard-coded Ripper test run is replaced by sign-run.

## Eligible inputs and what signing means

Any branch's started same-repository .github/workflows/ci.yaml run can supply
retained artifacts. Supported events are push, workflow_dispatch and same-repo
pull_request. Fork runs, other workflows, expired/ambiguous artifacts and changed
selection identities are refused. The selected platform must have produced its
expected payload: version.dll or stfc-community-mod-installer.dmg.

Test signing records the actual Build status/conclusion, including failure or
unfinished jobs elsewhere in that run. An available Mac installer does not need
to wait for Windows compilation. Missing requested artifacts still fail selection;
there is no silent platform fallback. Signing does not turn checks green or establish gameplay safety.
Release signing retains the existing successful play-push and ancestry rules.

Selection binds repository, run, attempt, HEAD, artifact ID and GitHub ZIP digest.
The explicit attempt endpoint supplies its start time; artifacts predating that
attempt are refused. Reusable signing jobs recheck the selection after approval.
The exact immutable ZIP is downloaded by ID, SHA256 verified and extracted with
only the expected regular file admitted. Producer scripts are never checked out
or executed with signing credentials. A rerun or expired input requires a fresh
selection; no silent latest-artifact or different-source substitution.

PR run HEAD is not necessarily the checkout/merge commit. Mac receipts retain
that as buildRunHeadSha and leave sourceCommit unknown for PR input rather than
inventing compile provenance. Windows additionally verifies the embedded source,
base, producing run/attempt and CI channel. A different PR embedded source must
be a repository commit with the selected run HEAD as a direct parent; the
relationship is recorded separately from the developer signature.

## Outputs and signing ownership

- signed-windows-test-<run-id>: timestamped Azure-signed version.dll and input /
  signature provenance. Existing publisher, RSA and Public Trust EKU checks stay.
- signed-macos-test-<run-id>: Developer ID-signed, Apple-notarized/stapled installer,
  matching signed dylib archive/checksum, Apple provenance and CI input receipt.
- Apple submission evidence and exact submitted payloads retain the existing
  recovery path when Apple's verdict is delayed. Do not resubmit automatically.

Only the trusted signing branch hosts optional signing. windows-release needs
that exact branch in its deployment allowlist, matching the existing Mac gate.
Required reviewers remain; Azure retains its existing windows-release OIDC
subject. No producer branch receives signing credentials or new Azure authority.
Release workflows and their signed release artifact names are unchanged.

The CI workflow on play-dev-ripper now accepts manual dispatch, so a new candidate
can be built without opening a PR or changing play. Its normal feature selection
still applies: native AAI host builds require their explicit source/host options;
an ordinary CI artifact is not silently relabeled as an AAI-enabled build.

## Local checks and qualification

```powershell
python -m unittest discover -s tests -p ci_signing_input_test.py -v
```

Fixture checks cover exact selection, release/test distinction, failed-CI truth,
changed attempt/SHA/artifact, wrong repository/workflow, expiry, ambiguity,
prior-attempt artifacts, ZIP digest mismatch, unexpected paths and symlinks.
Workflow syntax and shell/PowerShell parsing are separate checks. Actual Azure
signing, Apple's verdict, both Mach-O slices and downloaded output signatures
must be verified on the resulting hosted run; local fixtures do not prove them.

2026-10-31 candidate trial: Build41173064281 at
84584fa42c7dde6dee51ba0fc9c92b4c74899aab contains the preset corrections from
bfedc294 plus qualification notes/manual CI entry. Signing evidence and exact
download receipts belong under artifacts/ci-run-signing-20261031 in the shared
workspace. This document does not claim that an unfinished hosted job succeeded.
