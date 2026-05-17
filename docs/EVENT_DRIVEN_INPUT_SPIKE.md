# Event-Driven Input Spine Spike

Source folder: `D:\dev\stfc-mod`

Status: experimental architecture note for fork-side spikes. This is intentionally more aggressive than the current unified input implementation plan.

## Problem Statement

The current runtime still treats `ScreenManager.Update` as both the input collection point and the action execution point. That is convenient because Unity's `GetKeyDown` semantics are frame-based, but it keeps the mod mentally tied to the game's input loop.

The better target is:

- collect user input as mod-owned events;
- treat modifiers as state, never as standalone actions;
- queue semantic input events only when a non-modifier trigger key changes;
- execute game mutations only on a safe game-thread hook phase;
- keep Scopely shortcut initialization optional and separate from mod dispatch.

In that model, `Alt+1` is not “Alt fires, then 1 fires.” It is one `KeyChordDown{key=1, modifiers=Alt}` event. `Alt+Ctrl` alone is not an event because there is no non-modifier trigger key.

## Important Constraint

Game object mutation still needs a game-owned thread/phase.

Even if input collection moves out of `ScreenManager.Update`, actions such as opening cargo panels, selecting fleets, changing sections, or pressing Unity widgets should be drained on a known game-thread seam. A passive collector can run outside the frame loop; a game executor should not blindly mutate IL2CPP objects from an OS keyboard callback.

So the target is not “no frame involvement.” It is “no game shortcut dependency and no game input polling dependency on the dispatch path.” A frame/hook seam remains as the pump that safely drains already-decided mod events.

## Candidate Architecture

```text
OS/window input source OR Unity polling source
  -> InputCollector
  -> ModifierState
  -> ChordEventQueue
  -> InputDispatcher
  -> SemanticActionQueue
  -> GameThreadExecutor phase
  -> OriginalCallPolicy
```

### Input Sources

1. `UnityInputSource`
   - Current compatible backend.
   - Polls watched non-modifier keys and modifier state from Unity input APIs.
   - Good fallback for Windows and macOS.

2. `Win32MessageInputSource` experiment
   - Subclass the game window procedure or register raw input for the game window.
   - Converts `WM_KEYDOWN`, `WM_KEYUP`, mouse button events, and focus changes into mod-owned state.
   - Never sends synthetic key events to the game.
   - Pushes chord events into a queue; does not touch game objects.

3. `MacEventInputSource` future experiment
   - Equivalent platform-owned collector for macOS.
   - Must be designed separately; do not assume every non-Windows path is macOS-safe.

### Input Collector Rules

- Only non-modifier keys and buttons can produce `ChordDown`, `ChordPressed`, or `ChordUp` events.
- Modifier keys update `ModifierState` only.
- `CTRL`, `ALT`, `SHIFT`, `CTRL-ALT`, and similar modifier-only binds are invalid configuration.
- Exact modifier matching is the default.
- The collector stores physical modifier state and exposes effective logical groups.
- Input focus changes clear transient down/repeat state.

### Dispatcher Rules

- The dispatcher consumes events, not raw frame loops.
- Binding lookup is indexed by trigger key and trigger mode.
- Candidate resolution uses the same action schema, context gates, priorities, and conflict groups as the current unified model.
- A handled modified chord can request original-key suppression without enabling Scopely shortcuts.

### Executor Rules

- The executor drains semantic actions on safe game-thread phases.
- UI/game mutations never happen inside an OS input callback.
- Hook phases expose context and object handles; they do not own binding semantics.
- When an action is handled, the executor returns explicit `AllowOriginal`, `SuppressOriginal`, or `NoOpinion`.

## Security and Boundary Notes

This spike stays inside the current mod covenant when it is passive:

- reading keyboard/window input for the mod's own hotkeys is acceptable for this project;
- deciding mod actions from that input is acceptable;
- calling existing mod/game UI functions from established IL2CPP hooks is the current model.

Avoid these in the spike:

- synthesizing OS/game input events;
- installing broad system-wide hooks when the game-window scope is enough;
- mutating IL2CPP objects from the OS callback thread;
- bypassing game networking, authority, cooldowns, combat rules, or protected state.

The spike should be framed as a local UI/input architecture experiment, not as automation or gameplay authority bypass.

## First Spike Branch Shape

Suggested branch name: `spike/event-driven-input-spine`.

Slice 1: Pure event model

- Add `InputEvent`, `InputCollectorState`, and `ChordEventQueue` as pure/testable modules.
- Feed synthetic key up/down events in tests.
- Prove modifier-only changes do not enqueue chord events.
- Prove `Alt+1` enqueues one modified `1` event.
- Prove bare `1` is not emitted when `Alt+1` is emitted.

Slice 2: Runtime adapter without live behavior change

- Add an adapter that converts the current per-frame Unity snapshot into the new event queue.
- Keep live dispatch results identical.
- This de-risks the model before adding platform input hooks.

Slice 3: Windows scoped input source

- Add an opt-in Windows-only input source behind debug config, for example `[input].source = "win32_message"`.
- Scope it to the game window.
- Queue events only; drain on the existing game-thread phase.
- Compare logs between Unity and Win32 sources for the same key sequences.

Slice 4: Retire frame polling when proven

- Once the Windows source is stable, stop polling non-modifier keys every frame on Windows.
- Keep the Unity source as fallback and macOS path.

## Why This Is More Efficient

The current optimized frame path polls watched keys once per frame, which is already reasonable. The event-driven target can be better because idle frames do no key matching at all. Work happens only when a trigger key changes or when held/repeat actions need scheduled ticks.

The biggest win is architectural clarity, not raw CPU time:

- modifiers become state;
- trigger keys become events;
- dispatch becomes deterministic;
- game effects run only at safe seams;
- Scopely input stays out of the mod-owned hotkey path.

## Current Evidence From Alt+1

The runtime checks proved the TOML binding and dispatcher were not the failing point. The action toggled config, but the visible cargo/rewards panel needed an explicit refresh to make the change observable immediately.

Follow-up tracing then showed a second lower-level seam: native `ShortcutsManager.SelectShip(int)` could still run from the physical digit key before Unity frame polling saw the modified chord. The current fix routes native shortcut suppression through the same runtime binding plan using a physical key snapshot at the native shortcut seam. That keeps the logic binding-driven instead of hard-coding `ALT-1`.

The remaining architecture target is unchanged: collect chord intent once, decide action ownership once, and make native/game hooks ask the dispatcher whether their original behavior should continue. See `docs/CENTRAL_INPUT_DISPATCHER_FOLLOWUP.md` for the current handoff checklist.
