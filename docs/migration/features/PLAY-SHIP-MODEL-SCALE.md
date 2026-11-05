# PLAY-SHIP-MODEL-SCALE — Existing and newly loaded ship scale

Contract version: 1. Status: automated consumer qualification passed; native/platform acceptance pending.

## Inputs and scope

- Source: `frozen/play-dev-20261026` = `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target: existing `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`.
- Candidate inputs: `src/mod/features/ui_scale.cc` and `tests/ui_scale_bindings_cases.h`.
- Inventory ID: `PLAY-SHIP-MODEL-SCALE`; patch/feature owner: `UiScale`;
  borrowed instance collection owner: existing object tracking.
- Authorized: correct proven observation/recovery gaps and exercise actual
  consumers. No shared binding, tracking, registration, build or runtime changes.
- Scan date: 2026-10-28. Windows and macOS remain in scope.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Config/shortcut consumer | `mods/src/patches/parts/ui_scale.cc` has native asset and loaded-model paths; native Settings control not assessed. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Config, shortcuts and Settings control | Same paths; `settings/upstream_settings.cc:Number` refreshes loaded models. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Config, shortcuts and Settings control | Complete pinned workflow below. |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus listed inputs | Config, shortcuts and Settings control | Both core paths present; checked scale observation and valid-value recovery corrected in this cohort. |

## Source workflow and ownership

Frozen `mods/src/patches/parts/ui_scale.cc` has two consumers. The
`SystemViewShip_TryGetShipAssetBundleResource_Hook` calls native first, preserves
the native bool/resource outputs, and multiplies a successful nonnull scale output
by positive `ui_scale_ship`; nonpositive settings act as multiplier one.
`ApplyUiShipScaleToLoadedShips` enumerates tracked `FleetMeshSelector` instances,
gets each loaded GameObject/transform/local scale, skips unavailable observations
and multiplies all axes by `new_multiplier / old_multiplier` when both are positive.

Frozen `config.cc:AdjustUiShipScale` applies the full shared step, clamps shortcut
values to 0.1–20, rescales loaded models and persists. Default ship scale is one;
step is 0.05; shortcuts are CTRL-PGUP/CTRL-PGDOWN. The exact Settings consumer in
`settings/upstream_settings.cc:Number` accepts valid positive new values and
calls loaded-model rescaling, including when the previous configured value is
zero/negative. Reset follows that same setter workflow. File parsing changes
configuration; it is not a separate immediate loaded-model refresh callback.

Supporting frozen headers: `prime/FleetMeshSelector.h`, `prime/Transform.h`,
`prime/Vector3.h`; `patches/parts/object_tracker.cc` installs selector tracking.
The target already tracks `FleetMeshSelector` in `features/tracked_objects.cc`.
`unity::Instances` owns a snapshot of borrowed pointers for the current pass;
tracking owns scene-object removal. This feature retains no model, transform or
restoration state after the operation, and does not own/destroy Unity resources.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| O1 | Asset hook calls original once and only multiplies successful nonnull scale outputs. | Existing typed callback retained. | `scale-native` passed original-scale, bool, receiver/resource forwarding, failure and null-scale assertions. |
| O2 | Positive multiplier applies to new assets; nonpositive acts as one. | Existing positive test retained. | Actual callback cases passed for scale 4 × 3 and zero/native behavior. |
| O3 | `ApplyUiShipScaleToLoadedShips` applies new/old ratio to all axes and composes repeated changes. | Existing ratio algorithm retained. | Actual multi-object, repeated-change and reset-to-one cases passed. |
| O4 | Failed/null local-scale observation skips that loaded model; successful zero remains an observation. | Replaced defaulting `localScale()` read with `TryCall<get_localScale, Vector3>`. Failure no longer becomes an authorizing zero-vector write. | Throwing-getter/known-zero assertions passed in `scale-native`; absent getter metadata passed separately in `scale-missing-local`. |
| O5 | Missing loaded GameObject/transform is skipped without stopping other models. | Existing typed null handling retained. | Both missing paths and the separate failed-getter case passed in the multi-entry fixture. |
| O6 | Valid positive Settings values can recover zero/negative previous configuration. | `SetUiShipScale` validates the new value, then updates canonical configuration; invalid old ratio is skipped rather than blocking the edit. | Previous zero/negative/nonfinite, invalid-new-value and overflowed-ratio cases passed with checked model-write counts. |
| O7 | Shortcut uses full step, independent 0.1–20 limit and `graphics.ui_scale_ship` persistence. | Existing `gameplay_settings::AdjustScale` and save boundary reused. | Actual shortcut upper-clamp/submitted-value assertions and integrated settings/model checks passed. |
| O8 | No source ownership/cleanup state outlives each pass; tracking removes released objects. | No extra cache, event owner or hook added. | No-retained-root assertion passed; real tracker/native destruction qualification remains separate. |

## Design and behavior decisions

The source explicitly skips a null boxed local-scale getter result. The target's
former defaulting Vector3 read lost that distinction and could collapse a model
after a getter failure. The correction uses the existing checked call mechanism;
it adds no raw runtime API or binding engine.

The former target setter rejected valid edits solely because the old value was
nonpositive or nonfinite. The frozen Settings setter only validates the requested
new value, so that restriction prevented recovery. Loaded-model rescaling still
requires a usable old ratio; the new asset consumer immediately sees the accepted
canonical setting. No guessed native-model baseline is introduced.

Finite operands and a finite positive ratio are now required for loaded-model
rescaling. This deliberately avoids the source's unsafe NaN/infinite/overflowed
transform write; it does not round or clamp saved settings globally. An invalid
previous multiplier remains insufficient evidence to normalize existing models.

## Validation evidence

The new fragment includes the production feature and exercises its callbacks,
setting setter and shortcut exports. Only DPI/cursor APIs and already-established
tracking/save boundaries are synthetic. This is a production final-consumer
fixture; it does not install native detours, run Unity loading, qualify tracker
liveness or verify the real Settings editor. `scale-native` covers active and
failure paths; `scale-missing-local` runs with absent getter metadata in a fresh
process so resolution caches cannot hide the unavailable observation.

The coordinator registered both modes and qualified the settled inputs in
[cohort 20261028-02](../FEATURE-COHORT-20261028-02.md). Evidence under
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/` includes:

