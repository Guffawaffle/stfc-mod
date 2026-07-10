# Release Checklist

Before tagging a release:

1. Merge the release candidate to `Guffawaffle/stfc-mod:main`.
2. Wait for the `Build` workflow on the exact target SHA to complete successfully.
3. Smoke the exact production artifact from that run, not only a local dev build.
4. Run `global.stfc-mod-private.release-preflight` in dry-run mode with smoke acknowledged.
5. Push the proposed tag through the release preflight command only after blockers are clear.
6. Run the hook support-tier gate through the review contract and confirm any
   science/dormant hooks are either explicitly unsupported in
   `manifests/hook_support_tiers.json` or deliberately promoted before release.
7. Record known risk and test coverage gaps before publishing the tag.

After publishing, if a release is found bad:

1. Follow `docs/RELEASE_WITHDRAWAL_POLICY.md`.
2. Prefer `superseded` or `known-bad` unless keeping the artifact downloadable is actively harmful.
3. Use `global.stfc-mod-private.release-withdrawal` in dry-run mode before changing GitHub state.
4. Do not delete/yank a release or tag without a non-empty reason, a reviewed destructive-action printout, and a durable ledger record.
