# STFC Mod Stabilization Governance Pass

Date: 2026-06-10
Base checkpoint: `5a00512` (`fix: avoid unattended queue stalls from observers`)
Initial docs branch: `docs/gameplay-seam-ownership`
Follow-up audit branch: `audit/gameplay-seam-registry-baseline`

## Purpose

This document records the first governance pass after the Kir'shara/fleet-runtime emergency fix. It is planning and architecture guidance only. It does not rename config keys, move code, add hooks, or change runtime behavior.

Existing related docs remain relevant:

- `docs/NATIVE_PROBE_SAFETY.md`: native probe risk ladder and approval posture.
- `docs/NATIVE_SEAM_LEDGER.md`: seam-specific evidence ledger.
- `docs/OBSERVABILITY_SURFACE_OWNERSHIP.md`: earlier observability ownership snapshot.
- `docs/CONFIG_SYSTEM_RETHINK.md`: schema-driven config direction.
- `docs/NOTIFICATION_CONFIG_RETHINK.md`: prior notification namespace analysis.

This document adds the stricter rule exposed by the 2026-06 Kir'shara/fleet-runtime incident:

> Each gameplay seam has exactly one registered owner. Other features consume owned internal events or snapshots, not parallel raw gameplay hooks.

## Phase 0 Functional Checkpoint

Current functional checkpoint:

- Commit `5a00512` contains the current stable Kir'shara/fleet-runtime fix set.
- Digit's native unattended queue advancement is working with the mod loaded.
- Sidecar Fleet Watch is working with `fleetarrivalhooks = true`, `[sidecar.sync].fleet_runtime = true`, and `fleet_runtime_mode = "normal"`.
- Runtime validation proved queue advancement with `live_query`, `fleetarrivalhooks`, sidecar fleet runtime, and sidecar-absent backoff enabled.
- Battle log capture and sidecar event ingest were revalidated against the sidecar API/event store.

Functional commit scope:

- Kir'shara queue repair and marker detours are force-disabled pending native-patch revalidation.
- Sidecar-local fleet runtime no longer installs deployment event observer detours.
- Live-debug deployment event detours remain disabled.
- Fleet runtime requests are deferred to `ScreenManager.Update` with a quiet window before capture.
- Sidecar-local fleet runtime uses latest-wins enqueue/coalescing.
- Sidecar-local transport treats viewer absence as normal and enters bounded backoff.
- Temporary fleet runtime diagnostic modes exist for boundary testing.

Do not mix broad config cleanup or hook refactors into this functional checkpoint.

### Phase 0 Separation And Commit Plan

Functional fix already checkpointed:

- Commit `5a00512` is the minimal functional checkpoint for the current stable behavior.
- It intentionally includes `action_queue_repair.cc` because the native game patch made our Kir'shara repair/marker hooks unsafe to keep loaded.
- It intentionally includes fleet runtime, frame tick, sidecar ingest, and async queue changes because they enforce the tested safe runtime boundary.
- It intentionally includes tests for the new queue coalescing helper and sidecar fleet runtime mode parsing.

Temporary diagnostics in the functional commit:

- `sidecar.sync.fleet_runtime_mode` is useful for boundary testing (`normal`, `request_only`, `snapshot_only`, `enqueue_no_transport`).
- The mode should remain defaulted to `normal`.
- Its long-term config home is questionable because it is a probe/safety control inside a delivery namespace.

Unrelated dirty files:

- None at the time this docs branch was created.
- Local `.ax-priv` notes are untracked/private evidence and should not be committed.
- Live game TOML is outside the repo and is not part of the commit set.

Config cleanup candidates not included in the functional fix:

- Flat `[notifications].notifications_*` keys.
- `fleetarrivalhooks` as a normal user-facing feature name.
- `sidecar.sync.fleet_runtime_mode` as a sidecar delivery setting.
- Advanced/diagnostic settings that still appear as ordinary options in examples.

Still needed before mainline confidence:

- Optional deeper Battle Workbench/UI validation. The governed readiness checkpoint only validated capture, sidecar ingest, and event-store summary.

### Runtime Validation Evidence

Observed on 2026-06-10 with the live game config using `fleetarrivalhooks = true`, `[sidecar.sync].fleet_runtime = true`, `fleet_runtime_mode = "normal"`, and sidecar Fleet Watch enabled:

