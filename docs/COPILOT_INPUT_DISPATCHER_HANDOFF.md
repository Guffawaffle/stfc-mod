# Copilot Handoff: Native Shortcut Guard And Central Dispatcher

Source folder: `D:\dev\stfc-mod`

Status: historical handoff, sanitized after the public input-trace cleanup.

## Verified Behavior To Preserve

- `ALT-1` toggles cargo/default and no longer selects ship 1.
- Bare `1` still selects ship 1.
- Double-tap ship locate works after the dispatcher/native suppression changes.
- With the mod TOML binding `show_bookmarks = "B"`, bare `B` opens bookmarks through the mod dispatcher and does not also fire the game's native recall binding.
- Chat, native Manage Ship, and mod-owned `show_ships` behavior should continue to work under the current pointer-shaped native shortcut guard.

## Dispatcher / Native Ownership Semantics

Current intended policy:

- Missing binding config means use the registry/default binding. This preserves bootstrap behavior when no TOML exists.
- Explicit `""` and explicit `"NONE"` both mean the action is unbound.
- When `use_scopely_hotkeys = true`, the mod dispatcher should not own hotkeys; native Scopely handling is allowed.
- When `use_scopely_hotkeys = false`, any matching mod dispatcher winner owns that physical key/chord and should suppress native Scopely handling even if `allow_key_fallthrough = true`.
- With `use_scopely_hotkeys = false` and `allow_key_fallthrough = true`, only unbound keys should fall through to native.
- With `use_scopely_hotkeys = false` and `allow_key_fallthrough = false`, unbound keys should do nothing.

Implementation notes:

- `hotkey_router_native_shortcuts_suppressed(...)` treats dispatcher-owned winners as native-consuming regardless of whether the chord has modifiers.
- Pointer-shaped native shortcut callbacks route through a shared helper that refreshes dispatcher suppression and skips `original` when the dispatcher owns the current key.
- Fleet-bar selection hooks still guard against native fallthrough, but `HandleShipSelection(...)` uses a scoped bypass while making its own `FleetBarViewController::RequestSelect(...)` and `ElementAction(...)` calls. Without that bypass, bare `1` can toggle the ship card without actually selecting the ship.

## ABI Lesson

Do not re-enable by-value callback-context hooks.

This shape is unsafe for the native shortcut callback family:

```cpp
void Probe(auto original, void* _this, InputActionCallbackContext context);
```

Likely reason: `InputAction.CallbackContext` is a 16-byte value struct in the IL2CPP dump, and Windows x64 passes these by hidden reference/pointer. Treat callback-context methods as pointer-shaped at the native ABI unless a dedicated ABI audit proves otherwise.

Use this shape for the shared guard:

```cpp
void Guard(auto original, void* _this, void* context);
```

The public build should not expose per-key or per-callback input trace logging. Any future deep diagnostics for this path belong in a private-only patch or worktree outside the normal public build and AX command surface.

## Detour Single-Owner Policy

`mods/src/patches/hook_registry.h` provides `hook_registry_claim_owner(descriptor, module)`, called automatically by every `HOOK_REGISTRY_SPUD_STATIC_DETOUR` site. The first installer to claim a given `(assembly, namespace, class, method)` tuple wins. A second claim on the same target logs `[HookOwnerConflict]`, refuses to install the duplicate, and in `_MODDBG` builds calls `std::abort` so the conflict is impossible to miss in development.

Audit note from the earlier sprint: no two `SPUD_STATIC_DETOUR` sites targeted the same IL2CPP method. Repeated sync trampolines resolved different methods on different classes. Bare `SPUD_STATIC_DETOUR` sites are not yet covered by the registry; convert call sites to `HOOK_REGISTRY_SPUD_STATIC_DETOUR` opportunistically when touching them.

## Central Dispatcher Goal

The goal is still a unified keybind intercept and dispatcher, not hard-coded fixes.

Key rules:

- Do not hard-code `ALT-1` or specific user bindings.
- Use dispatcher winners and action metadata to decide whether native behavior should continue.
- Keep user rebind cases valid, including `ALT-1 = select_ship1`.
- Prefer shared low-level seams over many `On*Action` hooks.
- Do not mutate game objects from lower-level OS/window callbacks.
- Keep public diagnostics operational and sanitized.

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
