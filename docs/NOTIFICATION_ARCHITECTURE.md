# Notification Architecture

The notification stack is split between a thin platform router and targeted feature modules.

## Boundaries

- `notification_service.cc` owns generic toast routing, queue batching, localization fallback, and OS notification delivery.
- Feature modules own game-domain interpretation and user-facing message construction.
- Hook modules under `mods/src/patches/parts/` capture raw game signals and hand them to feature modules. They should not contain notification policy beyond safe extraction of hook arguments.

## Incoming Attacks

Incoming attack desktop notifications use one production source of truth:

- `ToastFleetObserver.QueueNotifications` provides the target fleet/station and quick-scan attacker data.
- `fleet_notifications.cc` maps quick-scan fleet type to hostile/player, resolves the target ship name from the fleet-bar cache, builds the message, and dedupes the event.
- `incoming_attack_notifications.cc` consumes matching `ToastObserver` incoming-attack toasts so they do not produce duplicate generic notifications.

Do not reintroduce peer fallback branches for incoming attacks unless the queue source is proven unavailable. The removed branches were noisy because they inferred targets from broad producer notifications, navigation UI activity, station warning UI, or fleet-bar state transitions.

Incoming attack copy intentionally avoids cargo and mining-node context. The core behavior is:

- Hostile: `Incoming Hostile Attack` / `Your <ship> is being chased.`
- Player: `Incoming Player Attack` / `Your <ship> is under attack by another player.`

## Duplicate Avoidance

Incoming-attack dedupe is keyed by target kind, target id, attacker kind, and attacker identity when quick-scan provides one. It uses TTL pruning plus a hard entry cap so repeated unique attackers cannot grow the cache without bound.