# PLAY-HAVEN-ORBIT — map-drag rotation, tilt and modifier reset

Contract version: 1. Status: implementation assessment; controlled-runtime validation passed, native qualification pending.

## Inputs and scope

Source: `frozen/play-dev-20261026` at `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target: `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`;
existing `features/haven_orbit.{cc,h}`, `game/haven_orbit.h`, shared constraint owner,
and new `tests/haven_native_cases.h`. No unrelated station camera rewrite is included.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | absent | Haven zoom source has no Haven drag/tilt consumer or shortcuts; station rotation is distinct |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | shortcut control and consumer | Same Haven workflow as frozen except SPUD include |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | shortcut/catalog control and consumer | `haven_zoom.cc`, `interior_zoom.cc`, shortcut/config registrations |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` | shortcut/catalog control and consumer | `haven_orbit.cc`, `game/haven_orbit.h`, `camera.cc`, `shortcuts.def` |

Scanned 2026-10-28. Shared Windows/macOS behavior. `patches.havenzoomhooks=true`
installs orbit independently of the live zoom distance. Drag defaults to `MOUSE2` (the middle mouse
button, as the pinned English example explicitly states), reset to `ALT-MOUSE2`; clearing drag disables orbit.
The inventory's earlier "right-drag" label does not describe that default. Global hotkey disable/native-hotkey mode
also relinquishes control. Source shortcut catalog and target catalog expose editing;
the feature has no persistence writer.

## Source workflow and ownership

Complete pinned implementation: `mods/src/patches/parts/haven_zoom.cc`.
Supporting source: `patches/orbit_constraints.h`, complete `parts/interior_zoom.cc`,
`prime/Hub.h`, `prime/Camera.h`, `prime/Vector3.h`, input consumers in
`patches/mapkey.cc`/`key.cc`, config/defaults/example, `settings/shortcut_catalog.h`,
`parts/hotkeys.cc` Haven entry callsite and `patches/patches.cc` registrations.
Only the shared constraint callback applies Haven runtime angles after native constraints;
native endpoint frame generation/pan then consumes those angles. Shared constraint assets stay native.

## Behavioral obligations

| ID | Pinned source behavior | Target mapping | Verification |
| --- | --- | --- | --- |
| O1 | Require active Haven section, compatible two-endpoint geometry, fixed pitch 15–80 and equal native yaw | Existing checked binding helpers and exact blend/endpoint roots | Actual production preparation/constraint callbacks in `haven-orbit-native` passed |
| O2 | Input is focus/text/UI/placement gated; an existing hold cannot begin a drag; fresh press required after blocked start | Existing source control flow retained; input polled before use | Held entry, UI continuing drag, placement cancellation and fresh-press cases passed; actual text/focus trial remains pending |
| O3 | Sideways yaw uses 0.15 degrees/pixel; vertical motion clamps pitch to 5–90 without inversion | Existing source formulas retained | Actual callback delta and 90-degree clamp cases passed |
| O4 | Continuing map drag can cross UI badges, reset wins held drag, reset changes orientation only | Existing pointer-continuation argument and reset precedence | Actual consumer continuation/reset cases passed |
| O5 | Scope endpoint mutation through native blend input, preserve native original once, restore nested scope | Existing scoped-source/target guard and shared constraint owner | Both-endpoint scope and original-call-count assertions passed; nested reentry remains a trial item |
| O6 | Leaving Haven/replacing owners/disabling restores native orientation and releases custody | Existing stronger per-endpoint/per-axis restoration | Disable/leave cases preserved competing yaw and restored owned axes; passed; replaced-blend case remains pending |

## Design and behavior decisions

No orbit production rewrite is needed. The target already separates orbit into its own
descriptor while retaining `havenzoomhooks` and the single `EnsureOrbitConstraints` owner.
It roots exact endpoints and restores each axis only while its current value still matches
the last applied value, preserving known competing edits. This is an intentional safety
improvement over source cleanup, which unconditionally restores one shared native yaw.
Unavailable restoration reads retain the obligation instead of silently retiring it.
The working `game/haven.h` adapter now enforces exact declared reference classes for the
same frozen orbit fields; `game/haven_orbit.h` reuses it instead of permitting widened
Object declarations through the generic typed match. No shared library behavior changes.

Source input invocation failure permanently disables orbit. The target safely rejects the
failed observation and may retry a later gesture; it never authorizes input from an unavailable
UI/placement/focus result. This is a deliberate availability improvement, not strict equivalence.

## Validation evidence and recovery

Source inspection completed; the orbit consumer algorithm remains unchanged in this lane.
The new fixture includes actual `haven_orbit.cc`, renaming only its exported constraint
namespace to coexist with the existing camera fixture sink. Shortcut booleans are controlled
inputs; actual section/UI/placement/field bindings are exercised. Installation readiness is
explicitly supplied, so this fixture cannot prove availability when a real shared host fails.
The implementing lane performed no build or test execution. The coordinator subsequently
qualified the settled inputs recorded in
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/source-inputs.json`.
See [FEATURE-COHORT-20261028-02](../FEATURE-COHORT-20261028-02.md).

Coordinator `tests-3.log` reports 416/416 steps succeeded on Windows x64, including
`haven-orbit-native`, all ten Haven modes and the existing visual fixture. Explicit embedded
and custom background integration variants each passed 302/302 steps (`tests-embedded.log`,
`tests-custom-build.log`). These are controlled-runtime results, separate from the signed
source checkpoint/product build and from actual native host availability. Historical user
trials do not qualify this candidate.

Next: test owner replacement and nested native callback
behavior; qualify actual text focus, badges, active placement, reset, release persistence,
scene exit, shared interior constraints and exact-client hook fit on Windows and both Mac architectures.
Overall parity remains open until those checks are attached to the final artifact.
