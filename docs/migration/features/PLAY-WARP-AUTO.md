# PLAY-WARP-AUTO — per-ship automatic course confirmation

Contract v1, source scan 2026-10-27. Source is
`frozen/play-dev-20261026@1baa55fedf1758b19f843cc59707b8901a0b4998`;
target is `play-dev-ripper`, starting at
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`.
Final candidate-bound qualification belongs in the
[eight-feature cohort](../FEATURE-COHORT-20261027-02.md).

## Source and activation

Read the complete frozen `patches/parts/instant_warp_confirmation.cc`,
`patches/instant_warp_policy.h`, `patches/warp_click_intent.h`,
`ship_name_match.h`, CourseData and CoursePromptPopupWidget wrappers, plus
configuration parsing/defaults, installation registration, the hotkey call site
and `mod_settings::CycleWarpMode`'s existing save ownership. All frozen paths
are under `mods/src/`. Target consumers are `features/instant_warp.cc`,
`text.h`, typed `game/fleet.h`, `navigation.h`, `ui.h`, the one frame dispatcher
and the existing registered Settings writer. The already-assessed
[route editors](SET-WARP-FILTERS.md) remain their own UI contract.

Entry: an ordinary native warp-or-jump popup, or Mod Settings → Map & Travel.
`ui.auto_confirm_instant_warp` defaults to `none`; the three
`ui.instant_warp_always_ask/auto_jump/auto_warp` filters default empty. Native
gameplay decides whether instant warp is available. `toggle_instant_warp`
(default Alt-I) cycles none → warp → jump and submits the canonical string to
the existing preserving writer; accepted live state is not a disk-save receipt.

| Inspected branch | Exact revision | Presence / source evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Consumer: `mods/src/patches/parts/instant_warp_confirmation.cc`; control inventory remains separately qualified. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer present at the same path. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete source workflow inspected. |
| refactor base | `a546b7c9f846aa22aa9a12b82367d8a2134fa5ff` | Existing `src/mod/features/instant_warp.cc`; this is reassessment, not a missing-feature claim. |

## Behavioral obligations

| ID | Source behavior | Target preservation / verification |
| --- | --- | --- |
| A1 | Localized hull display words, not instance/fleet IDs or arbitrary internal hull keys, drive name matching. Normalize case, USS punctuation, hyphens/underscores and trailing-word suffix. | Existing `FleetPlayerData::DisplayName`, `ShipNameWords` and `ShipNameMatches` retained. Actual production resolver fixture drives typed localization and punctuation/suffix matching. |
| A2 | Always ask precedes auto jump, auto warp, then default; `*` matches all. | Same priority and existing `ShipFilter` retained. Overlap/wildcard/default fixture; editor/parser regressions remain independent. |
| A3 | Original AboutToShow runs once before inspecting the popup. Only an instant-warp prompt may be answered. | Rooted controller/widget/context/course/fleet, checked availability and same-identity readback; native ordinary/missing-course/missing-fleet/rebound-course cases exercise the production callback. |
| A4 | Call the native regular-warp or instant-warp handler, rather than implement travel or mutate a course/cost. | Existing handlers and transactional two-hook owner retained. A failed native action is not retried through the other option. |
| A5 | Explicit modified click retains native choice; otherwise configured policy applies. | [Manual-choice contract](PLAY-WARP-MODIFIER.md), exact one-shot request and unknown-data semantics. |
| A6 | Cycle canonical mode via registered Settings/save ownership. | Existing `gameplay_settings::CycleWarp` and writer boundary unchanged; pure gameplay-setting and persistence tests qualify their actual layers. |

## Explicit corrections and limits

The frozen and initial target could fall back to the configured default when a
popup's course or player fleet was missing. This pass deliberately refuses that
automatic action: unavailable identity is not authorization for a paid option.
Named filters with unavailable localization likewise retain the native choice
when the higher-priority decision cannot be established. A wildcard/default
decision that does not need a name remains usable. These are correctness
corrections, not claims of byte-for-byte behavior equivalence with that defect.

Managed getters/localization may reenter. The target rechecks popup, context,
course, player-fleet and instant-warp availability immediately before an action;
temporary strong custody ends after the callback. No managed pointer is parked
for later automatic action. The existing registry, hook transaction, frame owner,
configuration defaults and writer remain unchanged.

`tests/instant_warp_bindings_cases.h` compiles the actual production feature into
the controlled IL2CPP runtime. It substitutes native method bodies, localization,
input and the existing test Config/save owner. It does not prove real course
availability, resource charging, popup visibility, native focus or macOS ABI.
Final build/fixture results must be attached by the cohort owner after all edits.

Native trial: overlapping filters, wildcard and default; Alt-I/reopen; ordinary
and Ctrl/Command-modified travel; change selected ship while planning; verify
normal unavailable/toll/wormhole paths remain native. Do not spend jump resources
only to test this migration. Windows player testing and macOS CI/Mcohall testing
remain separate from source inspection and fixture success.
