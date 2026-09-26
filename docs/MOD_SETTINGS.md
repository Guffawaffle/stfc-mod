# Mod settings: current architecture and behavior

This is the current contract for the expanded Windows x64 and macOS settings UI. The
[foundation notes](MOD_SETTINGS_FOUNDATION.md) describe the first FC-only slice;
their prototype counts and proposed budgets are historical, not current limits.

## Ownership

Feature adapters own live values, availability, defaults and persistence. A
`ValueSetting<T>` owns snapshot validation, guarded application and readback.
`PageCatalog` owns placement and presentation callbacks, without Unity objects
or a second copy of configuration. Registration freezes before native creation.
Each game settings context gets a fresh tree from that immutable plan.

The [native adapter map](MOD_SETTINGS_NATIVE_ADAPTER.md) identifies the owner of
each hook and its managed metadata. Interop, value widgets, action widgets,
navigation and styling are separate concerns. Existing XMake source discovery
builds them. No additional copies of shared game-method detours are installed.
The historical `ModConfirmationSettings` debug patch key remains compatible;
its C++ name is `installNativeSettings`.

## Placement and summaries

Player tasks determine labels and navigation; TOML sections remain the storage
reference. Moving a page does not rename stored keys or change defaults.

| Mod Settings page | Contents | Storage reference |
| --- | --- | --- |
| Camera | Keyboard zoom speed, pan glide | `[graphics]` |
| Fleet Labels | Collapsible Player and Non-player profiles | `[graphics]` |
| Map & Travel | Instant warp mode, shared with its shortcut | `[ui]` |
| Previews & Cargo | Ship selection indicators, preview shortcuts and automatic cargo previews | `[ui]` |
| Shortcuts | Actions grouped by effect | `[shortcuts]` |

Empty groups are omitted. Shortcut categories and actions are alphabetized;
Diagnostics stays last. Registered shortcuts without presentation overrides get
a generated label in Uncategorized, immediately above Diagnostics when populated.
Discovery runs once at startup and retains existing availability gates; it does
not discover arbitrary TOML keys or infer numeric controls. FC and Forbidden Tech
remain in the game's native confirmation page. Fleet headings start collapsed and summarize their current
mode, with a two-decimal threshold where relevant. Shortcut action rows show the
first binding and an additional-binding count. The warp page row shows its mode.
Summaries refresh on binding and existing setting/presentation notifications.

## Search

The Mod Settings landing page has a text field that filters settings and
shortcuts as you type. Search matches labels, category paths and the actual
TOML names, with case-insensitive matching and interchangeable spaces,
underscores and dots. Exact names/keys appear first. Results show their location
and TOML key; Open visits the existing settings page and expands the matching
collapsible section. Back returns directly to the preserved search only when
the visit began from a result. Normal browsing keeps the native parent path.
Clear restores the category list. Search text is session-local, not saved.

Search covers the registered Mod Settings controls, not every TOML option.
Dependent controls remain discoverable while their parent option is off; results
identify that they are currently hidden. Search does not enable them or change
any configuration. Shortcut and audio editor commands produce one result for
the setting rather than separate Change/Remove/Preview entries. Queries are
limited to 128 characters and at most 32 results are displayed at once.

The native adapter uses TMP_InputField for editing and the existing page and
screen-update hooks. Input focus suppresses game shortcuts; result navigation
runs after the row's click callback finishes. The input lives outside the pooled
result rows so filtering preserves its caret and focus. Presentation IDs that
differ from saved names are explicitly mapped by `SearchTomlKey`; update that
mapping when adding such a control. `tests/run-settings.ps1` and
`tests/run-settings.sh` include the pure search fixture.

## Honest state and persistence

Readback proves the live value, not file or cloud durability. Unknown state never
looks OFF. Failed application preserves authoritative readback and never issues
an automatic reverse write. Native indicators are suppressed when unknown.

