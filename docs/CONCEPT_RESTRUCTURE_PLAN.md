# Concept Restructure Plan

This document captures a concept-first cleanup sweep of the community patch. It is intentionally broader than the immediate hotkey and incoming-attack notification work: the goal is to name the reusable concepts that should exist in the codebase so future features do not grow by adding another one-off path.

## Short Answer

This is not just "C++ being C++." Some complexity is inherent to injected C++ in a Unity IL2CPP game, but much of the current mess is avoidable organic growth.

Unavoidable complexity:

- Detours run inside the game's process and must be conservative.
- IL2CPP methods and fields are resolved dynamically and may disappear or move after game updates.
- Raw game pointers have uncertain lifetime, so hooks need defensive null checks and best-effort diagnostics.
- `prime/` wrappers are necessary boundary objects for game types.
- Hook code must stay cheap and avoid blocking the frame thread.

Avoidable debt:

- Similar signal paths are implemented separately for toasts, fleet notifications, live-debug events, sync, and battle logs.
- Hook files sometimes own policy, extraction, diagnostics, config decisions, and delivery at once.
- State caches, dedupe windows, worker queues, method-resolution logging, and config parsing are repeatedly hand-rolled.
- New developers have to search for patterns by accident because reusable concepts are not named as modules.

## Magic-Wand Architecture

The target shape is a small set of named concepts with narrow responsibilities:

```text
Entry point
  -> HookRegistry
       -> thin detours capture game signals
       -> SignalDispatcher receives normalized events
            -> feature observers interpret domain-specific meaning
            -> delivery adapters handle OS notifications, live-debug, sync, files
```

The important rule: hooks capture, feature modules interpret, dispatchers route, adapters deliver.

## Reusable Concepts

### Hook Registry

Centralizes hook registration, dependencies, diagnostics, and install summaries.

Responsibilities:

- Resolve IL2CPP classes and methods.
- Install detours.
- Report hook purpose, method found/not found, detour attempted, and outcome.
- Keep one visible list of patch modules and frame-hook ownership.

What stays out:

- Notification copy.
- Hotkey policy.
- Sync serialization.
- Live-debug request handling.

### Signal Dispatcher

Receives normalized runtime signals from hooks and fans them out to subscribers.

Examples of signals:

- `toast.enqueued`
- `toast_fleet.queue_notifications`
- `fleet.state_observed`
- `hotkey.frame_tick`
- `battle.header_observed`
- `entity_group.updated`

This does not need to become a huge framework. A simple typed or tagged dispatcher is enough if it makes event ownership discoverable.

Current lightweight contracts live in `mods/src/patches/signals.h`. Add a
signal when a hook hands normalized runtime data to another module, especially
when the receiving module should not know IL2CPP offsets or hook argument
layout. Keep raw game pointers named `raw_*`, treat them as borrowed for the
current hook call only, and copy stable values such as ids, enum states, and
localized text before crossing async or queued boundaries.

### Feature Observer

Owns game-domain interpretation.

Examples:

- Incoming attack observer maps quick-scan data to hostile/player notifications.
- Fleet state observer maps fleet transitions to arrived/mining/repair notifications.
- Chat observer maps game callbacks to suppress/pass decisions.
- Battle observer maps battle headers to export requests.

Feature observers should return decisions or events, not perform platform delivery directly.

Add or extend a feature observer when the code is deciding what a signal means
for gameplay or mod behavior: consume/pass/filter a toast, classify an incoming
attack, convert fleet state changes into notification intents, or decide whether
a hotkey frame should allow the original game update. Observers may call a
delivery adapter after a decision in the current incremental code, but new code
should prefer returning a decision/event first so delivery can move behind a
thin adapter later.

### Delivery Adapter

Owns side effects outside game interpretation.

Examples:

- OS notification delivery.
- Live-debug recent-event buffer.
- HTTP sync transport.
- File export and persistent dedupe files.

Delivery adapters should be replaceable with test doubles.

Add a delivery adapter when the code performs a side effect outside gameplay
interpretation: OS notification APIs, live-debug storage, HTTP transport, file
export, or persistent dedupe state. Adapters should receive already-interpreted
events or text rather than raw hook pointers.

