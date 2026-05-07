# Unified Input Bind Strategy

Source folder: `D:\dev\stfc-mod`

Related baseline: `docs/KEYBIND_ACTION_SYSTEM_AUDIT.md`

Purpose: define a greenfield target architecture for all user-input-driven mod behavior, using the current implementation only as requirements evidence. The goal is a single efficient dispatcher, one shared action schema, deterministic conflict handling, and clear migration from the current `[shortcuts]` model.

## Target Position

The mod should not have separate input systems for router hotkeys, zoom hotkeys, right-click fleet actions, UI scale, cargo toggles, chat behavior, Escape behavior, and pan behavior. It should have one input runtime with domain-specific executors.

The new design should make these true:

- One schema defines every action, default bind, alias, gate, input mode, context rule, priority, conflict group, original-call policy, documentation label, and handler id.
- One parser compiles user config into a normalized binding set at startup.
- One per-frame key snapshot polls every watched key at most once.
- One dispatcher turns input events into semantic intents.
- Domain executors perform game-specific effects and return explicit dispatch results.
- Hook seams collect context and expose executor handles; they do not parse config or decide binding semantics.
- Right-click is one semantic primary intent, not three different actions sharing `MOUSE1` and racing through implicit priority order.
- Tests cover binding parsing, conflict validation, context resolution, original suppression, and fleet primary/right-click decisions without IL2CPP.

## Design Principles

1. Semantic actions, not key names.
   - The runtime should dispatch `FleetPrimary`, `FleetSecondary`, `ZoomIn`, `OpenChat`, or `ShowInventory`, not raw `SPACE`, `MOUSE1`, or `F1` decisions.

2. A binding is data.
   - Binding strings are parsed once, validated once, and compiled into indexed structures. No string work happens on the frame path.

3. Context is explicit.
   - Every action declares where it is allowed, where it is blocked, and which context providers it needs.

4. Input focus is a layer rule.
   - Text focus and chat focus should not be ad hoc checks scattered across handlers. They are global layer constraints, with narrow exceptions such as Escape clear-focus and in-chat channel switching.

5. Original-call policy is explicit.
   - Every handled action returns `SuppressOriginal`, `AllowOriginal`, or `NoOpinion`. A single runtime policy combines those decisions with Scopely fallback settings.

6. Greenfield fleet primary behavior is a policy engine.
   - `SPACE|MOUSE1` should produce one primary intent. Queue, engage, mine, cancel warp, join armada, set course, and deferred retry are outcomes of a tested fleet-action policy, not separate bindings with the same chord.

7. The hot path does no allocation.
   - Runtime dispatch uses fixed arrays, spans, bitsets, small vectors with reserved capacity, function pointers or handler ids, and precompiled binding indexes.

8. Compatibility is deliberate.
   - Old config keys should be accepted through aliases and migration warnings, but the canonical model should be simpler than today's config surface.

## High-Level Architecture

```text
Config TOML
  -> InputSchema registry
  -> InputConfigLoader
  -> BindingCompiler
  -> BindingSet + BindingIndex + ValidationReport

Game hooks
  -> InputRuntime::begin_frame() / begin_phase()
  -> InputSnapshot
  -> InputContext providers
  -> InputDispatcher
  -> Domain executors
  -> InputDispatchResult
  -> OriginalCallPolicy
```

### Core Modules

| Module | Responsibility |
| --- | --- |
| `input_action_schema.h/.cc` | Static action registry and default metadata. Single source of truth. |
| `input_binding.h/.cc` | Chord, modifier mask, trigger mode, parser, display formatting. No IL2CPP. |
| `input_config.h/.cc` | TOML loading, alias handling, migration warnings, validation report. |
| `input_snapshot.h/.cc` | Per-frame key/button state cache, watched-key polling, modifier masks. |
| `input_context.h/.cc` | Lightweight context flags and lazy domain context providers. |
| `input_dispatcher.h/.cc` | Candidate generation, layer/context filtering, priority/conflict resolution, handler invocation. |
| `input_runtime.h/.cc` | Frame/phase orchestration and original-call policy. |
| `input_actions_fleet.h/.cc` | Pure fleet action decision plus IL2CPP executor. |
| `input_actions_ui.h/.cc` | UI scale, cargo toggles, viewer panel, Escape viewer/rewards behavior. |
| `input_actions_navigation.h/.cc` | Section navigation and ship selection. |
| `input_actions_zoom.h/.cc` | Zoom phase actions and preset/default logic. |
| `input_actions_chat.h/.cc` | Chat open/channel actions plus chat UI hook event policy. |
| `input_actions_system.h/.cc` | hotkey enable/disable, quit, log-level changes, Scopely fallback control. |

