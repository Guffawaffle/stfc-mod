# Native settings adapter

The Windows x64 adapter presents feature-owned settings in the game's native
widgets. It owns native contexts, hook installation and temporary presentation;
feature adapters continue to own live values and persistence.

## File ownership

| File under `mods/src/` | Responsibility |
| --- | --- |
| `patches/parts/mod_settings.cc` | Entry point: install core value/session hooks, then optional navigation. A navigation failure leaves native confirmation controls available. |
| `settings/native/interop.*` | Managed invocation, temporary roots, weak-handle helpers, signature and native-extent checks, bounded list access. No feature state or hook installation. |
| `settings/native/value_widgets.*` | Boolean, choice and slider metadata; live view records; readback/render/write guards; native confirmation placement and session invalidation. Installs its value and session hooks. |
| `settings/native/value_widget_record.h` | Private lifetime record shared only with value-widget styling. Navigation queries whether values are busy without borrowing these records. |
| `settings/native/page_navigation.*` | Immutable page plan, fresh native page construction, navigation/Back, conditional sections, visit-local folding, headings and their hooks. Coordinates installation of optional widget families in the existing order. |
| `settings/native/action_widgets.*` | Command callback identity, indexed rows, visibility, invocation and release guards. Installs command-widget hooks. Shortcut drafts remain in `shortcut_settings.cc`. |
| `settings/native/row_style.*` | Scoped text, tint, arrow and selection-sprite overrides; restore prior native appearance on release/rebind. No setting writes or hook installation. |

These are internal adapter modules, not another settings framework. Native
metadata accessors support cross-module signature and hook-overlap checks; they
retain lazy resolution. Page plans are exposed read-only, and widget records and
active flags stay with their owners. Each detour remains installed by exactly
one module. The build continues to discover these sources through XMake's
existing `src/**.cc` rule.

## Contracts retained by the split

- UI-thread ownership and weak-view lifetime rules remain unchanged. Views never
  keep old settings pages or account state alive.
- A user request still uses the displayed snapshot, verifies the current owner,
  applies through that owner, then reads back. Rendering never authorizes writes.
- Internal page-list rebinding preserves command drafts. Row release retains the
  existing cancellation behavior; moving that responsibility to page departure is
  a separate lifecycle improvement, not part of this refactor.
- Value refreshes still defer list rebinding while a value widget is busy. There
  is no new update callback, polling, save worker or persistence path.
- Startup metadata/extent checks, overlap checks, activation gates and install
  order are preserved. Native support remains Windows x64; other platforms retain
  the existing no-op entry point.
- Feature wording stays with the feature. A slider may provide a short
  `disabledReason`; Fleet Labels supplies `Select Threshold`. Other sliders do
  not inherit that instruction. Unknown and failed-state wording is unchanged.

The C++ entry point and member are `InstallNativeSettings` and
`Config::installNativeSettings`. The existing debug patch key
`ModConfirmationSettings` deliberately remains unchanged so existing patch
configuration continues to work. No setting IDs, TOML keys, defaults or placement
change in this refactor.

## Validation

Run the settings fixtures and the normal Windows mod build. Review moved hooks
against the previous implementation, including original-call behavior and the
order in which widget families become active. Native smoke checks should cover
confirmation rows, folding and Back, choices and slider labels, and opening and
cancelling a shortcut draft. Builds and pure fixtures do not establish native
pooling behavior or macOS hook compatibility.
