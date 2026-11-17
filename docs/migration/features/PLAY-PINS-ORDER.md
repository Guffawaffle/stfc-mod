# PLAY-PINS-ORDER — Stable ship pin order

Contract v1, scanned 2026-10-26. Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`;
target `play-dev-ripper@f4f64180543a9231234cd2b3d3f547dfa580476b`.
Windows/macOS. Owner: `patches.pinnedshiphooks`; legacy `ui.pinned_ships` fallback.
Entry: Swap Ship. Persistence: exact ship IDs and unresolved legacy names in the
existing state owner's `pinned_ship_ids` and `legacy_pinned_ship_names` keys.

Source `mods/src/patches/parts/pinned_ship_sort.cc`, `pinned_ship_order.h`,
`pinned_ship_state.h`, `pending_pin_order.h`, ship identity/name matching, native
AssignShipsWidget refresh and the state writer form the complete workflow.

| Obligation | Source behavior | Target mapping / check |
| --- | --- | --- |
| O1 | Sort only ShipManagement context; explicit IDs lead in saved order, then legacy matches, then stable native order; placeholders retain precedence. | Existing typed dock owner and shared pure policy retained; independent fixtures. |
| O2 | Repeated legacy names claim highest-level unmatched instances, stable native tie order; never reclaim an explicit ID. | Compare both legacy mask and migration-assignment consumers. |
| O3 | Preserve unresolved names and refuse ambiguous unpin; toggles/moves compose against latest desired state. | Existing pending model and exact-ID migrations retained. |
| O4 | Queue admission is not saved/applied state; publish only latest exact durable receipt; failure retains prior applied order. | Existing state worker and receipt frame; test rapid edits, stale/failure receipts. |
| O5 | Refresh after two frames using weak selection context, promote to strong for calls, recheck actual widget binding. | Existing typed refresh path retained; no persistent borrowed managed context. |
| O6 | Native list acquisition/reordering must preserve all identities and fail honestly on incomplete reads or failed writes. | Retain target rooted snapshot and checked in-place write/rollback guards instead of legacy Clear/Add. |

The old parsed `pinnedshipclicktoggle` key has no behavioral consumer; source input
actually follows `pinnedshiphooks`. Do not invent a new switch or Shift-click
workflow. UI shape and native receipt publication need separate in-game checks.
[Cohort record](../FEATURE-COHORT-20261026-02.md).
