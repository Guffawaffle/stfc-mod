# Layout notification experiment

Local study branch based on PR269 at f402de6. This is an observer/self-test, not the proposed production cache rewrite. Existing layout lookup, action resolution, held-key protection and generated mapping diagnostics are unchanged.

The prototype compiles only on Windows with `_MODDBG` (releasedbg). It runs only when both `[control].keyboard_letter_mode = "layout"` and the game process environment variable `STFC_LAYOUT_NOTIFICATION_PROBE=selftest` are set. It does not enable itself in physical mode. Never publish this experiment as an upstream fix or add the probe setting to user examples.

It constructs a native callback through the existing AOT Action<InputDevice, InputDeviceChange> delegate class, using a private copy of compatible method metadata. A direct delegate invocation first verifies the native callback argument path. The game's method metadata is not modified and no detour is installed. Pinned handles keep the delegate and sampled keyboard alive during the experiment.

The self-test then queues three same-layout configuration-change events using Unity's public API:

1. Subscribe once, call the idempotent subscribe helper again, and observe one notification.
2. Unsubscribe, queue another event and observe no callback over at least three Input System updates.
3. Resubscribe (again exercising the idempotent helper), queue the last event, observe one notification and remove the listener.

It never changes the OS layout, sends key/text events, or forces an Input System update. Configuration events do invalidate Unity's control and action-binding caches, so this is an active runtime experiment, not passive monitoring. Natural configuration changes during the test may produce an inconclusive extra-event failure; the test cannot attribute every event uniquely to its own queue call. The detached observation window does not independently prove the queued event's dispatch time.

The callback only updates atomic counters for the sampled keyboard. Frame-side code logs a few lifecycle summaries under `[LayoutProbe]`; it records no keystrokes, text, account data, or keyboard pointer values. Stop/timeout paths unsubscribe and free handles. If removal fails, they disable the callback and retain the roots and private metadata until process exit rather than risk dangling state. A15-second deadline is checked on the next queried layout frame; pausing those queries pauses cleanup. A resolver failure cancels an active probe on the next query.

Expected pass line: `result=pass reason=same_layout_delivery_detach_and_reattach removed=true adds=2 removes=2 direct=1 config_events=2`.

Use the checkout's own AX wrapper to build/cycle. Capture the currently installed play DLL hash and original setting before a runtime trial, and restore the setting and the play checkout's DLL through its AX wrapper afterward. Do not leave this older isolated PR branch deployed as the full play stack. It does not include the separate PR272 chat-focus correction, so this experiment makes no chat-recovery claim.

Build-only success does not prove the delegate constructor/ABI or event-delivery path. Windows runtime results will not qualify macOS. Natural OS layout switches, device replacement, same-name custom-layout edits and a production invalidation consumer remain later experiments.
