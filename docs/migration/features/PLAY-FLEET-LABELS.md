# PLAY-FLEET-LABELS — player and non-player system label detail

Contract version: 1. Status: source-preserving implementation; current-candidate
execution and native qualification pending. Source scan: 2026-10-28.

## Inputs and scope

- Source: Downstream `frozen/play-dev-20261026`, exactly
  `1baa55fedf1758b19f843cc59707b8901a0b4998`; read through Git.
- Target: existing `play-dev-ripper`, assessment base
  `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0`.
- Inventory: `PLAY-FLEET-LABELS`; shared widget owner with
  `PLAY-FLEET-BADGES`, shared camera with `PLAY-CAMERA-ZOOM`, shared LOD hook
  with existing `PLAY-GALAXY-DETAIL`.
- Scope: source-preserving label profiles, refresh, fallback and pooled lifecycle;
  no new camera, Galaxy feature, input handler, UI framework or persistence API.
- Delegated implementation owns badge/label sources, their two contracts and the
  actual-consumer fixture fragment. Coordinator owns shared fixture/build
  registration, inventory, all execution and native qualification.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Consumer/config present | `mods/src/patches/parts/zoom.cc`, `config.cc`, `config.h`, `defaultconfig.h`; native Settings control not assessed here. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer and controls present | `parts/zoom.cc` and `settings/fleet_labels.cc`, plus registered Settings/configuration. |
| play-dev / frozen source | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Consumer and controls present | Complete label-policy, refresh/camera, widget lifecycle and installation paths in `parts/zoom.cc`; both Settings adapters, supporting bindings and defaults assessed. |
| refactor | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | Consumer and controls present | `src/mod/features/fleet_labels.cc`, camera/lifecycle/LOD services and `settings/config_settings.cc`; not an absent port. |

Both platforms share behavior. `[patches].zoomhooks` is the restart installation
gate. The four live keys are `graphics.zoom_label_player_detail`,
`zoom_label_non_player_detail`, `zoom_label_player_threshold` and
`zoom_label_non_player_threshold`. Both profiles default to Native with threshold
0.5. Detail aliases remain: always/expanded, never/compact, custom/threshold and
auto/none/off/empty/native. Thresholds must be finite and within 0..1. The Fleet
Labels catalog retains choice, exact-number and slider controls and registered
preserving saves. Label rendering itself performs no file I/O.

## Source workflow and ownership

Frozen `mods/src/patches/parts/zoom.cc` supplies the full workflow: installation
metadata/extent checks; weak widget/LOD tracking; operation-local strong roots;
system-camera identity/state; fleet-kind profile selection; threshold transitions;
`UpdateLOD` adaptation; Native restoration and enable/bind/disable/release cleanup.
`prime/NavigationFleetWidget.h`, `NavigationLOD.h`, `NavigationZoom.h`,
`FleetDeployedData.h` and `ZoomLevels.h` supply bindings. `settings/fleet_labels.*`
and the canonical `settings/upstream_settings.cc` catalog supply controls,
availability and save/refresh callbacks. Config readers/defaults supply aliases,
range validation and effective values.

The target reuses one `EnsureNavigationZoomUpdates`, one
`EnsureNavigationFleetLifecycle` and one `EnsureNavigationLodUpdates`. Shared
camera hooks call `FleetLabelsFollowDepth` before transitions and
`FleetLabelsAfterZoom` after native update. `FleetLabels` records weak widget and
LOD identities plus generations. A batch snapshots keys/generations, roots each
live owner locally, and retires if camera/profile state changes. Settings refresh
queues owner work; profile comparison also notices TOML reload changes. Native
profiles already set target on both sides of the original to undo old overrides.

