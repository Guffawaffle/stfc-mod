# PLAY-CURSOR — plain arrow and clipping release

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; refactor starting point
`5c3637af61a3fb270db28f2610e95fe62666d5c6`. Frozen owners are
`mods/src/patches/parts/testing.cc::Cursor_SetCursor` and the canvas-scale hook
in `parts/ui_scale.cc`. Typed owners are `src/mod/features/window.cc` and
`src/mod/features/ui_scale.cc`.

| ID | Preserved behavior | Typed boundary |
| --- | --- | --- |
| C1 | `[graphics] allow_cursor=true` preserves the game's requested cursor. | Forward texture, hotspot, mode and the existing hidden MethodInfo argument unchanged to original SetCursor_Injected. |
| C2 | With allow_cursor=false, Windows installs the standard arrow, releases Unity clipping and skips the custom-cursor original. | Keep SetCursor/LoadCursor and ClipCursor(nullptr) behavior; check the live setting at each invocation. |
| C3 | Canvas rescaling calls original, then reapplies the standard arrow if custom cursors are disabled. | Preserve ordering in the existing UI-scale owner. This companion path does not independently release clipping. |
| C4 | The cursor detour belongs to restart-only testpatches; allow_cursor defaults true. | Retain the Cursor descriptor's existing testpatches gate, default and Windows registration. UI-scale installation retains its separate gate. |

No cursor product change or ABI reshaping occurs in this slice. Frozen's
non-Windows detour was a pass-through; current omission of that Windows-only
feature descriptor does not imply a missing Mac plain-cursor implementation.
The detour remains on the same managed SetCursor_Injected target. A controlled
callback fixture can qualify forwarding and dispatch decisions, but cannot prove
the native parameter ABI, overwrite extent, visible arrow, real clipping release
or leave/reenter behavior in a particular client.

Direct callback fixture and Windows core compilation pass in the
[cohort](../FEATURE-COHORT-20261102-WINDOW-CURSOR.md). Catalog classification stays
Implemented, unverified; actual native cursor/display behavior and platform
qualification remain separate.
