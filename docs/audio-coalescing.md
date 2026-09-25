# Audio coalescing

Choose **Mod Settings > Audio Alerts > Audio coalescing**, or set
`coalescing = "same"` in the TOML `[audio]` section.

| Mode | Behavior during playback |
| --- | --- |
| None | Keep existing behavior: each request can replace/restart the current sound. |
| Same (default) | Absorb requests for the current sound. A different sound replaces it. |
| All | Absorb every sound request until the current sound finishes. |

Absorbed requests do not queue a replay or extend the suppression window.
Desktop notification delivery and bucketing are independent. Sound previews use
the same policy as alerts.

Identical clips share prepared storage, so absorbed requests only compare
identities instead of scanning audio bytes during alert delivery.

The window uses the prepared clip duration (decoded PCM length on Windows,
NSSound duration on macOS), measured with a monotonic clock after successful
playback starts. It is not an audio-device completion callback. Separate alerts
using identical prepared audio share the same window, even if loaded separately.
Changing the mode takes effect on the next request without restarting playback.
If a replacement stops the previous clip but fails to start, its suppression
window is cleared so subsequent alerts can retry.

## Playback check (Windows and macOS)

Use a recognizable ten-second WAV or MP3 assigned to two alert types. With Same,
trigger both types several times while the clip is playing: it should finish
once, without restarts or trailing repeats. Trigger again after it finishes to
confirm playback resumes. A different clip should replace it.

With All, repeat using different clips: the first should finish uninterrupted,
with no trailing replay. With None, confirm the previous restart/replacement
behavior. Confirm desktop notifications still arrive according to their own
settings in all three modes. Check both built-in sounds and custom files, and
check that the selection persists after restarting the game.

Mac smoke should record the tested artifact and architecture (ARM64 or x86_64);
a Windows build does not validate Mac playback.
