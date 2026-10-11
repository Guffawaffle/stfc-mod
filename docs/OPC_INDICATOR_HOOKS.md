# OPC indicator hook ownership

`InstallOpcIndicatorHooks` owns six event detours and one supplemental lifecycle
detour in `Assembly-CSharp`. All seven are
instance methods returning `System.Void`; the installer declares their managed
signatures through the shared `method_contract::Resolve` helper.

| Class | Method and parameters | Consumers |
| --- | --- | --- |
| `Digit.Prime.HUD.FleetStateWidget` | `SetWidgetData()`, `ClearWidgetData()` | OPC ETA |
| `Digit.Prime.HUD.FleetbarFlagWidget` | `SetWidgetData()`, `ClearWidgetData()` | OPC highlight |
| `Digit.Prime.Ships.FleetLocalViewController` | `BindDataContext(Digit.Client.UI.IDataContextProvider, System.Object)` | Both |
| `Digit.Prime.Ships.FleetLocalViewController` | `OnCurrentCargoReactiveEvent(System.Int32)` | Both |
| `Digit.Prime.Ships.FleetLocalViewController` | `LifecycleTick()` | Both; delayed initialization and live refresh |

The resolver discovers the current native pointers from metadata. Moving native
addresses between client builds does not invalidate these contracts. An absent
or ambiguous signature cannot be treated as the expected callback. Signature
resolution does not establish native detour fit or compatibility with another mod.

## Installation and feature controls

The registry group is `OpcIndicators`, controlled by `installOpcIndicatorHooks`.
Currently that flag is hardcoded to `true`; there is no `[patches]` TOML switch
for this group. The existing player settings are separate runtime controls:

- `[ui] fleet_hud_opc_eta` enables ETA behavior.
- `[ui] highlight_opc_fleets` enables highlight behavior.

Both features need the fleet-controller pair. ETA additionally needs the state
widget pair; highlight additionally needs the flag widget pair. On Windows an
incomplete feature-specific lookup family can leave the other feature available.
The existing macOS preflight requires all six targets to exist and have distinct
native pointers before installation. This resolver refactor does not change that
policy or add platform restrictions.

A failed detour installation disables both features. Any detours already installed
retain their native path; the installer does not retry partial installations.
This applies to the original six event detours. The supplemental lifecycle detour
is attempted afterward; a missing, overlapping or failed lifecycle target leaves
the original feature paths available on both platforms.

## Data arriving after binding

A ship already mining at startup can bind its widget before cargo, mining data or
the UI anchor is available. Event-only rendering then waits for another bind or
cargo event, which might require recalling and redeploying the ship.

After the game's own `FleetLocalViewController.LifecycleTick`, the mod refreshes
that controller's indicators using its current fleet and UI. This adds no fleet
scan, retained managed pointer, worker thread or independent polling dispatcher.
UI work is limited to once per second per controller; existing cargo caching and
render change detection remain in use. Rebinding clears the controller's cadence
entry, and disabled runtime features preserve the native tick without extra UI work.

The ETA render cache owns both the computed values and their refresh timestamp.
Computation is saved before UI setup, so missing anchors and failed setup do not
discard the latest result. The per-controller tick cadence limits UI traversal;
the per-fleet computation cadence shares results between views of the same fleet.

UI setup backoff remains separate: it limits repeated allocation and configuration
attempts after failures, up to 30 seconds. It does not schedule callbacks or solve
late model delivery. Cargo events still invalidate shared samples immediately;
bind and widget callbacks refresh after native layout changes, and clear callbacks
remove indicators from recycled views. These responsibilities remain useful with
the supplemental lifecycle tick.

## Responsibility boundaries

Settings adapters own user choices, persistence and availability reporting.
The installer owns binding contracts and installation readiness. Hook callbacks
call the native method and handle indicator rendering and lifecycle cleanup:
set/bind callbacks refresh after the native call; clear callbacks capture the old
context before the native call and then remove the old indicator; cargo callbacks
invalidate the sample before refreshing after the native call.

`fleet_opc_sample` owns a game-thread-only, value-only cargo sample cache shared
with fleet notifications. Sharing this data does not mean the alert feature owns
or independently installs the six indicator detours. Settings refresh invalidates
samples and removes disabled indicators without reinstalling hooks.
