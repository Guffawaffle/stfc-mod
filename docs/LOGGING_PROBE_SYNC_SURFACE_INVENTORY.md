# Logging, Probe, and Sync Surface Inventory

Date: 2026-06-02  
Repo: `D:\dev\stfc-mod`  
Branch: `feat/observability-config-cleanup`  
Baseline commit: `79d54c3` (`fix(queue): restore queue-only Kir'shara repair`)

## Scope
This inventory maps the current logging, probe, sync, and sidecar-adjacent observability surfaces on current `main`.

This pass records the current branch state:
- No runtime trace behavior changes.
- Only the live-query, runtime-trace, mod-impact reporting, refinery-diagnostics, and queue experiment key families migrated.
- No queue-repair changes.

Current-main rules for this snapshot:
- This branch starts from `main` at `79d54c3`, adds `[advanced.*]` schema support, and now keeps the active live-query, runtime-trace, mod-impact reporting, and refinery diagnostics keys under `[advanced.diagnostics]`, plus queue experiment/dev-test keys under `[advanced.queue]`.
- `manual_navigation_refresh`, ghost-hostile refresh, view drain/reload, and refresh hotkey work are abandoned and are not current runtime surfaces.
- The queue-only Kir'shara repair exists on current `main`; it is included here only where it emits probe or log artifacts.
- `.ax` operator tooling is not tracked on current `main`, so it is not documented here as active repo truth.

## Investigation Method
- Source scan for `spdlog::`, `live_debug`, `probe`, `runtime_trace`, `sync`, and `sidecar`.
- Focused review of:
  - `mods/src/config.h`
  - `mods/src/config.cc`
  - `mods/src/config_sidecar.cc`
  - `mods/src/defaultconfig.h`
  - `mods/src/patches/parts/live_debug.cc`
  - `mods/src/patches/parts/live_debug_connector.cc`
  - `mods/src/patches/parts/sync.cc`
  - `mods/src/patches/sync_*`
  - `mods/src/patches/sidecar_local_ingest*`
  - `mods/src/patches/action_queue_probe_logging.*`
  - `tests/src/test_sidecar_config.cc`

## A) Runtime Logging Infrastructure
- Root logger bootstrap and rotating archive policy:
  - `mods/src/patches/patches.cc`
  - Creates and rotates `community_patch.log`; no active user-facing `log_archive_count` config consumer was found on this branch.
- Canonical file naming and path resolution:
  - `mods/src/file.h`
  - `mods/src/file.cc`
- Runtime trace level model:
  - `mods/src/runtime_trace_config.h`
  - `mods/src/patches/mod_impact_monitor.h`
  - `mods/src/patches/mod_impact_monitor.cc`
- Runtime config snapshot writing:
  - `mods/src/config.cc`
  - `mods/src/config_sidecar.cc`

## B) On-Disk Runtime Artifact Surfaces
- `community_patch.log`
  - Main `spdlog` output.
  - Bootstrapped in `mods/src/patches/patches.cc`.
- `community_patch_action_queue_probe.jsonl`
  - Action queue probe JSONL.
  - Written by `mods/src/patches/action_queue_probe_logging.cc`.
- `community_patch_battle_feed.jsonl`
  - Sidecar-oriented local battle event JSONL feed.
  - Written and retained by `mods/src/patches/sync_battle_logs.cc`.
- `community_patch_navhook_trace.log`
  - Live debug navigation hook step trace with rotation.
  - Owned by `mods/src/patches/live_debug_navhook_trace_sink.*`.
- `community_patch_debug.cmd`
  - Live debug request file.
  - Consumed by `mods/src/patches/parts/live_debug_connector.cc`.
- `community_patch_debug.out`
  - Live debug response file.
  - Written by `mods/src/patches/parts/live_debug_connector.cc`.
- `patch_battlelogs_sent.json`
  - Battle log sent-id persistence path.
  - File naming in `mods/src/file.h`; read/write behavior in `mods/src/patches/sync_battle_logs.cc`.
- `community_patch_settings.toml`
  - Active user config.
- `community_patch_runtime.vars`
  - Resolved runtime config snapshot.

## C) Config Gates for Logging, Probe, Sync, and Sidecar Observability
- No active top-level `[debug]` observability gates remain on this branch.
- Historical stale references removed from this inventory after confirming there are no current consumers for:
  - `[debug].log_archive_count`
  - `[debug].action_queue_probe`