The current `key.cc` can either become the low-level Unity key polling backend or be replaced by `input_snapshot`. The current `mapkey.cc` and `modifierkey.cc` should be retired after compatibility wrappers are no longer needed.

## Shared Action Schema

The action schema is the heart of the redesign. It should be a single `constexpr` registry or generated table used by config loading, runtime dispatch, runtime vars, docs, and tests.

Example shape:

```cpp
enum class InputActionId : uint16_t {
  HotkeysDisable,
  HotkeysEnable,
  Quit,
  FleetPrimary,
  FleetSecondary,
  FleetService,
  FleetViewInfo,
  FleetQueueClear,
  FleetQueueToggle,
  SelectShip1,
  SelectShip2,
  SelectCurrent,
  OpenChat,
  SelectChatGlobal,
  ShowInventory,
  ZoomIn,
  ZoomPreset1,
  SetZoomPreset1,
  UiScaleUp,
  ToggleCargoPlayer,
  LogLevelDebug,
  Escape,
  Max,
};

enum class InputTriggerMode : uint8_t {
  Down,
  Pressed,
  Released,
  Repeat,
  Axis,
  HookEvent,
};

enum class InputPhase : uint8_t {
  Frame,
  NavigationZoomUpdate,
  NavigationPanLateUpdate,
  BackButtonPressed,
  ChatTabChanging,
  ChatPanelFocused,
  PreScanShown,
  RewardsBound,
};

enum class OriginalPolicy : uint8_t {
  NoOpinion,
  SuppressOnHandled,
  AllowOnHandled,
  AlwaysAllow,
  AlwaysSuppress,
  SeamOwned,
};

struct InputActionSpec {
  InputActionId id;
  std::string_view canonical_key;
  std::span<const std::string_view> aliases;
  std::span<const std::string_view> default_binds;
  InputTriggerMode trigger_mode;
  InputPhase phase;
  InputLayer layer;
  InputGateMask gates;
  InputContextMask required_context;
  InputContextMask blocked_context;
  InputContextNeedMask context_needs;
  InputConflictGroup conflict_group;
  uint16_t priority;
  OriginalPolicy original_policy;
  InputHandlerId handler;
  std::string_view docs_label;
};
```

This can be implemented as a normal table, an X-macro, or a small generated file. The important rule is that action metadata is declared once.

### Schema Outputs

The shared schema should drive:

- Default binds.
- Config key names and aliases.
- Config validation.
- Runtime vars output.
- Generated example config sections.
- User docs and keymapping output.
- Action category ordering.
- Dispatch table construction.
- Conflict-group validation.
- Test fixtures.

This removes the current drift where defaults, example config, parser keys, runtime vars, README tables, and handlers can disagree.

## Canonical Config Model

The old `[shortcuts]` table can remain as a compatibility input. The greenfield canonical model should be explicit and array-based.

```toml
[input]
enabled = true
scopely_shortcuts = "off"        # off | native | fallback
original_frame_policy = "mod"    # mod | fallthrough_unhandled | fallthrough_all
strict_conflicts = true
allow_extra_modifiers = false

[input.bindings]
fleet_primary = ["SPACE", "MOUSE1"]
fleet_secondary = ["TAB", "MOUSE4"]
fleet_service = ["R", "MOUSE3"]
fleet_view_info = ["V", "MOUSE2"]
fleet_queue_clear = ["CTRL-C"]
fleet_queue_toggle = ["CTRL-Q"]
select_ship1 = ["1"]
open_chat = ["C"]
zoom_in = ["Q"]
zoom_out = ["E"]
ui_scale_up = ["PGUP"]
log_debug = ["CTRL-SHIFT-F9"]

[input.bindings.aliases]
action_primary = "fleet_primary"
action_queue = "fleet_primary"
action_recall_cancel = "fleet_primary"
action_recall = "fleet_service"
action_repair = "fleet_service"
set_hotkeys_enable = "hotkeys_enable"
set_hotkeys_enabled = "hotkeys_enable"
```

