# Custom alert sounds

Every `[audio] alert_*` setting accepts a built-in cue name or a local WAV/MP3 path on Windows and macOS.

```toml
[audio]
alert_victory = 'sounds/victory.mp3'
alert_fleet_arrived_at_destination = 'C:\Sounds\arrival.wav'
alert_fleet_miner_opc = '/Users/player/Sounds/miner.mp3'
alert_defeat = 'alarm'
```

Single-quoted TOML strings are useful for Windows paths because backslashes remain literal.
Relative paths start in the directory containing the settings TOML. Absolute paths, spaces,
and Unicode filenames are supported. Paths are case-sensitive where the filesystem is.
URLs, including `file://`, are not supported; use a filesystem path directly.

Clips are loaded and prepared when configuration loads. Restart the game after changing a
file or its setting. Alert playback uses the prepared data without reading the file again.
Clips may be at most 16 MiB and 30 seconds long; Windows additionally limits decoded PCM
to 16 MiB. Unsupported or corrupt audio and missing files are logged and leave that alert
silent. The original configured path is preserved. Built-in names and `none` retain their
existing behavior, including the existing toast/fleet enablement settings.

A new cue replaces the currently playing cue rather than queuing a backlog.

## Regression fixture

Run `xmake build notification-audio-file-tests`, then
`xmake run notification-audio-file-tests <temporary-directory> <absolute-path-to-test.mp3>`.
The fixture creates WAVs and checks decoding, limits, Unicode/relative/absolute paths,
built-in aliases, and independence from the source file after loading. It does not play audio.

## Live Mod Settings

Open **Mod Settings > Audio Alerts**, then choose an alert. Select **Off** or a
built-in sound and use **Preview sound** to hear it. Changes apply immediately
and save to the existing TOML key; desktop notification preferences are unchanged.

Select **Custom...** to choose a local WAV or MP3 using the native file picker.
The **Choose file...** button also lets you replace or reload a custom sound.
The selected filename appears on the page. Files load in the background; the
current sound stays active until the replacement is ready. Canceling or choosing
an invalid file leaves the current sound and saved path unchanged.

An alert configured with a custom path retains its already-loaded clip for the
session, so you can try a built-in and select **Custom...** to switch back without
reloading. Merely opening settings does not change the path.
Selecting a built-in intentionally saves that built-in name; after restarting,
the previous custom path is no longer offered unless you restore it in TOML.
Missing/invalid custom clips remain represented but cannot be previewed.

Use **Choose file...** again to reload a changed file without restarting.
Alerts whose required hooks are unavailable cannot
be changed or previewed. Enabling a fleet alert starts fresh observation history,
so it does not report past fleet activity.
