# Scopely Native Shortcut Callback Audit

Source folder: `D:\dev\stfc-mod`

Status: source/dump audit plus implementation notes for central dispatcher-owned native shortcut suppression.

## 2026-05-16 Implementation Update

Fallback-mode native shortcut suppression uses a centralized pointer-shaped `ShortcutsManager.On*` callback guard registry in `mods/src/patches/parts/hotkeys.cc`. Each registered callback routes through the same dispatcher ownership check and skips the original native callback only when the callback came from Unity's input system and the unified dispatcher owns the current physical chord.

This replaces the earlier single-callback experiment. UI/direct callbacks with a zero/default `InputAction.CallbackContext` are allowed through so a held mod key does not accidentally swallow a legitimate button-driven action. The by-value `InputAction.CallbackContext` hook shape remains unsafe and should not be reintroduced.

The public build no longer exposes per-key or per-callback input trace logging for this path. Diagnostics should use sanitized hook health, settings, and operational status from AX.

## Evidence Used

Commands:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 dump-class -Name ShortcutsManager -Exact -Members -Compact -TokenBudget 4000
pwsh -NoProfile -File .ax\ax.ps1 dump-pack -Class ShortcutsManager -Exact -Compact -TokenBudget 9000
pwsh -NoProfile -File .ax\ax.ps1 dump-class -Name InputActionSettings -Exact -Members -Compact -TokenBudget 5000
pwsh -NoProfile -File .ax\ax.ps1 dump-class -Name ShortcutsSettings -Exact -Members -Compact -TokenBudget 4000
```

Key dump facts:

- `Digit.Prime.GameInput.ShortcutsManager` exists and inherits `MonoSingleton<ShortcutsManager>`.
- It owns `_actions: InputActionAsset`, `_actionSettings: List<InputActionSettings>`, `_supportedActions: Dictionary<Guid, InputAction>`, and `_areActionsInitialized`.
- It exposes `InitializeActions`, `LateUpdate`, `ActivateShortcuts`, `DeactivateAllShortcuts`, and many `On*Action(InputAction.CallbackContext)` callbacks.
- The ship-number callbacks are `OnShipAAction` through `OnShipHAction`; the private shared implementation is `SelectShip(int)`.

Current mod hooks:

- `ShortcutsManager.InitializeActions`: decides whether Scopely native shortcut actions initialize.
- `ShortcutsManager.LateUpdate`: skips native shortcut processing when the dispatcher consumed the current physical chord.
- `ShortcutsManager.SelectShip(int)`: suppresses native number-key ship selection for consumed modified chords.
- `ShortcutsManager.On*Action(InputAction.CallbackContext)` pointer callbacks: registered as one guard family for fallback-mode native shortcut callbacks that can fire before the frame router.
- `FleetBarViewController.RequestSelect(...)` and `ElementAction(...)`: defensive downstream fleet-selection guards.

## Current Implementation Model

`ShortcutsManager.SelectShip(int)` remains the right low-level seam for confirmed number-key ship-selection fallthrough. The broader `ShortcutsManager.On*` callback family is intercepted through one shared pointer-shaped registry, not bespoke per-callback logic.

Callbacks classify into these product buckets, but the runtime rule is uniform: if the callback came from Unity's input system and the unified dispatcher owns the current physical chord, skip the native callback; otherwise call through.

- `covered`: current upstream or downstream guard already handles the known conflict.
- `dispatcher-owned`: the mod has an equivalent canonical action and should own suppression when its dispatcher consumes the chord.
- `native-owned`: Scopely behavior has no current mod equivalent; allow it unless a consumed mod chord conflicts.
- `needs-private-diagnostics`: likely overlap, but any deep diagnostic work belongs in a private-only patch or worktree outside the public build path.

## Callback Inventory

### Lifecycle And Binding Management

| Callback | Current Status | Notes |
| --- | --- | --- |
| `Initialize` | native-owned | Sets up manager state; do not hook for action suppression. |
| `LateUpdate` | covered | Current hook refreshes physical-key suppression and can skip native shortcut update. |
| `InitializeActions` | covered | Current hook owns Scopely shortcut startup policy. |
| `UninitializeActions` | native-owned | Cleanup path; no dispatcher action. |
| `ActivateShortcuts` / `DeactivateAllShortcuts` | native-owned | Allowed-shortcut set management. Hook only if startup policy proves insufficient. |
| `LoadBindings` / `SaveBindings` / `ResetToDefaults` | native-owned | User/native binding persistence; not a dispatcher action. |
| `ApplyBindingOverride` / `RemoveBindingOverride` / `TriggerKeybindOverrideChange` | native-owned | Native keybinding UI support. Do not touch without a settings UI task. |

### Ship And Fleet Shortcuts

| Callback | Current Status | Mod Overlap | Recommendation |
| --- | --- | --- | --- |
| `OnShipAAction` ... `OnShipHAction` | covered | `select_ship1` ... `select_ship8` | Keep suppressing at `SelectShip(int)`, not each public callback. |
| `SelectShip(int)` | covered | `select_shipN` and modified-number conflicts | This is the confirmed seam for number-key fallthrough. |
| `OnShipManageAction` | dispatcher-owned | `show_ships`, maybe native manage selected ship | Keep covered by the shared pointer-shaped guard. |
| `OnShipLocateAction` | dispatcher-owned | double-tap locate, `select_shipN`, `select_current` | Keep covered by the shared pointer-shaped guard. |
| `OnShipRecallAction` | dispatcher-owned | `fleet_service`, `action_recall`, `action_recall_cancel` | Keep covered by the shared pointer-shaped guard; be conservative because recall/cancel is stateful. |

### Navigation And Panel Deep Links

The mod implements most panel navigation in `hotkey_dispatch.cc`, so consumed dispatcher chords should not also run native deep links. The shared pointer-shaped guard covers the registered native callback family when fallback-mode native shortcuts are active.

### Chat Shortcuts

| Callback | Current Status | Mod Overlap | Recommendation |
| --- | --- | --- | --- |
| `OnChatAction` | dispatcher-owned | `show_chat` | Keep covered by the shared pointer-shaped guard. |
| `OnSideChat` | dispatcher-owned | `show_chatside1`, `show_chatside2` | Keep covered by the shared pointer-shaped guard. |

No `ShortcutsManager` callback for `select_chatglobal`, `select_chatalliance`, or `select_chatprivate` appeared in the dump, so channel selection looks mod-owned rather than a Scopely shortcut overlap.

### Native-Only UI Toggles

| Callback | Current Status | Notes |
| --- | --- | --- |
| `OnToggleBattleViewAction` / `ToggleNavigationFilter` | native-owned | Same RVA in dump, likely one native feature. No canonical mod action today. |
| `OnShowKeybindingsAction` | native-owned | Native keybind visualization. Let it pass unless a mod chord was consumed. |
| `OnUseFleetCommanderAbility` | native-owned | No canonical mod action. Do not suppress without explicit product decision. |
| `OnToggleArenaScores` | native-owned | No canonical mod action. Do not suppress without explicit product decision. |

## ABI Guidance

`InputAction.CallbackContext` is a 16-byte struct in the IL2CPP dump:

```cpp
struct InputActionCallbackContext {
  void*   state;
  int32_t action_index;
  int32_t binding_index;
};
```

On the Windows x64 ABI, this kind of struct argument is passed via caller-provided storage. The public hook shape should therefore accept a raw pointer and inspect only the minimum discriminator needed for behavior:

```cpp
void Guard(auto original, void* _this, void* context);
```

The guard treats `state != nullptr` as an input-system callback and allows zero/default contexts through as UI/direct invocations. Do not restore by-value callback hooks.

## Hooking Guidance

Preferred suppression order:

1. Use `ShortcutsManager.LateUpdate` when it blocks a consumed native shortcut generically.
2. Use one shared low-level implementation seam when present, as with `SelectShip(int)`.
3. Use the shared pointer-shaped `On*Action(InputAction.CallbackContext)` guard only when there is no lower-level shared method and dispatcher ownership requires suppression.

Do not hook all `On*Action` callbacks just because they exist. macOS does not tolerate sloppy overlapping hooks, and the central dispatcher goal is ownership clarity, not hook volume.

## Open Questions

- Does `ShortcutsManager.LateUpdate` suppression already prevent every panel/chat native action, or only some action processing phases?
- Which Scopely native default bindings overlap with mod defaults on the current game build?
- Does `OnShipLocateAction` participate in native double-tap locate or a separate explicit locate shortcut?
- Are `OnChallengeTrackAction` and `OnChallengesAction` separate surfaces, and which one best maps to `show_qtrials`?
- Can we safely extract the current physical-key snapshot/native-suppression logic into a focused `native_shortcut_guard` module before any future private diagnostics?
