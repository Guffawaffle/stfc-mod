# Unified Input Bind Implementation Plan

Related docs:

- `docs/KEYBIND_ACTION_SYSTEM_AUDIT.md`
- `docs/UNIFIED_INPUT_BIND_STRATEGY.md`
- `docs/FLEET_RUNTIME_DISPATCH_PLAN.md`

Purpose: turn the greenfield unified input strategy into executable sprint slices. This plan synthesizes three read-only ensign investigations: pure binding core, fleet/right-click policy, and config/hook migration.

## North Star

Treat the current input system as requirements evidence, not as architecture gravity.

The target architecture is a shared schema plus common dispatcher. The first implementation work should create testable pure logic that can coexist with the current runtime. Live IL2CPP hooks should move last, after parser, schema, validation, and right-click policy are covered by tests.

## Work Tracks

### Track A: Pure Input Core

Goal: create the shared binding language without touching live dispatch.

New modules:

- `mods/src/patches/input_binding/input_action_schema.h/.cc`
- `mods/src/patches/input_binding/input_binding.h/.cc`
- `mods/src/patches/input_binding/input_binding_validation.h/.cc`
- `mods/src/patches/input_binding/input_binding_index.h/.cc`

Responsibilities:

- Define canonical action ids, config keys, aliases, default binds, trigger modes, phases, gates, layers, conflict groups, priorities, and handler ids.
- Parse chords and multi-bind strings into pure structs.
- Normalize modifier order and exact-modifier semantics.
- Preserve `NONE` as an empty bind list.
- Produce structured diagnostics for invalid tokens, alias conflicts, duplicate binds, shadowed binds, disabled gated binds, and extra-modifier ambiguity.
- Compile bindings into per-trigger/per-key indexes for future dispatch.

First tests:

- Simple key: `A`.
- Modifier key: `CTRL-A`.
- Multi modifier: `CTRL-SHIFT-A`.
- Left/right-specific modifiers.
- Mouse keys: `MOUSE1`, `MOUSE2`, `MOUSE3`, `MOUSE4`.
- Pipe binds: `SPACE|MOUSE1`.
- `NONE` unbind.
- Unknown key token.
- Modifier-only token such as `CTRL-SHIFT`.
- Partial invalid multi-bind such as `SPACE|INVALID|MOUSE1`.
- Exact modifier matching: `CTRL-A` does not match `CTRL-SHIFT-A` unless the action opts in.
- Binding conflict report for duplicate and overlapping chords.

### Track B: Fleet Primary Policy

Goal: make right-click/space behavior deterministic, pure, and test-covered.

New modules:

- `mods/src/patches/fleet_input_policy.h`
- `mods/src/patches/fleet_input_policy.cc`

Pure outcomes:

```cpp
enum class FleetPrimaryOutcome {
  None,
  DismissRewards,
  CancelWarp,
  AddToQueue,
  Mine,
  Engage,
  ArmadaAttack,
  JoinArmada,
  WarpToNode,
  SetCourse,
  DeferUntilTargetResolved,
};

enum class FleetSecondaryOutcome {
  None,
  ScanPreScan,
  ScanMining,
  ViewStarNode,
};

enum class FleetServiceOutcome {
  None,
  Recall,
  Repair,
};
```

Policy inputs should be primitive values only: fleet state, target type, queue flags, visibility flags, context resolution, and deferred identity flags. They must not reference IL2CPP objects except as opaque identity values when needed later.

Greenfield primary priority:

1. Dismiss rewards screen.
2. Cancel warp when the selected fleet is warp charging/warping and visible target context should not consume the click.
3. Add to queue when queue mode is enabled, queue is unlocked, the target can be queued, and the queue is not full.
4. Defer exactly one bounded retry when target context is unresolved.
5. Mine when the mining viewer is active.
6. Engage normal resolved pre-scan targets.
7. Armada attack when the pre-scan target is an armada target and attack is available.
8. Join armada when the armada widget is visible and interactable.
9. Warp to star node.
10. Set course from navigation interaction.
11. Otherwise no-op.

Service priority:

- Recall for away-space states when recall is allowed.
- Repair for docked/destroyed states when repair is allowed.
- No-op otherwise.

Secondary priority:

- Scan pre-scan target.
- Scan mining viewer.
- View star node.
- No-op otherwise.

First tests:

- Every primary outcome.
- Rewards beating every other context.
- Warp cancel blocked by visible target-consuming context.
- Queue mode disabled vs enabled.
- Queue full vs available.
- Armada target not queued as normal target.
- Unresolved target defers only when eligible.
- Mining viewer precedence.
- Navigation interaction set-course fallback.
- Recall and repair state matrix.

### Track C: Config Bridge

Goal: make the new schema useful before it owns runtime dispatch.

Steps:

