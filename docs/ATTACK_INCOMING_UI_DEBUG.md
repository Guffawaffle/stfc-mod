# Attack Incoming UI Debug Notes

## Current Conclusion

- The confirmed early source for attack-incoming notifications is `ToastFleetObserver.NotificationsChangedEventHandler(NotificationProducerType)` with producer type `7` (`IncomingFleet`).
- `ToastFleetObserver.QueueNotifications(List<Notification>)` can safely provide target-fleet context when using raw oneof field reads rather than managed property getters.
- A live test confirmed the Windows notification appeared at the first in-game incoming warning, before combat started.
- Raw trace evidence showed `toast-fleet-producer/before-notification` running before the original `NotificationsChangedEventHandler` returned, with `sender=0000000000000007`.
- The combat-time fleet-bar notifications remain useful as a fallback, but they are not the first-warning source.
- Per-frame UI polling of navigation/station warning controllers is not safe enough for this feature path and should stay disabled.

## Verified Findings

- `ToastFleetObserver.NotificationsChangedEventHandler` fires for `NotificationProducerType.IncomingFleet` before the first visible incoming-warning window reaches combat state.
- `NotificationProducerType.IncomingFleet` maps to integer value `7` in `Digit.PrimeServer.Models.proto`.
- `ToastFleetObserver.QueueNotifications(List<Notification>)` carries the incoming target fleet id for richer bodies such as `Your [ship] is under attack`.
- `Notification.IncomingFleetParams.QuickScanResult`, `TargetType`, and `TargetFleetId` managed property getters crashed in live tests; raw reads of `targetType` (`0x18`), oneof object (`0x28`), and oneof case (`0x30`) survived.
- The native red banner window is not currently controlled by the `StationWarningViewController` path we first guessed.
- `StationWarningViewController` lifecycle hooks never fired during real incoming-attack banner hits in live tests.
- `Notification.set_IncomingFleetParamsJson` materialization hooks also stayed silent during real incoming-attack banner hits.
- The first reliable UI sequence captured around the banner window was:
  - `HUD_AllianceHelpAndNews_Canvas` with child `ButtonContainer`
  - `NavigationInteractionUI` with child `Container`
  - `HUD_WaveDefense_Canvas` with child `BodyContainer`
- Later traces also showed `SystemInfo_Canvas` briefly in the same window.
- Runtime tracking confirmed `Digit.Prime.Navigation.NavigationInteractionUIViewController` instances are live during the banner window.
- With safe poll-based observation, one navigation-interaction controller appeared without context first, then another instance bound context exactly when the top canvas switched to `NavigationInteractionUI`.
- The bound navigation context was observed with:
  - `ContextDataState = Verified`
  - `InputInteractionType = HideAll`
  - `ThreatLevel = VeryHard`
  - `ValidNavigationInput` and `ShowSetCourseArm` flipping from `false` to `true`
- A direct lifecycle probe showed `NavigationInteractionUIViewController.OnEnable` and `NavigationInteractionUIViewController.AboutToShowCanvasEventHandler` firing immediately before the top canvas switched to `NavigationInteractionUI`.
- In an isolated single-lever test, `NavigationInteractionUIViewController.AboutToShowCanvasEventHandler` produced a deferred raw-pointer breadcrumb during a real incoming-attack trigger without crashing the game.
- In that same trace, the hook note aligned with:
  - top canvas changing from `HUD_AllianceHelpAndNews_Canvas` to `NavigationInteractionUI`
  - a second tracked `NavigationInteractionUIViewController` instance binding context
  - the `sender` pointer matching the visible `NavigationInteractionUI` canvas pointer
  - a follow-up navigation-context change where `ValidNavigationInput = true` and `ShowSetCourseArm = true`

## Working Surfaces

- Strongest current hook surface: `ToastFleetObserver.NotificationsChangedEventHandler`
  Why: filtering producer type `7` (`IncomingFleet`) fires at the first incoming-warning moment and remains the fallback signal when target context is unavailable.

- Rich target-context surface: `ToastFleetObserver.QueueNotifications`
  Why: it exposes the target fleet id early enough to build ship-specific OS notification bodies.

- Useful fallback surface: fleet-bar state transitions in `FleetStateWidget.SetWidgetData`
  Why: it catches late combat-time incoming attacks, but it is too late for first-warning notification semantics.

- Useful supporting surface: top-canvas polling
  Why: it gives a stable ordering anchor around `HUD_AllianceHelpAndNews_Canvas -> NavigationInteractionUI -> HUD_WaveDefense_Canvas -> SystemInfo_Canvas`.

