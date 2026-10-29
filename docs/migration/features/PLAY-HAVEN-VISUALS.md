# PLAY-HAVEN-VISUALS — reversible road depth and water visibility

Contract version: 1. Status: implementation assessment; production-policy validation passed, native qualification pending.

## Inputs and scope

Source: `frozen/play-dev-20261026` at `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target: `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`,
existing `features/haven.cc`, `haven_visuals_policy.h` and visual fixtures.
Camera checked-observation changes belong to [PLAY-HAVEN-ZOOM](PLAY-HAVEN-ZOOM.md).

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | road gameplay consumer; water absent | `haven_road_depth.cc` no-argument road bias lacks the source reversible/water workflow |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | config/control catalog and consumer | Reversible road/water exist, with separate discovery implementations |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | config/control catalog and consumer | Shared `haven_visuals_cache.h` discovery, separate road/water ownership |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` | config/control catalog and consumer | Shared `HavenVisualOwner`, policy and checked Unity adapters; frame cleanup |

Scanned 2026-10-28. Shared desktop implementation. `graphics.haven_zoom` expansion
activates road bias at every native zoom level; `graphics.hide_haven_water=false`
independently selects native water visibility. Both depend on the installed Haven camera
feature. Live control persistence belongs to `settings/config_settings.cc` and its writer.

## Source workflow and ownership

Complete pinned files: `mods/src/patches/parts/haven_zoom.cc`,
`parts/haven_road_depth.cc`, `parts/haven_water.cc`, `patches/haven_visuals_cache.h`
and `tests/haven_visuals_cache_test.cc`. Also inspected settings/default/config/example
registrations and `settings/camera_settings.{cc,h}`. The standalone water factory has no
pinned callsite; the general settings catalog has its own registration.

A qualified Haven camera callback requests visual updates before curve evaluation.
Application cadence is one second; owner lookup/classification refresh is five seconds.
Discovery is limited to `StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder`.
Roads use seven `mat_PB_3101101_asphalt_` through `mat_PB_3101701_asphalt_` prefixes
and `Custom/LitMSAE`. Water uses `mat_PB_water` and `Custom/WaterLite`.
Cached scene membership and restoration records are weak; native acquisitions are rooted locally.

## Behavioral obligations

| ID | Pinned source behavior | Target mapping | Verification |
| --- | --- | --- | --- |
| O1 | Share scoped owner/classification; back off missing owners, refresh streaming descendants | Existing shared cache and actual native adapter retained | `tests/haven_visuals_test.cc` discovery/backoff/replacement cases passed in coordinator tests-3 |
| O2 | Bias both offsets toward `min(native,-1)`; keep stronger native bias; retain original values | Existing `OwnedVisuals/RoadRestoration` retained | Actual production-policy road/finite/pair-observation fixtures passed |
| O3 | Restore dropped/disabled road assets only while still owning the current intervention | Independent property restoration, actual weak identity plus signed instance ID | Competing edit, same-ID replacement, partial restoration and unknown-read cases passed |
| O4 | Hide only classified renderers after recording enabled state; release uses current-or-original enabled state | Existing `WaterRestoration` retained | Known true/false, external enable and mutation failure/readback cases passed |
| O5 | Water independent of expansion or curve shape; leaving qualified scene must not strand changes | Existing qualification call before curves and existing frame cleanup | Policy fixture plus camera source inspection; actual scene/frame delivery trial pending |
| O6 | No persistent strong scene roots; bounded ownership; avoid shutdown GC calls | Weak process-lifetime owner, temporary strong access and admission limits retained | Weak expiry/failed custody/limit/reentry fixtures passed; native teardown remains pending |

## Design and behavior decisions

Existing target policy deliberately strengthens the source: all-or-nothing discovery
refuses oversized/failed snapshots rather than publishing an incomplete prefix; rechecking
the path detects a replacement owner while the old owner remains alive. Discovery errors
invalidate membership without discarding separately retained restoration obligations.
The complete discovery/apply operation excludes synchronous reentry. Frame cleanup drains
old ownership when the camera is no longer qualified, without global idle searches.
No new hook, discovery framework or feature setting was added in this cohort.

The inherited 256-road/eight-water ownership bounds and 4096-component/16-material
admission bounds are intervention-work limits, not a claim to cover arbitrarily large scenes.
They are existing explicit visual ownership policy, distinct from the removed history row cap.

## Validation evidence and recovery

Complete source/policy/fixture inspection completed. The existing portable fixture executes
production discovery/ownership/restoration algorithms with synthetic native observations,
not actual scene traversal or GPU rendering. Prior M8 receipts remain historical and do not
qualify current inputs. The implementing lane ran no build, native test or game cycle;
the coordinator subsequently ran current-input qualification recorded by
[FEATURE-COHORT-20261028-02](../FEATURE-COHORT-20261028-02.md).

Settled source inputs are recorded in
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/source-inputs.json`.
Coordinator `tests-3.log` reports 416/416 steps succeeded on Windows x64, including
`test-haven-visuals` and all ten Haven consumer modes. Explicit embedded/custom background
integration variants each passed 302/302 steps (`tests-embedded.log`, `tests-custom-build.log`).
Those variants do not add rendered-visual coverage. Product build, exact-client hook fit,
native/player behavior and both Mac architectures remain separate qualification.

Source coverage and current policy checks are complete; native visual parity remains
unverified. Next: toggle water independently at native/expanded zoom, verify road
depth across zoom and idle Haven without flicker, leave/reenter, replace/stream scene assets,
reload settings, and observe pending restoration/callback cost on exact Windows and Mac artifacts.
