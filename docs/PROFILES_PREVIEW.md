# Profiles test preview

This downstream test package combines downstream mod code with the pinned Profiles component and its matching
helper. `manifest.json` identifies the exact source and packaged file hashes. It is a preview, not a released
mod or proof that every game subsystem is isolated. PlayerPrefs and the sign-in browser are separated;
shared game caches and other SDK storage are outside that guarantee.

Start with two new test profiles. Importing an existing Windows user's saved login is a separate feature and
is not needed for these checks. Keep this package's runtime and helper together.

## Windows

Quit games using the installation you intend to test. Preserve its existing `version.dll` somewhere outside
the game directory, then place this package's `version.dll` beside `prime.exe`. Keep `stfc-profiles.exe` in
the extracted package. Use a game installation supported by the downstream mod; this build contains no AAI.

In PowerShell, replace the two paths below. The helper creates immutable IDs; a profile name is not a launch ID.

```powershell
$game = 'C:\Games\Star Trek Fleet Command\default\game'
$cli = 'C:\path\to\extracted-preview\stfc-profiles.exe'
$a = (& $cli create 'Net test A' --game $game --json | ConvertFrom-Json).profile
$b = (& $cli create 'Net test B' --game $game --json | ConvertFrom-Json).profile
& $cli list --json
& $cli launch --profile $a.id --game $game --json
& $cli launch --profile $b.id --game $game --json
& $cli sessions --json
```

Stop if a command reports an error. `readiness: ready` confirms runtime admission, not which game account
is signed in. The helper handles launch coordination, per-profile Unity logs and readiness reporting.

Once a profile exists, a launcher can invoke the game directly with the supporting mod installed:

```powershell
Start-Process -FilePath (Join-Path $game 'prime.exe') -WorkingDirectory $game -ArgumentList @(
  '-stfc-profile', $a.id, '-logFile', ('"' + (Join-Path $a.directory 'logs\Player.log') + '"')
)
```

Do not run this second launch while that same profile is already open. Direct flags use the same admission
and isolation rules but do not give a launcher the helper's structured readiness reply.

## Mac

Use the complete preview app, which supports Intel and Apple Silicon on macOS 14.6 or newer. It is signed
ad hoc and is not notarized. macOS may require explicit approval to open a downloaded test app. Keep your
ordinary launcher available so you can return to it afterward.

Open the preview launcher and choose **Profiles…** in its header or View menu (Command–Shift–P). Choose
the exact `Star Trek Fleet Command.app` to test, create `Net test A` and `Net test B`, then use **Launch
Profile** for each. The helper, runtime and browser guardian are bundled; Terminal is optional. Normal
**Engage!** continues to launch ordinary play.

Install Chrome or Edge in `/Applications` for isolated sign-in. Safari/default-browser fallback is deliberately
not used. If the game needs loader entitlements, the launcher uses its normal authorization flow to prepare
the selected app. Quit games using that installation before allowing it to change the game signature or update.

For another launcher's integration, the same helper accepts JSON commands. No shell parsing is required;
pass each argument separately. For a manual Terminal check:

```bash
contents='/Applications/STFC Community Mod.app/Contents'
game='/path/to/Star Trek Fleet Command.app/Contents/MacOS'
"$contents/stfc-profiles" create 'Net test A' --game "$game" --json
"$contents/stfc-profiles" list --json
# Replace PROFILE_ID with the immutable ID returned above.
"$contents/stfc-profiles" launch --profile PROFILE_ID --game "$game" \
  --runtime "$contents/libstfc-community-mod.dylib" --json
"$contents/stfc-profiles" sessions --json
```

Keep `stfc-profiles` immediately beside `libstfc-community-mod.dylib`, including when embedding them in another
app. The game uses that helper for isolated browser handoff. A launcher that also updates or re-signs the game
must hold the Profiles installation update lease in the process performing the writes; a CLI pre-check alone
does not reserve the installation. See the pinned component's C API for that lease.

## Check separation

1. Sign profile A and B into distinct test accounts. Verify the account and server in each game window and
   verify the browser sign-in returns to its intended game. Both profiles should remain usable concurrently.
2. Choose distinguishable local settings and bookmarks in each. Switch between windows and confirm they
   remain separate. Account-backed changes alone do not prove local storage isolation.
3. Try launching the same profile again. It must refuse a second session instead of opening ordinary play.
4. Quit both games and their isolated sign-in browser windows. Restart B, then A; verify each retains its own
   account and local state. Also check your ordinary game setup is unchanged.

On Mac, also check ordinary **Engage!** from the preview launcher. Launch and update operations must refuse
overlap on the same installation, including from another preview launcher window. Leave that update check
pending if no game update is available; do not force an update to test it.

On failure, record the package manifest, OS/architecture, game version, command/error or panel message,
and which profile/game/browser was open. Logs can contain account information: share only redacted excerpts.
Use **Open Profile Folder** on Mac, or `directory` from the helper's list on Windows, to locate profile logs.
Do not remove catalog files or use `--root` to work around a failed live launch.

## Build selection

Profiles support is an explicit `-Dprofiles=true` build option. Ordinary release and AAI development builds
keep their existing selection; this separate preview includes the complete downstream core without AAI.
The Mac app retains the downstream version/channel display and ordinary Engage launch path.
