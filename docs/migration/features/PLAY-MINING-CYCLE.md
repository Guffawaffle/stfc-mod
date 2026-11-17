# PLAY-MINING-CYCLE — mining node, occupant and cargo cycle

Contract version: 1. Source-preserving implementation assessment, 2026-10-27.
Automated/native qualification is recorded by the containing cohort; this document
does not treat source comparison or a pure polling fixture as in-game proof.

## Inputs and scope

Frozen `play-dev` is `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target is the existing `play-dev-ripper` checkout, base
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`. No branch was replaced.
Source paths at that immutable revision:

- `mods/src/patches/parts/mining_preview.cc`: entire native API, readiness,
  selection and session implementation.
- `mods/src/patches/mining_preview.h` and `mining_preview_poll.h`.
- `mods/src/patches/parts/hotkeys.cc`: owner-frame `Update`, ActionView dispatch,
  automatic cargo suppression and fallback into ordinary target cargo.
- `mods/src/settings/preview_settings.cc`, `mods/src/config.cc` and `config.h`:
  `show_cargo_default`, `show_player_cargo`, hotkey mode and persisted controls.
- `tests/mining_preview_poll_test.cc`: deterministic scheduling contract.

The current target maps these to `src/mod/features/mining_preview.{cc,h}`,
`mining_preview_poll.h`, `src/mod/game/mining_preview.h`, and fleet-actions
consumers. Branch-presence observations and exact resulting candidate belong to
the central inventory. This is a shared Windows/macOS workflow; platform builds
and native interaction remain independently qualified.

| Inspected branch | Exact revision | Consumer observation |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | **Mining-node/occupant/cargo cycle absent.** `mods/src/patches/parts/hotkeys.cc` ActionView only toggles cargo on existing PreScan widgets; category defaults also exist there. Those are PLAY-CARGO-PREVIEW support, not native node-to-occupant selection or a return-node session. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Present: `mods/src/patches/parts/mining_preview.cc`, header and ActionView `mining_preview::Cycle()` call. The later polling helper is not claimed at this revision. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Present: complete cycle plus source polling helper/test listed above. |
| refactor base | `a546b7c9f846aa22aa9a12b82367d8a2134fa5ff` | Present: existing typed native cycle in `src/mod/features/mining_preview.cc`; this pass restores missing source polling/readiness behavior. |

The upstream conclusion follows the actual ActionView producer/consumer and
tree-wide searches for `SelectPOI`, node-slot/occupant selection and cycle/session
paths, not the missing filename alone. Its `OtherPlayerOccupied` enum declaration
is dormant with respect to this workflow and does not establish a cycle consumer.

## Source workflow and ownership

ActionView first tries this cycle, then ordinary cargo handling when not consumed.
A fully settled foreign occupied mining card captures its node ID/type, occupant
fleet ID and weak navigation-context custody. The native POI selector opens the
occupant; the player can toggle cargo and return to the original node. The initial
cargo stage is selected by `show_cargo_default && show_player_cargo`. Owned miners
retain native fleet controls, and an empty node does not begin this session.

The source owns a tracked snapshot through widget calls. The typed target uses
existing `unity::Instances` enumeration and individually rooted widgets/context,
POI, slot and fleet objects. It retains only a weak context plus immutable IDs and
polling state across frames. That per-item custody is not a claim that the target
reproduces the legacy tracker's collection-wide exclusion. Native selection is
still owned by the game; no independent navigation or queue planner is added.

## Behavioral obligations

| ID | Frozen source obligation | Target mapping / qualification |
| --- | --- | --- |
| O1 | Begin only on foreign occupied node, visible settled card and positive/non-sentinel identities; own miners untouched. | Existing checked `Observe`/slot/fleet/native API reused. Original native POI type 5 is retained. Native occupied/vacant/own-miner trial pending. |
| O2 | Native context must own the loaded viewer and matching active POI before input can interrupt it. | Existing game binding verifies parent visibility, load completion, navigation loader ownership, active viewer and active POI. Input additionally verifies selected occupant ID. No alternate decoder. |
| O3 | Loading polls at 100 ms; established presence at 250 ms; user input does not wait for cadence; loading expires after 5 s with final current-state check. | Original pure `PreviewPoll` copied unchanged, include path adapted; existing target's per-frame complete scans and duplicate input Update scan removed. Pure original fixture exercises cadence/timeouts/reset; native frame timing pending. |
| O4 | Once ready, transient load/readiness changes do not masquerade as an absent ship. Full readiness is checked again for input. | Presence scan accepts shown/showing matching-context/fleet cards; input uses full `Observe`. A ready-but-unsettled card keeps the request without selection. |
| O5 | Cargo-first controls sequence, automatic cargo is suppressed only for this occupant while not requested, then return to exact original node/type. | Existing session and suppression helpers retained. Game `SelectPOI`, `RewardsClicked` and `Hide` continue to own effects. Exact context/POI checks in selection remain safer target behavior. |
| O6 | Disable/native-hotkey mode, leaving System, lost context, changed POI, missing established ship and expiry retire session. | Existing gates retained; invalid section/API input now explicitly resets, matching source. Weak handle disposed by reset. No game teardown calls from a static destructor. |
| O7 | Unavailable/missing native API declines safely, allowing ordinary native cargo behavior. | Existing descriptor is under hotkeyhooks and validates required metadata. Cycle false delegates to existing caller fallback; true means input consumed, not that a native cargo open definitely succeeded. |

## Decisions and limitations

The original scheduling helper was missing from the port. Restoring it is a
source-preserving lifecycle correction, not a new optimization framework.
The target's exact `Select` ownership revalidation and checked invocation result
are retained. Unknown IDs never become guessed destinations. Catch/reset on a
failed C++ path remains the existing target behavior; no replayed native action is
introduced. No new settings, defaults, persistence format or hook registry.

The pure fixture proves only scheduling. It does not run `SelectPOI`, asynchronous
asset loading, a real Rewards controller, IL2CPP collection teardown or native
scroll/input. No headless performance result is represented as a frame-time claim.

## Native qualification procedure

With a foreign occupied node, use ActionView through node → ship → cargo → node;
repeat with automatic player cargo disabled so the initial stage changes. Check
vacant and owned nodes, changing occupants, rapidly selecting a different node,
leaving System, disabling mod hotkeys and switching native-hotkey mode. A slow
popup must not be interrupted before readiness or acted upon after timeout.
Check Windows first; repeat on both supported macOS architectures after CI builds
and Mcohall's client trial. Never spend gameplay resources merely to force a race.
