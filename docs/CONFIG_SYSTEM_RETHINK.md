# Config System Rethink

Branch note for `guffa/notification-config-rethink`.

This expands the notification-specific rethink into a full config architecture
proposal aligned with upstream issue `netniV/stfc-mod#28`.

## Upstream Problem Statement

Issue #28 names three root problems:

- Config is owned by `mods` instead of a standalone shared module.
- Values are read by hand, with defaults and usage scattered through load code.
- Future launcher/UIs would need to duplicate config knowledge, which is fragile.

The notification work made the pain visible because the current file is already
large, and adding delivery channels (`system`, `audio`, in-game banner policy)
turns flat TOML names into a wall of `notifications_*` keys.

## Current State

The patch currently has one real parser: `mods/src/config.cc`.

- It parses `community_patch_settings.toml` with toml++.
- It applies defaults from `mods/src/defaultconfig.h`, plus a few local defaults
  in `config.cc`.
- It writes a fully resolved `community_patch_runtime.vars` snapshot.
- It populates the process-wide `Config::Get()` singleton.
- It handles legacy migration for sync and notification allowlist keys.

Other consumers are thinner:

- The macOS launcher creates/opens `community_patch_settings.toml`, but does not
  parse it.
- AX scripts inspect `community_patch_settings.toml` and
  `community_patch_runtime.vars`, but do not own schema validation.
- The Windows proxy DLL delegates config to the shared `mods` library.

That means the first overhaul can be internal to the repo without needing an
immediate multi-language rewrite. The important design constraint is that it
should create a stable schema/API that UIs can consume later.

## Goals

- Make `community_patch_settings.toml` sparse: user intent only, not every known
  default.
- Treat hand-edited TOML as a compatibility and power-user path, not the desired
  long-term configuration UX.
- Make defaults, docs, type validation, aliases, and UI metadata live in one
  schema catalog.
- Preserve existing user configs through compatibility aliases and clear
  deprecation warnings.
- Keep `community_patch_runtime.vars` or a successor as the resolved truth users
  can inspect.
- Enable AX and future UIs to ask the config system what keys exist instead of
  scraping C++ load code.
- Avoid exposing the current `Config` struct as ABI. It has layout-sensitive
  history, and a C API must not depend on its field order.

## Non-Goals For The First Slice

- Do not remove `community_patch_settings.toml`.
- Do not force users to split their config into multiple files.
- Do not rewrite every `Config::Get().field` callsite at once.
- Do not introduce remote config loading or network-backed settings.
- Do not build a UI before the schema is stable enough to support one.

## Does This Fix Issue #28?

The schema core fixes the internal problem named in issue #28: scattered config
knowledge, hand-written load code, duplicated defaults, and no shared contract for
future UIs.

It does not, by itself, fix the user-experience problem. A sparse TOML overlay is
better than a giant generated TOML file, but it is still a hand-edited config
file. The intended end state should be:

- schema-driven config core as the source of truth;
- optional launcher/sidecar UI as the normal editing experience;
- TOML import/export for compatibility, debugging, and power users;
- runtime vars or diagnostics output for inspection.

In other words: schema first, UI second, TOML as a bridge.

For the live-edit path where a player changes settings while STFC is already
running, see `docs/RUNTIME_CONFIG_BRIDGE.md`. That path should apply typed
schema patches over local IPC rather than re-reading TOML at runtime.

## App-First Direction

The current sidecar repo already points at the right shape: a platform-neutral
core that can be rendered by a local web UI, desktop app, CLI, or Overwolf host.
That should become the configuration home rather than growing an injected-mod UI.

The optional user-facing package can be an installable "STFC Community Mod
Companion" that owns:

- mod install/update/uninstall;
- game path detection;
- launch/start status;
- settings UI backed by the schema catalog;
- sidecar event server and local viewer;
- diagnostics bundle export;
- optional integrations after explicit user action.

The injected mod should stay small:

