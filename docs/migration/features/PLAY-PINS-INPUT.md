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

## Group and drag presentation restoration — 2026-11-01

Source remains frozen `1baa55fe`; target base is the player-verified mission-marker
correction `cba6fc110f37afc4ab9198ade09e1c6ca059679d`. This batch re-reads the
complete frozen pin-input workflow and the supporting ship-tech presentation
helpers and compares them with the existing typed owners. Pin state, durable
receipts, double-click/hold thresholds and shared hook ownership stay with their
current implementations. No new hooks, configuration settings or UI framework.

| Obligation | Source operation and typed adaptation | Verification |
| --- | --- | --- |
| V1 | Ten-unit group margin; normal/drag/hover fill, four rim edges and widths; fill stays masked while rim edges are unmasked. Existing own-card bounds remain the producer. | Production presentation adapter fixture and native visual trial pending. |
| V2 | Armed/dragged source has gold card edges; eligible pinned target has green edges, and an outside-group unpin target has red edges. Unpinned source outside the group has no target highlight. | Production helper transitions and real gesture trial pending. |
| V3 | Empty-group cue is capped at 90x90, fits the viewport minus eight units, includes centered 13-point "Pin here" text and a separate leading edge, with source hover colors. | Native creation/layout/cleanup fixture and first-pin trial pending. |
| V4 | Cancellation, rebind and normal release restore source/target badges and edges, normal group style, and hide both cue and separate edge. Active group/cue custody stays weak and gesture-owned. | Reset and replaced-viewport fixture; focus/rebind/drop trial pending. |
| V5 | Decorations under native layout owners remain excluded from layout and non-raycasting. Failed maskability or custody cannot leave an actionable first-pin cue; missing geometry is not proof that no pinned group exists. | Failure injection plus native trial pending. |

Windows and macOS share the implementation. Fixtures qualify presentation helper
calls against a controlled managed runtime, not real rendering, ScrollRect input,
account persistence or native platform behavior. Current local Mac SDK absence
remains a separate build limitation. I4 and overall pin parity stay open until the
rendered and gesture workflow is tested on the delivered artifact.

Intentional retained safeguard: green reorder preview and group drops both require
the existing checked band geometry. Frozen source could infer group membership
from a pinned tile alone; this batch does not expand the typed drop authorization
when geometry is unavailable. A known pinned group with failed geometry also does
not become an empty-group cue. These failure differences are not claimed equivalent.

Local results: Windows core/build-tests passed 196/196 steps; the binding/gameplay
suites passed 312/312 steps. `pin-presentation` executes production group/tile/cue
helpers and release cleanup using the existing controlled Unity scene, including
viewport replacement, small/invalid viewports, missing weak custody and contained
maskability failure. It does not simulate actual pointer dispatch or durable pin
publication. The initial fixture setup/rectangle-return failures were corrected;
they were not game crashes. Evidence and hashed inputs are retained in
`D:/dev/stfc-workspace/artifacts/pin-presentation-20261101/`. The test mode is local;
no CI registration was added. Native visual/gesture and current Mac results remain
pending.
