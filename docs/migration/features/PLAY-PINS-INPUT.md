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

## Own-card geometry correction — 2026-10-31

Target base `203bc90fed6631468440458f411b25bec9802bad`. Re-read the complete
frozen `swap_ship_pin_input.cc`, its bounds producer, group refresh/drop consumers,
and the supporting `UpdatePinGroupBand`/style/edge helpers in
`mods/src/patches/parts/ship_tech_indicators.cc` at `1baa55fe`.
Reviewed the retained player screenshot and current pin input/binding/Unity helpers.

The frozen `BoundsInContent` reads each card's own `RectTransform.rect`, projects
two diagonal corners through `TransformPoint` and `InverseTransformPoint`, then
unions the local min/max. The port instead used
`CalculateRelativeRectTransformBounds`, including child geometry. This is a
confirmed source mismatch and a plausible contributor to the reported oversized
group band; the exact native visual cause has not been established.

| Obligation | Typed mapping and boundary |
| --- | --- |
| G1 | `RectTransformUtility::OwnBounds` measures only the card's two source corners in content coordinates; descendants do not participate. Preserve the source's axis-aligned-card assumption rather than claiming arbitrary rotated-rectangle bounds. |
| G2 | Resolve exact Rect/Vector3 metadata and runtime invokers; reuse `TryReadVector3` and existing boxed-value checks/GC handles. Missing methods, foreign metadata/boxes and failed calls are unavailable geometry. |
| G3 | Root both transforms and each acquired value box across managed calls. Nonfinite inputs/results, overflow and source coordinate bounds refuse the measurement. These retain stronger typed-adapter safeguards. |
| G4 | `PinRefreshBand` unions the measured pinned cards in its existing scroll/context owner. Existing refresh timing, layout exclusion, native pointer policy, badge/state ownership and drop consumers remain intact. No new hook or global scene scan. |
| G5 | Pin input installation qualifies the own-card methods, replacing the obsolete descendant-bounds prerequisite. |

`tests/pin_geometry_cases.h` exercises the production Unity geometry adapter with
controlled managed boxing and transforms, including scale/translation, reversed
corners, metadata/box drift, missing invokers, managed failures and failed custody.
These local modes are not new CI registrations. This layer does not execute the
rendered pin band, real pooled ScrollRect dispatch or persistence.

Windows core compile/build-tests passed 196/196 steps and affected binding/mod
suites passed 312/312 steps. All five explicit geometry modes and the selected
station, diagnostic and timer-owner regressions passed. Exact source/ref snapshots,
input/executable hashes and scope are retained outside the checkout in
`artifacts/integration-audit-20261031/pin-geometry-verification.json`.

Remaining source gaps stay explicit: frozen group styling has a 10-unit margin,
normal/active/hover fill and rim colors/widths, unmasked rim edges and corresponding
reset behavior; the target's existing simpler decoration is not equivalent.
This bounded geometry correction does not close I4 or overall pin input parity.
Native screenshot/drag/drop qualification and Mac gameplay remain pending.
