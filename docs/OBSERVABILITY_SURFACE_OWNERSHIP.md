# Observability Surface Ownership

Date: 2026-08-15

## Current Boundary

Native diagnostic investigations use the targeted diagnostics subsystem. A
temporary concern must be registered at compile time with an owner, tracking
issue, status, introduction version, sunset version, and cleanup criteria.
Runtime activation is limited to the generic allowlist:

```toml
[advanced.diagnostics.concerns]
enabled = ["runtime-impact"]
```

There is no independent runtime-trace level, impact-monitor toggle, reporting
interval setting, or fleet-runtime diagnostic mode. Concern-specific policy is
reviewed in source. TOML remains activation-only.

## Ownership

| Surface | Owner | Boundary |
|---|---|---|
| Concern registry and lifecycle | `mods/src/targeted_diagnostic_registry.*` | The only activation and lifecycle authority for targeted diagnostics |
| Queue, serializer dispatch, and writer | `mods/src/targeted_diagnostics.*` | Shared bounded transport; never place concern-specific evidence in the root logger |
| File policy | `mods/src/diagnostics_file_policy.*` | Shared root and permanent global resource ceilings |
| Fleet notification scan concern | `mods/src/patches/fleet_notification_diagnostics.*` | Temporary `fleet-notification-scan` records for issue #255 |
| Runtime impact concern | `mods/src/patches/runtime_impact_diagnostics.*`, `runtime_impact_monitor.*` | Temporary `runtime-impact` aggregate timings and space-action timings for issue #257 |
| Fleet runtime sync | `mods/src/patches/fleet_runtime_sync.*` | Functional snapshot capture and delivery only; no diagnostic logger or diagnostic mode |
| Root application log | `community_patch.log` via the bootstrap logger | Operational status and actionable failures only; not high-volume timing evidence |
| Live query | `mods/src/patches/parts/live_debug*` | Explicit runtime state inspection, separate from targeted diagnostic files |

## File Contract

Each enabled concern writes only its own typed JSONL records to
`community_patch_target_<concern-id>.jsonl` under the configured diagnostics
root. The shared writer bounds queue memory, record size, file size, retained
generations, and shutdown work. Each concern is limited to 1 MiB per file and
two files total. Unknown concern IDs are reported once and ignored. Known
expired concerns are reported once as expired and ignored.

## Guardrails

- Do not add a concern-specific TOML section or toggle.
- Do not mirror targeted records into `community_patch.log`.
- Do not add identities, pointers, names, or raw native payloads without an
  explicit schema and privacy review.
- Do not use functional delivery namespaces such as `[sidecar.sync]` for probe
  policy.
- Remove temporary producers and registration metadata at their sunset unless
  a review explicitly revises or promotes them.
- Preserve historical evidence documents as snapshots; they are not current
  configuration or ownership guidance.

## Historical Context

`LOGGING_PROBE_SYNC_SURFACE_INVENTORY.md`, `MOD_IMPACT_SAMPLE_ANALYSIS.md`, and
`STABILIZATION_GOVERNANCE_PASS.md` describe earlier branches. Their legacy
runtime-trace and fleet-runtime diagnostic settings have been retired and must
not be restored as parallel paths.
