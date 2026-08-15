# Runtime Impact Sample Analysis

> Historical sample. The root-log impact monitor and its dedicated TOML keys
> have been replaced by the activation-only `runtime-impact` targeted concern
> tracked in issue #257.

Date: 2026-05-07  
Branch: `guffa-dev`  
Sample source: live `community_patch.log` impact rows emitted by `advanced.diagnostics.mod_impact_monitor = true`

## Summary

The original sample confirmed that the mod processes several hooks every frame. The measured mod-owned time was small relative to a 60 FPS frame budget, but it was not zero. The clearest improvement opportunity was the live-debug channel: with `advanced.diagnostics.live_query = true`, `ScreenManager.Update` calls the live-debug tick every frame, and before the throttle the steady cost was dominated by that path.

The sample does not point at zoom, pan, or UI scale as the primary source of glitchiness. Their steady costs are low. The biggest non-live-debug risk is `AspectRatioConstraintHandler::Update`, because it runs every frame and occasionally spikes into multi-millisecond territory while doing Win32 and Unity screen/resolution queries.

## Sample Shape

The aggregate below is weighted by sample count across all observed reporting windows.

| Probe | Reports | Samples | Weighted avg | Max | Over 250 us | Over 1000 us |
|---|---:|---:|---:|---:|---:|---:|
| `frame_tick.total` | 136+ | 93k+ | ~98-100 us | 7.24 ms | ~0.32% | ~0.01% |
| `frame_tick.live_debug` | 136+ | 93k+ | ~85-87 us | 5.23 ms | ~0.22% | rare |
| `aspect_ratio.update` | 136+ | 93k+ | ~14-15 us | 16.11 ms | ~0.18% | ~0.02% |
| `frame_tick.hotkeys` | 136+ | 93k+ | ~13 us | 7.14 ms | ~0.03% | rare |
| `navigation_pan.late_update` | 50+ | 33k+ | ~2-3 us | sub-ms | rare/none | 0 |
| `navigation_zoom.update` | 50+ | 33k+ | ~2 us | 0.37 ms | rare | 0 |
| `ui_scale.update_canvas_root` | 136+ | 93k+ | ~2 us | 0.17 ms | 0 | 0 |

The frame tick probes report about 600-715 samples per five-second window, so this game state is producing roughly 120-143 update calls per second. At 60 FPS, 100 us is roughly 0.6% of a 16.67 ms frame budget. At 120 FPS, it is roughly 1.2% of an 8.33 ms budget. That is acceptable for diagnostics, but it is high enough that always-on diagnostic polling should remain opt-in.

## Follow-Up: Three-Second Live Query Poll

A follow-up change throttled `live_debug_process_request_cycle()` to once every three seconds from `live_debug_tick()`. The deploy was verified with a fresh DLL hash, `ax debug-send -Cmd ping` returned successfully, and the post-throttle impact sample showed the intended drop:

| Probe | Before weighted avg | After weighted avg | Notes |
|---|---:|---:|---|
| `frame_tick.live_debug` | ~85-100 us | ~2 us | Idle live-query path no longer performs a filesystem check every frame. |
| `frame_tick.total` | ~98-120 us | ~13-15 us | Remaining cost is mostly hotkey routing plus cheap live-debug time checks. |

Operational impact: AX/live-debug commands can now take up to three seconds to execute, depending on where the command lands in the poll interval. This matches the expected diagnostic use case and is a large steady-state frame-cost reduction.

## Code-Backed Findings

### 1. Live Debug Polls the Filesystem Every Frame

`frame_tick.total` is the central `ScreenManager.Update` fan-out. The hook measures mod time while excluding the original game call, then calls `tick_live_debug()` before the hotkey router.

Relevant path:

- `mods/src/patches/frame_tick.cc`
- `mods/src/patches/parts/live_debug.cc`
- `mods/src/patches/parts/live_debug_connector.cc`

With `advanced.diagnostics.live_query = true`, `live_debug_tick()` runs every frame. In the originally sampled build, most UI polling inside `live_debug_tick()` was compile-time disabled, but the tick still called `live_debug_process_request_cycle()` every frame. That function performed a `std::filesystem::exists()` check for `community_patch_debug.cmd` on every frame and only then returned.

That matches the sample: `frame_tick.live_debug` accounts for most of `frame_tick.total` average cost. The average is consistent with repeated negative filesystem checks on Windows.

Follow-up status:

1. `live_debug_process_request_cycle()` is now throttled to a three-second cadence, removing almost all steady frame-owned filesystem checks.
2. A remaining cleanup option is to split `advanced.diagnostics.live_query` into request serving and frame observation modes. File request polling does not need to be structurally tied to every `ScreenManager.Update` tick.
3. Another remaining option is to add a runtime-visible warning when `advanced.diagnostics.live_query = true`, similar to the impact monitor toggle, because it is diagnostic infrastructure with measurable frame cost.