A finite loaded value outside a slider's UI range shows `Out of range; edit TOML`.
A non-finite value shows `Invalid value; edit TOML`. Neither is clamped, saved or
fixed by reopening. TOML is loaded at startup: a manual correction takes effect
after restarting. Ordinary unavailable readers retain `Reopen to retry`.
Disabled sliders receive a feature-owned reason; only Fleet Labels says
`Select Threshold`. Keep suffixes short: the native row truncates long labels.

The existing single writer serializes TOML edits and coalesces pending changes
per key. It preserves unrelated source and detects conflicting external edits.
Runtime failure/conflict leaves the live edit active. A failure-only notice at
the top of Mod Settings pages says `Active this session; couldn't save. See mod log.`
It concerns mod TOML saves, not the native FC cloud preference. Detailed key and
failure information stays in the log. No success notices or modal dialogs appear.

Failure state belongs to the writer's existing per-key records. A successful
save of one key cannot hide another key's failure. A later successful save of
the failed key clears that failure. Rejected submissions outside the writer
leave a conservative session warning, because they have no tracked completion.
The existing runtime callback observes aggregate status without taking the
writer lock; native UI work happens only when it changes, on the game thread.
Opening settings never retries or writes anything. F10's 500 ms best effort force
close and the ordinary quit/drain path are unchanged.

## Editor and view lifetime

Shortcut changes, additions, removals and defaults are drafts until Apply.
Restore reads the canonical default definition already registered by config;
`NONE` means no bindings. Stale drafts cannot replace a newer action list.
Overlap warnings allow keeping both; Next cycles through every affected action.
Force close and the game's native shortcut variants explain their behavior.

The page owns cancellation through `Page::leave`. Actual navigation, controller
destruction and session invalidation end the visit. Releasing or recycling an
individual command row does not. Conditional filtering and section folding keep
the page and draft. Recording cancels on Escape or focus loss and retains input
ownership until a **focused** sample observes all keys released. There is no
idle recording scan or global keyboard hook.

Native widgets keep weak ownership records and restore text, tint, sprites,
button visibility and interactability before reuse. Callback identity and the
currently bound context gate commands. Value rows defer list rebinding until
their request/readback scope finishes. Headings and command rows do not persist
presentation state. Dynamic command rows use the existing 128-child sanity bound;
the editor presents up to 60 bindings (Change and Remove plus fixed rows).
Longer player-authored lists remain live and stored in full.

There is no new polling hook, save worker or global localization hook.
Platform guards, managed signature checks and hook ownership checks remain part
of installation. Windows and macOS use the same settings adapter, while
layout-aware capture is Windows-only and macOS uses physical keys.

## Measurement and validation

In `_MODDBG` builds, set `STFC_MOD_SETTINGS_TIMING=1` for the launched process to
measure existing tree construction, page filtering/binding and action refresh
boundaries. Counts, mean and maximum elapsed milliseconds are aggregated until
page departure, then logged as `[SettingsTiming]`. There are no timing logs during
slider dragging, no additional hook or update callback, and no setting values or
bindings in the output. Timers compile out of ordinary release builds. Timings
include nested native work and are not additive across operations or whole-frame
measurements. A small sample cannot establish p95 or a universal frame budget.

Measure real opening/refreshing before adding caches. Repeated shortcut binding
copies currently remain straightforward authoritative reads; optimize only if
measurements show a material cost. No snapshot cache has been introduced here.

Run `tests/run-settings.ps1`, `tests/run-config-save.ps1` and the Windows build.
Pure fixtures cover readback/range preservation, stale/default drafts, duplicate
bindings, focused key draining, writer failures and shutdown. Native checks must
separately cover scrolling, Back, conditional rows, held-key focus transitions,
failure notice layout and pooled stock-row restoration on the identified build.
Record observations in [the delivery checklist](MOD_SETTINGS_POLISH.md).
Windows fixtures/builds do not establish macOS native hook compatibility.

Future work includes a real client restart command and additional settings with
proven live consumers. The existing cache-clear/reload shortcut is not a regular
restart. Neither generic TOML editing nor exposing every config key is implied.