These owners are already implemented and registered. The source study must not
replace them with new hooks or a second tracked-object system. Collections/maps
must not retain live iterators across managed callbacks; a rooted member alone
does not make its old binding current.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| FL1 | `InstallZoomHooks` validates lifecycle/LOD/camera metadata, fields, signature/alias constraints and Mac native entry fit before claiming label controls. | Existing transactional shared services retained; camera depth/level/normalized values now use existing checked getters before authorizing detail. Missing values cannot fabricate zero. | Actual-consumer missing-level/missing-normalized cases implemented. Installer/real hook-fit acceptance remains separate. |
| FL2 | `FleetLabelProfileFor` maps Player to player profile and other known kinds to non-player. | Existing checked FleetType read retained. Current failure returns native instead of frozen fallback-to-non-player; record this conservative unavailable-data decision. Legitimate Nonexistent/zero still maps non-player. | All declared kinds, valid zero, unavailable/throwing kind and replacement during query using actual profile consumer. Pending. |
| FL3 | Native passes native level; Expanded preserves Near and raises other levels to Middle; Compact uses Far. `FleetLabelZoomLevel`. | Existing `LevelFor` policy reused. Native profiles restore target, including when last override is disabled. | Actual LOD callback asserts original arguments/count and target before/after original for all modes/levels. Pending. |
| FL4 | Threshold uses normalized zoom <= threshold; zero never expands and one always expands. Unknown system camera uses native. | Retain boundary policy with checked finite/range-qualified observations. Failed normalized acquisition reapplies native LOD; failed depth/level/custody retires threshold authority. | Real callbacks cover endpoints/equality/crossing, invalid values, thrown/missing getter, missing/invalid level, valid zero and failed camera root. Implemented; execution pending. |
| FL5 | `BeginFleetLabelSystemState` resets on camera change; leaving active system depth retires its state. | Retain camera revisions, additionally holding weak camera identity. Reentrant camera/profile change retires an older normalized result. | Nested camera replacement and system exit tests use actual callbacks. Implemented; execution pending. |
| FL6 | Enable-before-context never forces LOD; bound widgets track, release/disable restore before original and remove ownership. | Existing shared lifecycle and local roots retained. Exact widget/LOD/generation must survive each query before mutation; dead/unavailable entries remain native. | Actual enable/bind/release callbacks, pooled replacement, changed LOD and failed weak/strong roots. Pending. |
| FL7 | `ApplyAllFleetLabelDetails` snapshots tracked objects because native LOD callbacks can rebind/release other entries. | Existing generation snapshot and profile/camera retirement are retained; do not iterate the live map across native calls. | Original callback releases current/other row, adds/rebinds rows, changes preferences or camera; verify no old post-original write. Pending. |
| FL8 | Live Settings refresh invalidates threshold cache; startup/reload profiles preserve exact keys/defaults/aliases. | Existing refresh queue/profile comparison and registered writer reused. Delayed owner refresh is intentional; no managed work from the writer thread. | Existing catalog fixture observes refresh requests; actual next-camera update and reload-triggered consumer refresh need production fixture. Pending. |
| FL9 | LODs outside tracked fleet ownership retain existing Galaxy/native path. Frozen shared hook sets changed Galaxy target before and after original. | Reuse `galaxy_controls::LodLevel`, restoring changed targets after original only while no replacement fleet owns that LOD. This corrects the adjacent shared-hook difference without a new Galaxy feature. | Existing Galaxy actual-consumer regression suite remains coordinator-owned; this fragment does not claim new Galaxy rendering qualification. |

## Design and behavior decisions

Unavailability is distinct from a valid zero. The target already declines to
classify a fleet when FleetType acquisition fails; retain native detail instead
of restoring the source's fallback-to-non-player assumption. Likewise, absent
or invalid camera observations cannot authorize a threshold override. Existing
checked observation helpers are sufficient; no runtime binding framework is needed.

The frozen source validates required camera metadata before declaring capability.
The assessment-base target could install then read unavailable camera members as
default values. The correction uses existing checked methods; valid normalized
zero remains supported and missing/invalid observations retain native behavior.

Existing queued Settings refresh plus profile comparison is a deliberate
adaptation: it avoids managed work in setting writers and catches direct TOML
reloads. Native target restoration already exists and should be tested instead
of ported a second time. Generations prevent older callbacks from overwriting a
new pooled owner. Shared Galaxy behavior belongs to its existing owner; any
correction must stay inside the shared hook boundary.

## Validation evidence

`tests/config_settings_test.cc` currently exercises both real catalog profiles:
changed detail requests refresh; unchanged does not; returning to Native still
requests restoration; exact finite threshold requests refresh; NaN is rejected.
Its gameplay refresh sink is a substitute, so it does not execute FleetLabels.
The camera/Galaxy fixture's `tests/map_fixture_stubs.h` explicitly substitutes
`FleetLabelsFollowDepth`/`FleetLabelsAfterZoom`; those passing tests cannot qualify
this consumer's camera state, threshold, weak ownership or native fallback.

`tests/fleet_presentation_bindings_cases.h` includes the actual label/badge sources,
using controlled managed metadata/native method bodies. Its native, missing-index,
missing-zoom and missing-level modes cover the named production-consumer paths;
they do not install hooks into STFC or render its UI. The delegated implementation
executes no tests/builds/runtime actions. All execution, exact-candidate builds
and native acceptance are coordinator-owned and retain their own artifact identity.
The older own/foreign ship-badge player acceptance belongs to the separate badge
workflow, not a claim that every label profile or threshold was tried.
Windows behavior does not qualify Mac hook extent or native Mac behavior.

## Disposition and recovery

- Migration coverage: source counterpart/settings already present; full selected
  workflow assessed at the stated base. Concrete observation/shared-hook deltas
  and actual production-consumer tests are implemented; execution pending.
- Correctness: current labels are not absent. The native/control integration,
  direct reload, lifecycle and unknown-observation cases retain separate evidence.
- Native procedure: apply Native/Expanded/Compact/Threshold independently to
  player and non-player fleets; cross exact threshold boundaries; switch final
  override back to Native; reload configuration; leave/reenter system and cause
  ordinary pooled rebinds. Check Galaxy labels remain correct separately.
- Next action: coordinator runs focused actual consumer tests and binds
  native/build/platform results to the final cohort.
  No new capability was discovered and no overall-parity claim is made.

## Coordinator qualification checkpoint — 2026-10-28

Implementation `d61b383d9e3b83d5c8ea4852c8c495e35d1a9084` passed the combined Windows assertion suite
(399/399 build/run/install steps), native AAI-enabled build (41/41), and
main-game startup with all 73 groups and frame delivery ready. See
[the cohort receipt](../FEATURE-COHORT-20261028-01.md#native-qualification) for
exact artifact, store differences and diagnostic limits. This supersedes earlier
execution-pending notes above only for the exercised fixture and startup layers.
Current player workflow and macOS acceptance remain pending.