### Dispatch Table

Maps enum/state/config keys to behavior in data rather than switch sprawl.

Existing good example:

- Hotkey action table.

Places to apply:

- Toast state titles and config flags.
- Notification routing by toast state.
- Sync target processing.
- Live-debug endpoint routing.

### Cache And Dedup Primitive

One bounded primitive for TTL windows, max-entry eviction, and key construction.

Places to apply:

- Recent toast pointer dedupe.
- Incoming attack identity dedupe.
- Battle-log sent IDs.
- Sync state-delta checks.
- Name/spec lookup caches.

### Work Queue

One async producer/consumer primitive with lifecycle diagnostics and shutdown behavior.

Places to apply:

- Notification queue.
- Sync upload queue.
- Battle-log enrichment queue.
- Any future file or HTTP background work.

### Config Metadata

One declarative place for defaults, aliases, validation, runtime-var names, docs, and deprecation warnings.

Places to apply first:

- Hotkey shortcut keys.
- Notification toggles.
- Sync options.
- Developer/live-debug options.

### Platform Bridge

Thin wrapper for platform-specific behavior.

Examples:

- Windows WinRT notifications.
- Window title APIs.
- Config/log paths.
- macOS launcher/dylib differences.

### Diagnostics Facade

Consistent logging and debug-event recording for runtime support.

Responsibilities:

- Structured event names.
- Config-gated verbosity.
- Hook health summaries.
- "Known ignored" versus new boot errors.

Diagnostics should never be required for gameplay behavior to work.

### Pure Decision Models

Small structs in, decisions out.

Places to apply:

- Hotkey input snapshot -> action request.
- Toast metadata -> notification/filter decision.
- Fleet observation -> fleet event.
- Incoming attack params -> attack kind/title/body/dedupe key.
- Config TOML node -> parsed config value plus warnings.

These are the main test seams.

## Concept Map By Current Hotspot

| Current Area | Problem Shape | Target Concepts |
| --- | --- | --- |
| `parts/live_debug.cc` | dispatcher, event store, observers, serializers, hook tick, file requests in one file | Signal Dispatcher, Delivery Adapter, Diagnostics Facade |
| `parts/sync.cc` | payload builders, queues, transport, state deltas, battle logs in one file | Work Queue, Delivery Adapter, Cache And Dedup, Feature Observer |
| `notification_service.cc` | toast routing, queue batching, text cleanup, WinRT delivery | Signal Dispatcher, Delivery Adapter, Dispatch Table |
| `fleet_notifications.cc` | fleet state machine, incoming attack policy, message copy, dedupe | Feature Observer, Cache And Dedup, Pure Decision Models |
| `hotkey_router.cc` / `hotkey_dispatch.cc` | config decisions, frame routing, action effects, fallthrough semantics | Pure Decision Models, Dispatch Table, Diagnostics Facade |
| `parts/hotkeys.cc` | hook install plus frame fanout plus Scopely shortcut init semantics | Hook Registry, Signal Dispatcher |
| `config.cc` / `config.h` | growing struct, aliases, validation, docs, runtime vars | Config Metadata |
| `object_tracker.cc` | tracker core, GC glue, hook install, signature scan | Cache And Dedup, Hook Registry, Pure Core |

## Cleanup Issue Set

The issues are ordered so early work stabilizes current behavior and creates reusable seams before large files are split.

### 1. Untangle Hotkey Fallthrough Semantics

Separate startup shortcut registration from per-frame key fallthrough. Add logs explaining why original `ShortcutsManager.InitializeActions` is called or suppressed.

Also keep Escape-exit suppression owned by the actual back-button seam (`SectionManager.BackButtonPressed`) instead of the generic `ScreenManager.Update` return path; the latter is too lossy once `allow_key_fallthrough` is involved.

Concepts: Pure Decision Models, Diagnostics Facade.

### 2. Introduce Hook Registry And Hook Health Summaries

Create a small registration wrapper/table that records method resolution, detour attempts, hook purpose, and install outcome.

Concepts: Hook Registry, Diagnostics Facade.

