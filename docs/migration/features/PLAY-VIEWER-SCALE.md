# PLAY-VIEWER-SCALE — Object-viewer scale

Contract version: 1. Status: automated consumer qualification passed; native/platform acceptance pending.

## Inputs and scope

- Frozen source: `1baa55fedf1758b19f843cc59707b8901a0b4998` (`frozen/play-dev-20261026`).
- Target: existing `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`.
- Candidate inputs: `src/mod/features/ui_scale.cc` and `tests/ui_scale_bindings_cases.h`.
- Inventory ID: `PLAY-VIEWER-SCALE`; parent implementation/patch owner: `UiScale`.
- Scope: source-preserving consumer qualification and complete hook-signature
  lookup; no new viewer UI, lifetime framework, build or runtime operation.
- Scan date: 2026-10-28. Both Windows and macOS remain in scope.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Config/shortcut consumer | `mods/src/patches/parts/ui_scale.cc:CanvasController_Show`; native Settings control not assessed. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Config, shortcuts and Settings control | Same consumer plus `settings/upstream_settings.cc` viewer control. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Config, shortcuts and Settings control | Pinned complete workflow described below. |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus listed inputs | Config, shortcuts and Settings control | Core behavior already present; complete `Show` signature now qualified. |

## Source workflow and ownership

Frozen `mods/src/patches/parts/ui_scale.cc:CanvasController_Show` selects the exact
canvas name `ObjectViewerTemplate_Canvas`. For nonzero `ui_scale_viewer`, it sets
all three local-scale axes before forwarding the original `Show(Int32, Boolean)`.
Missing transform/local-scale observations still forward native Show. Other
canvases are untouched. Supporting frozen headers are `prime/CanvasController.h`,
`prime/Transform.h` and `prime/Vector3.h`.

Frozen `config.cc:AdjustUiViewerScale` applies a quarter of `ui_scale_step`, clamps
shortcuts to 0.1–2, leaves zero disabled and persists. Defaults are viewer scale
1.2 and shared step 0.05. `config.cc` parses the setting and shortcuts;
`defaultconfig.h` assigns SHIFT-PGUP/SHIFT-PGDOWN. `patches/parts/hotkeys.cc` routes
the setting shortcuts. `settings/upstream_settings.cc` provides the exact editor,
slider and reset with reopen-view timing. The feature retains no canvas or
transform between Show calls.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| O1 | `ui_scale.cc:CanvasController_Show` only changes `ObjectViewerTemplate_Canvas`. | Exact managed string comparison retained. | Matching/unrelated-name cases passed in coordinator `scale-native`. |
| O2 | Nonzero viewer multiplier applies identically to x/y/z before native Show. | Existing typed transform setter retained. | Native-original stub's pre-Show scale assertions passed. |
| O3 | Original receives unchanged entry point/instant arguments exactly once, including missing-transform paths. | Existing forwarding retained. | Actual callback argument/count assertions passed for active, zero, unrelated and missing-transform paths. |
| O4 | Zero leaves the game path alone; no automatic restoration of an already modified reused canvas is promised. | Existing zero branch preserved. | No-write case passed. Real reused-viewer/native-reset behavior remains pending. |
| O5 | `config.cc:AdjustUiViewerScale` uses quarter step, shortcut limits 0.1–2, zero disabled and persistence. | Existing shared settings arithmetic and save boundary reused. | Actual shortcut cases passed for quarter step, zero skip and `graphics.ui_scale_viewer` save identity; integrated settings/model checks also passed. |
| O6 | Settings edits/reset take effect on reopen; same canonical configuration feeds the hook. | Existing `config_settings.cc` control retained. | Integrated catalog/model checks passed; real Settings-to-reopened-viewer trial pending. |
| O7 | Frozen installer selects exactly Int32/Boolean Show parameters. | Replaced the target's parameter-name lookup with `Method<void(int32_t, bool)>`; native target and callback unchanged. | Consumer test executable compiled/passed; native mod build and current-client hook qualification remain pending. |

## Design and behavior decisions

The target writes the known requested Vector3 directly; it does not allocate/read
the old boxed local-scale value merely to replace all three components. Failed
transform acquisition still preserves original Show. No prior transform scale is
retained, and no unbind hook or automatic rollback is added. These match the
source's one-Show-at-a-time ownership and avoid adding viewer state.

The target Settings catalog already permits zero, matching the source's TOML and
shortcut native-mode semantics. The frozen exact editor used a positive minimum;
that pre-existing target control difference is recorded rather than changed here.

## Validation evidence

`scale-native` includes the production feature and calls the actual viewer hook
and shortcut mutation. Managed names, transforms and properties are controlled
API objects. It verifies callback ordering and routing, not Unity rendering,
real native Show installation, keyboard dispatch or a full Settings page.
The fixture models the inherited `Component::transform` property at its real
binding owner. Its earlier derived-only metadata defect was corrected before the
passing coordinator run; production behavior did not change for that correction.

[Cohort 20261028-02](../FEATURE-COHORT-20261028-02.md) records the coordinator's
Windows results under `D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/`:
`tests-3.log` passed 416/416 integrated build/run/install steps; the explicit
embedded-background variant (`tests-embedded.log`) and custom build-image override
variant (`tests-custom-build.log`) each passed 302/302 steps. These are build-graph
step totals, not independent feature-test counts. `source-inputs.json` records the
settled source/fixture hashes; see [PLAY-UI-SCALE](PLAY-UI-SCALE.md) for those identities.

The initial worker ran no executable; these subsequent coordinator results now
qualify the registered consumer cases. Native mod build, player acceptance and
Mac build/trial evidence remain pending and must identify their own artifacts.

## Disposition and recovery

- Migration coverage: existing core behavior retained and qualified by focused
  production-consumer cases; no replacement viewer implementation added.
- Known source limitation: zero does not explicitly undo a scale on a reused
  canvas; native Show/rebinding owns subsequent reset behavior.
- Pending trial: reopen ship and officer viewers after edits/reset; exercise
  repeated shows, zero mode, SHIFT-PGUP/SHIFT-PGDOWN and unrelated canvases.
  Run on Windows and each available Mac architecture with exact artifact evidence.
- Dependencies: installed `UiScale` transaction, native canvas/transform bindings
  and existing setting persistence. No shared binding change requested.
- Next action: coordinator builds the native mod and records focused Windows/Mac
  acceptance against the final source/artifact; automated consumer checks are complete.
