# Keyboard layout refresh

`[control].keyboard_layout_mode = "layout"` maps configured printable keys through
Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout`. Physical mode remains the default
and bypasses the observer and all layout queries. Explicit modifiers, named controls, the legacy
physical input cache, and generated `keyboard_mapping` / `shortcuts_resolved` diagnostics
retain their existing meaning.

On Windows x64, an `InputSystem.onDeviceChange` observer invalidates the printable-key map
on device lifecycle events or configuration changes. This
also handles configuration events whose layout name has not changed. The callback
only sets atomic state; layout lookup and diagnostics stay on the game thread.

Unity's current keyboard, layout name, and configured key names are queried only
at initialization or after a notification. Quiet queries read the cached mapping
and dirty flag; they do not query Unity's frame clock, keyboard, or layout.
The frame clock is consulted on rebuild and while the transition-frame guard is
pending. Release checks apply only to a queried target blocked by that transition,
and stop once its release is observed. There is no per-frame scan of held keys.
A notification between consumers in the same frame rebuilds without clearing the
transition guard. Held mapped keys remain blocked until release is observed.

Without polling, switching the current keyboard through ordinary input alone is
not detected until a device notification arrives. Missing keyboards or layout
names are retried on the next notification, not on a timer or input frame.

The observer uses an existing AOT delegate class and a private copy of compatible
static method metadata; it adds no native detour and never edits game metadata.
The delegate is rooted for the process lifetime. A failed subscription logs
`notification_subscription_failed` and disables layout bindings until restart.
There is no polling or physical-position fallback. Failed removal disables the callback
and retains its root and metadata to avoid a dangling listener. Live DLL unloading
is not supported. Resolver failure disables layout-resolved bindings.

The native delegate path is qualified only for Windows x64. On other platforms,
including macOS, layout mode logs `notifications_unsupported` and disables layout
bindings until restart. Physical mode and named controls remain available.
The generated `keyboard_mapping.refresh` reports `device_notifications`, `disabled`,
`pending`, or `not_queried`. Disabled sessions report `effective_mode = "unavailable"`.

The adapter was derived against client 260, Unity 6000.0.59f2 and Input System 1.14.2.
Runtime metadata checks validate by-value parameter shapes and the enum's 32-bit
representation before subscribing. These guards do not replace client-update testing.
The standalone `keyboard-layout-tests` target covers mapping, held-key transitions,
same-frame invalidation, and zero clock/input calls on quiet cached queries.
OS layout switching and keyboard reconnection
still require runtime testing on each supported platform.
