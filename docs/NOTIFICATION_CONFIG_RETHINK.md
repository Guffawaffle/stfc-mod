# Canonical Notification Configuration

Issue #185 replaces the duplicated notification settings with one event-first
policy surface under `[notifications]`.

The TOML remains the power-user and compatibility surface. Release examples and
fresh configs expose the explicit release-supported event allowlist rather than
treating every retained catalog mapping as proven.

## Canonical values

Every release-supported event has one meaningful root key:

```toml
[notifications]
victory = true
armada_battle_won = { system = true, audio = true, sound = "success" }
armada_battle_lost = { system = true, audio = true, sound = "warning" }
armada_created = false
fleet_arrived_in_system = { system = true, audio = true, sound = "arrival" }
fleet_repair_complete = true
```

The value is the entire policy for that event:

- `false`: no system or audio delivery.
- `true`: system delivery only.
- `{ system = bool, audio = bool, sound = string }`: explicit channel policy.

An inline table never inherits omitted fields from deprecated settings. Omitted
`system` and `audio` fields are `false`. If audio is enabled without a sound,
the resolver warns and uses that event's catalog sound.

Supported sound names are `none`, `default`, `info`, `success`, `warning`,
`alarm`, `arrival`, `soft`, `ping`, and `repair`.

Invalid values warn and fall back to safe catalog fields for that event. They do
not fall through to deprecated values. Unknown fields are ignored and recorded
as diagnostics.

## Defaults and master switches

Every event defaults to `false`. A newly generated config contains the public
release-supported event keys and no notification master switches or nested
channel/event tables.

The old `notifications_enabled` and `notifications_audio_enabled` settings are
not canonical master switches. They remain hidden compatibility gates, default
to `true`, and affect only events resolved from deprecated settings. A canonical
event policy is complete and ignores them.

There is no global `default_sound`. Every event has an explicit catalog sound,
and a missing or invalid sound is warned and resolved against that catalog.

## Compatibility and precedence

Deprecated notification settings remain readable through the 2.x line and are
scheduled for removal in 3.0.0:

- flat `notifications_*` and `notifications_audio_*` keys;
- `[notifications.system]` and `[notifications.audio]` channel tables;
- `[notifications.events.<category>]` event tables;
- `[ui].notify_on_banner_types` and `[ui].notify_banner_types`.

For an event with no canonical key, the resolver combines the deprecated inputs
using the established compatibility precedence and then applies any explicit
legacy master gates. It emits an aggregate deprecation warning.

Sparse deprecated files are resolved per event: an old key for one event does
not activate historical defaults for unrelated events. An explicit deprecated
master is file-wide and therefore opts every event into its historical 2.x
default before applying the gate. Missing deprecated masters use the hidden
`true` compatibility default so an explicitly configured legacy event does not
need a second switch.

If a canonical key is present, it wins as a whole. All deprecated inputs for
that event are ignored, conflicts are warned, and there is no partial merge or
fallback into them.

## Runtime provenance

`community_patch_runtime.vars` always writes the normalized inline policy for
every event:

```toml
[notifications]
fleet_arrived_in_system = { system = true, audio = true, sound = "arrival" }

[notifications.provenance]
fleet_arrived_in_system = { source_kind = "canonical", system_source = "notifications.fleet_arrived_in_system.system", audio_source = "notifications.fleet_arrived_in_system.audio", sound_source = "notifications.fleet_arrived_in_system.sound", deprecated_inputs = false, conflict = false, removal_target = "", diagnostic_count = 0, diagnostics = [], diagnostics_truncated = false, ignored_sources = [], ignored_sources_truncated = false }
```

Provenance records:

- whether the result came from canonical input, deprecated compatibility, or an
  invalid canonical fallback;
- the source selected for each field;
- whether deprecated inputs were present and their removal target;
- ignored conflicting sources;
- bounded diagnostic facts and their total count;
- explicit truncation markers for diagnostic facts and ignored sources.

`notifications.resolution` separately lists unknown root keys, including likely
typos, with its own count and truncation marker.

When asking a user to report notification behavior, request both
`community_patch_settings.toml` and `community_patch_runtime.vars`. The first
shows intent; the second shows the exact resolved policy and why it won.

## Runtime ownership

`notification_catalog.h` is the authoritative event/name/sound catalog.
`notification_policy.cc` resolves configuration once during startup.
Notification producers query the resolved per-event policy; they do not inspect
master switches or configuration aliases directly.

Runtime notification testing should use the bridge described in
`RUNTIME_CONFIG_BRIDGE.md`, not an ad hoc TOML reload.
