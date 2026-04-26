# Tech Debt Repayment Plan

This plan captures the cleanup work surfaced while simplifying incoming attack notifications and investigating the likely keybinding regression.

## Current Findings

- The live runtime config currently has `allow_key_fallthrough = true` and `use_scopely_hotkeys = false`.
- `hotkey_router_init_actions()` returns `Config::Get().use_scopely_hotkeys || AllowKeyFallthrough()`.
- `InitializeActions_Hook()` calls the original Scopely `ShortcutsManager::InitializeActions` when `hotkey_router_init_actions()` returns true.
- That means the meaning of `allow_key_fallthrough` crosses two concerns: per-frame input fallthrough and startup shortcut registration. This is the most likely current keybinding break source.
- `HotkeyHooks` installed during boot, and the deployed build reports hooks installed `17/17`.
- `ScreenManager_Update_Hook()` currently runs `live_debug_tick()` before the hotkey router, so hotkeys and live-debug share the same frame hook path.
- Incoming attack notifications now use one production path, `ToastFleetObserver.QueueNotifications`; old producer/navigation/station/fleet-bar inference branches were removed.

## Hotspot Scan

Largest patch files by line count at the time of this plan:

| Lines | File | Cleanup Theme |
| ---: | --- | --- |
| 2669 | `mods/src/patches/parts/live_debug.cc` | Split dispatcher, observers, serializers, event store, hook tick |
| 2479 | `mods/src/patches/parts/sync.cc` | Split sync domains and transport/payload concerns |
| 717 | `mods/src/patches/notification_service.cc` | Split queue, formatting, platform delivery |
| 619 | `mods/src/patches/fleet_notifications.cc` | Split fleet state transitions from incoming attack policy |
| 409 | `mods/src/patches/fleet_actions.cc` | Extract action planner/facades |
| 377 | `mods/src/patches/hotkey_dispatch.cc` | Data-driven hotkey action table and effect handlers |
| 362 | `mods/src/patches/parts/object_tracker.cc` | Split tracker core, GC glue, hook installation |
| 319 | `mods/src/patches/key.cc` | Extract input snapshot/testable key state helpers |
| 260 | `mods/src/patches/hotkey_router.cc` | Extract pure routing decisions from game effects |

## Target Boundaries

- Hook files under `mods/src/patches/parts/` should only resolve methods, install detours, capture arguments, and call feature modules.
- Feature modules should own domain policy but avoid raw hook installation and platform APIs.
- Pure policy modules should accept small structs and return decisions; they should not touch IL2CPP, WinRT, files, global config, or threads.
- Runtime diagnostics should be best-effort and should never gate hotkey, notification, or gameplay behavior.
- Platform adapters should be thin wrappers behind interfaces where practical.

## Proposed Issues

### Issue 001 - Untangle Hotkey Fallthrough Semantics

Problem: `allow_key_fallthrough` affects both startup shortcut registration and per-frame input routing. This makes one config value decide whether Scopely initializes its shortcuts and whether the mod lets frames fall through.

Scope:
- Rename `hotkey_router_init_actions()` to something behavior-explicit, such as `hotkey_router_should_call_original_initialize_actions()`.
- Split config semantics into two decisions if needed: startup action initialization and per-frame key fallthrough.
- Add startup logging for `allow_key_fallthrough`, `use_scopely_hotkeys`, `hotkeys_enabled`, and `installHotkeyHooks`.
- Decide whether `allow_key_fallthrough` should ever cause original `InitializeActions` to run.

Files:
- `mods/src/patches/parts/hotkeys.cc`
- `mods/src/patches/hotkey_router.cc`
- `mods/src/config.cc`
- `mods/src/config.h`

Acceptance Criteria:
- The branch in `InitializeActions_Hook()` reads naturally without inverted semantics.
- Runtime logs state exactly why the original shortcut initialization was called or suppressed.
- A test covers the decision matrix for `allow_key_fallthrough` and `use_scopely_hotkeys`.
- Existing user config either migrates cleanly or preserves documented behavior.

### Issue 002 - Fix Hotkey Config Key Hygiene

Problem: The config key `set_hotkeys_disble` is misspelled and entrenched in runtime vars. This is small but risky because keybinding config already lacks validation feedback.

