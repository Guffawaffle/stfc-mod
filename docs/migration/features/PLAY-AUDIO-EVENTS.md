# PLAY-AUDIO-EVENTS — named game sound filters

Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`:
`mods/src/patches/parts/audio_events.cc`, config and patch registration.
Target `src/mod/features/audio.cc`, `game/app.h`, Config::Audio and Audio registry.
See [qualification](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Preserve the complete instance PostEvent(string, action, object, GameObject,
  InitialiseParameters, bool, callback) route, original arguments and return.
  Typed HookSet replaces raw installation; the integer-ID overload is not the
  consumer. Exact-client dispatch/fit remains a native qualification boundary.
- Null event names and disabled filters delegate. Trace logs the named event.
- `disable_all_events` suppresses only start actions 0/3/16/33; stop/unload and
  other cleanup still run. A listed named event suppresses all its actions,
  including cleanup, as frozen source does. Do not silently unify those policies.
- Settings are live; startup `[patches].audioeventhooks` stays separate.
  Existing `[audio] trace_events`, `disabled_events`, `disable_all_events`
  default false/empty/false. No defaults, file format or new hooks this pass.

Actual consumer cases cover null, all/named, cleanup, delegate arguments/return
and live changes. Frozen catch-all conversion fallback is not duplicated: typed
native calls retain their library containment; C++ allocation failure is not
claimed equivalent to the old catch-all. No live audio was muted in the fixture.
