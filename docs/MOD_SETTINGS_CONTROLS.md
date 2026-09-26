# Mod settings controls

See [current settings architecture](MOD_SETTINGS.md) for the common behavior and
navigation contract. This document retains feature and native-extent details.

Build real controls on the navigation foundation in small slices. Register only
working controls; omit empty groups. Stable setting keys and storage owners stay
independent of labels and placement.

## Current layout

| Location | Control | Existing owner |
| --- | --- | --- |
| Mod Settings > Map & Travel | Instant warp mode: Normal (ask), Warp, Jump | `ui.auto_confirm_instant_warp` and the Alt+I action |
| Mod Settings > Previews & Cargo > Ship selection | Show equipped FT/CT in the Swap Ship bar, with optional backgrounds | `ui.show_ship_tech_indicators`, `ui.show_ship_tech_indicator_backgrounds` |
| Mod Settings > Previews & Cargo > Other | Allow Locate / Recall while a preview is open; configure instant cargo and cargo auto-open | Inverse of `ui.disable_preview_locate`, `ui.disable_preview_recall`; existing cargo flags |
| Mod Settings > Previews & Cargo > Target types | Player / Station / Hostile / Armada cargo preferences | Existing `ui.show_*_cargo` flags |
| Mod Settings > Fleet Labels > Other | Show ship hotkey badges | `graphics.ship_hotkey_badges` |
| Mod Settings > Fleet Labels > Player | Player label detail and zoom threshold | `graphics.zoom_label_player_detail`, `graphics.zoom_label_player_threshold` |
| Mod Settings > Fleet Labels > Non-player | Non-player label detail and zoom threshold | `graphics.zoom_label_non_player_detail`, `graphics.zoom_label_non_player_threshold` |
| Mod Settings > Galaxy Labels > Other | Extended selection and multi-select | `graphics.galaxy_extended_selection`, `graphics.galaxy_multi_select` |
| Mod Settings > Camera | Keyboard zoom speed and pan glide | `graphics.keyboard_zoom_speed`, `graphics.system_pan_momentum_falloff` |
| Mod Settings > Shortcuts | Rebind existing actions | Existing shortcut parser and `MapKey` registrations |
| General > confirmation page | Confirm Forbidden Tech upgrades | Inverse of `ui.auto_confirm_ft_upgrade` |

The controls branch implements instant warp, Fleet Labels and Forbidden Tech on
Windows x64. Hotkey editing was developed on its separate feature branch. Native confirmation
controls stay on the native page. FC retains its existing owner.

## Instant warp mode

Use one selection, not three independently stored flags. The UI and Alt+I call
the same live mutation function. Cycle order remains Normal > Warp > Jump > Normal.
Selecting the current value does not enqueue another save. Invalid choices do
not alter live state or the file. Existing per-ship overrides retain precedence;
the picker changes only the global fallback mode.

On supported clients, hold Ctrl on Windows or Command on macOS and click the travel button to show
the native Warp/Jump choice for that trip, including when a ship rule normally
chooses automatically. The button restores its native localized Set Course label while the modifier is held.
Releasing the modifier after the click does not cancel the override. It does not change
the saved mode; an ordinary subsequent click follows the configured behavior.

The click hook resolves the managed signature and required mouse-input functions
before installing on Windows x64, Apple Silicon and Intel macOS. Missing managed
bindings leave ordinary travel behavior available.

Reuse the existing single runtime writer, optimistic conflict handling and
source-preserving TOML edits. UI readback confirms the live value, not durable
storage; asynchronous save failures show a quiet Mod Settings notice with details
in the log. Reopening must
read the current owner, and shortcut changes must refresh a visible selector.
Native selection callbacks need the same rendering, stale-context and reentry
protection already exercised for boolean controls.

Selected options use bold text and the native checkmark on a normal background,
including instant warp and both Fleet Labels profiles. White fill is transient
pressed feedback, not persistent selection or keyboard focus. The scoped adapter
uses native sprites already rendered by settings rows and restores each Image's
previous override before pooling. A Windows-only `Selectable.DoStateTransition`
hook observes input-state changes, calls the original once, then updates only
owned selection rows. Other controls take the native path; there is no frame
polling, animation replacement, asset loading or setting write in this hook.

## Preview shortcuts and cargo previews

These controls use the existing boolean rows and the same live Config members already
read by the preview and keyboard paths. No new hook implementation, polling
callback, config key, or default is introduced. The plain cargo heading uses the
framework's existing text-row hooks, which are installed when a page needs them.
The pages are admitted only after their existing consumer hooks were installed.
Hotkey enablement and Scopely-hotkey selection still determine
whether the mod's Locate/Recall actions run.

