# PLAY-SHIP-TECH-ART — exact-ship Swap Ship equipment artwork

Contract version: 1. Source-preserving implementation; candidate execution and
native qualification are recorded in [cohort 20261028-01](../FEATURE-COHORT-20261028-01.md).

## Inputs and scope

Frozen source is `1baa55fedf1758b19f843cc59707b8901a0b4998` on
`frozen/play-dev-20261026`. Target assessment base is `play-dev-ripper` at
`5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0`. Source scan: 2026-10-28.

| Branch | Inspected revision | Presence / evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Named Swap Ship FT/CT consumer not assessed in this contract; no absence inferred. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | `mods/src/patches/parts/ship_tech_indicators.cc`, shared tile/pin/identity owners and registered controls. |
| play-dev / frozen source | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Same complete consumer plus `prime/{ShipTileWidget,ShipManagementViewController,Transform}.h`, checked contracts, exact ship identity and shared tile registration. |
| refactor | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | `src/mod/features/ship_tech_indicators.cc`, `game/ship_tech.h`, shared `ship_pin_input.cc` tile hook, `tracked_objects.cc` and existing Settings registrations. |

Both platforms retain live `ui.show_ship_tech_indicators` (default true) and
`ui.show_ship_tech_indicator_backgrounds` (default false). Installation remains
the restart-only `patches.shiptechindicatorhooks` switch. No new setting,
layout, equipment lookup, artwork pipeline or hook owner is introduced.

## Source workflow and ownership

The frozen tile callback runs native binding first, discovers the selection
context, hides stale artwork, and uses the exact BaseShip instance ID. The
ForbiddenTech manager supplies active slots; slot-spec IDs distinguish FT/CT;
nullable Int64 item IDs lead to Tech → Spec → IdRefs.ArtId. An instance, fleet,
slot-spec, item and art ID are separate identities. Neither names nor ship types
substitute for them. Missing art hides decoration rather than inventing an icon.

`TokenImageSource` borrows the game's serialized ImageSelector template: cached
weak donor, five-second retry, existing Ship Management owner slots first, then
one global resource fallback per selection context. The ordinary native ship
image remains a fallback. The target's inherited `TokenSource` performed a
global scan on each cache miss; this is the confirmed omission corrected here.

Native hierarchy owns decorations. Process state retains the small shared
background asset and weak templates/tiles/context; temporary managed collections,
slots, selectors and views receive local custody. No borrowed native pointer is
sent to a worker. The existing tracker already tracks Ship Management; no extra
discovery detour or per-frame scanner is needed.

## Behavioral obligations

| ID | Preserved behavior / adaptation | Verification and boundary |
| --- | --- | --- |
| TA1 | Native tile binding and pin ownership remain first and independent. Feature-off/outside-selection binding hides old FT/CT children. | Shared tile hook and target Bind source compared; native recycled-tile trial remains required. |
| TA2 | Resolve exact ship, nullable Int64 equipment, slot category and ArtId through existing typed helpers. Preserve observed empty versus failed access. | Existing exact ship identity fixtures retained; slot/art decoder unchanged. Native equipment identity must be checked with multiple ships. |
| TA3 | Prefer the four serialized owner slots, newest tracked owner first, with rooted snapshot custody and checked runtime types/liveness. | `ShipTechNativeCases` executes all four actual slot-reader paths and owned-donor selection without a global scan. |
| TA4 | Cache a live donor weakly, throttle retries to five seconds, allow a global fallback once per context, and retain native ship-image fallback. | Production donor fixture uses explicit monotonic test coordinates: repeated misses, dead donor, changed context and late native widget. No wall-clock sleep. |
| TA5 | Failed root/weak identity cannot authorize repeated global discovery. Unknown or destroyed templates stay unavailable. | Fixture checks failed context custody; temporary roots and retained donor/context handles retire explicitly. Real GC remains outside controlled-host coverage. |
| TA6 | FT left, CT right, selected-card offsets, size fallback, non-raycast images, optional existing rounded backing and native sibling order remain unchanged. | Static comparison of original UpdateIndicator and target UpdateTechIndicator. Native geometry/selection/pooling acceptance remains distinct from donor tests. |
| TA7 | Update existing children rather than recreating them; live setting changes rebind observed tiles. Retire stale ship/context evidence after native lookup callbacks. | Existing frame/Bind flow retained; recheck context before each equipment icon. No UI shell reconstruction or new pool. |
| TA8 | Shared tile installer, checked equipment preflight and feature reporting remain separate from user-facing enablement. | Existing installer source retained. Candidate native startup must confirm the group and shared tile owner; build alone does not establish hook fit or artwork. |

The target retains improvements already established: exact nullable metadata and
scalar `System.Object` art setter, checked unavailable observations, rooted
temporary results, native layout exclusion and live setting refresh. It does not
restore source hard caps on resource/owner enumeration; actual collections are
fully traversed only under the bounded fallback policy.

## Validation and recovery

`tests/ship_tech_bindings_cases.h` includes the actual production implementation
and typed readers. Its only host replacements are serialized game widgets,
native liveness/GameObject/resource methods, the existing tracker and an inert
shared-hook admission seam. It does not instantiate images, render, simulate
Unity layout or qualify real native detours. The controlled runtime explicitly
refuses unexpected UI value boxing rather than loading GameAssembly.

Historical Windows acceptance in `PLAY-RESTORATION-PLAN.md` covers Swap Ship
technology icons on its older candidate. Preserve that observation; it is not
an exact-current test of the new donor policy. Current commands, source hashes,
binary identities and trial outcome belong to the cohort receipt. Mac build and
runtime qualification remain separate.

Native procedure: open Swap Ship, inspect ships with different FT/CT equipment,
scroll/reopen and change selection. Check selected offsets, late-loaded artwork
and both existing live controls without altering saved equipment. Missing native
artwork is a graceful fallback, not a reason to scan globally every bind.