- Weak or currently non-informative surfaces:
  - `NavigationInteractionUIViewController.OnEnable`: safe in pointer-only deferred form, but silent on the real banner trigger.
  - `StationWarningViewController` lifecycle hooks: silent on real banner hits.
  - `Notification.set_IncomingFleetParamsJson`: silent on real banner hits.

## Safety Findings

- Deep property walks are the recurring crash pattern in this codebase. Pointer-only presence checks and layout-verified raw field reads are materially safer.
- Treating UI visibility callback arguments (`IVisibilityHandler`, `object`) as managed objects and reading `klass` metadata was unsafe in practice.
- Emitting live-debug JSON or event recording from `object_tracker` constructor/destructor hooks was also unsafe in practice.
- Direct detours on `NavigationInteractionUIViewController` lifecycle methods were unstable in the attempted form, even after reducing payload complexity.
- Stable tracking and stable polling are not the same as stable hook-time instrumentation. A path may be valid to observe later from `live_debug_tick()` while still being unsafe to instrument directly at hook time.

## Current Safe Baseline

- Keep `ToastFleetObserver.QueueNotifications` as the rich incoming-attack feature path and `ToastFleetObserver.NotificationsChangedEventHandler` as the fallback early signal.
- Keep top-canvas and active-child-name probes only as diagnostic support.
- Keep navigation/station warning controller polling disabled for incoming-attack notification delivery.
- Do not currently detour `NavigationInteractionUIViewController` lifecycle methods.
- Do not currently emit live-debug events from `object_tracker.cc`.

## Historical UI Experiments

The following matrix records how we found the path. It should not be used as the default next step for the feature now that the ToastFleetObserver producer hook is confirmed.

## Experiment Rules

- Change one lever at a time.
- Prefer pointer-only capture first.
- Prefer deferred reporting from `live_debug_tick()` over JSON/log work inside hook bodies.
- Validate game boot after every instrumentation change before using banner hits as the next check.
- If a path crashes, do not treat it as permanently impossible; treat it as evidence that the current call shape or payload is wrong.

## Next Experiment Matrix

- Lever: `NavigationInteractionUIViewController.OnEnable`
  Hypothesis: the hook surface may be usable if the hook only stores a raw breadcrumb and defers reporting.
  First payload: raw pointer + phase only.
  Validation: boot stability first, then banner-hit stability.

- Lever: `NavigationInteractionUIViewController.AboutToShowCanvasEventHandler`
  Hypothesis: the hook surface may be usable if callback arguments are treated as opaque pointers only.
  First payload: controller pointer + sender pointer + context pointer only, deferred to `live_debug_tick()`.
  Validation: boot stability first, then banner-hit stability.

- Lever: navigation context fields in poll-based observation
  Hypothesis: one or more context field reads may still be riskier than pointer-only controller polling.
  First payload: controller pointers only.
  Validation: boot stability and normal UI navigation stability.

- Lever: canvas show/hide event surfaces outside the controller
  Hypothesis: a lower-level `CanvasController` or visibility-event registration point may be more stable than detouring the controller methods directly.
  First payload: canvas pointer + name only.
  Validation: boot stability first, then banner-hit alignment.

## Current Experiment Results

- Experiment 1: `NavigationInteractionUIViewController.OnEnable` only
  - Hook body: raw-pointer note only, deferred flush from `live_debug_tick()`.
  - Other navigation lifecycle detours: disabled.
  - Result: game booted cleanly after `ax cycle`.
  - Result: no `navigation-interaction-hook-note` fired during initial load.
  - Result: after an isolated incoming-attack trigger on `Serene Squall`, the game remained healthy but still emitted no `navigation-interaction-hook-note`.
  - Interpretation: `OnEnable` is currently a safe surface to detour with a deferred raw-pointer breadcrumb, but it is not the informative banner-aligned surface for this flow.

- Experiment 2: `NavigationInteractionUIViewController.AboutToShowCanvasEventHandler` only
  - Hook body: raw-pointer note only, deferred flush from `live_debug_tick()`.
  - Other navigation lifecycle detours: disabled.
  - Result: builds and boots cleanly.
  - Result: after an isolated incoming-attack trigger on `Serene Squall`, the game remained healthy and emitted `navigation-interaction-hook-note` with phase `AboutToShowCanvasEventHandler`.
  - Result: the hook note aligned with the top-canvas transition to `NavigationInteractionUI`, a context-bound navigation controller instance, and a follow-up context change to `ValidNavigationInput = true` / `ShowSetCourseArm = true`.
  - Interpretation: this is the first direct hook surface that is both stable and informative for the native banner window.