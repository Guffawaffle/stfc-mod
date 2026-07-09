# Release Checklist

Before tagging a release:

1. Merge the release candidate to `Guffawaffle/stfc-mod:main`.
2. Wait for the `Build` workflow on the exact target SHA to complete successfully.
3. Smoke the exact production artifact from that run, not only a local dev build.
4. Run `global.stfc-mod-private.release-preflight` in dry-run mode with smoke acknowledged.
5. Push the proposed tag through the release preflight command only after blockers are clear.
6. Confirm any science/dormant hooks are either explicitly unsupported or deliberately promoted before release.
7. Record known risk and test coverage gaps before publishing the tag.