Locate and Recall use positive UI labels: ON allows the action while a preview is
open, so the stored `disable_preview_*` value is false. These controls do not
perform Locate or Recall. They affect the next ordinary shortcut action.

Cargo auto-open is a master preference. The Target types heading and four target
switches appear only while it is on. Turning it off hides those rows without
changing their saved choices; turning it on shows the same choices again. UI and
shortcut changes refresh the section immediately. The native
cargo viewer reads them when a target preview opens or binds; changing a setting
does not forcibly close an already-open cargo panel. Re-select a target to see
the new auto-open behavior.

The existing Ctrl+R / Ctrl+T and Alt+1 through Alt+5 toggle actions now use the same
setting owners as these pages. An open page refreshes immediately after a shortcut
change. Both UI and shortcut edits submit the matching existing TOML key to the
single writer on supported Windows x64 builds. These shortcuts therefore retain
their preferences across restarts now; other platforms keep session-only behavior.
Repeatedly choosing the current value, rendering, and page navigation do not save.
Conflicts and failures leave live behavior in place and are reported in the log.

The Ship selection switch controls the equipped Forbidden Tech and Chaos Tech art
on `Manage Ship > Swap Ship` cards. While it is enabled, an independently saved
contrast-background switch appears directly below it and is off by default. The
display hook remains installed for the session; changing either preference affects
cards when the Swap Ship view next binds them and does not add indicators to the Fleet Bar.

## Camera

Keyboard zoom speed edits the existing System View keyboard zoom amount from 0
to 1000 in steps of 25 (default 350). This is a convenient slider range, roughly
three times the default at its upper end, not a new limit on player-authored TOML.
Zero stops incremental keyboard zoom; absolute zoom presets and mouse zoom keep
their existing behavior. Mod hotkeys must be enabled and Scopely hotkeys disabled
for the keyboard zoom actions to run.
The shared slider adapter selects the native Value label mode for this raw speed;
fractional sliders retain their existing Percentage label mode.

Display precision belongs to each shared `SliderSetting` (`displayDecimals`,
default 2; keyboard speed uses 0). The native slider label callback receives the
rounded applied snapshot, including when a later drag listener supplies an
unsnapped position. This covers all mod sliders, including Fleet Labels, while
preserving the game's number formatting. It does not change the live value,
slider step, player-authored TOML, or save timing. Stock slider labels take the
native path. No label override or extra frame callback is retained.

Pan glide edits the motion retained after mouse release from 0 to 0.99 in steps
of 0.01 (default 0.8). Lower values stop sooner. The upper end deliberately stays
below 1, which would preserve momentum indefinitely. It retains the existing
per-update decay, including its frame-rate dependence; this page does not change
the camera algorithm. `system_pan_momentum` is not exposed because the active pan
hook does not read it.

Both sliders read their current Config member and change it on the UI thread.
The existing camera hooks consume it on their next normal update; there are no
new camera hooks, refresh callbacks, or frame logging. Each row is admitted only if its
existing consumer detour installed successfully; the Camera page is omitted if
neither did. Live changes use the existing 150 ms coalesced TOML writer. Page
navigation never saves, and existing out-of-range or non-finite values remain
untouched with `Out of range; edit TOML` or `Invalid value; edit TOML`. Reopening
cannot fix a loaded value; a manual TOML correction takes effect after restart.
Defaults, config parsing and
other-platform behavior are unchanged. Native UI remains Windows x64 only.

## Fleet Labels and Forbidden Tech

One Fleet Labels page contains a collapsible Player heading, its Native /
Expanded / Compact / Threshold choices and percentage slider, followed by the
same controls under a Non-player heading. Each profile has its own owner and
selection. Click either heading to hide/show its controls independently, without
navigating away. Both sections start collapsed on each page visit; expansion is
temporary presentation state and never writes TOML or changes a setting value.
The native category arrow points down when expanded and right when collapsed.
Headings use larger bold cyan text and a darkened row background. An enabled
threshold slider uses a subtle cyan accent to connect it to the selected mode,
without a white selection fill. Tints affect the row's direct `BG` Image child
(`Background` for category headings),
when present; other prefab layouts retain the text styling. Native colors and
text are restored before refresh and pooling. Styling uses the existing bind,
refresh and release hooks, with no frame polling or shared-material changes.
Threshold is stored in [0, 1], edited in 1% steps,
and enabled only in Threshold mode. At 0% labels stay compact; at 100% they stay
expanded. Reading a player-authored fractional value does not round or save it.
Each user edit updates the existing live profile and refreshes tracked labels.
The native slider callbacks use the same typed snapshot/reentry guards as choices.
Unknown values suppress the slider and numeric label; disabled known values remain
visible. Releasing a pooled widget restores its label, active state and interaction.
The feature supplies the short disabled instruction (`Select Threshold`) through
`SliderSetting::disabledReason`; the shared widget has no Fleet Labels-specific
wording. Native ownership is described in [the adapter map](MOD_SETTINGS_NATIVE_ADAPTER.md).