- Active advanced diagnostics gates:
  - `[advanced.diagnostics].live_query`
  - `[advanced.diagnostics].runtime_trace`
  - `[advanced.diagnostics].runtime_trace_track_overhead`
  - `[advanced.diagnostics].mod_impact_monitor`
  - `[advanced.diagnostics].runtime_trace_report_interval_ms`
  - `[advanced.diagnostics].refinery_diagnostics`
- Battle decode and sidecar-event shaping gates:
  - `[battle_log_decoder].enabled`
  - `[battle_log_decoder].emit_segments`
  - `[battle_log_decoder].emit_feed`
- External sync gates:
  - `[sync]` booleans by type such as `battlelogs`, `battlelogs_realtime`, `fleet_runtime`, `ships`, `jobs`
  - `[sync].proxy`
  - `[sync].verify_ssl`
  - `[sync].allow_unsafe_tls_without_certificate_validation`
  - `[sync].resolver_cache_ttl`
  - `[sync.targets.<name>]` per-target outbound transport config
- Local sidecar gates:
  - `[sidecar.sync]`
    - Canonical local sidecar delivery and loopback transport config
    - `enabled`, `url`, `token`, `proxy`, `verify_ssl`, `allow_unsafe_tls_without_certificate_validation`,
      `battlelogs_realtime`, `fleet_runtime`
  - `[sidecar.logging]`
    - Sidecar-oriented local JSONL output behavior
    - `jsonl`, `jsonl_replay_seconds`, `jsonl_recent_logs`
  - `[sidecar.probes]` and `[sidecar.diagnostics]`
    - Deprecated legacy input aliases for reserved observability toggles now owned by `[advanced.diagnostics]`
- Advanced-native gates:
  - `[advanced.diagnostics]`
    - Canonical native observability/probing namespace
    - Active in this slice: `live_query`, `runtime_trace`, `runtime_trace_track_overhead`, `mod_impact_monitor`,
      `runtime_trace_report_interval_ms`, `refinery_diagnostics`
    - Still dormant/reserved: `ship_identity`, `battle_log_decoder`, `battle_catalog`, `debug`, `logging`
    - `debug` and `logging` are dormant compatibility placeholders, not new active diagnostics controls
  - `[advanced.queue]`
    - Canonical queue experiment/dev-test namespace
    - Active in this slice: `queue_add_direct_handler`, `queue_add_hide_viewers`
- Legacy and invalid sidecar config handling:
  - `sync.sidecar_jsonl*` legacy keys are rejected in favor of `[sidecar.logging]`
  - `[sync.targets.sidecar]` is invalid
  - `mode = "sidecar_broker"` is invalid
  - Loopback sidecar ingest URLs are invalid under `[sync]` and `[sync.targets.*]`

Primary definitions and parsing:
- `mods/src/config.h`
- `mods/src/defaultconfig.h`
- `mods/src/config.cc`
- `mods/src/config_sidecar.cc`
- `example_community_patch_settings.toml`
- `tests/src/test_sidecar_config.cc`

## D) Probe Surfaces

### D1) Live Debug Channel and Event Ring
- Install, tick, and event production:
  - `mods/src/patches/live_debug.h`
  - `mods/src/patches/parts/live_debug.cc`
  - `mods/src/patches/frame_tick.cc`
- File request and response transport:
  - `mods/src/patches/live_debug_connector.h`
  - `mods/src/patches/parts/live_debug_connector.cc`
- Recent event store and append facade:
  - `mods/src/patches/live_debug_event_store.h`
  - `mods/src/patches/live_debug_event_store.cc`
  - `mods/src/patches/live_debug_event_dispatcher.h`
  - `mods/src/patches/live_debug_event_dispatcher.cc`
- Request parsing and result helpers:
  - `mods/src/patches/live_debug_recent_event_requests.h`
  - `mods/src/patches/live_debug_recent_event_requests.cc`
  - `mods/src/patches/live_debug_request_dispatch.h`
  - `mods/src/patches/live_debug_state_results.h`
  - `mods/src/patches/live_debug_state_results.cc`
- Observation and serialization helpers:
  - `mods/src/patches/live_debug_fleet_*`
  - `mods/src/patches/live_debug_ui_*`
  - `mods/src/patches/live_debug_viewer_*`
  - `mods/src/patches/live_debug_observation_compare.*`
- Dedicated navhook trace sink:
  - `mods/src/patches/live_debug_navhook_trace_sink.h`
  - `mods/src/patches/live_debug_navhook_trace_sink.cc`

### D2) Queue Probe Surface
- Gate and JSONL writer:
  - `mods/src/patches/action_queue_probe_logging.h`
  - `mods/src/patches/action_queue_probe_logging.cc`
