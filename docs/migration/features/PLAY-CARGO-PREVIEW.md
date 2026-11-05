# PLAY-CARGO-PREVIEW — category defaults and delayed cargo opening

Contract v1, 2026-10-27. Source is frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff` on `play-dev-ripper`.
This is the travel/cargo batch, preserving category and native rendering behavior
while adapting delayed work to owned source identity.

## Source workflow and ownership

Read frozen `mods/src/patches/parts/hotkeys.cc` cargo ActionView dispatch,
`CheckShowCargo`, both binding hooks and their installation; complete
RewardsButtonWidget, PreScanTargetWidget, BattleTargetData, FleetDeployedData and
VisibilityController supporting bindings; preview toggle/settings persistence
call sites; and the mining-preview suppression interface. Current equivalents
are `src/mod/features/fleet_actions.cc`, `hotkeys.cc`, the existing Settings
catalog/registered writer, and `game/{viewers,fleet,ui}.h`.

These consumers exist in local upstream dev `e884efa7`, PLAY `cad98ef7`, frozen
play-dev `1baa55fe` and the current refactor. Their presence is not proof of
equivalent UI behavior. `ui.show_cargo_default`, `show_player_cargo`,
`show_station_cargo`, `show_hostile_cargo` and `show_armada_cargo` default true.
The current Preview controls and toggle shortcuts retain the existing registered
Settings/TOML writer; no settings, defaults, paths or storage formats change.

Automatic opening runs after the native rewards bind or pre-scan ShowWithFleet
original. Explicit ActionView first gives the mining-node cycle its existing
priority, then toggles visible pre-scan cargo. That dispatch remains under the
existing mod-hotkey, native-hotkey, capture and text-focus owners.

## Behavioral obligations

| ID | Frozen behavior | Current mapping / verification method |
| --- | --- | --- |
| C1 | Native originals execute once before automatic cargo opening. | Both production hook bodies retained with operation-local custody. Actual cargo-preview fixture counts the original; native bind/show timing remains a player trial. |
| C2 | Master flag precedes independent station/player/hostile/Armada flags. Known-null deployed data selects station; Player selects player; Marauder + Armada hull selects Armada; other Marauder hulls select hostile; other fleet kinds do not auto-open. | Existing rule preserved. New checked field/getter observations distinguish unavailable metadata/managed failure from an observed null. Fixtures cover each category and independent flag, unsupported type and managed category failure. |
| C3 | Mining-cycle suppression prevents automatic opening from skipping its node → occupant → cargo stages. | Existing `mining_preview::SuppressAutomaticCargo` remains the authority, called before category work. Its controlled-runtime tests substitute suppression, so the combined native workflow still needs the mining trial. |
| C4 | Explicit view toggles visible target cargo; delayed work retries the original five passes. Automatic binding shows immediately and allows its original one-pass retry. | Absolute Unity-frame windows preserve the normal five/one-pass behavior while preventing skipped dispatch from extending it indefinitely. A new explicit hide cancels pending reopens. Actual production fixture covers open/hide, expiration and mode disable. |
| C5 | Delayed native UI references must not become a different card/target after rebinding, closure or replacement. | Replaced the inherited global integer with exact owned viewer/rewards/context/controller references and an observed TargetID. Native getters are rechecked before display; target ID zero remains an available observation. Actual fixture covers changed context, same-object ID rebind, category reentrancy and root release. |
| C6 | Disabled/native-hotkey/capture/focused-input paths cannot keep old delayed work alive, and delayed cargo must not take over mining ActionView. | The existing always-delivered ability/frame callback retires cargo work even when ordinary shortcut dispatch is skipped. Successful mining-cycle admission cancels cargo requests; no additional frame hook, task framework or source registry is added. |
| C7 | Evidence/availability errors retain native behavior instead of guessing a target category or opening a replacement target. | Missing pointer-field metadata is refused before use; unreadable/null required owners decline. Native show/hide implementations retain rendering behavior. Known-null deployed data is handled separately from unknown field acquisition. |

## Design decisions and source defects

Both frozen and target code used one global pending-frame counter. A delayed
request could reopen a newly scanned/rebound card, and time spent outside normal
hotkey dispatch could extend the nominal retry indefinitely. This is an inherited
source defect, not a newly discovered migration-only regression. The bounded
ownership adaptation deliberately does not reproduce it.

Each pending request owns four managed roots only during its original bounded
window. The containing vector is process-lifetime so static destruction cannot
free GC handles after runtime teardown; completed/cancelled requests release
their own roots on the game thread. Requests detach before native callbacks;
generation invalidation prevents reentrant bind/cancel from publishing an old
episode afterward. No borrowed objects cross frames, no artificial viewer-count
cap is imposed, and no global pool or generalized persistence/task system is added.

Automatic bind/show remains immediate and independent of the normal keyboard
mode. Only delayed keyboard-dispatch work is cancelled when that mode cannot
process it. Failed category reads no longer become a station/null fallback;
successfully observed null deployed data still follows the original station rule.

## Validation and qualification limits

`tests/course_preview_bindings_cases.h` extends the already-included production
fleet adapter in the existing controlled IL2CPP runtime. The new modes
`preview-cargo` and `preview-cargo-field` supply metadata, native rendering
counters and controlled tracked viewers. They exercise actual category/ownership
helpers and hook bodies, not a duplicated cargo policy. They do not run Unity's
real pooled UI, native mine selection, or a game scene transition. The batch
record supplies candidate-bound execution/build results.

Pending Windows trial: scan player, hostile, Armada and station targets with the
corresponding flags; toggle view open/closed; move rapidly to another target;
switch hotkey modes or focus chat before a delayed retry; use ActionView on an
occupied mining node and verify the occupant/cargo/node sequence. Missing cargo
must remain native/unavailable, never another target's display. No player setting
is changed by the automated fixture. macOS builds and Mcohall native testing remain
separate qualification.

Migration coverage is implemented with explicit source-defect corrections;
player-verified parity remains limited to the final batch trial. Existing AAI,
fleet actions, previous same-action retry, queue recovery and repair/help order
remain unchanged outside this cargo subsection. Final checkpoints are recorded
centrally by the batch coordinator; no overall product parity is claimed.
