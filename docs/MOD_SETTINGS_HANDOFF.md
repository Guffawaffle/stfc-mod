# Combined Mod Settings

This branch combines native Mod Settings against upstream `dev`, including the
settings improvements developed in the fork's `play` branch. It is a source and
testing handoff; the individual upstream PRs remain separate.

## Included behavior

- Settings foundations, page navigation, live controls and comment-preserving
  asynchronous TOML saves, including conflicts and save-failure feedback.
- Shortcut capture, overlap warnings, immediate Remove/Restore/Undo saves,
  camera movement bindings, and Windows layout-aware input.
- Search by setting label or TOML key, with return-to-results navigation.
- Camera, preview/cargo, warp mode, Fleet Labels, Galaxy Labels and HUD controls.
- Live OPC, Galactic Anomaly timer, instant cargo and ship-hotkey badge toggles.
- Notification audio choices, previews, custom local files and coalescing.
- Swap Ship FT/CT indicator and indicator-background controls.
- "Other" groups for settings without a dedicated subsection.

Audio playback, fleet notifications, OPC rendering, extended galaxy selection,
ship badges and ship-tech rendering are included because their controls require
those consumers. Arbitrary TOML keys are not automatically turned into controls.
New controls need explicit registration and live read/write behavior; see
[control contracts](MOD_SETTINGS_CONTROLS.md),
[navigation](MOD_SETTINGS_NAVIGATION.md) and
[shortcut extension guidance](MOD_SHORTCUT_SETTINGS.md).

## Validation and tester handoff

Windows build:

```powershell
xmake f -p windows -a x64 -m release -y
xmake -y mods
./tests/run-settings.ps1
./tests/run-config-save.ps1
```

On macOS, build the exact handoff commit for each available architecture:

```sh
xmake f -p macosx -a arm64 -m debug --target_minver=14.6 -y
xmake -y mods
./tests/run-settings.sh
./tests/run-config-save.sh
# Repeat configure/build for -a x86_64 on an Intel Mac.
```

Record the source SHA, game-client version and architecture with any result.
Builds and pure fixtures do not establish native hook fit or in-game behavior.
Smoke page navigation/reopening, search and Back, shortcut Enter/Escape/focus
loss, live toggles, audio preview/file cancellation, HUD Auto restoration,
galaxy overlays, and save/restart persistence. Check that unavailable controls
remain disabled and existing player TOML comments and unrelated values survive.

macOS uses physical shortcut capture; Windows layout-aware capture is a separate
dependency. The instant cargo consumer retains its existing Windows client
compatibility restriction. Existing upstream settings hook-availability checks
remain in place. Platform-specific availability and smoke results must be
reported for the artifact actually tested.