- Queue repair call sites:
  - `mods/src/patches/parts/action_queue_repair.cc`

### D3) Fleet Runtime Diagnostics Surface
- Diagnostics model and counters:
  - `mods/src/patches/fleet_runtime_diagnostics.h`
  - `mods/src/patches/fleet_runtime_diagnostics.cc`
- Snapshot capture, suppression, and fanout:
  - `mods/src/patches/fleet_runtime_sync.h`
  - `mods/src/patches/fleet_runtime_sync.cc`

### D4) IL2CPP Introspection Probe Surface
- Generic runtime introspection utilities:
  - `mods/src/probe/probe.h`

### D5) Runtime Impact Probe Surface
- Timing taxonomy and report window:
  - `mods/src/patches/mod_impact_monitor.h`
  - `mods/src/patches/mod_impact_monitor.cc`

## E) Sync and Delivery Surfaces

### E1) Hook Ingress and Pipeline Wiring
- Main sync hook install and ingress:
  - `mods/src/patches/parts/sync.cc`
- Called from patch coordinator:
  - `mods/src/patches/patches.cc` via `InstallSyncPatches`

### E2) Payload Build and Queueing
- Entity-group parsers and delta state:
  - `mods/src/patches/sync_payload_builders.h`
  - `mods/src/patches/sync_payload_builders.cc`
- Main scheduler queue and worker:
  - `mods/src/patches/sync_scheduler.h`
  - `mods/src/patches/sync_scheduler.cc`

### E3) External Transport and Target Workers
- HTTP transport and per-target worker queues:
  - `mods/src/patches/sync_transport.h`
  - `mods/src/patches/sync_transport.cc`
- Target mode and policy contracts:
  - `mods/src/patches/sync_transport_policy.h`
  - `mods/src/patches/sync_transport_policy.cc`

### E4) Battle Sync and Sidecar Event Branch
- Battle header queueing, enrichment, dedupe, and export:
  - `mods/src/patches/sync_battle_logs.h`
  - `mods/src/patches/sync_battle_logs.cc`
- Battle decoder and sidecar event builders:
  - `mods/src/patches/battle_log_decoder.h`
  - `mods/src/patches/battle_log_decoder.cc`

### E5) Local Sidecar Ingest Branch
- Sidecar local queue, worker, and envelopes:
  - `mods/src/patches/sidecar_local_ingest.h`
  - `mods/src/patches/sidecar_local_ingest.cc`
- Sidecar ingest enablement policy:
  - `mods/src/patches/sidecar_local_ingest_policy.h`
  - `mods/src/patches/sidecar_local_ingest_policy.cc`

### E6) Capability Snapshot Surface
- Startup capability snapshot enqueue:
  - `mods/src/patches/sync_capability_snapshot.h`
  - `mods/src/patches/sync_capability_snapshot.cc`

## F) Logging File Emitter Index
Tracked `mods/src` files with direct `spdlog::` calls on current `main`: `38`.

1. `mods/src/config.cc`
2. `mods/src/errormsg.h`
3. `mods/src/probe/probe.h`
4. `mods/src/prime/ActionQueueManager.h`
5. `mods/src/patches/battle_notify_parser.cc`
6. `mods/src/patches/cargo_display.cc`
7. `mods/src/patches/fleet_actions.cc`
8. `mods/src/prime/ArmadaObjectViewerWidget.h`
9. `mods/src/patches/fleet_notifications.cc`
10. `mods/src/patches/fleet_runtime_diagnostics.cc`
11. `mods/src/patches/fleet_runtime_sync.cc`
12. `mods/src/patches/frame_tick.cc`
13. `mods/src/patches/hook_registry.cc`
14. `mods/src/patches/hotkey_dispatch.cc`
15. `mods/src/patches/hotkey_router.cc`
16. `mods/src/patches/incoming_attack_notifications.cc`
17. `mods/src/prime/ParentObjectViewerViewController.h`
18. `mods/src/patches/mod_impact_monitor.cc`
19. `mods/src/patches/notification_audio.cc`
20. `mods/src/patches/notification_platform.cc`
21. `mods/src/patches/notification_policy.cc`
22. `mods/src/patches/notification_service.cc`
23. `mods/src/patches/patches.cc`
24. `mods/src/patches/refinery_diagnostics.cc`
25. `mods/src/patches/sidecar_local_ingest.cc`
26. `mods/src/patches/sync_battle_logs.cc`
27. `mods/src/patches/sync_scheduler.cc`
28. `mods/src/patches/parts/zoom.cc`
29. `mods/src/patches/sync_payload_builders.cc`
30. `mods/src/patches/parts/object_tracker.cc`
31. `mods/src/patches/toast_dispatcher.cc`
32. `mods/src/patches/parts/live_debug.cc`
33. `mods/src/patches/sync_transport.cc`
34. `mods/src/patches/parts/fleet_arrival.cc`
35. `mods/src/patches/parts/action_queue_repair.cc`
36. `mods/src/patches/parts/improve_responsiveness.cc`
37. `mods/src/patches/parts/loading_screen_bg.cc`
38. `mods/src/patches/parts/hotkeys.cc`

