# Launcher configuration schema

`config-schema.guffawaffle.v1.json` is the generated launcher-facing contract
for the Guffawaffle release stream. It is derived from the mod's runtime-owned
C++ defaults and catalogs; do not edit it by hand.

Generate or validate it from the repository root:

```powershell
node scripts/generate_config_schema.mjs
node scripts/generate_config_schema.mjs --check
node --test scripts/test_config_schema.mjs
```

CI runs both the fixture tests and the stale-artifact check before the C++ test
suite.

## One settings model, three adapters

Every setting has the same launcher metadata envelope: canonical path, title,
description, type, runtime default, platform support, apply behavior,
sensitivity, stability, source support, aliases, and provenance.

The `control` field selects one of three value adapters:

- `scalar` for booleans, numbers, strings, enums, and dynamic sync-target
  fields;
- `keybinding` for values generated from the input action and compatibility
  alias registries;
- `notification-policy` for the boolean-or-inline-table notification union.

These are not separate player-facing configuration systems. The launcher uses
one search, category, validation, changed-state, and persistence experience,
then delegates value-specific behavior to the matching adapter.

## Authoritative producers

- Ordinary runtime defaults and descriptions come from
  `mods/src/defaultconfig.h`. The public surface comes from the reference TOML,
  while literal `get_config_or_default` reads in `Config::Load()` are scanned
  as a release-drift guard.
- Input defaults come from `ActionSpecs()`. Legacy and deprecated
  `[shortcuts]` paths come from `ShortcutConfigAliases()`.
- Notification names, legacy paths, sounds, and stability come from
  `notification_event_catalog()`. A canonical inline table replaces the whole
  event policy; it never partially inherits a deprecated value.
- Dynamic `sync.targets.*` fields are generated from `SyncOptions` and the
  runtime sync defaults.

The checked-in JSON is a build artifact, not another defaults catalog. A source
change that alters the generated result must update the artifact in the same
change.

## Release source and persistence boundary

The schema declares `source.id = "guffawaffle"`. It describes Guffawaffle
artifacts only. A NetniV installation must use a NetniV-published schema or an
explicit compatibility adapter; the launcher must not silently apply this
schema to a different release source.

Release-source selection belongs to launcher state, not the mod TOML. Switching
sources is a migration transaction with compatibility preview, confirmation,
backup, and rollback.

TOML remains the current runtime and interchange boundary. Launcher writes are
sparse and must preserve unknown keys and comments. A future richer
Guffawaffle-only profile store may compile to this model, but it must retain
deterministic TOML import/export while the C++ runtime consumes TOML and while
NetniV compatibility is supported.

## Sensitivity and provenance

`sensitivity` is mandatory so diagnostics and support bundles can redact
secrets and private endpoints/paths. `provenance.defaultSource` identifies the
runtime catalog that supplied the default; `provenance.runtimePath` gives the
corresponding runtime-vars location or wildcard template.

Runtime provenance can add the effective value source (`default`, canonical
TOML path, compatibility alias, or runtime override) without redefining any
schema metadata.
