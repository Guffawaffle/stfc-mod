# PLAY-CAMERA-ZOOM — extended system zoom range and clipping

Contract v1, complete relevant source scan 2026-10-27. Frozen source:
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` base
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`. Final candidate-bound build and
native qualification are recorded by [cohort 03](../FEATURE-COHORT-20261027-03.md).

## Source and activation

Read the original `mods/src/patches/parts/zoom.cc` range, camera update, entry,
install and shared-consumer paths; `prime/NavigationZoom.h`, `NavigationPan.h`,
Camera/MathUtils bindings, configuration, camera settings and frame registration.
Target is `src/mod/features/camera.cc`, checked `game/navigation.h`, Unity camera
bindings and the existing shared terminal `EnsureNavigationZoomUpdates` owner.
`[patches].zoomhooks` is a restart gate. Configured `graphics.zoom` remains the
maximum in the source's zoom-unit convention. Settings, defaults and keys are
not redesigned here.

| Inspected branch | Exact revision | Consumer observation |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Extended zoom/range consumer in `mods/src/patches/parts/zoom.cc`; source presence only. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Same range/update/entry workflow present. |
| frozen play-dev | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete source assessed; legacy LOD/labels share this file. |
| refactor base | `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` | Existing `features/camera.cc`; no missing-feature assumption. |

## Behavioral obligations

| ID | Source operation | Target mapping / qualification |
| --- | --- | --- |
| Z1 | On system entry, far ratios are `0.55 * zoom / radius` and `zoom / radius`; clip is `3.75 * zoom`, black clear/background. | Existing formulas and native camera setters retained. Qualified finite input/field admission precedes writes. Actual callback fixture checks finite ratios, maximum and clip. |
| Z2 | Before/after native Update, extend system maximum when needed and maintain `_zoomtotal = maximum - minimum`. Other depths retain native range. | Existing shared Update owner and native original call retained; positive finite spans and current readback prevent unknown/replaced ranges becoming invented zero. |
| Z3 | System entry requests configured default on a later camera update. | One weak pending owner replaces the source's global Boolean. Only that camera at system depth consumes it; departure and newer reentrant entries cannot redirect a stale default. Process-lifetime storage avoids post-IL2CPP cleanup. |
| Z4 | Notify fleet-label/depth and Galaxy consumers through existing owners. | `FleetLabelsFollowDepth`, `FleetLabelsAfterZoom`, `galaxy_controls::AfterZoom` and the shared terminal hook remain authoritative. No extra frame/LOD registry. |
| Z5 | Preserve native original callbacks and degraded behavior when bindings are unavailable. | Failed strong custody falls back to original; invalid range skips mod geometry, preset writes and absolute commands. No guessed default measurements. |
| Z6 | Preserve platform hook differences. | Windows-only SetDepth hook remains Windows-only; shared Update still uses CheckedEntry, SetViewParameters stays the shared entry path. Exact-client native extent and macOS runtime are separate from build success. |

## Corrections and exclusions

Frozen and prior target normalization could divide by a zero/reversed/nonfinite
span or consume missing native float reads as zero. This is a preserved-source
correctness defect, not a claimed migration regression. Checked native observations
and shared qualified conversion helpers now refuse those cases while retaining
the exact float formulas for valid data. Finite values outside the nominal range
are not silently clamped. Missing world coordinates do not become `(0,0,0)`.

The deferred-default owner correction likewise fixes a source lifecycle defect:
multiple camera instances must not consume each other's global flag. Only one
weak owner and a local entry-generation fence survive callbacks. There is no task
framework, cache/pool or retained camera history. Temporary strong custody spans
managed operations, and unknown native results are not represented as success.

The existing target deliberately applies an entry default even while a text field
is focused; that behavior is retained and tested separately from keyboard input.
Planet/backdrop scale, pan/glide, station/interior/Haven camera and excluded system
orbit remain outside this scan, even though some installers and source bodies
share this file. Their bodies, registrations and ownership are preserved.

## Verification layers and native procedure

`tests/camera_bindings_cases.h` includes actual camera production callbacks with
controlled native object fields/methods. It covers finite/invalid/unavailable
ranges, native original calls, source-bound default ownership and root release.
Its scroll-query facade substitutes native UI ownership only; it is not a Unity
rendering, raycast, frame-time or gameplay qualification. Final executed results,
exact candidate and binaries belong to the cohort receipt.

In-game: enter several systems, zoom to both limits, change system while a default
is pending, return to Galaxy and check native tier/label behavior. Check black
clear/clipping without changing scenery settings. Then inspect text-focus default
behavior and reload an existing zoom setting. Windows launch and user acceptance
remain separate; macOS requires both architecture builds and Mcohall's native trial.
