# Scopely Native Shortcut Callback Audit

Source folder: `D:\dev\stfc-mod`

Status: source/dump audit for central dispatcher follow-up. This is intentionally an audit and planning slice, not a behavior change.

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
- `FleetBarViewController.RequestSelect(...)` and `ElementAction(...)`: defensive downstream fleet-selection guards.

## Current Conclusion

`ShortcutsManager.SelectShip(int)` was the right low-level seam for the confirmed `ALT-1` fallthrough. The remaining native shortcut callbacks should not be hooked one by one blindly. First classify each callback into one of these buckets:

- `covered`: current upstream or downstream guard already handles the known conflict.
- `dispatcher-owned`: the mod has an equivalent canonical action and should own suppression when its dispatcher consumes the chord.
- `native-owned`: Scopely behavior has no current mod equivalent; allow it unless a consumed mod chord conflicts.
- `probe-needed`: likely overlap, but we need live trace before adding hooks.

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
| `OnShipManageAction` | dispatcher-owned, probe-needed | `show_ships`, maybe native manage selected ship | Add trace/probe before hook. If it fires for `N`/manage shortcuts, route through dispatcher ownership. |
| `OnShipLocateAction` | dispatcher-owned, probe-needed | double-tap locate, `select_shipN`, `select_current` | Do not hook yet. Need live evidence on which native bind invokes it and whether it bypasses mod double-tap policy. |
| `OnShipRecallAction` | dispatcher-owned, probe-needed | `fleet_service`, `action_recall`, `action_recall_cancel` | High-risk because recall/cancel is stateful. Add live probe before suppression. |

### Navigation And Panel Deep Links

| Callback | Current Status | Mod Overlap |
| --- | --- | --- |
| `OnInteriorAction` | dispatcher-owned | `show_stationinterior` |
| `OnExteriorAction` | dispatcher-owned | `show_stationexterior` |
| `OnSystemAction` | dispatcher-owned | `show_system` |
| `OnGalaxyAction` | dispatcher-owned | `show_galaxy` |
| `OnEventsAction` | dispatcher-owned | `show_events` |
| `OnShopOffersAction` | native-owned/probe-needed | no direct canonical action; near `show_gifts` |
| `OnGiftsAction` | dispatcher-owned | `show_gifts` |
| `OnHelpAllianceAction` | dispatcher-owned | `show_alliance_help` |
| `OnShipsAction` | dispatcher-owned | `show_ships` |
| `OnOfficersAction` | dispatcher-owned | `show_officers` |
| `OnCommandAction` | dispatcher-owned | `show_commander` |
| `OnFactionsAction` | dispatcher-owned | `show_factions` |
| `OnItemsAction` | dispatcher-owned | `show_inventory` |
| `OnRefineryAction` | dispatcher-owned | `show_refinery` |
| `OnAllianceAction` | dispatcher-owned | `show_alliance` |
| `OnInboxAction` | native-owned | no current canonical action |
| `OnMissionsAction` | dispatcher-owned | `show_missions` |
| `OnChallengeTrackAction` | dispatcher-owned | `show_qtrials` or related event/challenge surface |
| `OnAwayTeamsAction` | dispatcher-owned | `show_awayteam` |
| `OnDailyGoalsAction` | dispatcher-owned | `show_daily` |
| `OnFieldTrainingsAction` | native-owned | no current canonical action |
| `OnChallengesAction` | native-owned/probe-needed | may overlap `show_qtrials`; name is ambiguous |
| `OnResearchLandingAction` | dispatcher-owned | `show_research` |
| `OnConsumablesAction` | dispatcher-owned | `show_exocomp` |
| `OnPeaceShieldAction` | native-owned | no current canonical action |
| `OnArchivesAction` | native-owned | no current canonical action |

This is the biggest remaining overlap group. The mod already implements most panel navigation in `hotkey_dispatch.cc`, so consumed dispatcher chords should not also run these native deep links. Current `LateUpdate` suppression may already cover them, but this should be proven with live probes before adding many hooks.

### Chat Shortcuts

| Callback | Current Status | Mod Overlap | Recommendation |
| --- | --- | --- | --- |
| `OnChatAction` | dispatcher-owned, probe-needed | `show_chat` | Add live probe. Verify whether native chat opens/focuses differently from mod chat routing. |
| `OnSideChat` | dispatcher-owned, probe-needed | `show_chatside1`, `show_chatside2` | Add live probe. This is a likely fallthrough source for modified chat binds. |

No `ShortcutsManager` callback for `select_chatglobal`, `select_chatalliance`, or `select_chatprivate` appeared in the dump, so channel selection looks mod-owned rather than a Scopely shortcut overlap.

### Native-Only UI Toggles

| Callback | Current Status | Notes |
| --- | --- | --- |
| `OnToggleBattleViewAction` / `ToggleNavigationFilter` | native-owned/probe-needed | Same RVA in dump, likely one native feature. No canonical mod action today. |
| `OnShowKeybindingsAction` | native-owned | Native keybind visualization. Let it pass unless a mod chord was consumed. |
| `OnUseFleetCommanderAbility` | native-owned | No canonical mod action. Do not suppress without explicit product decision. |
| `OnToggleArenaScores` | native-owned | No canonical mod action. Do not suppress without explicit product decision. |

## Probe Slice Added

Observation-only hook definitions were added for:

