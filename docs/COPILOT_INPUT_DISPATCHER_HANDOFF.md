# Copilot Handoff: Native Shortcut Probe And Central Dispatcher

Source folder: `D:\dev\stfc-mod`

Branch at handoff: `feat/notification-audio-policy`

Last committed work:

- `4c376c8 Route native ship selection through input dispatcher`
- Branch is currently ahead of origin by 2 commits.

Current uncommitted work:

- `mods/src/patches/fleet_actions.cc`
- `mods/src/patches/hotkey_router.cc`
- `mods/src/patches/hotkey_router.h`
- `mods/src/patches/input_binding/input_config_bridge.cc`
- `mods/src/patches/parts/hotkeys.cc`
- `mods/src/testable_functions.cc`
- `mods/src/testable_functions.h`
- `tests/src/test_input_binding_core.cc`
- `tests/src/test_input_dispatcher.cc`
- `docs/CENTRAL_INPUT_DISPATCHER_FOLLOWUP.md`
- `docs/SCOPELY_NATIVE_SHORTCUT_CALLBACK_AUDIT.md`

## Current User-Verified Behavior

Confirmed working after the last deploy:

- `ALT-1` toggles cargo/default and no longer selects ship 1.
- Bare `1` still selects ship 1.
- Double-tap ship locate is working after the dispatcher/native suppression changes.
- With the mod TOML binding `show_bookmarks = "B"`, bare `B` now only opens bookmarks through the mod dispatcher and no longer also fires the game's native recall binding.
- `C` / chat works with the current pointer-shaped `OnChatAction` probe installed.
- With the game's own native manage binding explicitly set to `N`, bare `N` now falls through and correctly opens Manage Ship.
- With the mod TOML binding `show_ships = "M"`, `M` also correctly opens Manage Ship through the mod-owned path.

Current deployed DLL was produced by:

```powershell
xmake build stfc-mod-tests
xmake run stfc-mod-tests
xmake build mods
pwsh -NoProfile -File .ax\ax.ps1 cycle
```

Boot after the last cycle was healthy.

## Current Probe State

In `mods/src/patches/parts/hotkeys.cc`, there are five compile-time flags:

```cpp
constexpr bool kEnableNativeShortcutShipManageProbeHook = true;
constexpr bool kEnableNativeShortcutShipLocateProbeHook = true;
constexpr bool kEnableNativeShortcutShipRecallProbeHook = true;
constexpr bool kEnableNativeShortcutChatProbeHook       = true;
constexpr bool kEnableNativeShortcutSideChatProbeHook   = false;
```

`ShortcutsManager.OnShipManageAction`, `ShortcutsManager.OnShipLocateAction`, `ShortcutsManager.OnShipRecallAction`, and `ShortcutsManager.OnChatAction` are currently installed.

The installed pointer-shaped native shortcut callbacks now use a shared suppression-aware helper:

```cpp
template <typename OriginalFn>
void HandleNativeShortcutPointerCallback(OriginalFn original, void* _this, void* context, const char* callback)
{
   hotkey_router_refresh_native_shortcut_suppression();
   log_native_shortcut_probe_pointer(callback, context);
   if (hotkey_router_should_suppress_native_shortcuts()) {
      return;
   }

   original(_this, context);
}
```

It logs:

```text
[HotkeyProbe] native-shortcut callback=OnChatAction ...
```

It does not dereference `context`.

`OnSideChat` remains pointer-shaped but compile-time disabled.

## Current Follow-Up From This Session

Grounding and validation completed in `D:\dev\stfc-mod` on branch `feat/notification-audio-policy` using the repo-local `ax` tooling.

What was confirmed:

- `pwsh -NoProfile -File .ax\ax.ps1 status` reported the deployed DLL hash matched the repo build, the game was running, and the log had no errors.
- Boot at `2026-05-14 21:38:41` showed `ShortcutsManager.OnChatAction` installed and `ShortcutsManager.OnSideChat` still skipped as `compile-time disabled`.
- `xmake build mods` passed after one local prep change: the disabled `OnSideChat` probe stub was switched to the same pointer-shaped ABI as `OnChatAction` (`void* context`), but the compile-time flag remains `false` and that build has not been deployed yet.

What did not happen:

- After pressing `C` once in the running game, `pwsh -NoProfile -File .ax\ax.ps1 log -Session -Pattern HotkeyProbe` returned no lines.
- `pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 120` still showed warnings only, with no runtime errors.

What happened next:

- The disabled `OnShipLocateAction` probe stub was also switched to the pointer-shaped ABI and then enabled as the next live candidate.
- `xmake build mods` passed.
- `pwsh -NoProfile -File .ax\ax.ps1 cycle` passed with boot healthy.
- Boot at `2026-05-14 21:44:07` showed `ShortcutsManager.OnShipLocateAction` installed, `OnShipRecallAction` still skipped, `OnChatAction` installed, and `OnSideChat` still skipped.
- Triggering ship locate once produced:

   ```text
   [HotkeyProbe] native-shortcut callback=OnShipLocateAction suppress_native_shortcuts=false context_ptr=0x...
   ```

