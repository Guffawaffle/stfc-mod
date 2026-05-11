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
| `community_patch_settings.toml` | Mod + user | User config | Created if missing | Source of truth for user overrides. |
| `community_patch_runtime.vars` | Mod | Runtime state snapshot | Rewritten on launch | Resolved settings snapshot after defaults/aliases. Do not edit. |
| `community_patch.log` | Mod | Legacy troubleshooting log | Created by current bootstrap logger | Plain-text spdlog output for boot/load troubleshooting. Not the preferred durable export format. |
| `community_patch_battle_feed.jsonl` | Mod | Structured local export feed | Explicit opt-in | Canonical sidecar fallback feed. Cyclic/bounded by replay/group retention settings. |
| `community_patch_navhook_trace.log` | Live debug / developer-only | Legacy debug trace | Normally absent | Plain-text developer trace for specific navigation-hook investigation. Not a stable user-facing export surface. |

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

- The current branch still has legacy plain-text troubleshooting logs. Tightening those
  paths toward the JSONL/ingress-first doctrine remains tracked in `#89`.
- Local JSONL capture is intentionally treated as fallback/debug capture, not the main road.
- Unlimited append-only local capture must stay an explicit choice, never the quiet default.