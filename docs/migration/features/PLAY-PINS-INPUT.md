# PLAY-PINS-INPUT — Pin badge and held drag

Contract v1, scanned 2026-10-26. Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`;
target `play-dev-ripper@f4f64180543a9231234cd2b3d3f547dfa580476b`.
Windows/macOS; actual owner `patches.pinnedshiphooks`. Entry: Swap Ship badges.

Source `mods/src/patches/swap_ship_pin_input.cc/.h`, shared ship-tile binding/click,
ship-tech badge/group/cue helpers, pointer event/input geometry, pin sorting and
state receipts form the workflow. Target uses `features/ship_pin_input.cc`,
`pin_pointer_policy.h` and the shared dock/tile owners.

| Obligation | Source behavior | Target mapping / check |
| --- | --- | --- |
| I1 | A fresh mouse badge pair within 400ms toggles once; single badge click is consumed; third click cannot reuse a pair. | Existing pure pointer policy/native release adapter. |
| I2 | Touch/pen/keyboard remain native; match actual ScrollRect and pointer; early movement scrolls normally. | Existing checked typed event observations and frame epochs retained. |
| I3 | Hold125ms and move8px to drag; small held movement waits; native scroll cannot later become a pin action. | Existing policy; deterministic boundary cases. |
| I4 | Pin group/leading-edge cue, source/target/unpin highlighting; drop in group reorders/pins, outside group unpins only an already pinned ship. | Existing typed geometry/decorations and stable-ID owner; no speculative appearance redesign. |
| I5 | Cancel outside viewport, unavailable identity/position, stale/rebound tile, focus loss or lost EndDrag after250ms; suppress resulting click. | Check current native binding episode plus policy/cleanup. |
| I6 | Weak tile/context custody, fresh native selection and shared tile callback prevent unrelated dock/assignment actions. | Existing owners retained; no duplicate hook or action registry. |
| I7 | Failed durable receipt keeps old applied badges/order; ready state follows actual feature/frame ownership. | Pin owner handles publication; pointer policy must not claim saved state. |

Target safeguards intentionally exceed source in checked geometry, frame-epoch
admission, root custody and failure containment. Keep those improvements and
identify their fixture layer; model tests do not qualify real ScrollRect dispatch,
hit testing, layouts, pooled Unity objects or actual persistence. See
[cohort evidence](../FEATURE-COHORT-20261026-02.md).
