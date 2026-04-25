# Attack Incoming Troubleshooting Notes

This document records the working incoming-attack notification path and the debugging lessons from the live game investigation that produced it. Crashes were an expected part of the search, but the patterns here should keep the next round of probing smaller, safer, and easier to reason about.

## Shipped Baseline

- Early timing source: `ToastFleetObserver.NotificationsChangedEventHandler(NotificationProducerType)` with `NotificationProducerType.IncomingFleet == 7`.
- Rich target source: `ToastFleetObserver.QueueNotifications(List<Notification>)` filtered to producer type `7`.
- Target extraction is intentionally raw-field based after `Notification.IncomingFleetParams` is fetched:
  - `targetType` at `incoming_params + 0x18`.
  - oneof object pointer at `incoming_params + 0x28`.
  - oneof case at `incoming_params + 0x30`.
  - boxed target fleet id at `params_object + 0x10` when `params_case == 4`.
- Notification body construction maps the target fleet id back through the fleet-bar cache, producing text such as `Your Newton is under attack (Current Cargo: 23%)`.
- Plain WinRT notification delivery is the current stable backend. Click-to-focus is not solved in the shipped path.
- Fleet-bar combat transitions remain as late fallbacks, but they are not the first-warning signal.

## Working Data Flow

1. `FleetStateWidget.SetWidgetData` keeps a low-cost cache of local fleet ids, ship names, mining resources, cargo fill, and current fleet states.
2. `ToastFleetObserver.NotificationsChangedEventHandler` fires when the game's notification producer changes; producer type `7` is the early incoming-fleet signal.
3. `ToastFleetObserver.QueueNotifications` receives materialized `Notification` objects and exposes the incoming target payload.
4. The hook reads only the proven-safe `Notification` getters, then switches to raw offset reads for `IncomingFleetParams`.
5. `fleet_notifications_notify_incoming_attack_target(...)` dedupes, resolves the local target fleet from cache, builds the body, records live-debug context, and calls `notification_show(...)`.

## Crash Ledger

- `NotificationIncomingFleetParams.QuickScanResult` getter crashed during live queue-hook inspection.
- `NotificationIncomingFleetParams.TargetType` and `TargetFleetId` property getters also crashed in the same hook-time path.
- Raw reads of the same incoming params object survived once the offsets were identified.
- `StationWarningViewController` lifecycle hooks were not useful for real incoming-attack banner hits; they stayed silent during the path we needed.
- `NavigationInteractionUIViewController` direct lifecycle instrumentation was unstable when it did more than pointer-only capture. `AboutToShowCanvasEventHandler` became informative only after reducing it to raw breadcrumbs and deferring reporting.
- Treating `IVisibilityHandler` or generic `object` callback arguments as managed objects and reading `klass` metadata was unsafe in practice.
- Emitting live-debug JSON or recent-event records from `object_tracker` constructor/destructor hooks caused load/UI crashes. Object tracking can collect pointers there, but reporting must happen later.
- Per-frame UI polling and deep navigation/station-warning property walks produced too much crash surface for a production feature path.
- `Notification.set_IncomingFleetParamsJson` did not fire for the real incoming warning path, so it was a plausible but non-working materialization hook.
- WinRT toast activation callbacks did not fire reliably from the injected-process context. The Shell notification backend returned success in logs but produced no visible notifications and was removed from the active path.
- A release-prep regression disabled hotkey gates in `hotkeys.cc`; right-click/hotkeys came back when `kEnableHotkeyRouterFrame`, `kEnableShortcutInitializeHook`, `kEnableRewardsButtonHook`, and `kEnablePreScanTargetHook` were restored to `true`.
- macOS CI caught `std::string(File::MakePath(...))` because `File::MakePath` returns `std::u8string` on macOS. Use `File::MakePathString(...)` at string-consuming call sites.

## Mod/Game Interaction Lessons

- A stable object later in `live_debug_tick()` is not automatically safe to inspect inside the hook where it first appears.
- Managed property getters can execute game code, touch lazily materialized objects, or assume a call context the hook does not provide. Treat every new getter as a behavior change, not a simple field read.
- Pointer-only breadcrumbs are the safest first payload. Add one dereference at a time, validate boot, then validate the in-game trigger.
- Event-driven hooks are less noisy and safer than frame-sampled feature logic. Use polling as a temporary map, not as the preferred production source.
- The first obvious UI surface is often a symptom, not the owning model event. The incoming banner was best explained by the notification producer, not by station-warning or navigation UI controllers.
- Raw trace files are valuable when the game may crash before structured events flush. Use them to bracket the exact last successful read.
- Keep feature paths narrow. Diagnostic hooks should remain gated off after the stable event path is found.
- Cross-platform validation matters even for Windows-first debugging; libc++ and MSVC disagree on some conversions that look harmless locally.

## Safe Probe Rules

- Change one lever at a time.
- Capture raw pointers and primitive values first.
- Prefer deferred reporting from `live_debug_tick()` over logging, JSON, or event construction inside hot hooks.
- Validate game boot before using a banner hit as the next test.
- If a read crashes, keep the crash evidence and narrow the exact read; do not replace it with a broader guess.
- Before release, audit all temporary gates, isolation flags, and diagnostic-only hooks.

## Confirmed Signals

- `ToastFleetObserver.NotificationsChangedEventHandler` fires before combat and before the fleet-bar `Mining -> Battling` transition.
- `NotificationProducerType.IncomingFleet` is integer `7` in `Digit.PrimeServer.Models.proto`.
- `ToastFleetObserver.QueueNotifications` carries the incoming target fleet id early enough to build ship-specific notification bodies.
- The first reliable UI sequence captured around the warning was `HUD_AllianceHelpAndNews_Canvas -> NavigationInteractionUI -> HUD_WaveDefense_Canvas`, with `SystemInfo_Canvas` also appearing in later traces.
- A pointer-only `NavigationInteractionUIViewController.AboutToShowCanvasEventHandler` breadcrumb aligned with the native warning UI, but it is a diagnostic surface rather than the shipped notification source.

## Hostile vs Player Split Notes

The next likely split is not another UI hook. The model already hints at the classification path:

- `Notification.IncomingFleetParams` contains `quickScanResult = 2` in the proto.
- `QuickScanFleetData` contains `fleetType = 1`.
- `DeployedFleetType` values are:
  - `PLAYER = 1`
  - `MARAUDER = 2`
  - `NPCINSTANTIATED = 3`
  - `SENTINEL = 4`
  - `ALLIANCE = 5`
  - `CHALLENGE = 6`
- The likely classification mapping is `PLAYER` -> incoming attack by player, and `MARAUDER` / `NPCINSTANTIATED` / `SENTINEL` / `CHALLENGE` -> incoming attack by hostile. This is inferred from model names and still needs live validation.

Important safety constraint: do not call the existing `QuickScanResult` getter in the queue hook again. It already crashed. The next probe should bracket the raw quick-scan field layout with pointer-only traces first, then read only the primitive `fleetType` once the object pointer and offset are proven.

Suggested investigation order:

1. Add a gated raw-pointer trace around the `quickScanResult` field on `IncomingFleetParams`, without dereferencing the quick-scan object.
2. Validate boot and one incoming warning.
3. If stable, read only the quick-scan `fleetType` primitive and emit it to recent events.
4. Collect at least one known player attack and one known hostile attack.
5. Only after live validation, split config/toast state names into `_player` and `_hostile` variants.

Open question: the generated protobuf layout suggests `targetType`, `quickScanResult`, and the oneof payload are adjacent, but the IL2CPP in-memory layout must be validated from the live object rather than assumed from the proto alone.