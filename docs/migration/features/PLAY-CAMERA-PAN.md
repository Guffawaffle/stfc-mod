# PLAY-CAMERA-PAN — system pan and glide

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starting head
`f9e66f2428944c09f24ae338252de1eeb5dc1479`. Source ownership is
`mods/src/patches/parts/fix_pan.cc`, its NavigationPan/OrbitFrameProvider/TKTouch
wrappers, key/mapkey helpers, configuration and camera settings. Typed ownership
is `src/mod/features/camera.cc`, `game/navigation.h`, `game/camera.h` and Unity input.

## Activation and source workflow

The existing Pan descriptor installs NavigationPan.LateUpdate,
OrbitFrameProvider.UpdateInputData and TKTouch.populateWithPosition together,
under restart-only `[patches] panhooks` (true). This does not activate the parked
system-orbit feature or introduce another camera/frame owner.

Movement defaults are W/A/S/D and the corresponding arrows. Mod keyboard
movement requires enabled shortcuts, enabled movement keys, mod hotkey mode,
native tutorial/modal permission, no text focus, no pointer drag/touch and no
native pan block or tracked target. Modified arrow shortcuts retain ownership
until release; shortcut capture owns keyboard admission even when no arrow is
held. Native hotkey mode or disabled hotkey hooks selects the native LateUpdate
path when keyboard admission permits it.

| ID | Source obligation | Retained typed behavior / necessary correction |
| --- | --- | --- |
| P1 | `fix_pan.cc:57–73` resolves configured directions, cancels opposites and normalizes diagonals. Native keyboardDeltaScaler supplies the speed; deltaScaler converts the drag command. | Preserve signs, normalization, native scalar values and the existing one-movement-per-frame path. Do not invent a separate drag-speed control. |
| P2 | `fix_pan.cc:76–87` clears momentum for BlockPan/tracking; active pointer input leaves native pointer motion alone; otherwise lastDelta decays by system_pan_momentum_falloff each update. | Preserve pointer priority, blocking, per-update decay, native momentum argument and stored lastDelta. Keep the explicit existing target-unlock setting; its default remains false. |
| P3 | Frozen key.cc:230–246 treats capture/popup/search ownership as directional admission being unavailable regardless of which arrow is held. | Use existing capture ownership before selecting native keyboard pan. Checking only ownership of four arrows loses capture of non-arrow keys in native hotkey mode. Capture does not itself erase an already observed coast. |
| P4 | Frozen uses real pan/input fields; typed missing reads can become zero, false or null. | Unavailable BlockPan, tracking, last delta/scalars or pointer observations cannot authorize mod movement or state writes. Static BlockPan and instance fields retain distinct schema checks. |
| P5 | The native tutorial/shortcut calls can run managed code. | Retain no Config reference across them. A changed current policy invalidates the pending frame; a later stable frame uses the new policy/falloff. Recheck live gates before publishing a custom movement. |
| P6 | `fix_pan.cc:88` gives system extended pan bounds the normal radius ratio. | Preserve the selected native ratio when available, without publishing a fabricated zero after a failed observation. |
| P7 | `fix_pan.cc:93–115` calls station input original, then rotates only in the exterior with mod movement enabled and text unfocused. | Keep native-first ordering and 45 degrees/second using current frame delta and configured left/right actions. Do not suppress the station original as a new capture policy. |
| P8 | `fix_pan.cc:25–31` turns a populated Stationary touch into Moved. | Retain arguments/result and other phases, with the existing null-result safety improvement. |

The source's `system_pan_momentum` default is 0.4 and is read only with
enable_experimental, but no gameplay consumer applies it. It stays an inert
compatibility input. Active glide is `graphics.system_pan_momentum_falloff`,
default 0.8. The existing live Settings control edits/saves that key through the
current writer. Its convenience slider stops at 0.99; no new TOML clamp, setting,
save schema or CI target is introduced here.

## Deliberate movement difference and qualification

Frozen custom keyboard frames can call MoveCamera once for keyboard movement and
again for glide. Its coasting path can similarly submit a retained delta before
the glide submission, and its native mode can append mod glide after original.
The target's existing by_keys path submits once, and its native route returns
after original. This difference was already recorded in PLAY-PARITY's queued pan
follow-up; source comparison alone does not establish how much native displacement
or perceived glide each submission produces.

Keep the current single path while qualifying the observation/capture/reentry
fixes. A matched runtime comparison at equal zoom/falloff, input mode, frame rate
and focus/modal/tracking state remains required before changing motion feel. No
frame-rate normalization or duplicate-movement restoration is implied by this
source contract.

All24 explicit pan modes pass in the
[camera/confirmation cohort](../FEATURE-COHORT-20261102-CAMERA-CONFIRMATION.md).
Twelve failures were reproduced against unchanged production source after fixing
the fixture's resolver/focus setup; twelve controls passed there too.
`tests/camera_pan_cases.h` drives the actual production
callbacks using controlled metadata/native bodies and existing shortcut/capture
sinks. Its private input resolver supplies or withholds the actual mouse/touch
ICalls. Config replacement is induced inside native tutorial/shortcut bodies;
the fixture checks the relevant frame and the next stable falloff frame.
These are native-binding consumer checks, not real devices, Unity camera movement,
shortcut-editor interaction or exact-client hook/ABI fit. Windows/Mac gameplay
and matched motion-feel qualification remain separate; AAI and parked diagnostics
stay deferred.
