# PLAT-MAC-PACKAGING — bundle, archive and signing ownership

Source/operational contract v1, 2026-11-03. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; typed starting head is
`80bc425c7c06b36d22566d6e863531c3740fbbc5`. This assessment covers producer
layout, resources, ordinary assembly, installer/archive contracts and the existing
separate signing route. It changes no producer, workflow, script or product code.

## Producer and ordinary package contract

Frozen `macos-launcher/xmake.lua`, `scripts/create-mac-dmg.sh` and CI build the
launcher, loader and dylib for arm64 and x86_64, combine the three executables,
retain the launcher resources and ad-hoc sign the app before creating the DMG.
The target replaces that build wiring with Zig and shares one assembly helper
between manual packaging, development launch and CI.

| Obligation | Existing target mapping |
| --- | --- |
| Build both supported architectures with minimum macOS14.6. | zig/launcher.zig uses xcrun/swiftc on a Mac/Xcode host and maps aarch64 to Swift arm64; package-mac-app.sh builds both thin prefixes. CI uses native ARM64/Intel runners. zig/mod.zig separately registers the mod/loader and universal binary merge. |
| Keep the launcher, injected dylib and auxiliary loader together. | assemble-mac-app.sh copies the first prefix's macOSLauncher.app and merges its MacOS/macOSLauncher, bin/stfc-community-mod-loader and lib/libstfc-community-mod.dylib inputs into Contents/MacOS/macOSLauncher, Contents/stfc-community-mod-loader and Contents/libstfc-community-mod.dylib. The output app is STFC Community Mod.app. |
| Preserve the compiled asset catalog, app icon and versioned bundle identity. | zig/launcher.zig compiles Assets.xcassets with actool, installs assets/launcher.icns and substitutes the four version components and minimum OS into Info.plist.template. Bundle identifier, executable, product/display names and APPL type remain the frozen identities. The asset catalog moved unchanged; the shared icon and DMG artwork are unchanged. |
| Refuse an incomplete assembled app. | The helper requires Info.plist, Resources/Assets.car, Resources/launcher.icns and all three binaries, then checks linkage and ad-hoc signs/verifies the bundle. This strengthens explicit presence checks rather than removing resources. |
| Do not accidentally ship package-manager or OpenSSL dependencies. | check-mac-linkage.sh inspects the three binaries' direct otool -L output and rejects Homebrew paths and non-system libssl/libcrypto/libcurl. The system libcurl is allowed. This source guard is not a measured runtime dependency-closure result. |
| Produce the same standalone mod payload and checksum. | CI packages libstfc-community-mod.dylib at the root of stfc-community-mod-macos-universal.tar.zst, with its .sha256 companion, from the assembled app's Contents directory. Artifact names remain stfc-community-mod-macos-universal and stfc-community-mod-installer. |
| Preserve the installer artwork/layout and Applications shortcut. | create-mac-dmg.sh copies the assembled app into a private temporary source, retains APFS/ULFO, volume name/icon/background, window/icon coordinates and app-drop-link, then removes its owned temporary source. Unlike the old monolithic script, DMG creation does not consume the developer's assembled app. |
| Reuse the same assembly in development and CI. | package-mac-app.sh, mac-build-test-debug.sh and CI call assemble-mac-app.sh. CI verifies arm64 and x86_64 slices in all three merged binaries. The developer helper executes only after a successful build, preventing a stale bundle launch after failure. |

The template's `${MACOS_MIN}` replaces the frozen literal14.6 using the same
zig/ctx.zig minimum used by the Swift build. Current package/development helpers
also specify14.6. scripts/verify-version-substitution.py can inspect an actual
built plist for matching CFBundle versions and leftover placeholders; source
comparison is not execution of that check. The development convenience script
currently accepts Zig0.16.0 specifically, while direct repository builds also
support0.17; no compatibility expansion is made in this assessment.

## Three separate signing/publication routes

1. **Ordinary producer and package:** the current migration tree assembles an
   ad-hoc signed app/DMG and ordinary CI archive. Ad-hoc signing is not Developer
   ID signing, Apple notarization, stapling or a Gatekeeper result.
2. **Optional signed test download:** the accepted operational split keeps
   signing on `ci/macos-signing-play`, outside the migration source tree.
   The locally inspected controller is
   `735c6149d2b59cbc8b216e623de4c3f246096bd8`; its
   docs/ci-artifact-signing.md and reusable signing workflow describe selecting
   an explicit retained same-repository Build artifact, verifying immutable input
   identity, and signing it without rebuilding or executing producer code with
   credentials. The Mac signer checks both slices, signs nested code and the app
   with hardened runtime and controller-owned launcher entitlements, submits the
   final DMG to Apple, checks ticket coverage, staples/verifies it, and emits the
   matching signed dylib archive and provenance. Existing approval/environment
   and exact-input rules remain owned by that controller. This is inspected
   historical source and its documented contract, not a new hosted signing result.
3. **Downstream tagged release promotion:** frozen .github/workflows/release.yaml
   selects a successful `play` push for `v*-guffa.*`, signs both platforms and
   publishes the signed results. The migration tree currently has the upstream
   generic `v*` publisher, which reuses ordinary CI assets and has no downstream
   signing jobs. That publisher is not equivalent to the frozen downstream
   promotion policy. Reconcile it before any downstream tag/release promotion;
   no such publication is requested or performed by this source assessment.

The missing frozen sign-macos.yaml, validate-macos-signing.yaml,
sign-macos-release.sh, check-macos-notarization.sh and launcher entitlements file
in this migration tree are therefore not evidence that optional test signing was
lost. The deliberate separate-controller decision is recorded in
[the signed preset evidence](../OFFICER-PRESETS-MAC-20261030.md#reusable-optional-signing).
Do not copy an older controller's entire source tree into the typed branch or
present ordinary package output as the controller's signed output. The generic
publisher difference remains a separate release-policy gap.

## Qualification

No new player-facing bundle/archive/DMG source regression was established in the
inspected ordinary assembly path. Existing rsync and Swift archive fixtures test
patch/archive behavior, not lipo, actool resources, DMG layout, codesign, Apple
notarization or Gatekeeper. No builds, signing, network requests, real installer
operations or script execution were performed for this comparison.

The Windows host lacks the configured Mac SDK and Xcode. Current native builds,
both architecture slices, actual resource/layout inspection, signed bytes and
Mac launch/runtime qualification remain open. Earlier signed pre-release and
player results remain attached to their original artifacts. AAI is deferred and
ordinary CI does not silently include its optional host. Full PLAT-MAC-LAUNCH
and overall migration/release parity are not closed by this contract.
