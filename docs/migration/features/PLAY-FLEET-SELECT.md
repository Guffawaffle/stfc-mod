# PLAY-FLEET-SELECT — ship selection and selected-fleet location

Qualification update: the 382-step Windows suite and native build/boot passed at
`b36d2837`; the ordinary Windows trial was accepted ("looks good to me"). Evidence and limits:
[cohort 04](../FEATURE-COHORT-20261027-04.md).

Contract v1; source scan 2026-10-27. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` starts at
`557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8`. Cohort 04 owns final candidate,
executed tests, build identities and runtime qualification.

## Source and activation

Read the complete selection and SelectCurrent paths in frozen
`mods/src/patches/parts/hotkeys.cc::ScreenManager_Update_Hook`, its early returns,
`CanHideViewers`, fleet-bar/panel/fleet/manager/Canvas/Widget wrappers, MapKey
dispatch, configuration/defaults and Controls/Preview settings consumers.
Target consumers are `src/mod/features/fleet_actions.cc::SelectShipShortcuts`,
`FleetShortcuts` and `features/hotkeys.cc::HandleShortcuts`; the shared frame/input
owner remains `mod.cc`. No second input handler or feature hook is introduced.

| Inspected branch | Exact revision | Source observation |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Number-key select/repeat-location and SelectCurrent in `mods/src/patches/parts/hotkeys.cc`; TOML consumer present. Not native qualification. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Same workflows, with downstream native Settings integration. |
| frozen play-dev | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete selected source assessed, including adjacent Shift towing and native-hotkey/capture exclusions. |
| refactor | `557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8` | Existing typed selector; this pass qualifies its lifecycle rather than restoring an absent consumer. |

`[patches].hotkeyhooks` remains the restart gate. Mod hotkeys must be enabled,
native hotkeys off, and text/capture must not own input. Configured
`select_ship1`…`select_ship8` chords default to 1…8. `select_current` defaults to
Ctrl-Space, with existing platform modifier resolution. `control.select_timer`
defaults to 500 ms; `ui.disable_preview_locate` defaults false. Existing controls,
inverted preview adapter and registered Settings persistence are preserved;
selecting/locating itself writes no configuration or player store.

## Behavioral obligations

| ID | Frozen operation | Current mapping and verification boundary |
| --- | --- | --- |
| FS1 | First matching ship action maps to slot 0…7; first press uses `RequestSelect(index, false)`. | Ordered action array/native request retained. Actual consumer fixture exercises all eight slots, including empty. Unknown slot evidence does not invent a fleet or block ordinary native selection. |
| FS2 | Same slot strictly before timer endpoint locates only when native `IsIndexSelected` is true. | Native query and strict endpoint retained; checked failure cannot authorize location. Deterministic clock exercises 499 ms versus 500 ms without sleeping. |
| FS3 | Disable-preview-locate blocks repeat location while a supported viewer is shown, preserving ordinary selection. | Existing `TargetViewerShown` and original viewer-family precedence reused. Fixture supplies actual visibility metadata and drives production selector. |
| FS4 | Location hides interaction then requests native `RequestViewFleet(fleet, true)`. | Root bar/panel/fleet/manager locally; revalidate selected source after hide. Controlled hide changes fleet and proves stale location is consumed without transfer. |
| FS5 | SelectCurrent independently locates the selected fleet outside chat/focused input. | Existing host routing retained; qualified location helper reused. SelectCurrent does not gain a preview-block rule absent from source. |
| FS6 | Shift+ship belongs to Discovery towing. | Existing Shift resolver and towing entry remain; repeat identity retires at handoff. Towing is separately owned and qualified. |
| FS7 | Focus, capture, disabled hotkeys and native-hotkey mode exclude mod selection. | Host guards retained, consumer guards explicit; repeat identity retires through existing frame owner even if shortcut dispatch is skipped. Fixture covers modes and failed rooting. |

## Lifecycle correction and exclusions

Frozen and prior target remember only index/timestamp. A replacement fleet in the
same slot, rebound bar/context, or replaced vector can inherit another ship's
repeat press. This is an inherited source-lifecycle defect, not a claim that the
migration omitted number-key selection.

One bounded weak record now retains bar/context/vector/slot-fleet identities plus
observed ID and timestamp. No borrowed pointer or strong fleet ownership crosses
frames. Unknown/empty observations retire the record. Ordinary native selection
remains available; only repeat-location requires qualified identity. Roots span
native calls and release afterward. Native request acceptance is not proof that
selection completed; the next press still checks native `IsIndexSelected`.

No fleet ordering, shortcut-hint mapping, towing policy, queue/travel/combat or
native UI behavior is redesigned. Negative/zero timer settings retain the source's
effective no-repeat meaning. Clock regression cannot authorize a repeat.

## Verification and recovery

`tests/fleet_selection_ability_bindings_cases.h::FleetSelectionAbilityCases` uses
the real consumer via the existing fleet-actions fixture host, controlled native
metadata/methods and input sinks. Coverage: timing, eight slots, selected-state
failure, same-object ID replacement, changed fleet/bar/context/vector, preview
blocking, hide reentry, input modes, SelectCurrent and custody release. This does
not run Unity, native shortcut dispatch, real key layouts or a rendered fleet bar.
Child implementation ran no builds/tests; execution is coordinator-owned.

Native procedure: select occupied/empty slots, repeat at short/long intervals,
try Ctrl-Space, toggle preview-block with a target card, and test text/capture/
native-hotkey modes. Change screens and selected fleets between presses. Confirm
Shift towing separately. Windows smoke/player acceptance and macOS builds/
Mcohall's runtime trial remain separate qualifications.

Disposition: source-preserving integration implemented; automated/native results
pending the exact cohort candidate. No overall functional parity claim.
