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
7. Review the generated release notes. They should highlight merged PRs and
   fixed issues since the previous fork tag, not raw commit-title dumps.
8. Record known risk and test coverage gaps before publishing the tag.
9. For a launcher release, verify the signed archive contains both launcher
   executables plus install/uninstall scripts; inspect signer, icon, ProductVersion,
   archive size/SHA, and manifest `signedFiles` against the exact release assets.
10. Record the Windows launcher smoke matrix: clean per-user install and
    shortcut, manual-DLL adoption, mod update, game-update handoff and repair,
    transaction rollback, mod uninstall, offline launch, diagnostic preview/
    export redaction, self-update success, self-update startup rollback,
    launcher uninstall, 100/150/200% DPI, keyboard, and screen reader.

After publishing, if a release is found bad:

1. Follow `docs/RELEASE_WITHDRAWAL_POLICY.md`.
2. Prefer `superseded` or `known-bad` unless keeping the artifact downloadable is actively harmful.
3. Use `global.stfc-mod-private.release-withdrawal` in dry-run mode before changing GitHub state.
4. Do not delete/yank a release or tag without a non-empty reason, a reviewed destructive-action printout, and a durable ledger record.
5. A withdrawn launcher release must no longer be offered for new mod or
   launcher updates. Do not remove a healthy installed launcher automatically;
   publish a replacement and document manual rollback when needed.
