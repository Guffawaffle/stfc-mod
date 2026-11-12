# Downstream build identity

The fork's Mac launcher uses the waffle icon with three channel labels:

| Channel | Icon accent | Intended download |
| --- | --- | --- |
| Release | Cyan | Signed public release |
| Pre | Amber | Signed test candidate or named pre-release |
| DEV | Violet | Raw development build, which may include experimental tools |

The launcher's footer beside Game Version, its About panel, and the in-game
Mod Settings entry show the exact version and channel, such as
`1.1.9.5-guffa.6 · Release`. A named pre-release shows the same full tag followed
by `Pre`. Candidates without a planned tag use a unique CI label, such as
`1.1.9.5 · Pre b123.1`; raw CI output uses `DEV b123.1`. Local builds use `DEV local`.
These labels describe the selected build. They do not enable logging or other
development features; those remain part of the selected source and configuration.

The game reads the label once per process. On Mac, the launcher passes its own
bundle identity only to the loader it starts; the mod checks that the source
commit matches its compiled commit hash. On Windows, the signer puts the label
in the signed program description and the mod reads that description from its
own loaded module's file. It does not perform an online trust check. An unknown
channel shows only the known core version; an anonymous local build uses
`DEV local`.

## Build and promotion

Ordinary CI builds select DEV automatically and include their producing run,
attempt and source commit. A normal local Zig build also defaults to DEV.

The separate trusted signing controller selects Pre for signed test downloads.
For a named release it receives the exact planned `release_tag` and the same
`prerelease` boolean that will be used when publishing on GitHub. That boolean
chooses Pre or Release. The `-guffa.N` suffix alone does not decide the channel.
Signing a selected test artifact does not change its CI result.

The controller uses its own trusted metadata helper and icon assets before
signing or notarization. It preserves the library, loader and launcher bytes
during this step and records the selected identity in the signing receipts.
Windows uses the same canonical version metadata for the Authenticode program
description; the unsigned DLL and its existing source marker are not rewritten.
The app and installer volume use the same selected icon. Bundle identifier,
executable, app directory and compatibility download filenames remain stable.

Apple's numeric bundle fields remain separate from the complete player-facing
tag. The short numeric version is `1.1.9`; build `5.6` retains core patch 5 and
fork release 6. An unnamed candidate or development build uses `5.123.1` for
core patch 5, CI build 123, attempt 1. The full tag and channel are retained in
`STFCDisplayVersion` and `CFBundleGetInfoString`. Finder's exact presentation and
the launcher layout need a native Mac check on the final signed app.

## Rollout

1. Merge the launcher changes to play and obtain its successful CI artifacts.
2. Promote the matching helper, icon assets and workflow changes to the trusted
   `ci/macos-signing-play` controller. It must not run helper code from a candidate.
3. Use `sign-run` without a tag for an ordinary signed Pre test candidate. For a
   named candidate, pass its planned tag and `prerelease=true`; a public Release
   uses the planned tag and `prerelease=false` with release-eligible PLAY output.
4. Before publication, check that `macos-provenance.json` has the intended
   `releaseTag`, `prerelease` and `buildChannel`, and that the full launcher label
   and icon agree. Use those same values for the GitHub release.

Older bundles can receive new metadata and artwork before re-signing, but they
need rebuilt launcher and mod binaries to gain the new visible version rows.
Published releases are not rewritten.

## Assets

The committed icons need no generation dependencies in CI. See
[`assets/launcher-icons/README.md`](../assets/launcher-icons/README.md) for their
editable source, exact sizes and regeneration command.
