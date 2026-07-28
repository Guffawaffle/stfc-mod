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

The Settings workspace now uses the accepted persistent left navigation and
contextual save surface. Search is global and opens from a toolbar control
instead of permanently consuming a content row. Section navigation is schema-derived
through one category resolver, the compact rail contains only navigation, and
a compact toolbar aligns Back, the active section title, and Search directly
beneath the window chrome. The back arrow returns to Home without consuming a
labeled navigation row in the rail. Release-source identity appears in General
and About rather than forcing the rail to accommodate metadata. Search-open
state is a launcher UI preference stored outside both mod TOML formats; search
text remains session-only.

The first genuinely editable adapter covers the 82 directly editable scalar
booleans. A toggle changes only an in-memory editing session. A bottom action
bar appears only while changes are pending, reports their exact count, and owns
Discard and Save Changes with 44-DIP action targets. Removing an override is
also staged and restores the runtime default without materializing it into
TOML. Theme-aware tooltips explicitly own their foreground, background, and
border so raw-key and help hover content retains contrast in Light and Dark
themes. The settings scrollbar uses the same dynamic palette, and rows support
mouse drag-to-scroll from non-interactive content without taking input away
from toggles, buttons, editors, or the scrollbar itself. Faster flicks carry
bounded momentum after release and decay without overscroll; new input cancels
the motion, and Windows' reduced-motion preference disables inertia.

Save builds the complete staged document in memory and writes it as one atomic
replacement against the session's original contents. It creates a sibling
backup and reports a conflict instead of overwriting when the selected file or
its contents changed after the session began.

Numbers, strings, enums, keybindings, and notification policies remain
read-only and identify the dedicated adapter they require. Notification rows
already parse canonical `false`, `true`, and inline-table policies into compact
On/Off and delivery summaries. Invalid canonical policy values visibly fall
back to the event default, matching the runtime contract, instead of pretending
the policy is unbound. They are not routed through an unsafe generic text
editor. NetniV schema selection is not yet packaged; the current session uses
the Guffawaffle catalog while source-preserving tests prove that legacy NetniV
notification families survive unrelated edits.

## Accepted destination

The directional Settings design in
[UX_DIRECTION.md](UX_DIRECTION.md) remains the accepted product vision. The
remaining generic value rows are integration scaffolding, not a replacement
information architecture and not a pixel-polish target.

The next UI weave converges on:

- compact back navigation plus persistent major setting families;
- category-specific, compact editors instead of one generic card shape;
- grouped notification rows with event state and delivery policy visible at a
  glance;
- dedicated Hotkeys and Data Sync experiences;
- a contextual bottom action bar that reports unsaved changes and owns Discard
  and Save Changes without permanently consuming content space.

Schema-driven generation remains the implementation rule beneath that
category-specific presentation. The launcher must not turn the accepted design
back into a handwritten second configuration catalog.

## Next weave

1. Resolve deprecated aliases and expose canonical/default/alias provenance in
   the editing session and future diagnostic export.
2. Instantiate dynamic target templates with validated concrete target names;
   never pass a wildcard schema path to the TOML mutation API.
3. Add typed number, string, and enum scalar controls with constraints from the
   generated schema.
4. Add dedicated keybinding and complete notification-policy adapters.
5. Add restart/apply summaries and a recoverable conflict-reload flow.
6. Expand the curated real-world Guffawaffle and NetniV round-trip fixtures as
   new syntax families enter the editor.

The editor is not accepted as complete until unknown keys and comments survive
the manual save round-trip smoke on a disposable configuration copy and
invalid or unsupported input is proven never to receive a destructive rewrite.
