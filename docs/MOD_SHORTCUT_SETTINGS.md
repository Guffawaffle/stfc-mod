# Mod shortcut editor

Develop on `feature/play-shortcut-settings`, based on the tested settings build.
Reuse the existing MapKey parser, binding list, layout mapper and TOML writer.
The settings expansion and daily play branches are separate from this work.

The UI uses Settings > Mod Settings > Shortcuts, grouped by the action's effect:
User Interface, Fleet Controls, Map & Travel, Camera, Chat, Client and Diagnostics.
Opening inventory and artifacts belongs to User Interface; moving between map
views and cycling instant warp belongs to Map & Travel. Explicit presentation
metadata gives each action a human label without changing its config identity.
It is available in the Windows native settings adapter
when mod hotkeys are installed and Scopely hotkey mode is off. Only actions
registered by the existing configuration loader are listed. Their original
gameplay contexts and feature enablement still apply.
Shortcut-hint editing is omitted if its adapter was not installed at startup
(including an initial `NONE` binding); the editor does not claim a live change
for a startup-disabled feature.

## Behavior contract

- Each binding has its own Change button, followed by Add shortcut. There is no
  selected-binding cursor or Next button. More options starts collapsed and holds
  the explicit Remove buttons. Recording reveals its status and Cancel; a valid
  draft reveals Apply and any overlap warning. Inactive controls leave no gaps.
- Edit one action and one binding at a time. Preserve its other alternatives.
- Adding or replacing with a binding already on that action shows `Already bound`
  and leaves Apply disabled, without publishing or saving. Compare parsed key and
  modifier groups, ignoring modifier order, repeated groups and alias spelling;
  generic and sided modifiers remain distinct. Existing duplicate entries can be
  removed explicitly with Remove and Apply; opening a page never cleans up TOML.
- Capture is a draft. Apply publishes the complete action list once; Cancel and
  Escape do not change live bindings or enqueue a save. Removing a binding is an
  explicit action; removing the last one stores the existing `NONE` spelling.
- Show overlapping bindings as a warning and allow keeping both after explicit
  Apply. Never silently remove another action's binding. Existing contextual
  overlaps and modifier matching continue to work as before.
- Capture owns keyboard input until the captured/cancelled keys are released.
  The initiating click and previously held keys cannot become a binding or leak
  into gameplay. Leaving the editor or losing focus cancels capture.
- Store canonical `[shortcuts]` identities, not labels. Keep existing aliases
  readable; a new canonical edit takes precedence without deleting an alias.
- Replacing an action prepares the complete list and its shortcut hint first,
  then publishes them together on the game thread. Layout registration must
  include newly introduced keys. Existing parsing/default fallback is unchanged.
- Read back the actual active binding list. A stale editor draft must not replace
  a newer list. File conflicts/failures retain the live edit and go to the log,
  consistently with the existing settings writer.
- The game's rebinding popup uses Unity InputActions and Scopely's persistence.
  Reuse native UI only where mod ownership can be maintained; do not edit native
  InputAction assets or cloud shortcut preferences to represent a mod binding.

## Delivery checks

First verify atomic action replacement, alternative preservation, fresh hints and
invalid/unbound behavior in the existing MapKey fixture. Then exercise the native
editor, capture ownership, Escape/focus/held-key cancellation, advisory conflicts,
  live dispatch, reopen and restart persistence on an identified artifact.

## Capture and presentation limits

New recordings use either-side Ctrl/Alt/Shift/Win modifiers. Existing sided
bindings and alternative ordering are retained until explicitly replaced. Escape
is reserved for cancelling recording; existing Escape bindings remain readable.
OS shortcuts can still be handled by the OS; this is not a global keyboard hook.
Layout capture supports the same character set as the existing mapper and rejects
ambiguous/unavailable characters rather than storing the wrong physical key.

Overlap warnings list other mod actions whose resolved physical key and modifier
rules can match together. Bare `I` rejects modifiers, so it does not overlap
`SHIFT-I`. Explicit modifiers are minimum requirements: `SHIFT-I` and `CTRL-I`
can both match while Ctrl+Shift+I is held. Layout-required Shift and sided modifiers
are included. Contexts may still make an overlap intentional; this does not audit
Scopely or OS shortcuts. Gameplay dispatch rules remain unchanged.
An overlap changes the draft's button to `Apply anyway`; its warning remains
visible after applying, until the next edit or page departure.

The command row reuses ButtonAndTextOptionWidget. Its unique closed delegate
target is a plain managed Object owned by that native context. The cloned Object
constructor supplies only the `void()` instance callback schema; every executable
entry point is replaced, and no constructor/game method is called by the command.
Only a currently bound, visible, enabled row with that target may invoke it.
Release restores local text, button visibility and interactability overrides.
Rebinding the visible list preserves the draft; leaving the editor cancels it.
The Add row owns this visit callback, so recycling that row also cancels safely.

Action definitions can produce indexed rows and hide individual presentations.
Indices are presentation identities, not persistent binding IDs: a draft still
compares the complete observed list before publication. The native page owns rows
up to its largest binding count and reuses them after removals/additions. Refresh
adds missing contexts, filters surplus/hidden rows and follows catalog order.
The native adapter's existing 128-child sanity bound still applies. Two rows per
binding plus five fixed rows leave room for 61 bindings per action in the editor.
It rejects further UI additions before publishing. Existing longer TOML lists
stay live and saved in full: the UI explains that it shows the first 61, permits
replacement/removal, and exposes subsequent bindings as earlier ones are removed.
Opening an oversized list never rewrites it or removes unrelated settings pages.
Refreshes run
on settings actions and capture transitions; they do not add idle frame polling.
An unchanged visible list is not rebound.

The new detours are Windows x64 only. On client build261 (GameAssembly SHA256
487af4bb9c697c353be9714359a97dddcece5dab872622a6c498a27bbfc44f40),
ButtonAndTextOptionWidget.SetWidgetData spans CFD5F0..CFD8A5 (693 bytes), and
OnAboutToReleaseContext spans CFD430..CFD53F (271 bytes). Both were checked against
the PE unwind table and disassembly; SPUD reserves 24 bytes. Runtime metadata and
extent checks still gate installation. No additional ScreenManager detour is
installed: recording uses its existing dispatcher and does no idle input scan.
