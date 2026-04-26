# AX Runtime Regression Recipes

These recipes are for local runtime checks after hook, hotkey, notification, or live-debug changes. Prefer the structured `ax` commands over raw log greps so known ignored boot lines stay separated from new failures.

Run commands from the repo root unless noted otherwise:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 <command>
```

## Baseline Gate

Use this before feature-specific checks:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 build
pwsh -NoProfile -File .ax\ax.ps1 cycle
pwsh -NoProfile -File .ax\ax.ps1 config -RuntimeVars -Compact
pwsh -NoProfile -File .ax\ax.ps1 log -Boot -Summary
```

Expected baseline signals:

- `cycle.ok` is `true`.
- `boot.ok` is `true`.
- `boot.installed` matches `boot.hookCount`.
- `recentErrors.errorCount` is `0`.
- Known ignored boot lookup lines must appear under `ignoredLines`, not under active error lines.

Current known ignored lookup strings in the local Windows game build include:

- `Unable to find method 'GameObject::SetActive'`
- `Unable to find method 'UnityEngine.Screen->get_resolutions'`
- `Unable to find method 'GameServerModelRegsitry->HandleBinaryObjects'`
- `Unable to find method 'SlotDataContainer->ParseSlotUpdatedJson'`
- `Unable to find method 'SlotDataContainer->ParseSlotRemovedJson'`

Do not treat those strings as harmless if AX stops marking them ignored.

## Hook Health

Use hook health checks after adding, renaming, or moving detours:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 cycle
pwsh -NoProfile -File .ax\ax.ps1 log -Boot -Summary
pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 80
```

Useful log strings:

- `[HookRegistry]` lines show module, hook name, target, install status, and purpose.
- `status=installed` is the expected hook install state.
- `method_found=false`, `detour_attempted=false`, or `status=failed` are regression signals unless the hook is explicitly optional.

## Hotkeys

Use this path for fallthrough, Q/E, Escape, or shortcut initialization regressions:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 cycle
pwsh -NoProfile -File .ax\ax.ps1 config -RuntimeVars -Compact
pwsh -NoProfile -File .ax\ax.ps1 log -Boot -Summary
pwsh -NoProfile -File .ax\ax.ps1 log -Errors -Tail 80
```

Check the runtime vars for:

- `allow_key_fallthrough`
- `use_scopely_hotkeys`
- `disable_escape_exit`
- `set_hotkeys_disable`

Useful log strings:

- `[Hotkeys] allow_key_fallthrough is enabled without use_scopely_hotkeys` explains why unhandled frames will pass through to Scopely bindings.
- `Deprecation Warning: [shortcuts].set_hotkeys_disabled is deprecated. Use set_hotkeys_disable instead.` means the alias path is active and should not be mistaken for a parse failure.

Manual smoke checks still matter for input features:

- Q/E should fall through when the mod does not handle them.
- Ship selection hotkeys should still allow the original fleet panel behavior when fallthrough is intended.
- Escape exit suppression should be validated at the game back-button seam, not only by `ScreenManager.Update` return behavior.

## Notifications And Incoming Attacks

Use these checks after changes to toasts, fleet notifications, incoming attacks, notification queueing, or dedupe behavior:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 cycle
pwsh -NoProfile -File .ax\ax.ps1 config -RuntimeVars -Compact
pwsh -NoProfile -File .ax\ax.ps1 recent-events -Summary -Last 20
pwsh -NoProfile -File .ax\ax.ps1 recent-events -Summary -Kind toast-notification-observed,incoming-fleet-materialized,incoming-attack-notification-context -Last 20
pwsh -NoProfile -File .ax\ax.ps1 log -Session -Tail 120 -Pattern IncomingAttack
```

Useful event kinds:

- `toast-notification-observed`
- `incoming-fleet-materialized`
- `incoming-attack-notification-context`
- `fleet-slot-state-changed`
- `fleet-slot-docked`
- `fleet-slot-cargo-gained`

Useful log strings:

- `[IncomingAttack] source=toast-fleet-queue mode=targeted` shows the rich queue path produced a target-specific payload.
- `[IncomingAttack] source=toast-fleet-queue mode=suppressed reason=dedupe-window` shows dedupe suppressed a repeated alert.
- `[IncomingAttack] consumed toast state=` shows the toast dispatcher consumed an incoming-attack toast.

If incoming-attack behavior changes, keep the safety rules in `docs/ATTACK_INCOMING_UI_DEBUG.md` in mind: pointer-first breadcrumbs are safer than calling unvalidated managed getters inside hot hooks.

## Live Debug

Use this path after changing live-debug request handling, observers, recent-events, or AX clients:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 cycle
pwsh -NoProfile -File .ax\ax.ps1 debug-send -Cmd ping
pwsh -NoProfile -File .ax\ax.ps1 live-state tracker -Summary
pwsh -NoProfile -File .ax\ax.ps1 live-state top-canvas
pwsh -NoProfile -File .ax\ax.ps1 live-state fleetbar
pwsh -NoProfile -File .ax\ax.ps1 live-state fleet-slots
pwsh -NoProfile -File .ax\ax.ps1 recent-events -Summary -Last 20
```

For bounded reproduction watches, use `recent-events -Follow` instead of polling manually:

```powershell
pwsh -NoProfile -File .ax\ax.ps1 recent-events -Follow -DurationSec 10 -Kind fleet-slot-state-changed -StopOnMatch
pwsh -NoProfile -File .ax\ax.ps1 recent-events -Follow -DurationSec 10 -Match warp -Last 10
```

Expected live-debug signals:

- `debug-send -Cmd ping` returns `ok: true`.
- `recent-events` includes `count`, `returnedCount`, `matchedCount`, `capacity`, `firstSeq`, `lastSeq`, `nextSeq`, and kind-count metadata.
- Empty tracker or canvas results immediately after boot are runtime state observations, not transport failures, when the response itself is successful.

## Failure Triage

When a recipe fails:

1. Re-run `ax doctor` to catch missing prerequisites.
2. Re-run `ax log -Boot -Summary` to separate hook install failures from runtime feature failures.
3. Re-run `ax log -Errors -Tail 120` to see only warning/error lines.
4. Use `ax recent-events -Summary -Last 50` for feature-level event history before reading raw logs.
5. Only fall back to raw log or dump searches after the structured command shows what is missing.