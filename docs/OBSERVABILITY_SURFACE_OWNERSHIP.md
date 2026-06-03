# Observability Surface Ownership

Date: 2026-06-02  
Repo: `D:\dev\stfc-mod`  
Branch: `feat/observability-config-cleanup`  
Baseline commit: `79d54c3` (`fix(queue): restore queue-only Kir'shara repair`)

## Purpose
Define current ownership for logging, probe, sync, and sidecar-adjacent observability surfaces on current `main`, then pin low-risk future boundaries for follow-on cleanup.

This document is inventory and planning only:
- No code movement.
- No behavior changes.
- No config changes.

## Relationship to Inventory
- Source snapshot: [LOGGING_PROBE_SYNC_SURFACE_INVENTORY.md](./LOGGING_PROBE_SYNC_SURFACE_INVENTORY.md)
- This document: current ownership, guardrails, and future boundary proposals derived from that inventory

## Scope Notes
- Current code and config truth lives on `main` at `79d54c3`.
- The queue-only Kir'shara repair is present on current `main`, but it is not a target of this cleanup beyond its existing probe/log surfaces.
- `manual_navigation_refresh`, ghost-hostile refresh, view drain/reload, refresh hotkeys, and ghost-specific watch/probe behavior are abandoned and out of scope.
- Older branch-local docs that treated `.ax` as tracked repo truth are outdated for current `main`.
- Any future split boundaries are marked **proposed**.

## 1) Config Ownership Rules
| Surface | Current Owner (existing) | Current Responsibility | Boundary Rule |
|---|---|---|---|
| `[debug].runtime_trace*` | `mods/src/config.cc`, `mods/src/runtime_trace_config.h`, `mods/src/patches/mod_impact_monitor.*` | General native runtime trace level, overhead tracking, and report cadence | Keep as the current home for native runtime tracing |
| `[sidecar.sync]` | `mods/src/config_sidecar.cc`, `mods/src/patches/sidecar_local_ingest*`, `mods/src/patches/fleet_runtime_sync.cc`, `mods/src/patches/sync_battle_logs.cc` | Local native-to-sidecar delivery and loopback transport policy | Keep delivery-only; do not expand with unrelated probes or diagnostics |
| `[sidecar.logging]` | `mods/src/config_sidecar.cc`, `mods/src/patches/sync_battle_logs.cc` | Sidecar-oriented local JSONL output, replay window, and retention controls | Keep output-only; do not use for unrelated native diagnostics |
| `[sidecar.probes]` | `mods/src/config.h`, `mods/src/defaultconfig.h`, `mods/src/config_sidecar.cc`, `tests/src/test_sidecar_config.cc` | Current reserved sidecar-scoped probe toggles and runtime snapshot schema | Keep sidecar-scoped only; do not treat as a general diagnostics namespace |
| `[sidecar.diagnostics]` | `mods/src/config.h`, `mods/src/defaultconfig.h`, `mods/src/config_sidecar.cc`, `tests/src/test_sidecar_config.cc` | Current reserved sidecar-focused diagnostics toggles and runtime snapshot schema | Keep sidecar-scoped only; do not use as a dumping ground for broader native diagnostics |
| `[battle_log_decoder]` | `mods/src/config.cc`, `mods/src/defaultconfig.h`, `mods/src/patches/battle_log_decoder.*`, `mods/src/patches/sync_battle_logs.cc` | Decoder enablement and sidecar-event shaping controls for battle exports | Keep separate from sidecar transport namespaces |
| `[advanced.diagnostics]` | Not implemented on current `main` | Future proposed home for extra native diagnostics | **Proposed:** if broader native diagnostics are added, put them here and default them off |