- `pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 120` still showed warnings only, with no runtime errors after the locate callback fired.

What happened after the binding audit:

- The mod `show_ships` executor was confirmed to bypass `ShortcutsManager` and call the fleet panel controller directly, so pressing the configured mod ship-management key is not a clean native `OnShipManageAction` probe.
- The live TOML did not match the intended test layout: it had `show_bookmarks = "N"`, `show_missions = "M"`, and `show_ships = "P"`.
- The live TOML was updated to free bare `N` for native-manage probing while matching the intended mod layout:

   ```toml
   show_bookmarks = "B"
   show_missions = "P"
   show_ships = "M"
   ```

- The disabled `OnShipManageAction` probe stub was switched to the pointer-shaped ABI and enabled.
- `xmake build mods` passed.
- `pwsh -NoProfile -File .ax\ax.ps1 cycle` passed again with boot healthy at `2026-05-14 22:00:43`.
- No runtime errors appeared after the config reload and manage-probe deploy.
- After the user explicitly set the game's native Manage Ship binding to `N`, pressing bare `N` produced repeated `OnShipManageAction` probe hits:

   ```text
   [HotkeyProbe] native-shortcut callback=OnShipManageAction suppress_native_shortcuts=false context_ptr=0x...
   ```

- The mod-owned `M` ship-manage binding continued to work separately through the mod dispatcher.
- `pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 120` still showed warnings only, with no runtime errors after the native-manage rebound and test presses.

Current interpretation:

- Treat the missing `OnChatAction` probe line as real evidence, not as proof that logging is broken. Earlier sessions in the same log did capture `HotkeyProbe` entries for ship callbacks under the same `scopely_shortcuts=fallback` policy.
- The present config may route `C` entirely through the mod-owned chat path, or otherwise may not reach `ShortcutsManager.OnChatAction` even though the hook is installed.
- Do not enable `OnSideChat` yet on the assumption that `OnChatAction` has already been proven live.
- The pointer-shaped ABI is now live-proven for `OnShipLocateAction`, `OnShipManageAction`, and `OnShipRecallAction` on this build/config.
- The mod-owned `show_ships` binding cannot validate `OnShipManageAction` because it bypasses `ShortcutsManager`.
- Do not assume Scopely native bindings are still at defaults. Future probe tests should explicitly verify or temporarily set the game's own native binding for the callback under test before using a key as evidence.

## Dispatcher / Native Ownership Semantics

Current intended policy:

- Missing binding config means use the registry/default binding. This preserves bootstrap behavior when no TOML exists.
- Explicit `""` and explicit `"NONE"` both mean the action is unbound.
- When `use_scopely_hotkeys = true`, the mod dispatcher should not own hotkeys; native Scopely handling is allowed.
- When `use_scopely_hotkeys = false`, any matching mod dispatcher winner owns that physical key/chord and should suppress native Scopely handling even if `allow_key_fallthrough = true`.
- With `use_scopely_hotkeys = false` and `allow_key_fallthrough = true`, only unbound keys should fall through to native.
- With `use_scopely_hotkeys = false` and `allow_key_fallthrough = false`, unbound keys should do nothing.

Implementation notes from this session:

- `hotkey_router_native_shortcuts_suppressed(...)` now treats dispatcher-owned winners as native-consuming regardless of whether the chord has modifiers.
- Pointer-shaped native shortcut callbacks route through a shared helper that refreshes dispatcher suppression and skips `original` when the dispatcher owns the current key.
- Fleet-bar selection hooks still guard against native fallthrough, but `HandleShipSelection(...)` uses a scoped bypass while making its own `FleetBarViewController::RequestSelect(...)` and `ElementAction(...)` calls. Without that bypass, bare `1` can toggle the ship card without actually selecting the ship.

## Important Crash Lesson

Do not re-enable the by-value callback-context hooks.

This shape compiled and installed but crashed the game when ship recall fired:

```cpp
void Probe(auto original, void* _this, InputActionCallbackContext context);
```

Likely reason: `InputAction.CallbackContext` is a 16-byte value struct in the IL2CPP dump, and Windows x64 passes these by hidden reference/pointer. Treat callback-context methods as pointer-shaped at the native ABI until proven otherwise.

The crash was recovered by:

1. Setting the probe flags back to disabled.
2. Building `mods`.
3. Running `pwsh -NoProfile -File .ax\ax.ps1 cycle`.
4. Verifying boot showed those probes skipped.
5. User confirmed `B` recall worked again.

Later in this session, `OnShipRecallAction` was re-enabled only after changing it to the pointer-shaped ABI (`void* context`) and routing it through the shared native shortcut suppression helper. User verified that `B` bound to mod bookmarks no longer also fires native recall.

## Next Safest Steps

1. Treat `OnShipLocateAction`, `OnShipManageAction`, and `OnShipRecallAction` as the current live-proven pointer-shaped callbacks.