### 3. Create A Frame Tick Coordinator

Move `ScreenManager.Update` ownership into one coordinator that invokes hotkeys, live-debug, and future per-frame observers independently.

Concepts: Hook Registry, Signal Dispatcher.

### 4. Fix Config Key Hygiene And Add Alias Warnings

Add canonical `set_hotkeys_disable`, keep deprecated `set_hotkeys_disble` as an alias, and warn clearly.

Concepts: Config Metadata, Diagnostics Facade.

### 5. Extract Config Metadata For Hotkeys And Notifications

Begin a metadata table for defaults, aliases, validation, runtime vars, and docs. Start with hotkeys and notifications before touching all config.

Concepts: Config Metadata.

### 6. Create A Generic Bounded TTL Cache/Deduper

Extract bounded TTL/key dedupe into a reusable helper and migrate recent toast dedupe and incoming attack dedupe first.

Concepts: Cache And Dedup Primitive.

### 7. Create A Toast Dispatcher

Unify toast processing into one decision tree returning pass-through, consumed, or filtered. Keep feature policy in feature modules.

Concepts: Signal Dispatcher, Dispatch Table, Feature Observer.

### 8. Split Notification Service Into Router, Queue, Text, And Platform Delivery

Keep the public API stable while extracting queue/coalescing, text cleanup/localization fallback, and WinRT delivery.

Concepts: Delivery Adapter, Work Queue, Dispatch Table.

### 9. Lock Incoming Attack Behind Pure Policy Tests

Extract attacker kind mapping, title/body selection, target identity, and dedupe-key construction. Preserve queue-only production source.

Concepts: Pure Decision Models, Feature Observer, Cache And Dedup Primitive.

### 10. Create Live-Debug Event Dispatcher And Event Store

Move event recording and bounded recent-event storage out of `parts/live_debug.cc`. Keep hook tick wiring thin.

Concepts: Signal Dispatcher, Diagnostics Facade, Delivery Adapter.

### 11. Split Live-Debug Observers And Serializers

Move observation structs, collectors, and JSON serialization into separate modules that accept plain data where possible.

Concepts: Feature Observer, Pure Decision Models.

### 12. Create Async Work Queue Primitive

Extract a reusable producer/consumer queue with metrics, lifecycle logging, and shutdown behavior.

Concepts: Work Queue, Diagnostics Facade.

### 13. Split Sync Into Payload Builders, Scheduler, Transport, And Battle Logs

Reduce `parts/sync.cc` by moving domain payload construction, queueing, HTTP transport, and battle-log enrichment apart.

Concepts: Work Queue, Delivery Adapter, Feature Observer.

### 14. Make Hotkey Routing More Data-Driven And Testable

Model input snapshot plus UI state into action requests, then keep effectful game calls behind facades.

Concepts: Dispatch Table, Pure Decision Models.

### 15. Extract Object Tracker Core From GC And Hook Glue

Keep tracker data structures testable and isolate GC/finalizer/signature-scan hazards.

Concepts: Hook Registry, Cache And Dedup Primitive, Pure Core.

### 16. Add Runtime Regression Recipes To AX Docs

Document structured smoke checks for hotkeys, notification routing, live-debug, hook health, and incoming attacks.

Concepts: Diagnostics Facade.

## Non-Goals

- Do not build a giant enterprise event bus before smaller seams exist.
- Do not hide IL2CPP pointer hazards behind abstractions that make lifetime look safer than it is.
- Do not migrate every config field at once.
- Do not merge sync, live-debug, and OS notifications into one delivery system until each has clear adapters.
- Do not reintroduce low-signal incoming attack fallback paths.

## Success Criteria

- A new contributor can find where hooks are registered and why each exists.
- A new feature starts by choosing an existing concept: hook, signal, observer, delivery adapter, cache, queue, config metadata, or pure decision model.
- Runtime logs can answer hook/config failures without reading source.
- Pure policy tests protect hotkeys, notification routing, incoming attacks, config parsing, and dedupe behavior.
- Large files shrink because responsibilities move to named modules, not because code is shuffled into arbitrary folders.
