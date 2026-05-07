# Notification Config Rethink

Branch note for `guffa/notification-config-rethink`.

For the broader config architecture proposal, see `docs/CONFIG_SYSTEM_RETHINK.md`.

Important framing: nested notification TOML is a migration bridge, not the final
user experience. The long-term goal should be a schema-backed settings UI in the
optional launcher/sidecar companion, with TOML retained for compatibility and
power users.

Runtime notification testing should use the runtime config bridge described in
`docs/RUNTIME_CONFIG_BRIDGE.md`, not runtime TOML reloads.

## Context

The current notification settings are a crowded flat `[notifications]` section.
They now cover several different concerns:

- OS/system notifications, delivered outside the game.
- In-game banner suppression, currently configured under `[ui].disabled_banner_types`.
- In-game audio cues, recently added as `notifications_audio_*` keys.
- Event selection for both toast-backed game events and fleet-derived events.

Upstream issue `netniV/stfc-mod#28` already calls out the bigger config problem:
config values are hand-read in the patch, defaults live near load logic, and future
UIs would need duplicate schema knowledge. Notification config is a good contained
place to prototype a cleaner shape without trying to solve the whole config system
in one move.

## Naming Model

Prefer channel names that describe delivery or policy, not implementation details:

- `notifications.system`: OS-native notifications, such as Windows toast delivery.
- `notifications.audio`: in-game audible cues.
- `notifications.banners`: in-game banner policy. This replaces the mental model of
  "hide in game" with a concrete noun: banners.

Avoid `notifications.hide_in_game` as a top-level concept. It reads like a delivery
channel, but the mod is not delivering an in-game notification there; it is deciding
which game banners to suppress.

## Proposed TOML Shape

Channel-first keeps the user's question simple: "where should this event show up?"

```toml
[notifications.system]
enabled = false

[notifications.system.fleet]
arrived_in_system = true
node_depleted = true

[notifications.system.battle]
victory = true
defeat = true
partial_victory = true

[notifications.audio]
enabled = false
default_sound = "system:notification"
cooldown_ms = 1500

[notifications.audio.fleet]
arrived_in_system = true

[notifications.audio.incoming_attack]
player = true
hostile = true
sound = "system:exclamation"

[notifications.banners]
# Game banners remain visible by default. Put only suppressed banner/event names here.
hide = ["standard", "faction_warning"]
```

Important defaults:

- `notifications.system.enabled = false`
- `notifications.audio.enabled = false`
- Every event toggle defaults to `false` unless explicitly present.
- `notifications.banners.hide = []`, so in-game banners remain visible unless listed.

That lets user configs stay sparse. A player only writes the events and delivery
channels they actually want.

## Why Channel-First

An event-first shape is also possible:

```toml
[notifications.fleet.arrived_in_system]
system = true
audio = true
sound = "system:notification"
```

That is compact for one event, but it becomes harder to scan when a user wants to
answer questions like "what makes sound?" or "which events produce desktop
notifications?" Channel-first also maps cleanly onto future UI tabs: System, Audio,
Banners.

## Separate `notifications.toml`

A second file is tempting because the main config is large, but it should not be
the first migration step.

Pros:

- Keeps the main config shorter.
- Makes notification presets easier to share.
- Gives future UI code a smaller file to own.

Cons:

- Adds load-order questions: main file wins, notification file wins, or merge?
- Creates a new missing-file and partial-file failure mode.
- Makes runtime snapshots and generated defaults more complicated.
- Risks solving one feature's config problem separately from issue #28's unified
  config module goal.

Recommendation: keep notifications in `community_patch_settings.toml` for this
branch. Later, a generic config include/import system can support files like
`notifications.toml` consistently for every subsystem.

If an installable companion app becomes the normal settings surface, a separate
`notifications.toml` becomes less important. The UI can edit the same canonical
schema and write one sparse overlay file, while offering import/export presets for
notification profiles.

## Compatibility Plan

Do not remove existing keys in the first implementation.

Read order should be:

1. Load legacy flat keys such as `notifications_enabled` and
   `notifications_fleet_arrived_in_system`.
2. Load the new nested channel keys if present.
3. Let new nested keys override legacy keys.
4. Write resolved values to `community_patch_runtime.vars` using the new nested
   shape, plus optional legacy compatibility lines while the migration is active.

This lets existing configs keep working while new configs can be much smaller.

## Implementation Sketch

Introduce an event catalog shared by all notification channels:

```cpp
enum class NotificationEvent {
  BattleVictory,
  BattleDefeat,
  FleetArrivedInSystem,
  FleetNodeDepleted,
  IncomingAttackPlayer,
  IncomingAttackHostile,
};
```

Then model channels separately:

```cpp
struct NotificationChannelConfig {
  bool enabled = false;
  std::bitset<MaxNotificationEvents> events;
};

struct NotificationAudioConfig : NotificationChannelConfig {
  std::string default_sound = "system:notification";
  int cooldown_ms = 1500;
};

struct NotificationBannerConfig {
  std::bitset<MaxNotificationEvents> hidden_events;
};
```

The current `Toast::State` bitset can remain internally, but it should be derived
from the event catalog rather than exposed as the primary config model.

## First Code Slice

The first parser change should be intentionally narrow:

Status: implemented for the notification bool pilot. Canonical nested keys are
read first, flat `notifications_*` keys remain aliases, and canonical keys win
when both are present.

- Add nested TOML reads for `notifications.system.enabled`.
- Add nested TOML reads for `notifications.system.fleet.arrived_in_system`.
- Add nested TOML reads for `notifications.audio.enabled`.
- Add nested TOML reads for `notifications.audio.fleet.arrived_in_system`.
- Keep the current flat keys as fallback.
- Update only the example notification section to show the new shape.

This proves the migration strategy with the known-working fleet arrival event
before expanding the whole catalog.

The same two audio booleans are also the safest first runtime-applyable settings:
they change delivery policy only, so the player can enable them from the settings
app and test the audio path without restarting the game.