2. Do not treat `OnChatAction` as live-proven yet. The current session pressed `C` once and produced no `HotkeyProbe` line.

3. Before any future callback probe, explicitly verify or temporarily set the game's own native binding for that Scopely action. Do not assume default Scopely bindings or parity with mod TOML bindings.

4. Explain why the current chat keypath should be expected to reach `ShortcutsManager.OnChatAction` on this config before repeating the same live probe.

   Current likely explanations to test:

   - `C` is being handled entirely by the mod dispatcher under the current bindings/policy.

## Detour Single-Owner Policy (issue #97)

`mods/src/patches/hook_registry.h` now provides
`hook_registry_claim_owner(descriptor, module)`, called automatically by every
`HOOK_REGISTRY_SPUD_STATIC_DETOUR` site. The first installer to claim a given
`(assembly, namespace, class, method)` tuple wins. A second claim on the same
target logs `[HookOwnerConflict]` with both owners, refuses to install the
duplicate, and — in `_MODDBG` (releasedbg) builds — calls `std::abort` so the
conflict is impossible to miss in development.

Audit performed against the current tree (see `detour_inventory.txt` produced
during the #97 sprint): no two `SPUD_STATIC_DETOUR` sites target the same
IL2CPP method. The repeated `DataContainer_ParseBinaryObject` and
`GameServerModelRegistry_ProcessResultInternal` trampolines in
`mods/src/patches/parts/sync.cc` resolve different methods on different classes
each time (one `ParseBinaryObject` per data-container class). The bare
`SPUD_STATIC_DETOUR` sites are NOT yet covered by the registry; convert call
sites to `HOOK_REGISTRY_SPUD_STATIC_DETOUR` opportunistically when touching
them.
   - `scopely_shortcuts=fallback` leaves the chat path inactive unless a different native binding or UI state is present.

5. If the goal is ABI validation rather than chat specifically, continue with a low-impact callback that is known to fire under the current fallback policy instead of assuming chat is the next reachable seam.

   `OnShipManageAction` is now proven. The next unresolved chat-adjacent step should start from an explicitly verified native chat binding, not from default assumptions.

6. Only after an actual `OnChatAction` `HotkeyProbe` line appears should `OnSideChat` be enabled and deployed as the next chat-adjacent callback.

   ```powershell
   pwsh -NoProfile -File .ax\ax.ps1 log -Session -Pattern HotkeyProbe
   ```

7. Build and deploy after enabling only one additional probe:

   ```powershell
   git diff --check
   xmake build mods
   pwsh -NoProfile -File .ax\ax.ps1 cycle
   ```

8. Verify boot shows exactly the expected probe hooks installed/skipped.

   ```powershell
   pwsh -NoProfile -File .ax\ax.ps1 log -Boot -Tail 130
   ```

9. Ask the user to press only the relevant key once.

   Current relevant key is whatever in-game Scopely binding was explicitly verified for that callback.

10. Check logs:

   ```powershell
   pwsh -NoProfile -File .ax\ax.ps1 log -Session -Pattern HotkeyProbe
   pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 120
   ```

11. Repeat one callback at a time.

Do not enable `OnSideChat` until `OnChatAction` has produced an actual live `HotkeyProbe` line and survived with the pointer-shaped hook.

## Central Dispatcher Goal

The goal is still a unified keybind intercept and dispatcher, not hard-coded fixes.

Key rules:

- Do not hard-code `ALT-1` or specific user bindings.
- Use dispatcher winners and action metadata to decide whether native behavior should continue.
- Keep user rebind cases valid, including `ALT-1 = select_ship1`.
- Prefer shared low-level seams over many `On*Action` hooks.
- Do not mutate game objects from lower-level OS/window callbacks.
- Keep probes non-behavior-changing until live evidence says suppression is needed.

## Relevant Docs

- `docs/SCOPELY_NATIVE_SHORTCUT_CALLBACK_AUDIT.md`
- `docs/CENTRAL_INPUT_DISPATCHER_FOLLOWUP.md`
- `docs/EVENT_DRIVEN_INPUT_SPIKE.md`
- `docs/KEYBIND_ACTION_SYSTEM_AUDIT.md`
- `docs/UNIFIED_INPUT_BIND_IMPLEMENTATION_PLAN.md`

## Validation To Preserve Before Commit

For docs-only changes:

```powershell
git diff --check
```

For hotkey/runtime changes:

```powershell
git diff --check
xmake build stfc-mod-tests
xmake run stfc-mod-tests
xmake build mods
pwsh -NoProfile -File .ax\ax.ps1 cycle
```

Current tests from this session:

- `xmake build stfc-mod-tests`: passed
- `xmake run stfc-mod-tests`: 201 passed, 1809 assertions
- `xmake build mods`: passed
- `ax cycle`: passed, boot healthy
- Post-cycle `pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 120`: no errors
- User live check: bare `B` opens mod bookmarks only; bare `1` selects ships; double-tap locate works