Compatibility behavior:

- Read `[input.bindings]` first when present.
- Read old `[shortcuts]` keys as aliases when canonical keys are absent.
- If old keys that map to the same canonical action disagree, warn and choose a deterministic source.
- Preserve `NONE` as an empty bind list.
- Write runtime vars using canonical keys only, with a compatibility section listing consumed aliases and warnings.

## Binding Model

### Chords

A chord is a primary key/button plus a modifier mask.

```cpp
struct InputChord {
  KeyCode key = KeyCode::None;
  ModifierMask required_modifiers = ModifierMask::None;
  ModifierMask forbidden_modifiers = ModifierMask::AnyModifier;
  InputDeviceMask devices = InputDeviceMask::KeyboardMouse;
};
```

Default semantics should be exact modifiers:

- `A` means A with no modifiers.
- `CTRL-A` means Ctrl plus A with no extra modifiers unless the action explicitly opts into extra modifiers.
- `SHIFT-CTRL-A` and `CTRL-SHIFT-A` normalize to the same chord.
- Left/right-specific modifiers are represented in the mask, not as separate parser rules.

### Trigger Modes

| Mode | Meaning | Examples |
| --- | --- | --- |
| `Down` | Fires once on key/button down. | Open panels, ship select, primary click, log level. |
| `Pressed` | Fires every frame while held. | Smooth zoom, UI scale repeat. |
| `Released` | Fires on release. | Future drag/hold actions. |
| `Repeat` | Fires after delay/interval. | Future keyboard pan repeat. |
| `Axis` | Reads analog delta. | Mouse wheel, pan delta, future scroll binds. |
| `HookEvent` | Fires from a non-polled UI hook event. | Chat tab changing, back button, rewards bound. |

### Binding Indexes

Compile bindings into indexes keyed by trigger mode and primary key.

```cpp
struct CompiledBinding {
  InputActionId action;
  InputChord chord;
  InputTriggerMode trigger_mode;
  uint16_t priority;
  InputLayer layer;
};

struct BindingIndex {
  std::array<SmallVector<CompiledBinding, 4>, KeyCodeMax> down;
  std::array<SmallVector<CompiledBinding, 4>, KeyCodeMax> pressed;
  std::array<SmallVector<CompiledBinding, 4>, KeyCodeMax> released;
  KeyBitset watched_keys;
  KeyBitset watched_pressed_keys;
};
```

The dispatcher should only inspect candidates for keys that changed or are in the pressed-watch set. This turns the current "scan many actions every frame" pattern into "match only possible actions for observed input."

## Context and Layers

### Layers

Layer order should be fixed and source-verified by tests.

1. Emergency global:
   - Enable/disable hotkeys, quit, logging changes if allowed.

2. Text focus:
   - Most actions blocked.
   - Escape clear-focus allowed.

3. Chat:
   - Channel selection and chat focus actions allowed.
   - Fleet/navigation actions blocked.

4. Overlay/viewer:
   - Escape hide viewers, rewards dismiss, `fleet_view_info`, cargo toggles.

5. Navigation and fleet:
   - Ship select, fleet primary/secondary/service, set course, zoom, pan.

6. Section navigation:
   - Show inventory, officers, refinery, station, galaxy/system, etc.

7. Diagnostics and debug:
   - Live debug/log actions, gated by config or build mode.

### Context Snapshot

The cheap context should be collected once per frame:

```cpp
struct InputContextSnapshot {
  bool hotkeys_enabled = true;
  bool input_focused = false;
  bool in_chat = false;
  bool in_system = false;
  bool in_galaxy = false;
  bool in_starbase = false;
  bool viewer_visible = false;
  bool rewards_visible = false;
  bool queue_enabled = false;
  bool queue_unlocked = false;
};
```

Heavy context should be lazy and domain-specific:

- Fleet state and selected fleet id.
- Visible pre-scan target summary.
- Mining viewer summary.
- Star node viewer summary.
- Navigation interaction summary.
- Armada widget summary.
- Chat tab index summary.
- Zoom controller pointer and view depth.

The action schema declares `context_needs`, so the dispatcher asks for heavy context only after a matching binding exists.

## Unified Dispatcher Flow

Frame path:

