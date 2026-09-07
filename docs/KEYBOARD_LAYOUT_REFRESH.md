# Keyboard layout refresh

`[control].keyboard_letter_mode = "layout"` still maps configured A-Z through
Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout`. Physical mode remains the default
and bypasses the observer and all layout queries. Modifiers, nonletters, the legacy
physical input cache, and generated `keyboard_mapping` / `shortcuts_resolved` diagnostics
retain their existing meaning.

On Windows x64, an `InputSystem.onDeviceChange` observer invalidates the letter map
on device lifecycle events or configuration changes for the mapped keyboard. This
also handles configuration events whose layout name has not changed. The callback
only sets atomic state; layout lookup and diagnostics stay on the game thread.

Every queried frame still checks `Keyboard.current`, because ordinary input can
change the current keyboard without a device-change notification. The layout-name
getter and 26 letter lookups run when invalidated or when the keyboard changes.
Held-key release is checked once per queried frame. A notification between two
consumers in the same frame causes another refresh without clearing that frame's
transition suppression. Held mapped keys remain blocked until release.

The observer uses an existing AOT delegate class and a private copy of compatible
static method metadata; it adds no native detour and never edits game metadata.
The delegate is rooted for the process lifetime. A failed subscription falls back
to the prior once-per-frame layout-name polling. Failed removal disables the callback
and retains its root and metadata to avoid a dangling listener. Live DLL unloading
is not supported. Resolver failure disables layout letters as before.

The native delegate path is qualified only for Windows x64. Other platforms retain
layout-name polling, which cannot detect a same-name configuration edit. The generated
`keyboard_mapping.refresh` value reports `device_notifications`, `layout_name_polling`,
`pending`, or `not_queried`; no new user setting is required.

The adapter was derived against client 260, Unity 6000.0.59f2 and Input System 1.14.2.
Runtime metadata checks validate by-value parameter shapes and the enum's 32-bit
representation before subscribing. These guards do not replace client-update testing.
The standalone `keyboard-layout-tests` target covers mapping, held-key transitions,
and same-frame invalidation timing. OS layout switching and keyboard reconnection
still require runtime testing on each supported platform.