- Accept canonical `[input.bindings]` while preserving old `[shortcuts]`.
- Map old action keys to canonical action ids through schema aliases.
- Accept both `set_hotkeys_enable` and `set_hotkeys_enabled`.
- Map old `action_primary`, `action_queue`, and `action_recall_cancel` to canonical `fleet_primary` during migration.
- Map old `action_recall` and `action_repair` to canonical `fleet_service` during migration.
- Emit runtime vars from canonical schema entries.
- Emit a compatibility warning section for deprecated or conflicting aliases.

This track should not replace `MapKey` immediately. It should first compile and validate the new model side-by-side, then compare it with the old bindings.

### Track D: Runtime Dispatcher

Goal: move live input handling to the common dispatcher after the pure core and config bridge are stable.

First migrated actions:

- Hotkeys enable/disable.
- Log-level actions.
- Quit.
- Simple section navigation.
- UI scale repeat.

Later migrated actions:

- Chat open/channel actions.
- Viewer/rewards Escape behavior.
- Fleet primary/secondary/service.
- Zoom phase actions.
- Pan phase actions.

Dispatch rules:

- Poll watched keys once per frame.
- Match only candidates for changed or held watched keys.
- Apply layer/context masks before priority.
- Execute at most one winner per exclusive conflict group.
- Return explicit original-call decisions.

### Track E: Hook Phase Migration

Goal: keep hook seams narrow and make them feed the runtime instead of owning input semantics.

Phase seams:

- `ScreenManager.Update`: frame phase, key snapshot, global dispatch, original update policy.
- `ShortcutsManager.InitializeActions`: Scopely shortcut policy only.
- `NavigationZoom.Update`: zoom phase context.
- `NavigationPan.LateUpdate`: pan phase context.
- `SectionManager.BackButtonPressed`: Escape exit seam.
- Chat hooks: `HookEvent` policy calls.
- Pre-scan/rewards hooks: cached context updates for fleet/cargo executors.

macOS rule: avoid adding overlapping hooks to the same method. Existing hooks should fan out through runtime phases.

## Fallthrough Policy Split

Current `allow_key_fallthrough` blends two behaviors:

- whether Scopely shortcuts initialize;
- whether original `ScreenManager.Update` runs after mod dispatch.

Target config:

```toml
[input]
scopely_shortcuts = "off"        # off | native | fallback
original_frame_policy = "mod"    # mod | fallthrough_unhandled | fallthrough_all
```

Migration:

- `use_scopely_hotkeys = true` maps to `scopely_shortcuts = "native"`.
- `allow_key_fallthrough = true` maps to `original_frame_policy = "fallthrough_all"` and emits a warning that the new settings are split.

Do not change live behavior until pure tests cover the decision matrix.

## First Execution Slice

Start with Track B plus a small part of Track A. The fleet policy gives immediate value for the riskiest behavior while avoiding hook churn.

Deliverables:

1. Add `fleet_input_policy.h/.cc`.
2. Add pure outcome tests to `tests/src/test_pure_logic.cc`.
3. Keep `ExecuteSpaceAction` untouched.
4. Build and run `stfc-mod-tests`.

Why this slice first:

- It directly addresses right-click ambiguity.
- It is pure and low risk.
- It creates a model that the future dispatcher can call.
- It proves the greenfield approach without destabilizing runtime hooks.

## Second Execution Slice

Add the pure binding core skeleton.

Deliverables:

1. Add canonical action ids and schema metadata for a small action subset.
2. Add chord parser and validation report.
3. Add parser tests.
4. Do not route live input through it yet.

Recommended subset:

- `fleet_primary`
- `fleet_secondary`
- `fleet_service`
- `fleet_view_info`
- `hotkeys_disable`
- `hotkeys_enable`
- `log_debug`
- `zoom_in`
- `zoom_out`

## Validation Gates

Docs-only or pure-code slices:

- `git diff --check`
- `xmake build -y stfc-mod-tests`
- `xmake run -y stfc-mod-tests`

Runtime migration slices:

- `git diff --check`
- `xmake build -y stfc-mod-tests`
- `xmake run -y stfc-mod-tests`
- `xmake build -y mods`
- relevant `.ax\ax.ps1` checks if the change touches deploy/runtime behavior.

## Risks

- Accidentally preserving current `MOUSE1` triple-bind behavior in the new schema instead of collapsing it into `fleet_primary`.
- Changing right-click live behavior before the pure policy is tested.
- Letting alias compatibility obscure canonical config simplicity.
- Reintroducing broad per-frame scans in the dispatcher.
- Treating `allow_key_fallthrough` as one setting in the new runtime.

## Immediate Checklist

- [ ] Add fleet input policy types and pure decision functions.
- [ ] Add test cases for every fleet primary/secondary/service outcome.
- [ ] Run focused tests.
- [ ] Commit first pure policy slice only after validation.
- [ ] Start binding parser/schema slice after the fleet policy is stable.