- load resolved config;
- accept only narrow schema-validated runtime patches;
- apply hooks;
- emit local events;
- never host a rich UI;
- never own complex updater or integration policy unless no external launcher is
  present.

This gives existing users zero-friction continuity: `community_patch_settings.toml`
continues to work. Users who want a managed experience install the companion app.

## PR #145 Implications

PR `netniV/stfc-mod#145` demonstrates that update/install ergonomics are already
being explored upstream. It adds a prerelease workflow, an update channel, macOS
launcher update behavior, and a Windows prelaunch DLL updater concept.

That PR supports the broader direction, but it also highlights a boundary risk:
the more update/config logic lives inside `version.dll`, the more the injected mod
becomes an installer, network client, updater, and policy engine.

Preferred split:

- Companion app handles updates, installer UX, channel selection, and settings.
- Mod keeps a minimal fallback update/channel config only if the companion app is
  absent.
- Shared schema/API prevents launcher, sidecar, and mod from each parsing config
  differently.

That still allows a PR #145-style prerelease release pipeline. The difference is
where the intelligence lives.

## Proposed File Model

Keep one canonical user file for now:

- `community_patch_settings.toml`: sparse user overrides.
- `community_patch_runtime.vars`: resolved values after defaults, aliases, and
  validation.
- Future optional generated file: `community_patch_reference.toml`, a full
  documented reference produced from the schema but not parsed as user input.

This avoids making `notifications.toml`, `sync.toml`, etc. special. If the repo
later gains an include/import mechanism, it should be generic and schema-aware:

```toml
[config]
includes = ["notifications.toml", "sync-targets.toml"]
```

Until that exists, separate files add merge-order and precedence problems without
solving the shared-schema problem from issue #28.

## Overlay Semantics

The user file should be an overlay on top of schema defaults.

Missing key means: use the schema default. It should not mean: write the default
back into the user's file.

Example sparse config:

```toml
[graphics]
ui_scale = 1.0
zoom = 3000

[control]
allow_key_fallthrough = true

[notifications.audio]
enabled = true

[notifications.audio.fleet]
arrived_in_system = true

[sidecar.logging]
jsonl_recent_logs = 300
```

The runtime snapshot can still be exhaustive, because its job is inspection:

```toml
[notifications.audio]
enabled = true
default_sound = "system:notification"
cooldown_ms = 1500

[notifications.audio.fleet]
arrived_in_system = true
```

This gets the main config out of the business of being both user input and
reference documentation.

## Schema Catalog

Introduce a config schema catalog as data, not handwritten read code.

Each entry should describe:

- Canonical path, for example `notifications.audio.fleet.arrived_in_system`.
- Type: bool, integer, float, string, enum, string array, table, secret.
- Default value.
- Human-readable label and description.
- Category/group for generated docs and UI layout.
- Platform support: all, Windows, macOS, debug-only, `_MODDBG` only.
- Stability: stable, experimental, deprecated, internal.
- Validation rules: min/max, enum choices, path restrictions, string pattern.
- Deprecated aliases, for example `notifications_fleet_arrived_in_system`.
- Runtime sensitivity: whether values should be redacted in logs/snapshots.

Sketch:

```cpp
enum class ConfigValueType {
  Bool,
  Integer,
  Float,
  String,
  StringArray,
  Enum,
};

struct ConfigSchemaEntry {
  std::string_view path;
  ConfigValueType type;
  ConfigDefaultValue default_value;
  std::string_view label;
  std::string_view description;
  std::span<const std::string_view> aliases;
  ConfigFlags flags;
  ConfigValidation validation;
};
```

The schema catalog becomes the single source for:

- Parsing.
- Runtime snapshot generation.
- Example/reference generation.
- Deprecation warnings.
- AX `config-validate` and `config-diff`.
- Future launcher/UI controls.

## C API Shape

The C API should expose an opaque context and typed accessors. Do not expose
`Config` or C++ containers.

Sketch:

```c
typedef struct stfc_config_context stfc_config_context;

typedef enum stfc_config_value_type {
  STFC_CONFIG_BOOL,
  STFC_CONFIG_INT,
  STFC_CONFIG_FLOAT,
  STFC_CONFIG_STRING,
  STFC_CONFIG_STRING_ARRAY,
} stfc_config_value_type;

stfc_config_context* stfc_config_create(void);
void stfc_config_destroy(stfc_config_context* ctx);

int stfc_config_load(stfc_config_context* ctx, const char* path);
int stfc_config_get_bool(stfc_config_context* ctx, const char* path, bool* out_value);
int stfc_config_get_int(stfc_config_context* ctx, const char* path, int64_t* out_value);
int stfc_config_get_double(stfc_config_context* ctx, const char* path, double* out_value);
int stfc_config_get_string(stfc_config_context* ctx, const char* path, char* buffer, size_t buffer_len);

size_t stfc_config_schema_count(void);
int stfc_config_schema_entry(size_t index, stfc_config_schema_entry* out_entry);
size_t stfc_config_diagnostic_count(stfc_config_context* ctx);
int stfc_config_diagnostic(stfc_config_context* ctx, size_t index, stfc_config_diagnostic* out_diag);
```

Use versioned structs with `size` fields for ABI evolution. The C++ patch can
wrap this with ergonomic accessors, while Swift/Windows UI code can consume the
same schema without parsing `config.cc`.

## Patch Integration Strategy

Do not rewrite every patch callsite first.

Instead:

1. Add the schema-driven loader behind the current `Config::Load()` path.
2. Populate the existing `Config` facade from the resolved store.
3. Keep file-scope accessors such as `AllowKeyFallthrough()` intact.
4. Add new typed accessors only for new or migrated areas.
5. Gradually move callsites from public mutable fields toward named accessors.

This preserves behavior while making config parsing and validation data-driven.

## TOML Shape Principles

Use nested tables where they reduce repeated prefixes and group by user intent.

Good:

```toml
[notifications.system]
enabled = false

[notifications.system.fleet]
arrived_in_system = true
node_depleted = true

[notifications.audio]
enabled = false
default_sound = "system:notification"

[notifications.audio.fleet]
arrived_in_system = true

[notifications.banners]
hide = ["standard", "faction_warning"]
```

Avoid repeating the section name in every key:

```toml
[notifications]
notifications_audio_fleet_arrived_in_system = true
```

Use arrays for allow/deny lists instead of comma-delimited strings:

```toml
[ui.banners]
hidden = ["standard", "faction_warning"]
```

For non-sidecar external sync targets, the current nested table pattern is good and should be kept:

```toml
[sync.targets.majel]
url = "https://majel.example.test/api/ingest/events"
token = "..."
battlelogs_realtime = true
battlelogs = false
```

Local sidecar delivery should move to its own root namespace instead of staying under `[sync.targets.*]`:

```toml
[sidecar.sync]
enabled = true
url = "http://127.0.0.1:43127/api/sidecar/ingest"
token = "..."
battlelogs_realtime = true
```

## Defaults Policy

Current defaults are mixed: some notifications default true inside
`DefaultConfig`, while the master switch defaults false. That is workable but
confusing in a sparse overlay world.

Recommended policy:

- Master delivery channels default off.
- Experimental/risky features default off.
- Visual/gameplay quality-of-life defaults can remain as they are, but the
  schema should label them clearly.
- Per-event notification toggles default off unless there is a strong historical
  compatibility reason.
- The migration layer can preserve old effective behavior for legacy flat keys.

In other words, a missing notification event key should mean "no notification for
that event," not "maybe enabled because the old example file listed it true."

## Validation And Diagnostics

Schema-driven validation should produce diagnostics rather than silently ignoring
mistakes.

Validation should catch:

- Unknown sections and keys.
- Deprecated aliases.
- Invalid types.
- Out-of-range numeric values.
- Unsupported keys on the current platform.
- Invalid shortcut tokens.
- Unsafe paths for local assets such as future notification sounds.
- Secrets accidentally placed in loggable fields.

AX can then implement the already-noted commands:

- `config-validate`: parse and report diagnostics.
- `config-diff`: compare user overlay with resolved runtime values.

The runtime snapshot should include diagnostic comments or a companion JSON report
so users can distinguish "defaulted" from "explicitly set."

## Secrets And Safety

Treat sync tokens and future credentials as secret values in the schema.

- Redact them in logs.
- Redact or mark them in runtime snapshots.
- Never copy them into generated reference files.
- Avoid remote includes or auto-downloaded config.

For local file settings, such as future audio files:

- Prefer relative paths under an approved mod assets directory.
- Reject URLs.
- Cap file size where relevant.
- Prefer simple formats with OS support before adding decoder libraries.

## Migration Plan

### Phase 0: Inventory And Design

- Keep this design doc on the rethink branch.
- Inventory every current key, default, alias, and consumer.
- Decide canonical paths for the new schema.

### Phase 1: Schema Catalog Without Behavior Change

- Add a `config_schema` module.
- Encode existing flat keys as canonical schema paths first.
- Generate the same resolved `Config` values currently produced by `Config::Load()`.
- Add tests for parse/default/alias behavior.

### Phase 2: Runtime Snapshot And Validation From Schema

- Generate `community_patch_runtime.vars` from the resolved store.
- Add unknown-key and invalid-type diagnostics.
- Add AX `config-validate` and `config-diff` support using schema output.

### Phase 3: Nested Canonical Paths

- Introduce nested canonical paths for selected domains, starting with
  notifications.
- Keep flat keys as aliases.
- If both old and new keys are present, new canonical paths win and a warning is
  emitted.

### Phase 4: Generated Reference Config

- Generate `community_patch_reference.toml` or a docs page from schema metadata.
- Stop treating `example_community_patch_settings.toml` as the exhaustive source
  of all settings.
- Replace it with a smaller starter config plus generated reference docs.

### Phase 5: Shared C API

- Expose schema, resolved values, and diagnostics through a stable C API.
- Keep `Config::Get()` as a patch-internal compatibility facade.
- Let the macOS launcher and future Windows UI read schema metadata instead of
  duplicating key lists.

### Phase 6: Optional Includes

- Add generic include support only after schema validation and precedence rules
  are mature.
- Support files such as `notifications.toml` as user convenience, not as a
  special-case notification system.

## First Real Code Slice

The first implementation should avoid a giant branch.

Status: the initial pilot is implemented for the notification bool keys. It adds
canonical nested paths, legacy flat-key aliases, conflict diagnostics, and nested
runtime snapshot writing while continuing to populate the existing `Config`
facade.

Recommended first slice:

- Add a schema catalog for four to eight notification keys.
- Read both old flat and new nested notification paths.
- Keep writing current `Config` fields.
- Emit diagnostics for alias usage and conflicts.
- Generate resolved runtime vars for those keys from the schema path.
- Add pure tests for old key only, new key only, conflict, invalid type, and
  default behavior.

Good pilot keys:

- `notifications.system.enabled`
- `notifications.system.fleet.arrived_in_system`
- `notifications.audio.enabled`
- `notifications.audio.fleet.arrived_in_system`
- `notifications.banners.hide`

That pilot exercises booleans, nested TOML, aliases, one array/list field, and
the three notification policy domains without touching sync credentials or all
patch toggles at once.

## Open Questions

- Should runtime vars remain TOML, or should the schema also write a JSON report
  for UIs and AX?
- Should generated reference config be committed, generated during build, or
  produced on demand by an AX command?
- How long should flat-key aliases remain supported?
- Should debug-only patch toggles live in the same public schema or in a separate
  developer schema tier?
- How much should the schema know about UI layout versus only types/docs/defaults?