- `tests-3.log`: 416/416 integrated Windows build/run/install steps succeeded with
  Debug assertions, including both actual scaling consumer modes.
- `tests-embedded.log` and `tests-custom-build.log`: explicit embedded-background
  and custom build-image override variants each passed 302/302 steps.
- `source-inputs.json`: exact tested source/fixture manifest; scaling identities
  are also recorded in [PLAY-UI-SCALE](PLAY-UI-SCALE.md).

The totals count build-graph steps, not independent tests. The initial worker's
inspection-only/no-execution attribution remains historical; later coordinator
execution now qualifies these cases. Native mod build, player acceptance and Mac
build/trial qualification remain pending and require separate artifact evidence.

## Disposition and recovery

- Migration coverage: both consumers were already present; proven observation and
  canonical-setting recovery gaps were corrected without replacing their logic.
- Known behavior: old nonpositive/invalid multipliers cannot establish a ratio
  for existing models; accepting a new setting does not guess their original scale.
- Pending trial: mix ships loaded before/after edits; apply repeated CTRL-PGUP/
  CTRL-PGDOWN changes and Settings reset; verify proportions and new asset scale.
  Exercise destroyed/replaced selectors through real scene changes. Use exact
  Windows/Mac artifacts and available architectures, recording each result separately.
- Dependencies: existing `UiScale` hooks, selector tracking, transform bindings
  and setting persistence. No shared binding change requested.
- Next checkpoint: coordinator native mod build and focused Windows/Mac player
  acceptance. The registered consumer and integrated settings/model checks have passed.