| Requirement | Status | Evidence |
| --- | --- | --- |
| Attended queue advances | Confirmed | User-visible queue confirmation completed after tool/log validation. |
| Unattended queue advances | Confirmed | User-visible queue confirmation completed after prior boundary matrix runs with request-only, snapshot-only, enqueue-no-transport, and normal sidecar-absent modes. |
| Sidecar Fleet Watch works with sidecar running | Confirmed | `fleet.runtime` payloads were sent successfully; `/api/fleet/projection` returned `available=true`, fresh `observedAt`, and populated slots. |
| Sidecar-offline mode does not stall or hot-loop | Confirmed for transport behavior; queue non-stall was previously user-confirmed in normal sidecar-absent mode | Sidecar absence produced bounded failures/backoff, suppressed sends during backoff, and later transport recovery instead of a hot loop. |
| Fleet arrival notification/audio still works if configured | Confirmed | Log showed `fleet.arrived_in_system`, `NotifyAudio` playing `arrival`, WinRT notification request, and follow-up fleet runtime capture. |
| Battle log behavior | Governed readiness confirmed; full UI not exhaustively tested | Mod sent `battle.events`; sidecar health reported fresh battle data; `/api/events?scope=battle&detail=summary` returned current `battle.capture` rows. |

## Gameplay Seam Ownership Rule

A gameplay seam is any native/game surface where the mod touches live game behavior or state. Examples include fleet slot state transitions, deployment events, battle-end events, arrival/docking/mining transitions, fleet-bar derived state, queue advancement repair, notification capture, and sidecar runtime capture.

Rules:

1. A low-level gameplay hook or detour has one owner module.
2. Other features must not install parallel hooks into the same seam just to get their own data.
3. The owner may publish a safe internal event or snapshot.
4. Subscribers consume the internal event or snapshot, not the raw gameplay hook.
5. Every dispatch includes source/reason metadata: owner, event or reason, originating hook/seam, and effect class.
6. Effect class must be one of: passive observation, bounded capture, repair/action, notification, export, diagnostic.
7. New synchronous gameplay access must explain why existing owned surfaces are insufficient.
8. Any new gameplay hook requires explicit approval after lower-risk alternatives are tested.

Owner modules are allowed to do bounded game-thread capture. They are not allowed to become feature grab-bags.

## First-Pass Gameplay Seam Inventory

This table is a static source inventory plus runtime findings from the current incident. It is not a complete safety ledger. Promote seam-specific runtime claims through `docs/NATIVE_SEAM_LEDGER.md` before new hook work.