Windows installs the existing fleet-label and Forbidden Tech hooks when the mod
settings UI is enabled, so changing their values does not require a restart.
Each FT hook consults the current bypass flag; hook availability is separate from
the value. Other platforms retain startup-controlled installation and omit this UI.
Confirmation ON means the bypass flag is false. Toggling must never invoke an
upgrade callback by itself.

The existing TOML writer now registers these additional keys at startup. One
worker serializes changes to the same file, keeping the latest pending intent
**per key**. A 150 ms quiet period coalesces slider motion; normal quit flushes the
pending value without waiting out that delay. F10 retains its existing force-close
cancellation and 500 ms best effort bound. Save failures/conflicts log the affected
section and key and leave the live setting in place. Numeric edits use the TOML
serializer and the same source-preserving edit/reparse/external-edit checks.
Page opens, section folding and native rendering never enqueue saves.

Collapsible headings reuse the existing category bind/release and page-selection
hooks. A heading click gives the native option panel a filtered `OptionContext[]`
through the existing `BindDataContext(provider, object)` virtual slot. The original page's
children and navigation parent stay intact, including controls omitted from the
visible list. Native rebinding releases hidden widgets and refreshes expanded
ones through the same guarded readers as a normal page visit. Plain headings
remain non-interactive. No new detour, frame polling or persistence owner is added.

On entry, native navigation establishes the selected page and Back target first;
the same callback then applies the initial collapsed list before returning. If
that presentation bind fails, the adapter attempts to restore the expanded list
so controls remain accessible. Expanding either section reads its current values.

Exact Windows build261 unwind extents, checked before expanding installation:

| Native target | RVA | Bytes |
| --- | --- | --- |
| SliderOptionWidget.SetWidgetData | D09C50 | 592 |
| SliderOptionWidget.OnSliderValueChanged | D0A1E0 | 117 |
| SliderOptionWidget.OnAboutToReleaseContext | D09EA0 | 288 |
| SliderOptionWidget.UpdateValueLabel | D0A260 | 458 |
| NavigationLOD.UpdateLOD | F8ECF0 | 75 |
| NavigationFleetWidget.OnDidBindContext | F7C870 | 335 |
| NavigationFleetWidget.OnAboutToReleaseContext | F7CEA0 | 283 |
| NavigationFleetWidget.OnEnable | F7D8B0 | 344 |
| NavigationFleetWidget.OnDisable | F7DAF0 | 236 |
| MessageBox.Show(context) | 70B5F0 | 81 |
| MessageBox.Show(context, callback) | 70B650 | 257 |
| TextOptionWidget.SetWidgetData | D0A470 | 293 |
| TextOptionWidget.ClearWidgetData | D0A680 | 271 |
| Selectable.DoStateTransition | 47A9650 | 805 |

These historical measurements exceed the bundled x64 SPUD 24-byte overwrite.
Installation resolves current targets through managed metadata. Measured client SHA256:
`487af4bb9c697c353be9714359a97dddcece5dab872622a6c498a27bbfc44f40`.
This is Windows evidence, not proof of macOS hook fit or native widget behavior.

## Future organization and commands (design notes)

The initial TOML-section grouping has been superseded by player tasks, documented
in [the current architecture](MOD_SETTINGS.md). Keep TOML sections as a storage
reference. Introduce groups only when they gain working controls with explicit
apply paths; do not build a generic editor for every key. Confirmations continue
to use the native confirmation page.

A future **Restart client** command could support controls that explicitly need
restart. It would perform an ordinary client restart, settle pending saves using
the existing lifecycle, and relaunch through a supported lifecycle owner. Cache
clearing is a separate operation and must not be called by this command. This is
an idea only: the current branch adds neither restart-only controls nor a restart
command. The ownership/relaunch details need their own design before implementation.

Hotkey editing follows the first real selection and persistence checks. Reuse the
current parser and binding map; add an explicit capture mode with Escape to cancel,
conflict feedback and a deliberate unbind action. Gameplay shortcuts must not fire
while a chord is being captured. Do not serialize display labels as key identities.

## Runtime gate

Before promoting the Navigation slice, verify all three choices, Alt+I changes
while visible, Back/reopen, restart persistence, and an external TOML edit conflict.
Bind build receipts to the installed artifact. The existing synthetic navigation
probe is not evidence that a new selection widget or real persistence path works.
