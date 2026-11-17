# PLAY-AUDIO-CUES — built-in and local notification sounds

Frozen `1baa55fe`: complete `patches/notification_audio.cc`, audio types,
coalescing, file loader and Windows/Mac playback/decoding implementations;
config activation and `settings/audio_alerts.cc` consumers. Target keeps those
algorithms in `features/notification_audio*`, `notification_audio_windows.cc`,
Config::Audio and existing Audio Alerts settings. Exact refs in
[cohort](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Preserve ten cue names, existing aliases, PCM tone patterns, sample format and
  fade behavior. Target lazily builds each immutable built-in, instead of all
  tones on first playback; this is existing target behavior, not a new change.
- Parse retains source spelling and does no file I/O. Activation/picker work
  prepares accepted local WAV/MP3 bytes relative to the settings directory.
  Reject URLs, unsupported/missing/corrupt files, >16 MiB and >30 seconds.
- Retain immutable prepared clips across config replacement and file removal;
  preparation interns equal clips with weak entries. Playback does not read files
  or managed objects. Windows asynchronously borrows retained PCM; Mac NSSound
  owns its data. Native platforms retain their separate decoding implementations.
- none/same/all coalescing uses a monotonic playback window, not combat time.
  Only successful starts replace identity/window. Stopped clears the window;
  failure leaving prior playback intact preserves it. Suppression never extends it.
- All cues default silent. Sound, native banners and desktop delivery remain
  independently configurable. Existing Settings save/outcome semantics unchanged.

Existing actual dispatch/file/settings fixtures cover replacement/failure,
interning, lifetime, native Windows WAV decoding, invalid files, limits,
cancellation and stale picker completion. Fixtures do not play audible sounds;
MP3 requires the optional supplied sample and is not inferred from WAV success.
Mac decoder, permission and audible behavior need their own platform trial.
