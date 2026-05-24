# Scopely Native Shortcut Callback Audit

Source folder: `D:\dev\stfc-mod`

Status: historical source/dump audit plus implementation notes for central dispatcher-owned native shortcut suppression. Post-RCA native seam safety policy lives in [Native Probe Safety](NATIVE_PROBE_SAFETY.md), and seam-specific confidence lives in [Native Seam Ledger](NATIVE_SEAM_LEDGER.md).

## 2026-05-23 Safety Update

The `Shift+Space` / `OnShipLocateAction` crash RCA supersedes the original locate-callback recommendation in this audit. Removing `X(ShipLocate, OnShipLocateAction)` from the generated pointer callback guard list stopped the crash, and the remaining generated pointer callback guard family has since been removed/quarantined from product hook code. Treat `OnShipLocateAction` as a failed/unsafe seam unless a future branch validates it as a one-callback, default-off canary with its own ledger entry. See [Native Probe Safety](NATIVE_PROBE_SAFETY.md) for the current policy and [Native Seam Ledger](NATIVE_SEAM_LEDGER.md) for seeded seam status.

Historical rows below that recommend keeping callbacks covered by the shared pointer-shaped guard are static audit recommendations only. They are not ledger-backed runtime confidence. Before relying on, expanding, restoring, or reintroducing any `ShortcutsManager.On*` callback guard, add a seam-specific ledger entry and follow the safety ladder.

## 2026-05-16 Historical Implementation Snapshot

At the time of this audit, fallback-mode native shortcut suppression used a centralized pointer-shaped `ShortcutsManager.On*` callback guard registry in `mods/src/patches/parts/hotkeys.cc`. Each registered callback routed through the same dispatcher ownership check and skipped the original native callback only when the callback came from Unity's input system and the unified dispatcher owned the current physical chord. That generated registry is now removed/quarantined from product hook code.

This replaces the earlier single-callback experiment. UI/direct callbacks with a zero/default `InputAction.CallbackContext` are allowed through so a held mod key does not accidentally swallow a legitimate button-driven action. The by-value `InputAction.CallbackContext` hook shape remains unsafe and should not be reintroduced.

Post-RCA safety note: this implementation shape is now classified as a removed/quarantined broad generated-family risk surface in the ledger. ABI shape and static callback membership do not prove that any specific callback is safe, product-ready, or safe to call through.

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

Mod hooks described by this historical audit:

- `ShortcutsManager.InitializeActions`: decides whether Scopely native shortcut actions initialize.
- `ShortcutsManager.LateUpdate`: skips native shortcut processing when the dispatcher consumed the current physical chord.
- `ShortcutsManager.SelectShip(int)`: suppresses native number-key ship selection for consumed modified chords.
- `ShortcutsManager.On*Action(InputAction.CallbackContext)` pointer callbacks: historically registered as one guard family for fallback-mode native shortcut callbacks that can fire before the frame router. The family is now removed/quarantined and tracked as broad generated-family risk in the ledger, not as per-callback confidence.
- `FleetBarViewController.RequestSelect(...)` and `ElementAction(...)`: defensive downstream fleet-selection guards.

## Historical Implementation Model

`ShortcutsManager.SelectShip(int)` remains the right low-level seam for confirmed number-key ship-selection fallthrough. The broader `ShortcutsManager.On*` callback family was historically intercepted through one shared pointer-shaped registry, not bespoke per-callback logic; that registry is now removed/quarantined.

Callbacks were classified into these product buckets as static/action-ownership notes, not native seam confidence. Under the old runtime rule, if the callback came from Unity's input system and the unified dispatcher owned the current physical chord, the guard skipped the native callback; otherwise it called through.