| Gameplay seam | Current owner | Current subscribers / consumers | Current status | Suspected duplicate or tangle |
| --- | --- | --- | --- | --- |
| `ScreenManager.Update` frame tick | `mods/src/patches/frame_tick.cc` | Hotkey router, live debug tick, fleet runtime deferred flush | Acceptable central fan-out if subscribers stay bounded | Must not grow into arbitrary per-frame polling. |
| `FleetStateWidget.SetWidgetData` fleet-bar transition | `mods/src/patches/parts/fleet_arrival.cc` | `fleet_notifications_observe_fleet_bar`, `fleet_runtime_sync_trigger` | Current safe owner for Fleet Watch and fleet notifications | Config name `fleetarrivalhooks` exposes the mechanism instead of the capability. |
| `ToastFleetObserver.QueueNotifications` incoming attack materialization | `mods/src/patches/parts/fleet_arrival.cc` | `fleet_notifications_notify_incoming_attack_target`, live-debug incoming materialization marker | Targeted source of truth for incoming attack notifications | Keep targeted; do not reintroduce broad inference fallbacks. |
| `ToastFleetObserver.HandleMiningDepleted` | `mods/src/patches/parts/fleet_arrival.cc` | Fleet node depleted notification | Acceptable targeted notification hook | Should publish event metadata if generalized. |
| `MiningObjectViewerWidget.UpdateTimerWidget` | `mods/src/patches/parts/fleet_arrival.cc` | Mining ETA/cache for notifications | Acceptable targeted notification hook | Mechanism should remain internal/advanced, not user-facing as a feature name. |
| `DeploymentEvents.Trigger*` family | `mods/src/patches/parts/deployment_runtime_observers.cc` for legacy/cloud `sync.fleet_runtime` only | `fleet_runtime_sync_trigger` for legacy/cloud sync | High-risk. Sidecar fleet runtime must not install this seam. | `parts/live_debug.cc` has a disabled duplicate hook family; this caused observer spaghetti. |
| Live-debug deployment event hooks | `mods/src/patches/parts/live_debug.cc` | Live debug recent events, fleet notification runtime walk, fleet runtime trigger | Disabled by current functional fix | Duplicate of `DeploymentRuntimeObservers`; do not re-enable without ownership redesign. |
| Fleet runtime snapshot read | `mods/src/patches/live_debug_fleet_runtime_observers.cc` | Fleet runtime sync, live-debug state results, live-debug recent model polling | Snapshot-only passed when deployment observer detours were skipped | Needs explicit capture metadata; live-debug request reads should remain diagnostic and bounded. |
| Sidecar-local `fleet.runtime` enqueue/transport | `mods/src/patches/sidecar_local_ingest.cc` | Fleet Watch / local sidecar server | Current safe path uses latest-wins coalescing and backoff; sidecar-running API path revalidated | Continue to audit sidecar-offline and queue behavior as separate acceptance cases. |
| Battle log capture/decode/export | `mods/src/patches/sync_battle_logs.cc`, `mods/src/patches/battle_log_decoder.*` | External sync, sidecar local battle events, sidecar JSONL | Capture and sidecar event-store summary revalidated | Mixed queueing, decode, JSONL retention, sidecar delivery, and cloud delivery still need a dedicated audit. |
| Sync model ingest / data container parse hooks | `mods/src/patches/parts/sync.cc` | `sync_payload_builders`, scheduler, transport | Existing broad sync surface | Needs separate audit, not part of Kir'shara stabilization. |
| Action queue repair and markers | `mods/src/patches/parts/action_queue_repair.cc` | Queue repair/probe diagnostics | Force-disabled after native queue patch | Do not re-enable marker or repair detours without new runtime approval. |
| Toast banner suppression | `mods/src/patches/parts/disable_banners.cc` | Banner visibility policy | Existing behavior | Config relationship to notifications/banners is semantically tangled. |
| Hotkey and fleet action seams | `mods/src/patches/parts/hotkeys.cc`, `hotkey_router.cc`, `fleet_actions.cc` | Unified input, fleet action runtime | Existing high-risk behavioral hooks | Continue separately under input/fleet action migration docs. |

## Future Enforcement: Gameplay Seam Registry And Hook Ingress

The long-term enforcement direction is a single gameplay hook ingress point backed by a `GameplaySeamRegistry`, but that is not the current state.

Current state:

- governance law for seam ownership and game-access boundaries
- manual unmanaged seam/hook inventory
- existing partial `HookModuleHealth` / hook owner registry coverage
- no complete scanner/tripwire yet
- no runtime `GameplaySeamRegistry` system of record yet

Next state:

- scanner/tripwire that detects raw hook-like calls outside the future registry
- checked-in unmanaged legacy baseline
- failure only for net-new unmanaged hook-like calls after the baseline exists

Later state:

- runtime `GameplaySeamRegistry` / hook ingress system of record
- one-at-a-time migration of existing seams into the system of record

The eventual registry should record:

- gameplay seam
- owner module
- source file/function
- reason / why the seam is touched
- originating effect class
- allowed extraction scope
- published evidence surface
- known subscribers
- risk level
- validation evidence
- status: active, disabled, deprecated, probe-only, or migration exception

Once scanner/tripwire enforcement exists, all new gameplay-affecting hooks or detours must either enter through the approved registry path or carry an explicit reviewed exception. Raw hook-like installs outside the future registry are not acceptable as silent growth.

Flashlight first, cop second:

1. Identify hook-like behavior outside the future registry.
2. Create a legacy unmanaged-hook baseline.
3. Allow existing baseline findings temporarily.
4. Add scanner/tripwire enforcement that fails only new unmanaged hook-like calls.
5. Migrate existing seams into the system of record one at a time.

This audit branch is the flashlight stage. It must not change runtime hook behavior.

Long-term rule:

A gameplay hook cannot be installed without a registered seam claim. Duplicate active claims are rejected unless explicitly marked as temporary migration/probe exceptions with owner, reason, expiry, and validation plan.

## Game Access And Async Boundary Policy

Do not reduce the rule to "everything async." Unity/STFC state usually must be read on the game/main thread.

Required pipeline:

```text
game/main thread:
  touch Unity/STFC objects only inside the owned gameplay seam
  copy primitive/owned facts into an immutable mod-owned snapshot
  release all game pointers/references
  publish snapshot/event through a bounded non-blocking internal boundary
  return

worker/background:
  serialize copied snapshot/event
  enqueue/export/send/retry/backoff/drop/coalesce
  never touch Unity/STFC objects
  never block gameplay
```

Rules:

1. Worker threads must not touch Unity/STFC objects, pointers, references, lazy accessors, or live object graphs.
2. Sidecar absence is normal. It must not create a hot error loop.
3. Sidecar transport must use bounded retry/backoff.
4. Fleet runtime is latest-state telemetry by default and should coalesce.
5. Battle events may preserve ordered history, but queues and retries must still be bounded and backoff-safe.
6. Game-thread publish must not block on transport, worker locks, HTTP, logging-heavy failure paths, or unbounded queues.
7. A synchronous full capture/export path must be explicitly justified and signed off after alternatives are tested.

Current findings:

- Fleet runtime now mostly matches this policy: lightweight request, deferred quiet-window capture, owned payload, sidecar enqueue, worker transport.
- `sidecar_local_ingest` now has bounded backoff and fleet runtime coalescing.
- Battle events were revalidated for capture and sidecar ingest, but still need a fresh audit for ordered-history bounds, sidecar-offline behavior, and UI/workbench presentation.
- Live-debug request handlers can synchronously call `observe_fleet_runtime_snapshot()`. Keep this diagnostic-only, bounded, and clearly labeled as observed/captured state.
- `fleet_notifications_observe_runtime_fleets()` remains a synchronous fleet walk. It must not be casually reattached to deployment event detours.
- External/cloud `sync.fleet_runtime` still has a legacy route through deployment observers. It should be reviewed before being treated as product-safe under the new rule.

## Config Taxonomy Findings

User-facing config should describe capabilities and behavior. Advanced config may expose diagnostics, unsafe levers, or implementation controls. Mechanism names should not be normal user-facing feature names.

Current concerns:

| Current setting / family | Problem | Proposed destination |
| --- | --- | --- |
| `[notifications].notifications_*` | Redundant prefix inside the namespace; mixes event selection and channel policy | Structured notification schema with legacy aliases. |
| `notifications_audio_fleet_arrived_in_system` | Flat channel/event key; only one event has audio pilot shape | Structured notification event/channel entry. |
| `fleetarrivalhooks` | Mechanism-level hook installer presented as a normal feature option | Hide from normal examples or move to advanced hook/diagnostic namespace; user-facing behavior belongs under notifications and sidecar Fleet Watch. |
| `sidecar.sync.fleet_runtime_mode` | Diagnostic/probe mode inside delivery namespace | Keep default `normal`; eventually move or hide under advanced diagnostics if retained. |
| `sync.fleet_runtime` vs `[sidecar.sync].fleet_runtime` | Same phrase with different delivery paths and risk profiles | Clarify legacy/cloud runtime sync vs local sidecar Fleet Watch in docs/schema. |
| `[advanced.diagnostics].live_query` | Correctly advanced, but it can still imply broad live reads | Keep advanced and document request-driven bounded reads. |
| `[advanced.queue].queue_repair_enabled` and queue marker controls | Correctly advanced, high-risk behavior | Keep off by default; do not show as normal feature controls. |

Recommended destination shape for user-facing notification TOML:

```toml
[notifications]
enabled = true

[notifications.fleet]
arrived_in_system = { system = true, audio = true, sound = "success" }
arrived_at_destination = { system = true, audio = false, sound = "info" }
started_mining = { system = false, audio = false, sound = "info" }
node_depleted = { system = true, audio = true, sound = "warning" }

[notifications.battle]
victory = { system = true, audio = false, sound = "success" }
defeat = { system = true, audio = true, sound = "warning" }
incoming_attack_player = { system = true, audio = true, sound = "alarm" }
incoming_attack_hostile = { system = true, audio = true, sound = "alarm" }

[notifications.experimental]
standard = false
faction_warning = false
```

Compatibility and migration plan:

1. Keep all existing flat `[notifications].notifications_*` keys as aliases.
2. Let structured keys override flat aliases when both are present.
3. Emit deprecation diagnostics for flat aliases only after the structured parser has parity coverage.
4. Keep runtime snapshots able to show canonical structured names and, if needed during migration, legacy compatibility lines.
5. Do not remove existing keys until at least one release cycle after the settings UI/schema path can represent the new structure.

