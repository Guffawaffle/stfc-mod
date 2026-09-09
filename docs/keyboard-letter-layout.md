# Layout-aware shortcuts

`[control].keyboard_layout_mode = "physical"` preserves existing behavior (default).
Set it to `"layout"` to resolve configured printable keys using Unity's active
keyboard-layout display names: letters, digits, and punctuation accepted by the
shortcut parser. `keyboard_letter_mode` remains a compatibility alias with the
expanded behavior; the new setting takes precedence if both are present.
For example, on German QWERTZ `show_daily = "Z"` follows German Z, which occupies
the US-QWERTY Y position. Existing Y/Z workaround configurations should be undone
when opting in. Restart once to change this setting; subsequent notified layout
changes are detected in-game without restarting. The notification adapter is enabled
on Windows x64 and experimentally on macOS arm64/x86_64. The Mac port still needs
in-game validation on each architecture; see the [Mac handoff](KEYBOARD_LAYOUT_REFRESH.md#macos-validation-handoff).
Unsupported or failed notification setup logs and disables layout-resolved bindings;
physical mode still works. There is no polling fallback.

This is action binding, not text input. Shift/Ctrl/Alt chords retain their existing
semantics; no modifiers are inferred from a character. Named controls (Escape,
arrows, function keys, Space, explicit numpad keys, and mouse buttons), hardcoded
controls, and native Scopely shortcuts retain their existing behavior.

Every configured printable key is eligible for lookup, but Unity must report a
matching display name and a physical position supported by the legacy input API.
A missing/unsupported key is disabled in layout mode, never silently treated as
physical. This includes symbols accessible only through Shift/AltGr when Unity
does not expose them as display names. On Windows, an unresolved unshifted dead
key can use a native layout lookup when the current Windows layout name matches
Unity's. For example, German `ALT-^` uses Alt plus the circumflex key directly.
This fallback only accepts a unique supported physical position marked as a dead
key by Windows; it does not infer Shift/AltGr or change text-composition state.
Unicode text composition, dead-key sequences, and automatic Shift/AltGr character
translation are not supported.
Use an explicit chord with a resolvable base key in those cases. For example,
German `(` is `SHIFT-8`; US `(` is `SHIFT-9`. These examples describe chords, not
a promise that a literal `(` binding resolves on either layout. French number-row
display names can also differ from digits; inspect the vars diagnostics before
assuming `1` or `SHIFT-1` resolves. Explicit `KEY1` retains numpad identity.

Use `MINUS` for `-` and `PIPE` for `|`, since those characters separate modifiers
and alternatives in configuration. `EQUAL` is an alias for `=`. These aliases
resolve like their corresponding punctuation, not as forced physical positions.
Select physical mode and restart to recover original behavior if needed.

## Implementation and provenance

`MapKey` preserves configured keys/text and resolves a printable key just before querying
the existing physical `Key` cache. Device notifications invalidate the layout cache,
including when the layout name stays the same. Only distinct configured printable
keys are looked up at initialization or after notification; duplicate bindings share
the cached result. Quiet queries do not poll the frame clock, current keyboard, or
layout. Unsupported or failed notifications log the reason and disable layout
bindings until restart, with no polling fallback.
See [refresh implementation](KEYBOARD_LAYOUT_REFRESH.md) for lifetime and failure details.
Physical mode does not resolve Unity layout methods or poll keyboard layout state.
There are no new detours, native offsets, OS layout changes, input injection, or
key-event logging. A transition frame is suppressed; held resolved positions must
be observed released by a binding query before activating under the new layout.
Only blocked target keys require release checks; no per-frame held-key scan runs.

The Windows dead-key fallback uses `MapVirtualKeyExW` with `MAPVK_VK_TO_CHAR`
(the unshifted character and dead-key flag), only after a missing Unity lookup
and only during the existing refresh. It adds no per-frame OS queries. macOS
retains the Unity-only lookup. `resolved_windows_dead_key` identifies this path
in detailed diagnostics; the legacy code records the resolved physical position.

Resolution uses Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout` and translates
its `Key` enum explicitly to legacy `KeyCode`. These enums are **not** numerically
interchangeable. Unity documents physical key positions separately from
[layout-dependent display names](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/manual/Keyboard.html).
The Windows research session verified live US/German lookup changes and unchanged
legacy physical key reporting, including across a game restart. That evidence
does not constitute macOS or every-layout runtime validation.

The generated vars file preserves `[shortcuts]` and `[shortcuts_source]`.
By default, layout output is only a compact `[keyboard_mapping]` snapshot: effective
mode, refresh mechanism, layout, status, generation and reason. Release builds always use this compact output and ignore the development diagnostic setting. For troubleshooting with a `debug` or `releasedbg` build,
set `[control].keyboard_layout_diagnostics = true` and restart with layout mode enabled.
This adds `[shortcuts_resolved]` entries for configured printable alternatives only:
configured chord (including modifiers), `configured_character`, physical US-reference
key, layout display name, legacy code and resolution status. Named controls are
omitted because layout resolution does not change them. Physical mode never emits
this detailed table. Disable the diagnostics setting and restart to remove it. Diagnostic getter failures leave bindings intact; unavailable diagnostic strings are empty. Release builds omit the detailed table, the setting from vars, the extra name/display getters, and exception stack formatting.
The physical key name describes the US-reference position, not the printed keycap.
Startup is `pending` until a game-thread printable-key query can inspect the keyboard.
Derived vars are rewritten on mapping/status changes only, not every frame; no
user settings are rewritten. Transient hold suppression does not change the
resolved mapping and is not a vars generation. Missing Unity APIs or a managed
exception in required mapping APIs outside the individual display-name search disable layout-resolved
bindings for that session; keyboard absence is
retried only on another device notification. Current-keyboard changes without a
notification remain undetected until one arrives.
`key_unavailable` means Unity could not find the display name;
`lookup_failed` means the search threw, disabling only that character until the
next notification rebuilds the mapping. Other bindings remain usable. Unity
1.14.2 can throw for unmatched symbols; see [failure details](KEYBOARD_LAYOUT_REFRESH.md).
`unsupported_physical_key` means its result cannot be bridged to legacy input.
Overall `resolved`/`partial` status describes the configured printable keys, not
every key on the keyboard. Diagnostics do not change resolution or its notification
schedule. Failures are still logged with diagnostics disabled; details are written
to the current vars snapshot, not appended as a history or emitted per keystroke.

## Validation

`xmake build keyboard-layout-tests` then `xmake run keyboard-layout-tests` exercises
the production conversion/transition core without launching STFC. In managed
workspaces use the local AX route for these commands.

Before release, live-test physical default and layout mode with US -> German -> US,
Y/Z actions, configured digits/punctuation, explicit Ctrl/Shift chords, a held key during transition, typing/focus and vars
generation/provenance. French A/Q, W/Z and M test beyond a Y/Z-only implementation.
Build success and modelled unit tests do not prove platform runtime behavior.
The prior live Y/Z evidence predates the expanded scope. The expanded mapping and
action tests cover digit/punctuation resolution and suppression, but reporter
testing on actual keyboards is still needed. No live punctuation pass is claimed.
