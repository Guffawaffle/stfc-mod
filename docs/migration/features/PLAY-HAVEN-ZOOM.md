# PLAY-HAVEN-ZOOM — qualified planetary zoom

Contract version: 1. Status: implementation; controlled-runtime validation passed, native qualification pending.

## Inputs and scope

Source: `frozen/play-dev-20261026` at `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target: existing `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`.
Assessed inputs are `src/mod/features/haven.cc`, `game/haven.h`, existing camera/orbit bindings,
and `tests/haven_native_cases.h`. No deployment, native trial or publication is authorized by this contract.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | config/gameplay consumer | `mods/src/patches/parts/haven_zoom.cc`; qualified blend geometry already present |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | control/catalog and consumer | Same camera workflow as frozen; only its SPUD include differs |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | control/catalog and consumer | `haven_zoom.cc`, `settings/upstream_settings.cc`, config/defaults/example |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus scoped working changes | control/catalog and consumer | `features/haven.cc`, `settings/config_settings.cc`, `game/camera.h`, `game/haven_orbit.h` |

Scanned 2026-10-28. Both desktop platforms share behavior. Restart-only
`patches.havenzoomhooks=true` installs the camera consumer; live
`graphics.haven_zoom=4000` controls the maximum distance, with zero selecting native zoom.
The current reader preserves the source finite/nonnegative constraint. The setting catalog owns
in-game editing and persistence; the feature owns no settings file writer.

## Source workflow and ownership

Complete pinned implementation: `mods/src/patches/parts/haven_zoom.cc`.
Supporting source: `prime/Camera.h`, `prime/Vector3.h`, `prime/Hub.h`,
`patches/parts/interior_zoom.cc`, `patches/orbit_constraints.h`, `config.h`,
`config.cc`, `defaultconfig.h`, `patches/patches.cc`,
`settings/camera_settings.{h,cc}`, `settings/upstream_settings.cc` and the English example.
The standalone `HavenZoomSetting()` factory has no pinned call site; the general settings
catalog is a separate reachable registration. The factory alone does not establish a native control.

Native `UpdateCameraFrame` runs once first. The consumer qualifies two distinct fixed-radius
planetary endpoints with a common pivot/look target, perspective frames, matching unit rotations
and FOV, valid soft limits and separate output storage. It changes only the freshly generated
blend result, along the camera back axis, and expands the far plane equally. It never edits
endpoint radii, normalized LOD, zoom position or shared constraint assets. Visual adjustments
are owned by [PLAY-HAVEN-VISUALS](PLAY-HAVEN-VISUALS.md).

## Behavioral obligations

| ID | Pinned source behavior | Target mapping | Verification |
| --- | --- | --- | --- |
| O1 | `HavenCamera_UpdateCameraFrame_Hook`: native original precedes every observation; native output on invalid geometry | Reuse original ordering and numeric predicates | Actual callback original-count and mismatched optics/look-target cases passed in coordinator tests-3 |
| O2 | `ReadHavenRadius` plus installer `FieldBinding[]`: all required geometry is observed from exact declared metadata | Haven-local exact `haven_binding::Field` and existing radius algorithm; no zero/default, related base declaration or foreign same-size value can authorize writes | All seven fresh-process metadata refusal modes passed: missing position/orthographic, foreign position/rotation, widened provider/source/output frame |
| O3 | Both endpoint orders, clamped curve weights, native inner stop and bounded extra distance | Source equations preserved | `haven-zoom-native` both-direction and inner/outer-stop cases passed |
| O4 | Native curve values must be finite; failed invocation must not become a valid zero | Checked `TryCall<Evaluate>`; unavailable/throwing curve preserves native output | Actual callback exception/flat-curve cases passed |
| O5 | Only position and far-plane change; finite final values and positive clip required | Existing binding setters after checked observations | Exact position/clip and untouched radius/constraint assertions passed |
| O6 | Live zero/native selection releases visual ownership and stops camera expansion | Reuse source selection, with existing frame-based visual cleanup | Camera zero-setting and production visual-policy fixtures passed; actual scene/frame cleanup trial remains pending |

## Design and behavior decisions

The target previously used default-return field accessors for geometry. A missing result
position could become `{0,0,0}`, and a missing orthographic field could become false.
The frozen installer rejected either schema before installing. Checked observations restore
that safety without adding a parallel binding engine or preflighting every field globally.
Follow-up source comparison found the ordinary typed helper still admitted related reference
declarations and same-size foreign value classes. `game/haven.h` now adds the frozen exact
declared-class requirement for Haven references and Unity Vector3/Quaternion geometry;
the existing orbit helper uses that same observation adapter. Shared library semantics and
the radius/control algorithms are unchanged. Negative fixtures keep valid current-client
object values in the widened fields, so only the changed declaration can cause refusal.
Curve exceptions are now contained by the normal typed binding and explicitly distinguished
from a successful numeric zero. No camera equation was rewritten.

Source defect discovered during comparison: the geometric predicate checks the source
quaternion norm and the pair's absolute dot product, but not the target norm. For example,
source `(0,0,0,1)` and target `(1,0,0,1)` pass those two tests despite the target being nonunit.
The working correction explicitly checks both norms with the existing tolerance and adds
that invalid-target case to the actual consumer fixture. This is a bounded correction to a
preserved source defect, not an accidental migration omission. The actual consumer's invalid-target case passed.

## Validation evidence and recovery

Source comparison and `git diff --check` completed. The implementing lane performed no builds
or test execution; the coordinator subsequently qualified the settled inputs recorded in
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/source-inputs.json`.
[FEATURE-COHORT-20261028-02](../FEATURE-COHORT-20261028-02.md) links the integrated evidence.

Coordinator `tests-3.log` reports 416/416 steps succeeded on Windows x64, including all ten
registered Haven fresh-process modes and `test-haven-visuals`. Explicit embedded and custom
background variants also passed 302/302 steps each (`tests-embedded.log` and
`tests-custom-build.log`). The image-option variants are integration regression evidence;
they do not extend Haven's native qualification. These checks compile and execute production
callbacks and typed reads/writes against a controlled runtime, without installed game hooks
or current-game geometry. Signed source checkpoint and product-build evidence belong to
separate coordinator records; these results do not qualify later source changes.

Migration coverage is source-mapped, not overall parity completion. Next: qualify the product
artifact on Windows and both supported Mac architectures, exact-client hook extent and player
zoom/pan behavior. Inspect both blend
directions, soft limits, native zero, invalid geometry and scene exit on the exact artifact.