Priority: high. This is the largest steady-state cost in the sample and the easiest to make less frame-owned.

### 2. Aspect Ratio Update Is Mostly Cheap, But Has the Largest Spikes

`aspect_ratio.update` is measured from `AspectRatioConstraintHandler::Update`. This hook bypasses the original method and performs fullscreen/resolution correction plus first-run title setup.

Relevant path:

- `mods/src/patches/parts/free_resize.cc`

The steady cost is modest at roughly 14-15 us, but this probe produced the largest observed max spike, including a 16.11 ms window max. The hook performs several per-frame calls that do not obviously need frame cadence:

- `UnityEngine.Screen::get_fullScreen()`
- `UnityEngine.Screen::get_height()`
- `UnityEngine.Screen::get_width()`
- `UnityEngine.Screen::get_currentResolution_Injected(...)`
- `GetWindowPlacement(...)`
- `GetMonitorInfo(...)`
- optional `SetResolution(...)`
- first-success `WindowTitle::Set(...)`

Improvement candidates:

1. Gate the resolution repair to run only while fullscreen and only at a lower cadence after startup or after a known window/fullscreen transition.
2. Cache monitor dimensions and refresh them on fullscreen toggle/window changes instead of calling `GetWindowPlacement()` and `GetMonitorInfo()` every frame.
3. Make title setup independent of the aspect-ratio frame hook, or stop entering this hook's title branch after a bounded number of attempts.
4. Review the resolution mismatch condition. The expression currently checks `m_width != width || m_width != height`; the second comparison looks like it should probably be `m_height != height`.

Priority: high for spikes, medium for steady-state cost.

### 3. Hotkey Router Is Not the Main Steady Cost, But It Still Does Too Much Per Frame

`frame_tick.hotkeys` averages around 13 us, which is not the current bottleneck. It occasionally spikes, but the largest hotkey spikes line up with large total frame-tick spikes, so the sample does not prove the hotkey router alone caused them.

Relevant path:

- `mods/src/patches/hotkey_router.cc`
- `mods/src/patches/mapkey.cc`
- `mods/src/patches/key.cc`
- `mods/src/patches/input_binding/*`

The router still performs many `MapKey::IsDown()` / `MapKey::IsPressed()` checks every frame and contains broad action routing in one frame-owned function. The unified input binding work is the right longer-term fix: compile bindings once, compute the active chord/event once, then dispatch through a common action table.

Improvement candidates:

1. Continue the unified input dispatcher migration so the router stops repeatedly asking every action whether it is active.
2. Build a per-frame input snapshot once, then dispatch off the snapshot rather than querying keys throughout the router.
3. Keep `allow_key_fallthrough` and Scopely original-call decisions centralized in the dispatcher so `ScreenManager.Update` only combines one execution decision.

Priority: medium. This is architecturally important, but the sample says it is less urgent than live-debug throttling.

### 4. UI Scale, Zoom, and Pan Are Low-Cost in This Sample

`ui_scale.update_canvas_root`, `navigation_zoom.update`, and `navigation_pan.late_update` all appear low-cost in the observed gameplay state.

Relevant paths:

- `mods/src/patches/parts/ui_scale.cc`
- `mods/src/patches/parts/zoom.cc`
- `mods/src/patches/parts/fix_pan.cc`

Observations:

1. UI scale averages around 2 us and produced no threshold crossings. It does still run every frame and may call `SetCursor(...)` every frame when cursor override is active, but that is not showing up as a meaningful cost here.
2. Zoom averages around 2 us. It checks several key bindings each update, but its heavier world-position path only runs when zoom input is active.
3. Pan averages around 2-3 us. It still runs custom post-original logic when `disable_move_keys = false`, but the measured cost is small.

Priority: low for performance based on this sample. Keep them monitored, but do not spend optimization time here before live-debug and aspect-ratio cleanup.

## Recommended Next Work

1. Add an aspect-ratio repair cadence or state gate. Re-sample while toggling fullscreen/windowed states to verify spike reduction.
2. Keep the unified input dispatcher work moving, but do not treat hotkeys as the likely primary cause of the current glitchiness unless a later sample with live-debug disabled shows otherwise.
3. If AX/live-debug command latency becomes annoying, make the request poll interval configurable while keeping the default above frame cadence.

## Operational Notes

1. `advanced.diagnostics.mod_impact_monitor` should stay off by default. It is useful for diagnosis, but it adds instrumentation and log writes every five seconds.
2. `advanced.diagnostics.live_query` should be treated as a development/AX feature, not a normal gameplay feature; with the three-second poll throttle, commands can take up to three seconds to execute.
3. Any future live sample should record whether the game is boot/menu/system/galaxy and whether AX live-query commands are active. The current sample includes both early and navigation-present phases, so the aggregate is useful but not a controlled benchmark.