Mechanism-level settings to move, hide, or mark advanced:

- `fleetarrivalhooks`
- `sidecar.sync.fleet_runtime_mode`
- any queue repair marker settings
- dormant or probe-only native diagnostics
- hook installer toggles whose user-facing behavior is already represented by a capability setting

## Whole-Mod SOC Audit Plan

Principles for the audit:

- Appearance is not reality: names must match behavior.
- Memory is not evidence: runtime/cache/observed state needs provenance.
- Recall is not certainty: snapshots are captured observations, not guaranteed truth.
- Contradiction is signal: conflicting fleet/deployment/battle observations should be visible diagnostics.
- Forgetting is a feature: queues, sidecar payloads, diagnostics, and snapshots must be bounded.
- Assimilation must add distinctiveness: retain/export only useful changes, not every hook firing.
- Provenance is dignity: dispatches and exports carry source/reason metadata.
- Boundaries are consent: features consume owned surfaces, not raw cross-module game state.
- Power requires audit: hooks, detours, repair actions, and exports need ownership and diagnostics.
- The operator remains sovereign: user settings describe capabilities, not accidental implementation levers.

Recommended branch sequence:

| Branch | Purpose | Risk / blast radius | Tests |
| --- | --- | --- | --- |
| `docs/gameplay-seam-ownership` | Docs/inventory only; establish owner and async boundary rules | Low | `git diff --check`; markdown review |
| `audit/gameplay-seam-registry-baseline` | Manual unmanaged hook/seam baseline and scanner/tripwire design notes without changing runtime behavior | Low; audit-only | Baseline review; `git diff --check`; no runtime behavior changes |
| `audit/gameplay-seam-scanner-tripwire` | Static scanner, deterministic unmanaged baseline file, and fixture self-test for new raw hook-like additions | Low; tooling-only | Scanner command; scanner self-test; `git diff --check`; pure tests |
| `refactor/gameplay-dispatch-ownership` | Introduce owner/source/reason metadata where lowest risk | Medium; metadata only first | Pure tests for metadata builders; runtime smoke for queue/Fleet Watch |
| `refactor/sidecar-runtime-boundary` | Harden copied snapshot, bounded queue/coalescing, offline backoff contracts | Medium; sidecar and telemetry paths | Sidecar-offline, sidecar-running Fleet Watch, battle event ingest |
| `config/notification-namespace-cleanup` | Structured notification config with aliases/deprecations | Medium; config and notification behavior | Config parser tests, runtime notification/audio smoke |
| `config/advanced-settings-taxonomy` | Move mechanism/diagnostic settings out of normal examples | Low to medium; config compatibility risk | Config alias tests, runtime vars snapshot review |
| `audit/mod-wide-soc-pass` | Continue subsystem-by-subsystem SOC cleanup | Variable | Per-subsystem narrow tests and runtime validation |

Current sequencing:

- `docs/gameplay-seam-ownership` has landed locally on `main`.
- `audit/gameplay-seam-registry-baseline` has landed as a pushed audit-only branch.
- `audit/gameplay-seam-scanner-tripwire` is the current tooling branch.
- This branch is audit/tooling-only and must not change runtime hook behavior.

Push sequencing should keep remote ancestry sane: push local `main` first, then the baseline branch, then the scanner branch.

## Test Plan For Stabilization And Follow-Up

Required before treating the functional fix as mainline-ready:

- Attended queue with sidecar Fleet Watch on and sidecar running.
- Unattended queue with sidecar Fleet Watch on and sidecar running.
- Unattended queue with sidecar Fleet Watch on and sidecar absent.
- Fleet Watch UI/API receives fresh `fleet.runtime` after fleet transition.
- Fleet-arrival notification still fires with audio when configured.
- Battle logs still capture and sidecar event-store ingest works when sidecar is running.
- Sidecar-offline mode does not hot-loop logs or stall gameplay.

Do not touch yet:

- Do not re-enable live-debug deployment event detours.
- Do not re-enable Kir'shara queue repair or marker hooks.
- Do not rename notification config in the functional fix branch.
- Do not merge battle log refactors with fleet runtime stabilization.
- Do not generalize fleet runtime conclusions to all deployment event hooks without separate seam evidence.
