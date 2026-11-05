# PLAT-MAC-LAUNCHER-UI — launcher file actions and controls

Contract v1, 2026-11-03. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; typed starting point is
`80bc425c7c06b36d22566d6e863531c3740fbbc5`. This is a UI/control slice of
[PLAT-MAC-LAUNCH](PLAT-MAC-LAUNCH.md), separate from updater filesystem and native
entitlement/signing qualification.

## Restored frozen workflows

Frozen ActionView.swift exposes Configure Mod, Open TOML, Update Game and Engage.
The typed starting point omitted Open TOML and its ModFiles.swift helper. Restore
the ordinary user Library/Preferences/com.stfcmod.startrekpatch directory and
community_patch_settings.toml filename through the small shared ModFiles owner.
This is the normal mod namespace, not account or named-profile selection.

Open TOML checks that the file exists and is not a directory, then uses its
associated application, falling back to TextEdit. Missing paths/applications and
failed open requests report through the existing alert. It never creates files
or edits their contents. Both file/config actions stay outside the installed,
updating and game-running gates, as in frozen source.

Frozen LauncherMain.swift also exposes View Mod Folder from its command group.
Restore that action with the existing-directory check, Finder open and failure
alert. The typed starting point omitted this command and its helper.

The four-button row retains the frozen geometry: four 92-point buttons and three
8-point gaps fit 392 points in the existing 600-point window. Configure Mod uses
two lines; long labels retain trailing alignment and the 0.7 minimum scale. The
game-not-installed state uses the frozen 192-point message area beside the two
configuration buttons. Adding a fourth 125-point button to the typed layout would
not preserve that geometry.

## Retained controls and deliberate improvements

The window remains fixed at 600×400 and quits when its last window closes.
Configure Mod still opens the existing configurator site. Update remains offered
only for an installed game with a newer version; its state is set before starting
the task, preventing the frozen rapid-click admission race. Engage likewise sets
the running state before background work and preserves the current missing-loader
and process-start error reports. Launch waits off the main thread so entitlement
UI can operate.

Progress labels and the one-minute update poll remain intact. Removed numeric
progress state was not rendered in frozen UI, and the frozen pulsing view's color
index never changed from Tan. The LCARS palette/font/structure remain with the
existing ContentView owner. No new update, repair, cancel or rollback control is
introduced: neither the frozen nor typed window contains those controls beyond
the existing Update action. The update task is unstructured in both versions;
closing the last window terminates the app. Loader nonzero exit status after a
successful Process start remains an inherited limitation, separate from start
errors.

INI parsing keeps the frozen section/key/comment rules. The typed installed-version
reader's malformed-input guard and newline trimming remain. The old entitlement
reapplication flag was only written/cleared, never used to bypass verification;
restoring that unused UserDefaults value is not required behavior.

## Qualification

The comparisons and restored source/actions are inspectable locally, but SwiftUI,
AppKit file-opening behavior, associated-app fallback, alerts, four-button layout,
menu publication and actual launch are not compiled or executed on this Windows
host. Mac SDK/Swift/Xcode qualification remains pending in the parent cohort.
Source presence does not establish native UI parity. No new test target, CI
registration, live configuration change, actual app open, signing or game launch
is performed by this source slice. AAI and new upstream features remain deferred.