```text
InputRuntime::begin_frame()
  -> ensure one frame id
  -> poll watched keys once
  -> build cheap context
  -> dispatch Frame phase actions
  -> combine dispatch results into ScreenManager original policy
```

Phase path:

```text
NavigationZoom.Update hook
  -> InputRuntime::begin_phase(NavigationZoomUpdate, zoom_context)
  -> dispatch actions whose phase is NavigationZoomUpdate
  -> executor updates zoom or returns no-op
  -> hook calls original according to phase result
```

Candidate resolution:

```text
for each input event:
  candidates = binding_index[event.mode][event.key]
  filter exact modifiers
  filter enabled gates
  filter layer/context masks
  sort by priority and specificity
  run first exclusive candidate per conflict group
  call handler
  record result and diagnostics
```

Dispatch result:

```cpp
enum class InputActionStatus {
  Ignored,
  Blocked,
  Handled,
  Deferred,
  Failed,
};

struct InputDispatchResult {
  InputActionStatus status = InputActionStatus::Ignored;
  OriginalDecision original = OriginalDecision::NoOpinion;
  InputActionId action = InputActionId::Max;
  InputDiagnosticCode diagnostic = InputDiagnosticCode::None;
  DeferredInputAction deferred;
};
```

No handler should rely on implicit router fallthrough. The result object is the only way to affect original-call behavior.

## Greenfield Fleet Primary Design

The current default maps `SPACE|MOUSE1` to primary, queue, and recall-cancel. Greenfield should collapse this into one semantic action:

```text
fleet_primary = ["SPACE", "MOUSE1"]
```

