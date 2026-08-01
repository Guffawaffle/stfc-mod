# Notification Architecture

The notification stack is split between a thin platform router and targeted feature modules.

## Configuration

`notification_catalog.h` owns the complete event catalog and each event's
canonical root key and fallback sound. `notification_policy.cc` resolves the
canonical `false` / `true` / inline-table value or, when no canonical value is
present, the deprecated compatibility inputs.

The complete catalog is a compatibility and research surface. The smaller
`kPublicNotificationKinds` allowlist owns which release-supported events appear
in release examples and fresh generated configs. Adding a catalog entry does
not promote it to that public surface.

Notification producers consume only the resolved event policy. The old system
and audio masters are compatibility gates for deprecated inputs and must not be
checked in producer or delivery code.

See `NOTIFICATION_CONFIG_RETHINK.md` for the canonical schema, precedence, 3.0.0
removal target, and runtime provenance contract.

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
