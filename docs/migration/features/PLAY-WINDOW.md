# PLAY-WINDOW — Windows resize forwarding

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; refactor starting point
`5c3637af61a3fb270db28f2610e95fe62666d5c6`. Source owner:
`mods/src/patches/parts/free_resize.cc`; typed owner:
`src/mod/features/window.cc`, with `AspectRatioConstraintHandler` in `game/app.h`.

The Windows `FreeResize` descriptor installs the handler's Update and WndProc
hooks under restart-only `[patches] freeresizehooks`. Live `[graphics] free_resize`
controls forwarding; both switches retain their true defaults. Title work shares
Update but has a [separate contract](PLAY-WINDOW-PROFILE-TITLE.md).

| ID | Frozen operation | Retained typed behavior / correction |
| --- | --- | --- |
| W1 | `free_resize.cc:153–158` forwards enabled resize messages to the game's saved Unity procedure, otherwise calls original. | Preserve message, window and parameter values. Select A/W forwarding from the actual window's Unicode form; retain this correction to frozen always-A forwarding. |
| W2 | Frozen Update at97–151 suppresses original unconditionally. | Keep target's disabled-setting correction: skip the aspect-ratio Update only when free_resize is enabled, otherwise call original. |
| W3 | The saved `_unityWndProc` field is read through the old helper. | A missing/unavailable/null saved procedure must fall back to original WndProc. A default zero is not permission to call a native null procedure. |
| W4 | Config's window helper chooses a visible, unowned top-level window from this process. | Preserve selection and retry discovery when unavailable; retain target's cached-handle validity check. This is process/window routing, not account or profile identity. |
| W5 | Config's DPI helper compares physical and logical monitor width. | Retain target's game-window monitor selection while backgrounded, monitor-change cache and explicit refresh. UI-scale behavior remains with its existing owner. |

Frozen `SetWindowLongW_Hook` is only defined, never installed. Consequently its
F11 wrapper, captured unityWindow, borderless placement/style changes and
1024×768 fullscreen fallback have no connected trigger. Keep target's dormant
borderless implementation unregistered. Its configuration key remains compatible;
activating the experiment is separate feature work.

Controlled dispatch fixtures and Windows core compilation pass in the
[cohort](../FEATURE-COHORT-20261102-WINDOW-CURSOR.md). They
cannot establish actual resize/maximize behavior, HWND selection across clients,
display/DPI transitions or exact-client native hook fit. Native Windows trials
remain open. This capability is Windows-only; Mac acceptance is not inferred.
