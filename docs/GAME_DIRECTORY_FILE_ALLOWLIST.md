# Game Directory File Allowlist

Issue: `Guffawaffle/stfc-mod#89`

This document classifies the files the Community Mod and nearby companion tooling
may place beside the game or config folder. The policy target is:

- performance first
- security/privacy neck-and-neck
- local diagnostics/export explicit opt-in
- JSONL preferred for local structured capture
- cyclic/bounded retention by default
- ingress-first export preferred over long-lived local files

## Current Allowlist

| Path | Owner | Class | Default State | Notes |
| --- | --- | --- | --- | --- |
| `version.dll` | Windows mod / launcher | Managed deployment artifact | Present after manual or launcher install | The launcher may replace or remove this file only after explicit target validation, release verification, ownership/adoption checks, and the game-running guard. Manual installation remains supported. |
| `.version.dll.<transaction-id>.stage` | Windows launcher | Transaction staging | Normally absent | Same-volume verified staging file. It exists only during a journaled mutation and is removed on commit or rollback. |
| `.version.dll.<transaction-id>.rollback` | Windows launcher | Transaction rollback | Normally absent | Same-volume recovery file used while replacing or removing `version.dll`. An incomplete transaction must recover it before another mutation starts. |
| `community_patch_settings.toml` | Mod + user | User config | Created if missing | Source of truth for user overrides. |
| `community_patch_runtime.vars` | Mod | Runtime state snapshot | Rewritten on launch | Resolved settings snapshot after defaults/aliases. Do not edit. |
| `community_patch.log` / `community_patch.*.log` | Mod | Legacy troubleshooting log | Created by current bootstrap logger | Plain-text spdlog output for boot/load troubleshooting. Rotated at a small bounded size. Not the preferred durable export format. The bootstrap logger still owns its path on this branch. |
| `community_patch_battle_feed.jsonl` | Mod | Structured local evidence/export feed | Explicit opt-in | Optional sidecar diagnostics/import-replay feed. Cyclic/bounded by replay/group retention settings. |
| `community_patch_navhook_trace.log` / `community_patch_navhook_trace.*.log` | Live debug / developer-only | Legacy debug trace | Normally absent | Plain-text developer trace for specific navigation-hook investigation. Bounded by `[advanced.diagnostics.files]` and can be redirected with `root`. Not a stable user-facing export surface. |
| `community_patch_action_queue_probe.jsonl` / `community_patch_action_queue_probe.*.jsonl` | Mod diagnostics | Structured local queue probe trace | Runtime-trace gated | Structured action queue probe output used during detailed/verbose runtime tracing. Bounded by `[advanced.diagnostics.files]` and can be redirected with `root`. |
| `community_patch_target_<concern-id>.jsonl` / `community_patch_target_<concern-id>.*.jsonl` | Targeted diagnostics | Concern-isolated structured diagnostic capture | Explicit concern allowlist | Temporary or promoted concern records only. The process-wide writer applies finite queue, record, file, and retention budgets under `[advanced.diagnostics.files].root`; concern registrations carry an owner, issue, and enforced sunset. |

## Adjacent Artifacts

These files are part of the broader workflow but are not all owned as stable
core-mod export surfaces today:

| Path | Owner | Class | Notes |
| --- | --- | --- | --- |
| `community_patch_settings.toml.bak.sidecar` | Sidecar | Settings backup | Created by the companion before replacing an existing config file. |
| `patch_battlelogs_sent.json` | Legacy sync path | Runtime bookkeeping | Historical/legacy battle-log sync bookkeeping path still named in `File::Battles()`. |
| `community_patch_battle_probe*.jsonl` | Tooling / experimental diagnostics | Diagnostic artifacts | Referenced by AX and sidecar cleanup logic, but the current writer ownership is not yet fully documented in this repo. |
| `community_patch_settings_parsed.toml` | Obsolete legacy file | Removed on startup if found | Kept here only so cleanup behavior is documented. |

## Notes

- The current branch still has legacy plain-text troubleshooting logs. The bootstrap log is
  bounded, but tightening those paths toward the JSONL/ingress-first doctrine remains tracked in `#89`.
- Local JSONL capture is intentionally treated as diagnostics/evidence/import-replay capture, not the main road.
- Unlimited append-only local capture must stay an explicit choice, never the quiet default.
