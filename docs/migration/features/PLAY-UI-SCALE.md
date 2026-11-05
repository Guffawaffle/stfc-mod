# PLAY-UI-SCALE — Interface scale and DPI

Contract version: 1. Status: automated consumer qualification passed; native/platform acceptance pending.

## Inputs and scope

- Source: `frozen/play-dev-20261026`, verified as `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target: existing `play-dev-ripper` worktree, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`.
- Assessed inputs: `src/mod/features/ui_scale.cc`, its typed dependencies, and
  `tests/ui_scale_bindings_cases.h`; associated configuration and controls were inspected.
- Inventory ID: `PLAY-UI-SCALE`; siblings: `PLAY-VIEWER-SCALE`, `PLAY-SHIP-MODEL-SCALE`.
- Authorized: bounded source-preservation corrections, consumer tests and this contract.
  Shared bindings, registration, inventory, builds and runtime work remain coordinator-owned.
- Scan date: 2026-10-28. Windows and macOS behavior remain in scope.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Config/shortcut consumer | `mods/src/patches/parts/ui_scale.cc`, `mods/src/config.cc`, `mods/src/defaultconfig.h`; native Settings control not assessed. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Config, shortcuts and Settings control | Same consumer; `mods/src/settings/upstream_settings.cc` publishes interface scale and shortcut step. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Config, shortcuts and Settings control | Complete pinned source workflow below. |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus listed working inputs | Config, shortcuts and Settings control | Existing typed consumer retained; this cohort qualifies it without changing its formula. |

## Source workflow and ownership

The `[patches] uiscalehooks` switch defaults to true. The frozen installer in
`mods/src/patches/parts/ui_scale.cc:InstallUiScaleHooks` hooks the interface,
viewer and ship consumers. `ScreenManager_UpdateCanvasRootScaleFactor_Hook`
calls native first, restores the Windows cursor when `allow_cursor` is false,
then writes the root `CanvasScaler.scaleFactor` property when `ui_scale` is nonzero.

Frozen `mods/src/config.cc:AdjustUiScale` changes the configured scale with
`ui_scale_step`, clamps shortcut values to 0.1–2, persists the setting and refreshes
DPI. The default is 0.6 with step 0.05. `ui_scale_adjust` remains a parser alias.
`mods/src/patches/parts/hotkeys.cc` handles PGUP/PGDOWN through the setting-shortcut
chain. `mods/src/settings/upstream_settings.cc` publishes a live exact-number and
slider control; zero is a valid native-scaling choice.

Supporting frozen headers are `prime/ScreenManager.h`, `prime/CanvasScaler.h`,
`prime/CanvasController.h` and `prime/Transform.h`. `config.cc:GetDPI` caches the
Windows monitor's physical/logical width ratio; macOS returns one. The frozen
startup calls `RefreshDPI`. The target's `WindowDpiScale` computes lazily and uses
the game's own window, preserving monitor identity while the game is inactive.
The feature retains no canvas ownership between calls.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| O1 | `ui_scale.cc:ScreenManager_UpdateCanvasRootScaleFactor_Hook` calls original once before overrides. | Unchanged typed callback; native field access through `ScreenManager::canvasRootScaler`. | `scale-native` passed in coordinator `tests-3`, verifying native update precedes factor write. |
| O2 | Zero preserves native canvas scaling. | Existing early return retained; cursor behavior still occurs after original. | `scale-native` passed its no-scaler-write assertion for zero. |
| O3 | Active factor is `ui_scale * DPI`, NaN becomes one, result clamps to 0.1–5. | Existing formula retained. | Actual callback cases passed for DPI 1.5, upper/lower clamp and NaN fallback; real-display qualification pending. |
| O4 | `config.cc:GetDPI/RefreshDPI` follows monitor changes; macOS uses one. | Existing `WindowDpiScale` adapter; no platform implementation changed. | Source inspection; Windows monitor/background movement and Mac trial remain pending. |
| O5 | Windows `allow_cursor=false` restores arrow cursor after native rescaling. | Existing narrow Windows block retained. | Mocked cursor-invocation case passed; OS cursor trial pending. |
| O6 | `config.cc:AdjustUiScale` skips zero, steps by the full configured amount, clamps 0.1–2 and persists. | Existing `gameplay_settings::AdjustScale` and runtime save boundary reused. | Actual shortcut cases passed for zero, upper limit, save key/value and DPI refresh. Integrated model/settings checks also passed. |
| O7 | Settings edits/reset mutate the same configured value; next native scaling update consumes it. | Existing `config_settings.cc` catalog and typed configuration reused. | Integrated catalog/model checks passed; native Settings rendering is not qualified by these tests. |
| O8 | `adjust_scale_res` is dormant, not a reachable setting. | No option restored. | Full frozen grep finds declaration/read only; static `Config` initialization leaves false and no parser/writer changes it. |

## Design and behavior decisions

The old screen-height branch is deliberately dormant. Adding a resolution option
would introduce behavior beyond the frozen source's reachable workflow. The
target's property setter matches the old `scaleFactor` property, rather than the
distinct immediate `CanvasScaler.SetScaleFactor` method.

All three typed anchors remain in one `UiScale` transaction. This replaces the
source's independently attempted detours with the existing host's installation
contract. No new native entry or frame owner is introduced. The viewer `Show`
lookup now uses its complete existing `void(Int32, Boolean)` signature.

## Validation evidence

Source comparison is complete for the pinned implementations, configuration,
defaults, controls, hotkey call sites and headers. `tests/ui_scale_bindings_cases.h`
includes the production feature and drives its callbacks. Tracking snapshots and
the setting-writer boundary use existing controlled-runtime sinks; DPI/cursor
calls are synthetic. The fixture is a production consumer test, not a display,
tracker lifecycle, installation or native Settings host test.

The coordinator registered `scale-native` and `scale-missing-local` in the existing
controlled-runtime executable. [Cohort 20261028-02](../FEATURE-COHORT-20261028-02.md)
records the integration. Evidence is under
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/`:

- `tests-3.log`: all 416/416 Windows integrated build/run/install steps succeeded,
  including both scaling modes and existing Settings/model regressions, with Debug assertions.
- `tests-embedded.log`: explicit embedded-background (`use_original_bg=false`)
  variant passed 302/302 steps.
- `tests-custom-build.log`: custom build-image override variant passed 302/302 steps.
- `source-inputs.json`: tested source manifest, including scaling source SHA256
  `eced13031eafd47b72745cbdb9cf0e82743ef75ccbb474517f3e9461dc3ced47` and fixture SHA256
  `ba3fb9f7344abb5c08147a20934802b4cbae614928d574eb9600fa5636f43bb7`.

These totals count build-graph steps, not independent feature tests. The initial
worker performed source inspection only; the later coordinator executions qualify
the listed inputs. Native mod build, game/player acceptance and Mac builds/trials
remain pending and require their own exact artifact/client evidence.

## Disposition and recovery

- Migration coverage: core interface/DPI behavior was already implemented; now
  mapped to obligations and focused actual-consumer assertions.
- Feature correctness: real monitor/cursor behavior and exact native installation
  are separate from source presence or mocked callback coverage.
- Pending trial: test zero, PGUP/PGDOWN limits and persistence; resize and move
  between 100%/150% Windows monitors with foreground/background transitions.
  Test active scaling and zero on each available Mac architecture with its exact artifact.
- Dependencies: installed `UiScale` anchors and the existing DPI adapter. No shared
  binding change requested; automated registration/qualification is complete.
- Next checkpoint: coordinator native mod build and focused Windows/Mac acceptance,
  attached to the final artifact and client identities.
