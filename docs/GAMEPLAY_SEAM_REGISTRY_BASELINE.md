# Gameplay Seam Registry Baseline

Date: 2026-06-10
Branch: `audit/gameplay-seam-registry-baseline`
Base checkpoint: `0f33462` (`docs: record stabilization validation evidence`)

## Purpose

This is the audit-only, initial/manual baseline before implementing a broader gameplay seam registry or current-operating-posture enforcement. It records the current hook-like surface so future branches can distinguish existing unmanaged legacy from newly introduced unmanaged hooks.

This document does not change runtime hook behavior.

This document is not scanner-generated output. It is a manually curated baseline produced from static searches and source review. The scanner/tripwire does not exist yet.

Current state:

- governance law
- manual unmanaged seam/hook inventory
- partial existing hook owner registry for some hook families

Next state:

- scanner/tripwire that detects raw hook-like calls outside the future registry
- checked-in machine-readable baseline if useful
- review gate that fails only net-new unmanaged hook-like calls

Later state:

- runtime `GameplaySeamRegistry` / hook ingress system of record
- migrated seam ownership metadata
- one-at-a-time migration of existing seams into the system of record

## Scope

Included:

- `SPUD_STATIC_DETOUR` and `HOOK_REGISTRY_SPUD_STATIC_DETOUR` call sites under `mods/src`.
- Existing hook registry coverage and current unmanaged direct-detour baseline.
- High-risk hook-like dispatch/capture boundaries exposed by the Kir'shara/fleet-runtime stabilization.
- Future enforcement rules that can be implemented without reclassifying every legacy seam first.

Excluded:

- Generated/third-party IL2CPP wrapper lookups in `mods/src/prime`.
- Ordinary `GetMethod`/`GetMethodInfo` calls that invoke methods but do not install detours.
- Runtime behavior changes, hook conversion, config migration, or new C++ enforcement.

## Manual Static Inventory Summary

Static query used:

```powershell
rg -n "\b(HOOK_REGISTRY_SPUD_STATIC_DETOUR|SPUD_STATIC_DETOUR)\b" mods/src --glob "*.cc" --glob "*.h"
```

Manual inventory posture:

| Category | Count | Notes |
| --- | ---: | --- |
| Direct legacy `SPUD_STATIC_DETOUR` sites | 93 | Existing unmanaged baseline. Do not add to this count without explicit review. |
| Registry-backed ordinary call sites | 11 | `FrameTickHooks` plus `HotkeyHooks`. |
| Registry-backed macro family | 21 hooks | `RefineryDiagnosticsHooks` uses `INSTALL_REFINERY_DIAG_HOOK(...)`, which routes through `HOOK_REGISTRY_SPUD_STATIC_DETOUR`. |

The most important number for future scanner enforcement is the direct legacy count: `93`. The future scanner should initially preserve this count as a grandfathered baseline and fail only increases or new unmanaged direct-hook files unless an exception is reviewed.

## Direct Legacy Baseline By File

These files currently contain direct unmanaged `SPUD_STATIC_DETOUR` installs. This is a grandfathered baseline, not an endorsement that each seam is well-owned.

| File | Direct sites | Current posture |
| --- | ---: | --- |
| `mods/src/patches/parts/action_queue_repair.cc` | 10 | Force-disabled after native Kir'shara queue fix; do not re-enable without new runtime approval. |
| `mods/src/patches/parts/buff_fixes.cc` | 1 | Legacy functional hook. |
| `mods/src/patches/parts/chat.cc` | 6 | Chat UI event hooks; candidate for hook-event ownership metadata. |
| `mods/src/patches/parts/deployment_runtime_observers.cc` | 10 | High-risk deployment-event observer family; sidecar Fleet Watch must not install this path. |
| `mods/src/patches/parts/disable_banners.cc` | 3 | Toast/banner policy hooks. |
| `mods/src/patches/parts/fix_pan.cc` | 2 | Navigation/touch behavior hooks. |
| `mods/src/patches/parts/fleet_arrival.cc` | 4 | Current fleet-bar/arrival notification owner; also requests fleet runtime sync. |
| `mods/src/patches/parts/free_resize.cc` | 2 | UI/window resize hooks. |
| `mods/src/patches/parts/improve_responsiveness.cc` | 1 | Loading/transition responsiveness hook. |
| `mods/src/patches/parts/live_debug.cc` | 10 | Live-debug deployment-event detours are currently disabled and duplicate deployment observer seams. |
| `mods/src/patches/parts/loading_screen_bg.cc` | 7 | Loading screen customization hooks. |
| `mods/src/patches/parts/misc.cc` | 7 | Mixed feature hooks; candidate for future split by capability. |
| `mods/src/patches/parts/object_tracker.cc` | 3 | Object/GC/liveness tracking hooks, not normal gameplay hooks. |
| `mods/src/patches/parts/sync.cc` | 18 | Broad sync ingest surface; high-value future ownership audit target. |
| `mods/src/patches/parts/testing.cc` | 2 | Testing-only hooks; should stay clearly non-product. |
| `mods/src/patches/parts/testing_config_override.cc` | 1 | Testing/config override hook. |
| `mods/src/patches/parts/ui_scale.cc` | 2 | UI scaling hooks. |
| `mods/src/patches/parts/zoom.cc` | 3 | Navigation zoom hooks. |
| `mods/src/patches/patches.cc` | 1 | Bootstrap `il2cpp_init` detour; not a gameplay seam. |