The fleet primary policy decides the outcome:

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
```

Inputs to the pure decision function:

```cpp
struct FleetPrimaryDecisionInput {
  FleetState fleet_state;
  bool rewards_visible;
  bool queue_mode_enabled;
  bool queue_unlocked;
  bool queue_full;
  bool visible_prescan;
  bool visible_mining_viewer;
  bool visible_star_node;
  bool visible_navigation_interaction;
  bool visible_armada_widget;
  bool armada_join_interactable;
  HullType target_hull_type;
  bool target_context_resolved;
  bool is_deferred_retry;
};
```

This gives one place to define the intended right-click behavior. The default policy should preserve current user intent but remove duplicate-bind ambiguity:

1. If rewards screen is active, primary dismisses it.
2. If selected fleet is warp charging/warping and no visible target context should consume the click, primary cancels warp.
3. If queue mode is enabled, queue is unlocked, target can be queued, and queue is not full, primary adds to queue.
4. If mining viewer is active, primary mines.
5. If pre-scan target is hostile/player and resolved, primary engages.
6. If pre-scan target is armada and attack button is available, primary attacks or opens armada flow.
7. If armada join widget is visible and interactable, primary joins.
8. If star node is visible, primary warps.
9. If navigation interaction is visible, primary sets course.
10. If target context is unresolved, primary defers exactly one bounded retry tied to fleet id, widget id, and target context id.

Secondary and service actions:

- `fleet_secondary = ["TAB", "MOUSE4"]`: scan/view alternate target information.
- `fleet_service = ["R", "MOUSE3"]`: recall when away, repair when docked/destroyed.
- Advanced optional binds may expose `fleet_recall` and `fleet_repair`, but the canonical default should be the semantic service action.
- `fleet_view_info = ["V", "MOUSE2"]`: toggle cargo/reward details.
- `fleet_queue_toggle = ["CTRL-Q"]`: controls queue mode used by `fleet_primary`.
- `fleet_queue_clear = ["CTRL-C"]`: clear queue.

## Original Scopely Input Policy

Replace `use_scopely_hotkeys` plus `allow_key_fallthrough` with explicit policy fields:

```toml
[input]
scopely_shortcuts = "off"        # off | native | fallback
original_frame_policy = "mod"    # mod | fallthrough_unhandled | fallthrough_all
```

Semantics:

- `scopely_shortcuts = "off"`: suppress `ShortcutsManager.InitializeActions`.
- `scopely_shortcuts = "native"`: call Scopely initializer and do not bind overlapping mod shortcuts unless user explicitly enables them.
- `scopely_shortcuts = "fallback"`: call Scopely initializer, but mod actions keep first right of refusal.
- `original_frame_policy = "mod"`: original update is called only when dispatcher allows it.
- `original_frame_policy = "fallthrough_unhandled"`: original update is called when no mod action handled the input.
- `original_frame_policy = "fallthrough_all"`: original update always runs after mod dispatch.

The migration layer can keep reading old booleans:

- `use_scopely_hotkeys = true` -> `scopely_shortcuts = "native"`.
- `allow_key_fallthrough = true` -> `original_frame_policy = "fallthrough_all"` plus warning if Scopely shortcut initialization changes too.

## Hook Strategy

Greenfield does not mean more hooks. It means fewer semantic owners.

| Hook seam | New role |
| --- | --- |
| `ScreenManager.Update` | Owns `Frame` phase, key snapshot, global dispatch, original update policy. |
| `ShortcutsManager.InitializeActions` | Applies `scopely_shortcuts` policy only. |
| `NavigationZoom.Update` | Supplies zoom context and runs `NavigationZoomUpdate` phase actions. |
| `NavigationPan.LateUpdate` | Supplies pan context and runs pan/momentum phase. |
| `TKTouch.populateWithPosition` | Remains a low-level touch normalization hook, not a bind action. |
| `SectionManager.BackButtonPressed` | Owns Escape exit suppression at the real back-button seam. |
| Chat controller hooks | Become `HookEvent` inputs for chat policy, not separate config islands. |
| PreScan/rewards hooks | Update cached viewer context for fleet/cargo executors. |

macOS safety rule: avoid overlapping detours for the same method. If a method already has a hook, it should fan out through the input runtime or a phase subscriber list rather than installing another hook for a new action.

## Efficiency Plan

Startup work:

- Parse all bindings once.
- Normalize aliases and old config keys once.
- Build exact modifier masks once.
- Build per-mode, per-key binding indexes once.
- Build watched-key bitsets once.
- Validate conflicts once.
- Emit runtime vars and warnings once.

Frame work:

- Poll only watched keys plus fixed seam keys such as Escape.
- Cache `Down`, `Pressed`, and `Released` state in one snapshot.
- Build cheap context once.
- Match only candidates for keys that changed or are watched for held input.
- Build heavy fleet/viewer context only when a fleet action candidate exists.
- Build zoom context only during zoom phase.
- Avoid `ObjectFinder::GetAll()` until an active candidate needs that context.
- Avoid heap allocations and string formatting on normal frames.

Diagnostics:

- Use structured diagnostic codes and rate-limited logs.
- Log config validation at startup.
- Log ambiguous contexts only when they affect a decision.
- Keep optional ring-buffer diagnostics for live debug, not per-frame spam.

Expected complexity:

- Current effective shape: `actions * bindings` checks across multiple hooks, with some expensive context scans inside action paths.
- Target shape: `changed_or_held_bound_keys + active_candidates + lazy_context`. For ordinary frames with no key input, dispatch should be nearly empty.

## Conflict Handling

Conflict validation should run at config load and produce actionable diagnostics.

Conflict categories:

- Exact duplicate chord in the same layer and phase.
- Overlapping modifier chord, such as `CTRL-A` and `CTRL-SHIFT-A`, when extra modifiers are allowed.
- Same chord across exclusive conflict groups.
- Alias collision, where old config keys map to the same canonical action but define different binds.
- Disabled action with configured binds.
- Bind configured in a gated feature that is off.

Resolution rules:

- Exact duplicates in strict mode are warnings or errors, depending on `strict_conflicts`.
- Higher specificity wins when overlap is explicitly allowed.
- Higher priority wins only within a declared conflict group.
- Emergency global actions always win.
- Text-focus layer blocks gameplay actions before priority is considered.
- The runtime vars file should show shadowed binds and the winning action.

## Migration Strategy

### Slice 1: Pure Core

- Add schema, chord parser, modifier masks, validation report, and pure tests.
- Keep existing runtime untouched.
- Test old strings: `SPACE|MOUSE1`, `CTRL-ALT-=`, `NONE`, invalid tokens, modifier-only values, mouse keys, left/right modifiers, and extra modifiers.

### Slice 2: Config Bridge

- Load canonical `[input.bindings]` and old `[shortcuts]` into the new binding compiler.
- Emit runtime vars from the schema.
- Accept `set_hotkeys_enable` and `set_hotkeys_enabled` aliases.
- Keep old `MapKey` runtime active until dispatch migration begins.

### Slice 3: Frame Dispatcher Skeleton

- Add `InputRuntime` and `InputSnapshot` under `ScreenManager.Update`.
- Migrate hotkey enable/disable, log level, quit, and simple section navigation first.
- Keep behavior parity with existing defaults.

### Slice 4: UI and Chat Actions

- Move UI scale, cargo toggles, chat open/channel selection, and viewer/rewards Escape handling to dispatcher-owned handlers.
- Convert chat tab/swipe/message hooks into `HookEvent` policy calls.

### Slice 5: Fleet Primary and Right-Click

- Add pure fleet primary/service/secondary decision functions.
- Migrate `SPACE|MOUSE1` to canonical `fleet_primary`.
- Map old `action_primary`, `action_queue`, and `action_recall_cancel` aliases into `fleet_primary` with warnings for conflicting user overrides.
- Preserve current practical outcomes, but make every priority step test-covered.

### Slice 6: Zoom and Pan Phases

- Migrate zoom binds into the schema while keeping `NavigationZoom.Update` as the phase seam.
- Decide whether WASD movement exists. Implement it through an explicit pan/keyboard camera action or remove it from generated docs.
- Clarify or rename `disable_move_keys` if it remains a pan-original suppression flag.

### Slice 7: Retire Old Binding Runtime

- Remove `MapKey` and `ModifierKey` runtime use after all actions have migrated.
- Keep minimal compatibility parsing only if needed for old config migration.
- Update README, `KEYMAPPING.md`, example config, and runtime vars format.

## Test Strategy

Pure tests should come first and carry most of the confidence.

Required pure test groups:

- Chord parsing and formatting.
- Modifier exactness and specificity.
- Alias migration.
- Conflict validation.
- Binding index candidate generation.
- Layer filtering for text focus, chat, overlays, and global actions.
- Original-call policy.
- Fleet primary outcome decisions.
- Fleet service recall/repair decisions.
- Deferred action binding to fleet/target context.
- Zoom action selection by phase.

Integration tests should be narrow:

- Config load writes canonical runtime vars.
- Existing default config produces expected canonical binds.
- Example config does not contain keys unknown to the schema.

Manual/live checks should focus on:

- Right-click hostile engage.
- Right-click queue when queue mode is enabled and queue is available.
- Right-click warp cancel while warping.
- R/MOUSE3 recall and repair in correct states.
- Chat focus and Escape behavior.
- Zoom held input and preset recall/store.
- UI scale repeat.

## Acceptance Criteria

The redesign is successful when:

- Adding a new bindable action requires editing one schema entry and one handler, not config load lists, docs, defaults, runtime vars, and dispatch loops separately.
- Every default bind is generated from the same action schema used by the dispatcher.
- No action scans all configured actions per frame.
- No binding parser allocates on the heap per token.
- Invalid or shadowed binds are visible at startup.
- Right-click behavior is deterministic, testable, and explained by one fleet primary policy.
- `allow_key_fallthrough` no longer blends startup Scopely shortcut initialization with per-frame original update policy.
- Zoom, UI scale, chat, section navigation, fleet actions, Escape, cargo, and diagnostics all pass through the same input runtime model, even when their hook seams differ.

## Open Design Choices

- Whether canonical config should use `fleet_primary` or dotted names such as `fleet.primary`. Underscore keys are simpler for TOML compatibility and current code style.
- Whether conflict strictness should default to warning or error. A warning default is safer for migration; strict error can be opt-in first.
- Whether `fleet_service` should fully replace user-facing `action_recall` and `action_repair`, or whether separate advanced actions should remain documented.
- Whether Scopely fallback should be a supported user mode or a debug-only escape hatch.
- Whether documentation should be generated at build time or by an explicit maintenance script.

## Immediate Next Work

For the next implementation sprint, start with the pure core. Do not begin by moving hooks.

1. Create the action schema table and parser types.
2. Write parser, alias, and validation tests.
3. Compile current defaults into the new model and compare the canonical runtime output to current runtime vars.
4. Extract pure fleet primary decision logic and cover current right-click outcomes.
5. Only then migrate live dispatch paths.

That order gives the redesign a testable spine before touching IL2CPP hooks or user-facing behavior.