- `ShortcutsManager.OnShipManageAction`
- `ShortcutsManager.OnShipLocateAction`
- `ShortcutsManager.OnShipRecallAction`
- `ShortcutsManager.OnChatAction`
- `ShortcutsManager.OnSideChat`

They log:

```text
[HotkeyProbe] native-shortcut callback=...
```

The probes intentionally always call the original callback. They also intentionally do not refresh native shortcut suppression before logging, because refreshing state inside a probe could affect existing downstream guards and turn the probe into a behavior change.

Important: these probes are currently compile-time disabled after `OnShipRecallAction` crashed the game when the ship-recall shortcut fired. The likely cause is ABI mismatch for `InputAction.CallbackContext`: the dump reports it as a by-value 16-byte struct, but Windows x64 passes that kind of struct via an implicit pointer. A normal C++ by-value SPUD hook is not safe for this callback family.

Current boot expectation for the ABI experiment:

- `ShortcutsManager.OnShipManageAction`: skipped, compile-time disabled.
- `ShortcutsManager.OnShipLocateAction`: skipped, compile-time disabled.
- `ShortcutsManager.OnShipRecallAction`: skipped, compile-time disabled.
- `ShortcutsManager.OnChatAction`: installed with pointer-shaped context logging only.
- `ShortcutsManager.OnSideChat`: skipped, compile-time disabled.

Do not re-enable the ship or side-chat hooks until the pointer-shaped `OnChatAction` probe survives a live keypress.

## Callback-Context Probe Recovery Process

What happened:

1. We added observation-only SPUD hooks for five `ShortcutsManager.On*Action(InputAction.CallbackContext)` methods.
2. The hooks compiled and installed.
3. Pressing `B`, which triggered the ship-recall native callback on this config, crashed the game.
4. We immediately set `kEnableNativeShortcutProbeHooks = false`.
5. We rebuilt `mods` and ran `pwsh -NoProfile -File .ax\ax.ps1 cycle`.
6. Boot then showed the five probe hooks skipped as `compile-time disabled`.
7. Manual retest confirmed `B` works again.

Keep this as the future probe loop:

1. Add or change only one callback-context probe at a time.
2. Prefer a low-impact callback first, not recall or fleet mutation.
3. Build with `xmake build mods`.
4. Deploy with `pwsh -NoProfile -File .ax\ax.ps1 cycle`.
5. Confirm boot shows exactly one probe installed.
6. Press only the corresponding key once.
7. Check `community_patch.log` for `[HotkeyProbe]`.
8. If it crashes, immediately disable that one probe, rebuild, cycle, and document the exact callback and signature shape.

Candidate ABI-safe shapes to test later:

```cpp
struct InputActionCallbackContext {
  void*   state;
  int32_t action_index;
};

// Unsafe shape already tried for this callback family:
void Probe(auto original, void* _this, InputActionCallbackContext context);

// Candidate shape 1: pointer-shaped ABI mirror.
void Probe(auto original, void* _this, InputActionCallbackContext* context);

// Candidate shape 2: raw hidden-reference pointer with no field reads until proven.
void Probe(auto original, void* _this, void* context);
```

Current experiment uses candidate shape 2 for `OnChatAction` only. It logs `context_ptr` and calls `original(_this, context)`. Do not dereference `context`, do not copy the struct, and do not inspect fields until this pointer-only pass survives one live callback.

## Recommended Next Slice

Next, gather live evidence through a safer path for:

- `OnShipManageAction`
- `OnShipLocateAction`
- `OnShipRecallAction`
- `OnChatAction`
- `OnSideChat`

Then add panel/deeplink probes only if `ShortcutsManager.LateUpdate` does not already block consumed dispatcher chords for the current five callbacks.

Safer probe options:

- Hook a lower-level shared method with simple pointer/integer parameters when one exists.
- Use `ShortcutsManager.LateUpdate` plus dispatcher/native-suppression logs to infer callback coverage before detouring callback-context methods.
- If callback-context hooks are still needed, first make a dedicated ABI-safe hook for one low-impact callback using a pointer-shaped `InputAction.CallbackContext` argument, verify boot and one keypress, then repeat for the next callback.

The probe output should answer:

- Which callbacks fire for native defaults.
- Whether callback firing happens before or after current native suppression state is set.
- Whether `LateUpdate` suppression is sufficient for chat/ship manage/ship locate/ship recall.
- Whether any callback fires for a mod-owned chord that already has a dispatcher winner.

## Hooking Guidance

Preferred suppression order:

1. Use `ShortcutsManager.LateUpdate` when it blocks a consumed native shortcut generically.
2. Use one shared low-level implementation seam when present, as with `SelectShip(int)`.
3. Use individual `On*Action(InputAction.CallbackContext)` hooks only when there is no lower-level shared method and live evidence proves fallthrough.

Do not hook all `On*Action` callbacks just because they exist. macOS does not tolerate sloppy overlapping hooks, and the central dispatcher goal is ownership clarity, not hook volume.

## Open Questions

- Does `ShortcutsManager.LateUpdate` suppression already prevent panel/chat native actions, or only some action processing phases?
- Which Scopely native default bindings overlap with mod defaults on the current game build?
- Does `OnShipLocateAction` participate in native double-tap locate or a separate explicit locate shortcut?
- Are `OnChallengeTrackAction` and `OnChallengesAction` separate surfaces, and which one best maps to `show_qtrials`?
- Can we safely extract the current physical-key snapshot/native-suppression logic into a focused `native_shortcut_guard` module before probing, or should that wait until after the probe confirms the remaining seams?