## 2) Logging Sinks and On-Disk Evidence
| Surface | Current Owner (existing) | Current Responsibility | Proposed Boundary (proposed) |
|---|---|---|---|
| `community_patch.log` | `mods/src/patches/patches.cc`, `mods/src/file.*` | Root `spdlog` bootstrap, rotating archive, and path resolution | Keep as root logger bootstrap; no split needed now |
| `community_patch_navhook_trace.log` | `mods/src/patches/live_debug_navhook_trace_sink.*` | Dedicated navhook trace append, rotate, and size handling | Already isolated; keep as a dedicated sink |
| `community_patch_debug.cmd` / `.out` | `mods/src/patches/parts/live_debug_connector.cc`, `mods/src/patches/live_debug_request_dispatch.h` | File-backed live debug request and response transport | **Proposed:** keep transport-only and continue pulling request dispatch away from `parts/live_debug.cc` |
| `community_patch_action_queue_probe.jsonl` | `mods/src/patches/action_queue_probe_logging.*`, `mods/src/patches/parts/action_queue_repair.cc` | Queue probe JSONL emission for queue-repair investigation | Keep helper split; do not widen queue-repair scope during observability cleanup |
| `community_patch_battle_feed.jsonl` | `mods/src/patches/sync_battle_logs.cc` | Sidecar-oriented local battle event JSONL append and retention under `[sidecar.logging]` | **Proposed:** split JSONL sink mechanics from battle decode and export workflow |
| `community_patch_runtime.vars` | `mods/src/config.cc`, `mods/src/config_sidecar.cc`, `mods/src/file.*` | Resolved runtime config snapshot write, including sidecar redaction | Keep with config system |
| `patch_battlelogs_sent.json` | `mods/src/file.*` naming, `mods/src/patches/sync_battle_logs.cc` behavior | Legacy battle-log sent-id persistence | **Proposed:** document whether the legacy file contract remains intentional before deeper refactors |

## 3) Probe Producers
| Surface | Current Owner (existing) | Current Responsibility | Proposed Boundary (proposed) |
|---|---|---|---|
| Live debug event producers | `mods/src/patches/parts/live_debug.cc` plus `live_debug_*` observers, serializers, and event modules | Fleet, UI, viewer, and deployment observation with recent-event emission | **Proposed:** reduce `parts/live_debug.cc` to hook and tick wiring over time |
| Recent event ring | `mods/src/patches/live_debug_event_store.*`, `live_debug_event_dispatcher.*` | Bounded event storage and append facade | Keep as a dedicated shared store |
| Queue probe logging | `mods/src/patches/action_queue_probe_logging.*` | Structured queue probe JSONL emission | Keep separate from queue-repair behavior |
| Fleet runtime diagnostics | `mods/src/patches/fleet_runtime_diagnostics.*` | Diagnostic counters, queue results, post results, and breadcrumbs | Keep separate from transport primitives |
| Fleet runtime capture | `mods/src/patches/fleet_runtime_sync.*` | Snapshot capture, delta suppression, and fanout to sync and sidecar paths | **Proposed:** split capture/state-key logic from routing and delivery fanout |
| Runtime impact probes | `mods/src/patches/mod_impact_monitor.*` | Runtime trace probe timing and periodic impact reports | Keep as dedicated observability utility |
| IL2CPP introspection probes | `mods/src/probe/probe.h` | Runtime class, method, and field introspection logging | Keep isolated and diagnostic-only |

## 4) Live Debug Transport and Query Surfaces
| Surface | Current Owner (existing) | Current Responsibility | Proposed Boundary (proposed) |
|---|---|---|---|
| File transport | `mods/src/patches/parts/live_debug_connector.cc` | One request-cycle read, delete, dispatch, and atomic response write | Keep transport-only |
| Command dispatch and response shaping | `mods/src/patches/parts/live_debug.cc`, `mods/src/patches/live_debug_request_dispatch.h`, `mods/src/patches/live_debug_state_results.*` | Parse request JSON, dispatch commands, and shape results | **Proposed:** continue separating dispatch helpers from the main hook-and-tick file |
| `recent-events` request helpers | `mods/src/patches/live_debug_recent_event_requests.*` | Request-to-query mapping and response envelope building | Keep as reusable pure helper layer |
| Dedicated navhook trace sink | `mods/src/patches/live_debug_navhook_trace_sink.*` | Step-trace append and file lifecycle | Keep separate from observer wiring |

