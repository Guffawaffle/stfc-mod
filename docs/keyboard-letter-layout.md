# Layout-aware shortcuts

`[control].keyboard_layout_mode = "physical"` preserves existing behavior (default).
Set it to `"layout"` to resolve configured printable keys using Unity's active
keyboard-layout display names: letters, digits, and punctuation accepted by the
shortcut parser. `keyboard_letter_mode` remains a compatibility alias with the
expanded behavior; the new setting takes precedence if both are present.
For example, on German QWERTZ `show_daily = "Z"` follows German Z, which occupies
the US-QWERTY Y position. Existing Y/Z workaround configurations should be undone
when opting in. Restart once to change this setting; subsequent OS layout changes
are detected in-game without restarting.

This is action binding, not text input. Shift/Ctrl/Alt chords retain their existing
semantics; no modifiers are inferred from a character. Named controls (Escape,
arrows, function keys, Space, explicit numpad keys, and mouse buttons), hardcoded
controls, and native Scopely shortcuts retain their existing behavior.

Every configured printable key is eligible for lookup, but Unity must report a
matching display name and a physical position supported by the legacy input API.
A missing/unsupported key is disabled in layout mode, never silently treated as
physical. This includes symbols accessible only through Shift/AltGr when Unity
does not expose them as display names. Unicode text composition, dead-key
sequences, and automatic Shift/AltGr character translation are not supported.
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
the existing physical `Key` cache. A per-frame check reads Unity's current keyboard.
On Windows x64, device notifications invalidate the layout cache, including when
the layout name stays the same. Other platforms retain per-frame layout-name
polling. Only distinct configured printable keys are looked up on invalidation or
keyboard/layout change; duplicate bindings share the cached result.
See [refresh implementation](KEYBOARD_LAYOUT_REFRESH.md) for lifetime and fallback details.
Physical mode does not resolve Unity layout methods or poll keyboard layout state.
There are no new detours, native offsets, OS layout changes, input injection, or
key-event logging. A transition frame is suppressed; held resolved positions must
be released before activating a binding under the new layout.

Resolution uses Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout` and translates
its `Key` enum explicitly to legacy `KeyCode`. These enums are **not** numerically
interchangeable. Unity documents physical key positions separately from
[layout-dependent display names](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/manual/Keyboard.html).
The Windows research session verified live US/German lookup changes and unchanged
legacy physical key reporting, including across a game restart. That evidence
does not constitute macOS or every-layout runtime validation.

The generated vars file preserves `[shortcuts]` and `[shortcuts_source]`.
`[keyboard_mapping]` reports mode, provider, refresh mechanism, layout, status, generation, reason,
scope and the US-reference position convention. `[shortcuts_resolved]` records
each parsed alternative with its configured chord (including modifiers), `configured_character`,
physical US-reference key, layout display name, legacy code and resolution status.
Startup is `pending` until a game-thread printable-key query can inspect the keyboard.
Derived vars are rewritten on mapping/status changes only, not every frame; no
user settings are rewritten. Transient hold suppression does not change the
resolved mapping and is not a vars generation. Missing Unity APIs or a managed
exception disable layout-resolved bindings for that session; keyboard absence is retried.
`key_unavailable` means Unity could not find the display name;
`unsupported_physical_key` means its result cannot be bridged to legacy input.
Named controls report `unchanged_named_control`. Overall `resolved`/`partial`
status describes the configured printable keys, not every key on the keyboard.

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
