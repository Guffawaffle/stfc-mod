# Windows Launcher Configuration Editor

Status: WL-006 vertical slice in progress

## Contract

The launcher presents one Settings experience while delegating value behavior
to scalar, keybinding, and notification-policy adapters. The generated schema
is the catalog authority; the mod runtime remains the TOML parser and default
authority.

The selected release source chooses the matching schema and capabilities.
Guffawaffle is the only packaged source in this slice. NetniV support retains
TOML as a hard compatibility boundary and must not reuse Guffawaffle-only
capabilities without matching source metadata.

## Implemented foundation

- The packaged launcher embeds and fail-closed loads schema version `1.0.0`.
- The catalog retains all 332 settings while exposing only player-facing
  directly editable settings to the normal workspace. Dynamic
  `sync.targets.*.*` templates remain machine-readable but are withheld until
  a concrete target-name context exists.
- Search covers title, description, category, and canonical path without
  rendering raw paths in the normal row.
- Category filtering and the large settings list are keyboard accessible and
  virtualized.
- The source-preserving TOML engine updates or removes one canonical
  assignment without reserializing surrounding content.
- Unknown keys, comments, ordering, whitespace, BOM, and line endings survive
  supported edits.
- Duplicate targets, malformed statements, array tables, unsupported target
  syntax, invalid UTF-8, and unsafe multiline target edits fail closed.
- The atomic store writes and flushes a sibling temporary file, rechecks the
  transformed document against the conservative supported grammar, refreshes
  a backup, serializes same-process writes per path, and performs a
  content-hash recheck immediately before replacement. Injected failures and
  concurrent edits detected by that optimistic check leave the destination
  unchanged by the launcher.

## Current UI boundary

The first Settings surface is deliberately read-only. It proves that the real
schema can drive a compact, searchable, accessible workspace and that an
existing active TOML can be opened explicitly. Remove override remains visibly
disabled until staged change tracking and Save/Discard are connected.

No generic row writes directly to disk. This prevents the first visual shell
from accidentally introducing immediate-save behavior that conflicts with the
accepted staged-edit contract.

## Next weave

1. Load the active TOML into a staged configuration session and resolve
   canonical overrides, aliases, and provenance.
2. Instantiate dynamic target templates with validated concrete target names;
   never pass a wildcard schema path to the TOML mutation API.
3. Add scalar controls with schema validation.
4. Add dedicated keybinding and complete notification-policy adapters.
5. Implement persistent unsaved-change state, validation summary, Discard,
   backup, and atomic Save.
6. Exercise golden round trips against real-world Guffawaffle and NetniV
   configuration fixtures.

The editor is not accepted as complete until unknown keys and comments survive
the manual round-trip smoke and invalid or unsupported input is proven never
to receive a destructive rewrite.