## 5) Sync and Delivery Surfaces
| Surface | Current Owner (existing) | Current Responsibility | Proposed Boundary (proposed) |
|---|---|---|---|
| Hook ingress | `mods/src/patches/parts/sync.cc` | Detours, install wiring, and ingress into the sync pipeline | Keep as a thin ingress layer |
| Payload build and domain delta logic | `mods/src/patches/sync_payload_builders.cc` | Parse entity groups, diff and cache domain state, enqueue sync work | **Proposed:** split by domain payload families only when parity checks are ready |
| Main queue scheduler | `mods/src/patches/sync_scheduler.*` | Queue depth, drop policy, worker thread, and fanout entry | Keep separate from HTTP transport |
| External target transport | `mods/src/patches/sync_transport.*`, `mods/src/patches/sync_transport_policy.*` | Per-target worker queues, HTTP post logic, and target mode handling | Keep separate from ingress and payload parsing |
| Battle sub-pipeline | `mods/src/patches/sync_battle_logs.*`, `mods/src/patches/battle_log_decoder.*` | Combat queueing, enrichment, decoder integration, sidecar event building, JSONL export, and delivery branching | **Proposed:** split battle feed JSONL sink operations from decode and delivery workflow |
| Sidecar local ingest | `mods/src/patches/sidecar_local_ingest.*`, `mods/src/patches/sidecar_local_ingest_policy.*` | Local sidecar queueing, batching, readiness gates, and POST transport | Keep as adapter logic separate from external targets |
| Capability snapshot | `mods/src/patches/sync_capability_snapshot.*` | Startup capability snapshot emission | Keep as a small dedicated surface |

## 6) Mixed-Responsibility Hotspots
1. `mods/src/patches/parts/live_debug.cc`
- Still mixes hook install, event production, navigation context logic, command dispatch, and tick orchestration.

2. `mods/src/patches/sync_battle_logs.cc`
- Still mixes queueing, enrichment, decoder integration, sidecar JSONL retention, and delivery branching.

3. `mods/src/patches/sync_payload_builders.cc`
- Still mixes many domain parsers, delta caches, and scheduling dispatch.

4. `mods/src/patches/fleet_runtime_sync.cc`
- Still mixes snapshot-state derivation, suppression policy, and output routing.

5. Sidecar diagnostics namespaces
- `[sidecar.probes]` and `[sidecar.diagnostics]` exist today, but broad native diagnostics do not belong there unless they directly concern sidecar delivery or sidecar-oriented output behavior.

## 7) Risk Matrix
| Change Type | Risk | Notes |
|---|---|---|
| Documentation-only ownership mapping | Low | No runtime impact |
| Tighten current namespace guardrails in docs only | Low | Clarifies ownership without changing parsing |
| Extract sink-only file I/O from producer logic | Low | Navhook trace sink is already the proven example |
| Split live debug command dispatch from `parts/live_debug.cc` | Medium | Must preserve request semantics and tick timing |
| Split battle JSONL sink logic from battle pipeline | Medium | Must preserve retention and ordering behavior |
| Re-home general native diagnostics under sidecar namespaces | Medium | Blurs config ownership and makes later cleanup harder |
| Introduce future `[advanced.diagnostics]` config surface | Medium | Should be off by default and come with explicit config validation updates |
| Touch sync ingress, queue behavior, or queue-only Kir'shara repair | High | Out of scope for this slice and high regression risk |

## 8) Recommended Sequencing
1. Land docs that pin current ownership boundaries and correct old branch-local assumptions.
2. Preserve the current rule that `[sidecar.sync]` is delivery-only and `[sidecar.logging]` is sidecar-output-only.
3. If broader native diagnostics are needed, introduce `[advanced.diagnostics]` as a new off-by-default namespace instead of expanding `[sidecar.*]`.
4. Continue separating live debug command dispatch from `parts/live_debug.cc` while keeping the request-cycle ordering unchanged.
5. Isolate sidecar JSONL sink operations from `sync_battle_logs.cc` without changing event content, ordering, or retention behavior.
6. Consider splitting `fleet_runtime_sync.cc` into capture and routing helpers only after targeted parity checks are ready.

## 9) Ownership Guardrails
- Current code and config truth lives on `main`; do not resurrect abandoned manual-refresh-era or ghost-specific observability surfaces.
- Treat file-backed transport and evidence paths such as `*.cmd`, `*.out`, and `*.jsonl` as contracts once tooling or operators depend on them.
- Preserve existing defaults and gates:
  - `[debug].runtime_trace = "off"` by default
  - live query opt-in
  - sidecar and sync opt-in behavior
- Keep `config_sidecar.cc` and `tests/src/test_sidecar_config.cc` aligned on legacy rejection rules such as `sync.sidecar_jsonl*`, `[sync.targets.sidecar]`, `mode = "sidecar_broker"`, and loopback sidecar URLs under `[sync]`.
- Do not put extra native probes or diagnostics under `[sidecar.*]` unless they directly concern sidecar delivery or sidecar-oriented logging/output behavior.
- If `[advanced.diagnostics]` is added later, keep it off by default and scope it to general native diagnostics that are not sidecar-specific.