Scope:
- Add support for correctly spelled `set_hotkeys_disable`.
- Keep `set_hotkeys_disble` as a deprecated alias for compatibility.
- Emit a warning when the deprecated spelling is used.
- Document the canonical key.

Files:
- `mods/src/config.cc`
- `mods/src/defaultconfig.h`
- `example_community_patch_settings.toml`
- `docs/` user-facing config docs if present

Acceptance Criteria:
- Both spellings work temporarily.
- Generated/default config uses only `set_hotkeys_disable`.
- A pure config parsing test covers canonical and deprecated spellings.

### Issue 003 - Add Hotkey Hook Health Diagnostics

Problem: Hook install failures are spread across `ErrorMsg::Missing*` lines and are hard to connect to user-visible hotkey failure.

Scope:
- Emit a concise HotkeyHooks install summary after resolving `ShortcutsManager.InitializeActions`, `ScreenManager.Update`, `RewardsButtonWidget.OnDidBindContext`, and `PreScanTargetWidget.ShowWithFleet`.
- Include method found/not found and detour attempted status.
- Add an AX/log checklist for hotkey regression triage.

Files:
- `mods/src/patches/parts/hotkeys.cc`
- `docs/` troubleshooting docs

Acceptance Criteria:
- Boot log can answer whether hotkey hooks installed without reading source.
- Missing methods include hook purpose and likely user-visible symptom.

### Issue 004 - Separate Live-Debug From Hotkey Frame Hook

Problem: `ScreenManager_Update_Hook()` runs live-debug before hotkey processing, and live-debug also has its own fallback update detour when hotkeys are disabled. This couples diagnostics to input behavior.

Scope:
- Define one owner for `ScreenManager.Update` detouring.
- Move per-frame fanout into a small coordinator that calls live-debug and hotkey router independently.
- Ensure live-debug cannot consume or skip the game update.

Files:
- `mods/src/patches/parts/hotkeys.cc`
- `mods/src/patches/parts/live_debug.cc`
- `mods/src/patches/patches.cc`

Acceptance Criteria:
- Only one module installs the `ScreenManager.Update` detour in a given build.
- Hotkey enabled/disabled state does not change whether live-debug can tick.
- Live-debug errors cannot suppress original `ScreenManager.Update`.

### Issue 005 - Split `live_debug.cc` Into Dispatcher, Event Store, Observers, Serializers, and Hook Tick

Problem: `parts/live_debug.cc` is the largest file and mixes JSON dispatch, file-backed request handling, object observation, serialization, event buffering, hook installation, and tick behavior.

Scope:
- Extract request parsing/dispatch into `live_debug/dispatcher`.
- Extract recent event storage into `live_debug/event_store`.
- Extract observation structs and collectors into `live_debug/observers`.
- Extract JSON conversion into `live_debug/serializers`.
- Leave `parts/live_debug.cc` as hook/tick wiring.

Acceptance Criteria:
- Dispatcher can be tested without IL2CPP objects or files.
- Event store has capacity/order/clear tests.
- Serializers accept plain structs and have unit tests.
- `parts/live_debug.cc` loses at least half its line count without behavior loss.

### Issue 006 - Split `sync.cc` By Domain and Transport

Problem: `parts/sync.cc` is nearly as large as live-debug and likely combines transport, scheduling, payload construction, and multiple sync domains.

Scope:
- Map sync responsibilities first, then split into domain payload builders, transport/client, scheduler, and hook glue.
- Move reusable serialization helpers into common modules.

Acceptance Criteria:
- Each sync domain has its own payload builder or adapter.
- Transport can be tested independently with fake responses.
- Hook layer only schedules or triggers sync work.

### Issue 007 - Split Notification Service Into Queue, Formatting, and Platform Delivery

Problem: `notification_service.cc` owns localization, text cleanup, queue batching, threading, and WinRT delivery.

Scope:
- Move text helpers to `notifications/text` or pure helpers.
- Move batching/coalescing to `notifications/queue`.
- Move WinRT calls to `notifications/platform_winrt`.
- Keep `notification_service.cc` as API/router glue.

Acceptance Criteria:
- Queue/coalescing logic has deterministic unit tests.
- Platform delivery can be replaced with a test double.
- Toast routing remains thin and feature-specific policy stays outside the service.

### Issue 008 - Keep Incoming Attack Queue-Only and Test Its Policy

