# Windows Launcher UX Direction

Status: accepted product direction

Date: 2026-07-27

Applies to: `WL-002`, `WL-006`, `WL-008`, and `WL-010`

## Decision

The production launcher uses a modern, compact Windows application shell with
light and dark themes. LCARS is no longer a product requirement.

The home surface follows the focused-launcher and compact-utility concepts:

- show whether the player can act;
- show only the small number of states that affect that action;
- offer one contextual primary action;
- move configuration, product metadata, and diagnostics behind explicit
  navigation.

The settings surface is a separate, larger workspace inside the same
application. It may use denser navigation because configuration is an
intentional task. This distinction prevents the extensive TOML schema,
especially notifications and hotkeys, from turning the launcher home into a
developer dashboard.

![Directional launcher home and notification settings mockup](assets/launcher-hybrid-direction.png)

The image is a directional reference, not a pixel contract. Product behavior,
accessibility, native Windows conventions, and the generated configuration
schema take precedence over exact spacing, icons, colors, or copy in the
mockup.

## Complexity reference

The public [community configuration tool](https://modconfig.pages.dev/) was
reviewed as a complexity reference, not as a visual template. Its shipped
definitions currently describe roughly 159 controls, including about 90
shortcuts, 20 sync settings, 17 graphics settings, 14 support-level patch
toggles, and 13 interface settings.

That snapshot does not include the full notification system planned for this
fork and is not an authoritative launcher schema. It demonstrates why a flat
form, permanent output panel, or single row of top-level tabs will not scale.

## Why the WL-002 preview feels clunky

The WL-002 shell renders the launcher's internal discovery and health model
directly. That was useful while proving bounded discovery, path validation,
process safety, and composable health, but it exposes implementation concepts
that most players cannot act on:

- candidate counts and provenance;
- process-boundary explanations;
- per-user storage ownership;
- every health dimension at once;
- long diagnostic descriptions.

Those facts remain valuable to support and development. They belong in
structured logs and an explicit diagnostic view, not on the normal launcher
surface.

## Product principles

### Outcome first

The first screen answers:

1. Is the game installation usable?
2. Is the community mod usable and current?
3. What is the next safe action?

It does not explain the discovery algorithm unless the user asks for
diagnostics.

### Progressive disclosure

Common actions remain visible. Technical detail appears only when it changes
the user's decision or when the user opens Settings, About, or Diagnostics.

### Privacy by default

Normal UI never renders game, profile, launcher-storage, or candidate paths.
Paths remain available internally for validation and may be included in an
explicit diagnostic export only after the user opts in.

### Schema-driven configuration

The launcher does not maintain a second handwritten list of TOML settings.
Categories, controls, defaults, constraints, platform support, deprecations,
restart behavior, descriptions, and search terms come from the shared,
versioned configuration schema defined by `WL-005`.

### Sparse user intent

The editor writes explicit user choices rather than materializing every
runtime default. Removing an override is a first-class action.

## Application shell

### Adaptive window density

The launcher has two presentation modes:

- **Home:** compact, centered, and action-oriented.
- **Workspace:** wider and navigable for Settings, Diagnostics, and other
  deliberate management tasks.

Moving between them may resize or reflow the same window. It must not open
multiple competing launcher windows.

### Integrated window chrome

The application name appears once in an integrated draggable title area. The
Home does not repeat a native window title, product heading, and platform
subtitle.

The integrated chrome retains ordinary Windows behavior:

- accessible minimize, maximize/restore, and close controls;
- resize borders, title-area dragging, and double-click maximize;
- system commands and keyboard shortcuts;
- edge snapping and the Windows `Win+Z` Snap Layout shortcut.

Caption controls use centered vector geometry rather than font glyphs. The
application still exposes its full product title to Windows, assistive
technology, and the task switcher.

Returning `HTMAXBUTTON` from custom WPF chrome caused Windows to draw native
caption visuals over the launcher's control and is not used. Hover-triggered
Snap Layouts may be reconsidered only if they can be enabled without mixed
native/custom rendering.

### Application identity assets

The production launcher uses the existing community-mod artwork in
`assets/launcher.png` and `assets/launcher.icns`. The Windows asset pipeline
produces a multi-resolution `.ico` from that approved source and applies it
consistently to:

- the executable and taskbar;
- the window and task switcher;
- shortcuts and installer/update surfaces;
- About and release-facing launcher artwork where appropriate.

The launcher does not invent substitute release artwork. The final asset must
remain legible at small Windows icon sizes and in both light and dark shell
contexts.

### Theme

The theme preference is:

1. System default;
2. Light override;
3. Dark override.

Light and dark themes preserve the same hierarchy and semantics. Green is
reserved for healthy state, amber for warning, red for action-required or
failure state, and the primary action color remains distinct from health
colors.

Theme, motion, and scale preferences are launcher-owned state. They do not
modify the mod TOML.

## Home surface

The healthy home contains:

- application name;
- theme and settings access;
- stable product copy that does not repeat row-level health;
- a game-installation row;
- a community-mod row;
- one contextual primary action;
- quiet About access.

Example healthy rows:

```text
Game folder       [success icon]       Change
Community mod     [success icon] Current
```

The game row offers a quiet `Change` action when appropriate. It does not show
the selected path. A successful game-folder row does not repeat `Set` beside a
success icon; its accessible name still announces `Game folder set`.

The default Home copy is:

```text
Make STFC yours.
Install, update, and configure the community mod in one place.
```

Row-level state remains in its row. Product copy is replaced only when a
blocking or operation-wide condition materially changes the user's next safe
action.

The primary action changes with the resolved product state:

| State | Headline | Primary action |
|---|---|---|
| Game missing or selection invalid | Game folder needed | Set game folder |
| Game ready, mod missing | Ready to install | Install mod |
| Mod update available | Update available | Update mod |
| Game and mod healthy | Ready to play | Launch game |
| Repair required | Needs attention | Repair |
| Game running | Game is running | Bring game forward or disabled launch |
| Operation active | Working | Progress/cancel when safe |
| Offline but locally healthy | Ready offline | Launch game |

Detailed process safety, discovery provenance, artifact hashes, and internal
health dimensions are logged. The home shows them only when they change the
safe action.

## Status semantics

Success, warning, and failure use consistent vector icons with adjacent row
labels and explicit status text when the user must distinguish or act on the
state. Literal emoji are not the implementation contract because their
appearance varies by Windows font and assistive technology.

Every icon has:

- an adjacent row label plus visible status text when it adds information;
- a screen-reader name;
- meaning that does not depend on color;
- a deterministic action when user intervention is possible.

The game-client row uses semantic process states:

| State | Visual treatment | Visible text |
|---|---|---|
| Running | Filled green status light | `Running` |
| Not running | Hollow neutral status light | `Not running` |
| Checking | Blue progress indicator | `Checking…` |
| Unavailable | Amber warning icon | `Status unavailable` |

`Not running` is a normal inactive state and is never rendered as an error.
Process-state icons are indicators, not controls; play or launch glyphs are not
used because they imply a clickable launch action.
`Checking` and `Unavailable` appear only when the process service can
truthfully distinguish those states; the synchronous `WL-002` probe currently
resolves only `Running` and `Not running`.

The launcher uses an unprivileged Windows shell window-created signal to
identify a new `prime.exe` process and the tracked process's exit signal to
detect shutdown. It re-runs the authoritative process inspection only after a
transition and does not continuously poll or require WMI. A manual Refresh
action remains available if the operating-system event subscription cannot be
established.

## Settings workspace

Settings is not a modal and is not constrained to the compact home dimensions.
It uses:

- category navigation;
- a global search field;
- a scrollable content region;
- a persistent changed-state/save area;
- a route back to Home.

Initial categories are:

1. General
2. Interface
3. Graphics
4. Notifications
5. Hotkeys
6. Data Sync
7. Advanced
8. About

Advanced contains experimental, developer, and support-directed controls. Patch
installation toggles do not appear in common categories.

### Launcher preferences and launch profiles

Launcher-owned preferences are distinct from the schema-driven mod settings.
The future General area may include:

- start the launcher with Windows, explicitly opt-in and default off;
- automatically check for launcher and mod updates;
- separate consent and policy for downloading or installing an update;
- release channel, theme, reduced motion, and close/minimize behavior;
- behavior after launching STFC, such as remaining open, minimizing, or
  closing.

Labels must distinguish `Start the launcher with Windows` from starting STFC.
Likewise, checking for an update is not permission to install it.

Named launch profiles are a separate future product concept. A profile may
select a mod configuration, launch mode, and supported launch-time behavior,
but profiles must reuse the shared configuration schema rather than copy its
definitions. The active profile must be visible before launch, and switching
profiles must never silently materialize defaults, duplicate secrets, or
rewrite unrelated TOML.

Profiles and launcher preferences require dedicated PM work before
implementation. `WL-006` supplies the schema-driven configuration foundation,
while `WL-007` supplies the launch handoff on which profile selection can
operate.

### Setting rows

A normal setting row contains:

- friendly title;
- short consequence-oriented description;
- appropriate control;
- effective or explicit value state;
- changed-from-default state;
- reset/remove-override action;
- restart or next-launch indicator when required.

The canonical TOML key, provenance, deprecated aliases, and detailed validation
may appear in an expandable technical area or tooltip. They are searchable but
not primary labels.

### Search and filtering

Search covers friendly titles, descriptions, canonical keys, and deprecated
aliases. The workspace supports filters for:

- changed settings;
- enabled settings;
- settings needing attention;
- advanced settings.

The hotkey category also detects duplicate or conflicting bindings. Data Sync
uses a repeatable target editor rather than exposing flattened target keys.

### Save behavior

Edits are staged until the user saves. A persistent footer reports the number
of unsaved changes and offers `Discard` and `Save changes`.

Save follows the configuration contract:

1. preserve comments and unknown keys;
2. validate the staged document;
3. create a recoverable backup;
4. atomically replace the destination;
5. report when the game must restart or relaunch.

Unsupported syntax or values never trigger a destructive rewrite.

## Notification settings at scale

Notifications are a first-class settings category and are expected to exceed
the current community configurator's complexity.

### Information architecture

Events are grouped by meaningful domain, for example:

- Battle and incoming attacks
- Fleet movement and mining
- Repairs and docking
- Armada
- Events and tournaments
- Territory and takeover
- Economy and treasury
- Experimental or generic toasts

Meaningful canonical prefixes remain visible in names such as
`fleet_arrived_in_system`, `armada_created`, and `fleet_repair_complete`.
Ambiguous flattened labels such as `created` or `repair_complete` are not used.

### Event row

Each event is one compact, searchable row. The collapsed row shows:

- friendly event name;
- enabled/off state;
- concise delivery summary;
- changed or deprecated status when relevant.

Expanding the row exposes the complete event policy:

- system notification;
- audio notification;
- sound selection and preview when supported;
- runtime default/effective value;
- reset/remove override;
- migration or validation warning.

The UI does not render every delivery control for every event simultaneously.
Grouped rows, search, filters, and per-event expansion keep the category
scannable.

### Canonical value model

The editor represents the accepted canonical forms:

```toml
event_name = false
event_name = true
event_name = { system = true, audio = true, sound = "alarm" }
```

- `false` disables the event.
- `true` enables system delivery only.
- An inline table replaces the complete event policy.

An inline table never partially inherits deprecated values. Invalid canonical
values are shown as needing attention and resolve according to the runtime
default contract; the editor never silently revives a deprecated enabled
value.

Legacy notification sections remain readable during their compatibility
window. The UI identifies their provenance, explains the migration, and writes
canonical settings when the user accepts or edits the policy. Deprecated
fallback is visible in diagnostics and configuration provenance.

There is no visible global system or audio master switch. Per-event policy is
the user-facing source of intent.

## About and Diagnostics

About remains product-focused:

- launcher and mod versions;
- release channel;
- project and support links;
- licenses and acknowledgements;
- access to Diagnostics.

About is not a catch-all operational dashboard.

About is rendered through the reusable in-application dialog host rather than
a Windows message box. The host provides theme-aware presentation, arbitrary
content, accessible naming, Escape dismissal, focus transfer, and focus
restoration. Confirmations and other small modal interactions should reuse this
primitive rather than create one-off popups.

Diagnostics is an explicit drawer, page, or modal reachable from About and
Advanced settings. It provides:

- resolved health dimensions;
- discovery evidence without raw paths by default;
- relevant installed and available versions;
- configuration parse and migration state;
- recent operation and repair state;
- preview and `Copy diagnostics`;
- preview and export of the redacted support bundle;
- an opt-in `Include filesystem paths` control that defaults off.

Diagnostic facts are also written to structured launcher logs so support data
does not depend on the main UI remaining verbose. Diagnostics are never
uploaded automatically.

## Accessibility and Windows behavior

Accessibility is a design-system requirement, not a final validation pass.
Reusable interaction primitives carry keyboard, focus, target-size, contrast,
automation-name, and disabled-state behavior so individual screens cannot
silently omit them.

Ordinary actions such as About, Refresh Status, and dialog Close use one shared
utility-action button style rather than custom subclasses. The shared primitive
provides a minimum 44-pixel target, a dual-contrast focus treatment, consistent
padding and typography, and a control boundary with at least 3:1 contrast.
Custom controls are reserved for reusable behavior, such as the in-application
dialog host.

The Windows typography stack prefers `Segoe UI Variable Text` with `Segoe UI`
fallback and uses medium body weight to remain readable across display scales
without presenting as bold.

The current palette audit meets WCAG 2.2 AA for normal text and meaningful
control boundaries:

| Token use | Dark | Light | Requirement |
|---|---:|---:|---:|
| Primary text on window | 17.86:1 | 14.85:1 | 4.5:1 |
| Secondary text on surface | 8.47:1 | 5.37:1 | 4.5:1 |
| Text on primary action | 5.03:1 | 5.61:1 | 4.5:1 |
| Success text on surface | 8.89:1 | 5.42:1 | 4.5:1 |
| Warning text on surface | 10.19:1 | 4.87:1 | 4.5:1 |
| Error text on surface | 6.41:1 | 5.26:1 | 4.5:1 |
| Utility control boundary on surface | 3.57:1 | 3.65:1 | 3:1 |

The implementation must support:

- keyboard access to every action and setting;
- visible focus and logical focus order;
- accessible names for icons, controls, and status changes;
- text scaling and 100%, 150%, and 200% display scaling;
- sufficient contrast in both themes;
- reduced motion;
- no state communicated only by color;
- virtualized large setting lists without breaking screen-reader navigation;
- validation summaries linked to the affected setting.

## Explicit non-goals

- Reproducing the community web configurator's visual design.
- Rendering raw TOML beside the settings editor by default.
- Showing candidate provenance or filesystem paths on Home.
- Keeping LCARS as a visual requirement.
- Treating the mockup as generated production assets.
- Hand-maintaining notification or other setting definitions in WPF.
- Autosaving partially valid configuration.

## Delivery implications

- `WL-002` keeps its discovery and health model but presents only actionable
  resolved state on Home.
- `WL-005` must model notification event families, complete inline policies,
  defaults, deprecations, provenance, sounds, validation, and restart behavior.
- `WL-006` implements the adaptive Settings workspace and staged sparse save.
- `WL-008` owns the redacted diagnostic surface and support export.
- `WL-010` validates both themes, adaptive window density, keyboard navigation,
  screen readers, reduced motion, and high DPI.

Implementation may refine copy and layout, but any change that weakens privacy,
schema authority, sparse writes, progressive disclosure, or accessibility
requires an explicit product decision.