## G) High-Value Separation Boundaries
These are the cleanest current seams for follow-on no-behavior-change work:

1. `LiveDebug Channel` vs `LiveDebug Observers` vs `RecentEvent Store`
- Transport: `parts/live_debug_connector.cc`
- Domain logic: `parts/live_debug.cc` and `live_debug_*` observer modules
- Storage and query: `live_debug_event_store.*`, `live_debug_recent_event_requests.*`, `live_debug_state_results.*`

2. `Sync Ingress` vs `Payload Builders` vs `Transport`
- Ingress hooks: `parts/sync.cc`
- Parse and transform: `sync_payload_builders.cc`
- Queue and target I/O: `sync_scheduler.cc` plus `sync_transport.cc`

3. `Battle Sync` as a standalone sub-pipeline
- `sync_battle_logs.cc` still combines queueing, enrichment, decoder integration, JSONL retention, and delivery branching.

4. `Sidecar Local Ingest` as a standalone adapter
- `sidecar_local_ingest.cc` remains a cleaner adapter boundary than mixing local sidecar delivery into external target transport.

5. `Queue Probe Logging` as a reusable helper
- Probe JSONL is already split into `action_queue_probe_logging.*`.
- The queue-only Kir'shara repair itself is not part of this cleanup slice.

6. `Runtime Trace and Impact Monitor` as independent observability
- `runtime_trace_config.h`, `mod_impact_monitor.*`, `[advanced.diagnostics].runtime_trace`,
  `[advanced.diagnostics].runtime_trace_track_overhead`, `[advanced.diagnostics].mod_impact_monitor`, and
  `[advanced.diagnostics].runtime_trace_report_interval_ms`

7. `General native diagnostics config` as the canonical namespace
- Current branch implements `[advanced.diagnostics]` as the canonical home for active live-query config, runtime trace config, mod-impact reporting, refinery diagnostics, and additional dormant observability toggles.
- Current branch implements `[advanced.queue]` as the canonical home for active queue experiment/dev-test controls.
- `[sidecar.probes]` and `[sidecar.diagnostics]` remain deprecated input aliases only.
- Broader native diagnostics should not be added to `[sidecar.*]` unless they directly concern sidecar delivery or sidecar-oriented logging.

## H) Current-Main Corrections
Compared with older branch-local observability notes, current `main` requires these corrections:

- `community_patch_navhook_trace.log` is owned by `live_debug_navhook_trace_sink.*`, not by `parts/live_debug.cc` alone.
- The tracked `spdlog::` emitter count is `38`, not `40`.
- `.ax` is not tracked on current `main`, so AX command surfaces are not active repo truth here.
- `manual_navigation_refresh` and ghost-hostile refresh diagnostics are not present on current `main`.
- `[advanced.diagnostics]` and `[advanced.queue]` now exist as canonical config surfaces on this branch; the live-query, runtime-trace, mod-impact reporting, refinery-diagnostics, and queue experiment keys are active so far.
- `dev_commands` is not consumed by current repo config. If it appears in a live TOML, treat it as stale local residue rather than repo truth.
- `[debug].log_archive_count` and `[debug].action_queue_probe` were stale documentation references only; no parser or runtime consumers were found on this branch.
- `[sidecar.probes]` and `[sidecar.diagnostics]` are retained only as deprecated input aliases for reserved observability toggles.

## I) Notes for the Next Planning Pass
- Current `main` already has a dedicated `config_sidecar.cc` parser and validation surface.
- `tests/src/test_sidecar_config.cc` is the source of truth for rejecting invalid legacy sidecar config shapes.
- The next docs or code pass should preserve the current ownership rule:
  - `[sidecar.sync]` for local native-to-sidecar delivery
  - `[sidecar.logging]` for sidecar-oriented local output behavior
  - broader native diagnostics under `[advanced.diagnostics]`, off by default
  - future queue experiment/dev-test keys under `[advanced.queue]`, not back under `[sidecar.*]`
