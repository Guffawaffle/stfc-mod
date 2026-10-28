# PLAY-GALAXY-DETAIL — independent major/minor label detail

Contract v1, 2026-10-27. Frozen source:
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` on `play-dev-ripper`.
This reassesses an existing implementation, not a presumed missing feature.
Final candidate/build/runtime qualification belongs to the MAP8 cohort record.

## Source scan and presence

Read complete frozen `mods/src/patches/parts/galaxy_labels.{cc,h}`,
`galaxy_policy.h`, `galaxy_policy_checks.cc`, the housing companion, shared zoom/LOD
call sites, `mods/src/settings/galaxy_labels.{cc,h}`, configuration and relevant
tracked-layout/native UI helpers. Current owners are `features/galaxy.cc`,
`game/galaxy.h`, `galaxy_policy.h`, the existing Settings catalog, and shared
camera/LOD dispatch. No second label model or camera-hook owner is introduced.

| Inspected branch | Exact revision | Named capability |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Absent: no major/minor profile keys or corresponding consumer in the inspected patch/settings source. Native labels still exist. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Present: Galaxy label/profile/settings sources. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Present; authoritative source for this pass. |
| refactor starting base | `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` | Present: `GalaxyEffective`, `AfterZoom`, `LodLevel`, filter/handler adapters and Settings consumers. |

Both profiles default Native with a 0.5 threshold. Existing `[graphics]`
`galaxy_label_major_detail`, `galaxy_label_minor_detail` and matching `_threshold`
keys retain Native/Always/Threshold values and finite range validation. Current
`settings/config_settings.cc` already registers both choices and threshold
controls. Earlier M8 statements that this page was missing are historical.

## Behavioral obligations

| ID | Source behavior | Retained/adapted current operation and evidence |
| --- | --- | --- |
| D1 | Major and minor profiles are independent. Identical decisions avoid per-star managed minor classification. Missing classification retains native callback behavior. | `GalaxyEffective` reuses the existing policy and actual `World::Minor` by-ref GalaxyNode binding. Production fixture covers independent choices, equal-decision optimization, native failure and custody failure. |
| D2 | Always expands without a zoom measurement. Threshold requires known normalized zoom; its endpoint is inclusive, threshold zero is compact, threshold one expands. | Existing policy retained. Actual consumer fixture covers the inclusive midpoint, within-tier changes, Always and unknown zoom. Pure fixture retains endpoint coverage. |
| D3 | Normalized zoom changes inside one native LOD tier must refresh labels/culling. Returning Native clears an old forced target. | `AfterZoom` compares profiles, camera identity, native tier and effective decisions; `LodLevel` stays under the shared owner. Checked depth/normalized/tier observations now prevent a failed getter from becoming a measured zero. Native scene/LOD behavior remains a separate trial. |
| D4 | Expanding minor labels may relax only the native Far/minor culling exclusion; restore `_currentLevel` after the original rebuild. | Existing filter adapter and scoped restoration preserved; no native eligibility/range inputs invented. Real culling is not exercised by the fake UI fixture. |
| D5 | Pooled/replaced/collected owners retain cleanup custody; allocation failure is not object disappearance. | Corrected raw-record replacement and refresh pruning. Pending restoration survives failed strong handles and managed failure. Expired weak owners can complete cleanup; reentrant replacement records win. Actual lifecycle fixture covers these paths. |
| D6 | One terminal shared camera-update and LOD-update owner serves all consumers. | `camera.cc::EnsureNavigationZoomUpdates` and `fleet_labels.cc::EnsureNavigationLodUpdates` remain authoritative. Galaxy adds no frame or competing LOD detour. |
| D7 | The frozen Mac installer rejects aliased callback targets before installation. | Restored a private distinct-entry preflight before staging the eight callbacks. Applied on all platforms because this family mixes receiver types and arities; chaining an alias would violate those callback contracts. The actual helper fixture tests aliases; it does not qualify the complete native installer. |

## Architecture and qualification

The source used reflection, fixed offsets, raw handles and explicit layout
journals. The target already adapts those operations through typed bindings,
checked entry points and a transactional GalaxyLabels hook set. This pass keeps
that adaptation and repairs narrow availability/cleanup omissions. It does not
restore fixed-offset access or independent SPUD installation.

`tests/galaxy_presentation_bindings_cases.h` compiles the actual production
consumer in the existing controlled IL2CPP runtime. Its profiles mode substitutes
the native world/minor getter and copied observations; its lifecycle mode
substitutes layout/marker bodies. These tests do not run Unity layout, a real
Galaxy scene, native culling or in-game Settings controls. Existing pure policy,
Settings/config and layout/custody fixtures remain separate evidence layers.

Exact-client checks: independently change major/minor choices and thresholds;
cross thresholds without changing native tiers; replace/return to Galaxy camera;
disable presentation with pooled/inactive labels; restore Native and inspect
names/culling. Missing native bindings must retain native behavior, never guessed
major/minor classification. Windows and macOS evidence remain distinct; Mac
requires architecture builds and Mcohall's native trial.

Implementation/automated results and any remaining installer qualification limits
are recorded in the cohort. No full native parity, current Mac fit, new UI
framework, AAI behavior or unrelated camera feature is claimed by this contract.