## Registry-Backed Baseline

Already registry-backed:

| Area | File | Coverage |
| --- | --- | --- |
| Frame tick owner | `mods/src/patches/frame_tick.cc` | `ScreenManager.Update` detour owns frame fan-out for hotkeys, live debug, and fleet runtime deferred processing. |
| Hotkey/input seams | `mods/src/patches/parts/hotkeys.cc` | Shortcut initialization, shortcut late update, select ship, cargo context, fleet-bar selection, back button, and set-course suppression seams. |
| Refinery diagnostics | `mods/src/patches/refinery_diagnostics.cc` | Macro-backed diagnostic hook family routes through `HOOK_REGISTRY_SPUD_STATIC_DETOUR`. |

Current registry implementation:

- `HookModuleHealth` records method lookup, detour attempts, install status, and likely symptoms.
- `hook_registry_claim_owner(...)` uses the resolved method pointer as the single-owner key.
- Duplicate registry claims log `[HookOwnerConflict]` and abort in `_MODDBG`.

Current limitation:

- Bare `SPUD_STATIC_DETOUR` sites bypass `hook_registry_claim_owner(...)`.
- The registry can prevent duplicate claims only after a hook site is converted to the wrapper.

## Hook-Like Boundaries Exposed By Stabilization

These are not all detour install sites, but they are behaviorally important seams and should be tracked by future registry/current-operating-posture work:

| Boundary | Current owner | Current risk |
| --- | --- | --- |
| `ScreenManager.Update` frame fan-out | `FrameTickHooks` | Acceptable central fan-out if subscribers stay bounded. |
| `FleetStateWidget.SetWidgetData` fleet-bar transition | `fleet_arrival.cc` | Owner for fleet arrival notifications and sidecar Fleet Watch runtime sync requests. |
| `DeploymentEvents.Trigger*` observer family | `deployment_runtime_observers.cc` for legacy/cloud runtime sync only | High-risk; must not be reattached to sidecar Fleet Watch without ownership redesign. |
| Live-debug deployment-event detours | `live_debug.cc` | Currently disabled duplicate family; retain as unmanaged legacy, not active product behavior. |
| Fleet runtime snapshot read | `observe_fleet_runtime_snapshot()` | Must stay bounded and game-thread-owned; copied snapshots only may cross async boundaries. |
| Sidecar-local fleet runtime enqueue/transport | `sidecar_local_ingest.cc` | Latest-wins/coalescing telemetry; sidecar absence is normal and must stay backoff-safe. |
| Battle event sidecar ingest | `sync_battle_logs.cc` and `sidecar_local_ingest.cc` | Ordered history stream; needs bounded/backoff audit separate from Fleet Watch. |
| Kir'shara queue repair/markers | `action_queue_repair.cc` | Force-disabled; no runtime reactivation without new dump/runtime approval. |

## Future Enforcement Contract

The first enforcement branch should not demand immediate conversion of all legacy direct detours. That would create unnecessary runtime risk.

Instead:

1. Treat this document as the legacy unmanaged baseline.
2. Build scanner/tripwire tooling that identifies hook-like calls outside the future registry.
3. Allow existing baseline findings temporarily.
4. Fail only net-new unmanaged hook-like calls after the baseline/scanner exists.
5. Require any net-new `SPUD_STATIC_DETOUR` site to use `HOOK_REGISTRY_SPUD_STATIC_DETOUR` or carry an explicit reviewed exception and rationale.
6. Require any new gameplay seam to declare owner, target, purpose, effect class, source/reason metadata, and expected subscribers before runtime implementation.
7. Migrate existing direct sites opportunistically only when the owning feature is already being touched and runtime validation is available.
8. Reject duplicate detours to a known gameplay seam unless the existing owner cannot publish the needed event/snapshot and that gap is documented.
9. Count disabled/probe-only detours as seams. They must not be re-enabled because they are "already present" in legacy code.
10. The enforcement tool should fail on net-new unmanaged hook installs, not on the existing `93` direct legacy sites.

Suggested future checks:

- A static hook inventory command that emits direct and registry-backed hook sites.
- A checked-in machine-readable baseline generated from the scanner or derived from this manual inventory.
- A CI/review gate that fails when direct unmanaged count increases or a new direct unmanaged file appears.
- A softer warning when registry-backed hook descriptors lack owner/effect metadata.

## Next Branch Boundary

The next implementation branch may add scanner tooling or metadata, but should still avoid runtime hook behavior changes until the baseline is accepted.

Recommended next branch after this baseline:

`audit/gameplay-seam-scanner-tripwire`

Allowed changes on this branch:

- Documentation inventory.
- Manual baseline inventory.
- Scanner/audit design notes.
- Future branch plan.

Not allowed on this branch:

- Hook install wrapper changes.
- Runtime `GameplaySeamRegistry` wiring.
- Converting direct detours to registry wrappers.
- Re-enabling disabled deployment/live-debug/Kir'shara hooks.
- Adding runtime subscribers.
- Detour migration.
- Config migration.
- Changing config defaults or hook install conditions.
- Sidecar behavior changes.
- Queue behavior changes.
- Notification behavior changes.
- Battle log behavior changes.

Allowed changes on the future scanner/tripwire branch:

- Static audit scripts.
- Machine-readable baseline data.
- Tests for audit scripts that do not compile into the mod DLL.
- CI/review tripwire wiring that fails only net-new unmanaged hook-like calls.