- `covered`: current upstream or downstream guard already handles the known conflict.
- `dispatcher-owned`: the static audit found an equivalent canonical mod action; any native callback suppression still needs ledger-backed seam confidence.
- `native-owned`: the static audit found no current mod equivalent; do not suppress without explicit product need and ledger-backed seam confidence.
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
| `OnShipManageAction` | static overlap / unclassified seam | `show_ships`, maybe native manage selected ship | Historical shared-guard recommendation only; requires a ledger-backed safety review before relying on this callback seam. |
| `OnShipLocateAction` | failed/unsafe | double-tap locate, `select_shipN`, `select_current` | Keep out of the shared pointer-shaped guard. Only revisit as a one-callback, default-off canary with a native seam ledger entry. |
| `OnShipRecallAction` | static overlap / unclassified seam | `fleet_service`, `action_recall`, `action_recall_cancel` | Historical shared-guard recommendation only; requires a ledger-backed safety review before relying on this callback seam. Recall/cancel is stateful, so be conservative. |

### Navigation And Panel Deep Links

The mod implements most panel navigation in `hotkey_dispatch.cc`, so consumed dispatcher chords should not also run native deep links. Historical note: the May 16 design expected the shared pointer-shaped guard to cover the registered native callback family when fallback-mode native shortcuts were active. Post-RCA, that expectation requires per-callback ledger-backed safety review and reintroduction through a one-callback allowlist before it is treated as product guidance.

### Chat Shortcuts

| Callback | Current Status | Mod Overlap | Recommendation |
| --- | --- | --- | --- |
| `OnChatAction` | static overlap / unclassified seam | `show_chat` | Historical shared-guard recommendation only; requires a ledger-backed safety review before relying on this callback seam. |
| `OnSideChat` | static overlap / unclassified seam | `show_chatside1`, `show_chatside2` | Historical shared-guard recommendation only; requires a ledger-backed safety review before relying on this callback seam. |

No `ShortcutsManager` callback for `select_chatglobal`, `select_chatalliance`, or `select_chatprivate` appeared in the dump, so channel selection looks mod-owned rather than a Scopely shortcut overlap.

### Native-Only UI Toggles

| Callback | Current Status | Notes |
| --- | --- | --- |
| `OnToggleBattleViewAction` / `ToggleNavigationFilter` | native-owned static classification | Same RVA in dump, likely one native feature. No canonical mod action today. Do not suppress without explicit product and ledger-backed safety review. |
| `OnShowKeybindingsAction` | native-owned static classification | Native keybind visualization. Do not suppress without explicit product and ledger-backed safety review. |
| `OnUseFleetCommanderAbility` | native-owned static classification | No canonical mod action. Do not suppress without explicit product and ledger-backed safety review. |
| `OnToggleArenaScores` | native-owned static classification | No canonical mod action. Do not suppress without explicit product and ledger-backed safety review. |

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

The historical guard treated `state != nullptr` as an input-system callback and allowed zero/default contexts through as UI/direct invocations. Do not restore by-value callback hooks. Pointer-shaped ABI handling is a minimum shape requirement only; it is not proof that a callback seam is safe.

## Hooking Guidance

Preferred suppression order:

1. Use `ShortcutsManager.LateUpdate` when it blocks a consumed native shortcut generically.
2. Use one shared low-level implementation seam when present, as with `SelectShip(int)`.
3. Only consider a pointer-shaped `On*Action(InputAction.CallbackContext)` guard when there is no lower-level shared method, dispatcher ownership requires suppression, and the specific callback has ledger-backed seam confidence.

Do not hook all `On*Action` callbacks just because they exist. Do not install broad generated guard lists for discovery. macOS does not tolerate sloppy overlapping hooks, and the central dispatcher goal is ownership clarity, not hook volume.

## Open Questions

- Does `ShortcutsManager.LateUpdate` suppression already prevent every panel/chat native action, or only some action processing phases?
- Which Scopely native default bindings overlap with mod defaults on the current game build?
- What lower-risk signal should replace `OnShipLocateAction` for native locate/fallthrough detection, if this question still matters?
- Are `OnChallengeTrackAction` and `OnChallengesAction` separate surfaces, and which one best maps to `show_qtrials`?
- Can we safely extract the current physical-key snapshot/native-suppression logic into a focused `native_shortcut_guard` module before any future private diagnostics?