Problem: Earlier attempts accumulated multiple competing incoming-attack branches. The current cleanup intentionally made `ToastFleetObserver.QueueNotifications` the only production source.

Scope:
- Extract incoming attack attacker-kind mapping, title/body selection, and dedupe key construction into pure helpers.
- Add tests for hostile/player/unknown behavior, station/fleet targets, and bounded dedupe.
- Keep toast consumption separate from visible notification generation.

Acceptance Criteria:
- Tests cover `fleetType=1` player and `2/3/4/6` hostile.
- Tests cover dedupe TTL and max-entry eviction.
- No producer/navigation/station/fleet-bar inferred branch is reintroduced.

### Issue 009 - Make Hotkey Dispatch Data-Driven and Testable

Problem: `hotkey_dispatch.cc`, `hotkey_router.cc`, `fleet_actions.cc`, and `viewer_mgmt.cc` mix input decisions with game object mutations.

Scope:
- Define a pure hotkey decision model: input snapshot + UI state -> action request.
- Move effectful operations behind small facades.
- Replace repeated one-off handler functions with a table of action descriptors and executors.

Acceptance Criteria:
- Pure router tests cover chat focus, ship selection, queue toggle, escape handling, and action fallthrough.
- Effect handlers can be exercised with fake facades.
- Existing bindings behave the same in runtime smoke tests.

### Issue 010 - Extract Key/Input Parsing Tests

Problem: Key parsing and config binding are high-impact and under-tested. A typo or ambiguous binding can break core interaction.

Scope:
- Move key binding parsing and display formatting into a pure module.
- Add tests for multi-binding strings like `MOUSE1 | MOUSE2`, `CTRL-ALT-=`, empty entries, `NONE`, and invalid tokens.
- Add diagnostics for unknown config shortcut tokens.

Acceptance Criteria:
- Invalid key tokens produce clear log warnings.
- Canonical runtime vars are stable and predictable.
- Test coverage protects hotkey enable/disable bindings.

### Issue 011 - Split Object Tracker Core From GC/Hook Glue

Problem: Object tracking mixes runtime object maps, GC/finalizer integration, signature scanning, and hook installation.

Scope:
- Extract thread-safe tracker core into a module with add/remove/query APIs.
- Isolate GC/finalizer and signature-scan code.
- Leave hook install code as wiring only.

Acceptance Criteria:
- Core tracker has unit tests for add/remove/latest/list behavior.
- Signature scan code is isolated from core data structures.
- Live-debug uses tracker APIs, not internals.

### Issue 012 - Create Common Utility Modules

Problem: Pointer formatting, time helpers, string cleanup, JSON helper patterns, and file atomics are repeated across features.

Scope:
- Create common helpers for pointer-to-string, monotonic/system timestamps, atomic file writes, and compact string escaping.
- Move callers incrementally.

Acceptance Criteria:
- No new include cycles.
- Unit tests cover utility edge cases.
- Existing behavior remains byte-for-byte equivalent where relevant.

### Issue 013 - Add AX Runtime Regression Recipes

Problem: Build passing is not enough for injected hooks. We need repeatable runtime smoke checks for hotkeys, notification boot, and live-debug.

Scope:
- Add documented AX recipes for hotkey health, notification queue health, and live-debug request health.
- Prefer structured AX output where available.
- Include exact log strings to watch.

Acceptance Criteria:
- A contributor can reproduce the hotkey triage path without reading this plan.
- Recipes distinguish ignored known boot errors from new hook failures.

## Suggested Execution Order

1. Issue 001, 002, 003: stabilize and diagnose the current hotkey problem.
2. Issue 004: remove the biggest hotkey/live-debug crossed stream.
3. Issue 008: lock down the incoming-attack cleanup with tests before more notification refactors.
4. Issue 007 and 010: extract low-risk pure helpers and tests.
5. Issue 005: split live-debug once the smaller seams are protected.
6. Issue 006 and 011: split the highest-risk large runtime modules.
7. Issue 012 and 013: keep utility consolidation and regression recipes moving alongside the feature work.

## Validation Commands

- Build: `ax build`
- Deploy/runtime smoke: `ax cycle`
- Effective config: `ax config -RuntimeVars -Compact`
- Boot summary: `ax log-boot -Summary`
- Recent runtime events: `ax recent-events -Last 50`
