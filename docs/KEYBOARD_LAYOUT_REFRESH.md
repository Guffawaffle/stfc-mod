# Keyboard layout refresh

`[control].keyboard_layout_mode = "layout"` maps configured printable keys through
Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout`. Physical mode remains the default
and bypasses the observer and all layout queries. Explicit modifiers, named controls, the legacy
physical input cache, and generated `keyboard_mapping` / `shortcuts_resolved` diagnostics
retain their existing meaning.

On Windows x64 and experimentally on macOS arm64/x86_64, an `InputSystem.onDeviceChange`
observer invalidates the printable-key map
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

The native delegate path has Windows x64 runtime evidence. The macOS arm64/x86_64
port is experimental and logs that status once when layout mode initializes;
neither Mac architecture has an in-game validation receipt yet. Other platforms
log `notifications_unsupported` and disable layout bindings until restart.
Physical mode and named controls remain available.
The generated `keyboard_mapping.refresh` reports `device_notifications`, `disabled`,
`pending`, or `not_queried`. Disabled sessions report `effective_mode = "unavailable"`.

The adapter was derived against client 260, Unity 6000.0.59f2 and Input System 1.14.2.
Runtime metadata checks validate by-value parameter shapes and the enum's 32-bit
representation before subscribing. These guards do not replace client-update testing.
The standalone `keyboard-layout-tests` target covers mapping, held-key transitions,
same-frame invalidation, and zero clock/input calls on quiet cached queries.
OS layout switching and keyboard reconnection
still require runtime testing on each supported platform.

## macOS validation handoff

The Mac port reuses the same Unity event subscription, private method metadata,
rooted delegate and atomic invalidation. It adds no Cocoa observer, native detour,
timer or polling fallback. Construction follows the vendored libil2cpp
`Type::InvokeDelegateConstructor` in `third_party/libil2cpp/vm/Type.cpp`: the game's
generated invoker marshals the constructor arguments for the current platform.
The native callback takes a device pointer, a validated by-value 32-bit enum and
the method metadata pointer, with no variadic or aggregate arguments. The compiler
supplies the target ABI; no Windows register or stack assumptions are reused.
This source-level rationale does not prove the exact Mac client's delegate stub
or notification delivery. Check those on both arm64 and x86_64 before declaring
the port runtime-validated; testing one architecture does not qualify the other.

For a Mac developer reviewing this port:

1. Record PR head, built artifact, game version, macOS version and the architecture
   of the running game/mod. Confirm delegate metadata and invocation in the exact
   client's IL2CPP runtime, especially constructor setup and the static callback.
2. With physical mode, confirm existing shortcuts still work. Opt into layout mode
   and restart; expect the experimental notice and `refresh=device_notifications`.
   A successful subscription log alone does not prove callbacks arrive.
3. Switch between two input sources with different key positions and back, then
   query a configured printable shortcut. Confirm a new mapping generation and
   the intended action without restarting. Try a digit/punctuation binding and an
   explicit modifier chord that Unity can resolve on that layout.
4. Hold a target during a switch, release and press again; check chat suppression
   and recovery. If available, disconnect/reconnect an external keyboard. Record
   which scenarios actually emitted a notification; do not infer event delivery
   solely from changing the OS input source or from a successful build.
5. If setup fails, capture the relevant `[KeyboardLayout]` log lines and mapping
   entries. It must remain disabled until restart with no polling recovery. Share
   only those entries, not the complete user config or typed chat.

If the Mac client does not deliver the needed Unity events, revise the notification
approach with platform evidence. Keep the user's requirement: no per-frame layout
polling and no polling fallback.
