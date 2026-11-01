# CFG-LOAD-RELOAD — settings activation and reload

Contract version: 1. Status: bounded startup/reload correction; complete key-by-key
configuration parity remains open.

## Inputs and scope

Frozen source: `1baa55fedf1758b19f843cc59707b8901a0b4998`,
`mods/src/config.cc` (`Config::Load`, `Save`, `Get`) and `mods/src/config.h`.
Target: existing `play-dev-ripper` checkout, base
`71e49f9dacd94916396d5e326521fa1c4231610d`; changed production input is
`src/mod/config.cc`, with regression in `tests/config_reload.cc`.
Evidence directory: `D:/dev/stfc-workspace/artifacts/settings-lifecycle-20261101/`.
`candidate-inputs.json` binds the changed inputs and tests to this base.

| Inspected branch | Exact revision | Presence and bounded observation |
| --- | --- | --- |
| upstream dev | `ff179589f834b95331b4d7e38b1054fb6ff5ace9` | Config::Load present in mods/src/config.cc; source symbol observation only |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Config::Load present in mods/src/config.cc; source symbol observation only |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Load fallback, primary creation, generated vars and runtime writer baseline inspected |
| refactor | `71e49f9dacd94916396d5e326521fa1c4231610d` | Get/Parse/Activate/Reload and frame polling inspected; correction bound by manifest |

The default settings loader runs at initialization. `mod.cc` owns guarded frame
delivery of `Config::ReloadIfChanged`; `features/hotkeys.cc` owns explicit reload.
`runtime_config` continues owning pending/session-only save intent and the shared
quit lifecycle. This correction adds no hooks, keys, persistence format, Settings
surface or CI registration. It retains the accepted mission/pin work and existing
AAI/UI Lab selection. Writer/state-store reconciliation remains separate work.

## Behavioral obligations

| ID | Source behavior / target decision | Mapping and evidence |
| --- | --- | --- |
| L1 | Unreadable/malformed startup input uses defaults; preserve malformed primary bytes | Existing Get/ReadSettingsFile/SaveEffective; startup-invalid-primary mode passes |
| L2 | Create complete defaults only for a missing primary; optional startup output failure must not prevent activation | Existing SaveEffective/SaveStartupConfigDocument; startup-missing-primary and startup-output-failure modes pass |
| L3 | Primary settings and generated effective vars have different roles; edits/reloads must not rewrite the primary | Existing SaveRuntimeVars; new watcher mode verifies exact primary bytes including comment |
| L4 | Reload is an existing refactor improvement. A settings edit after Get but before the first frame must eventually apply without a second edit | Narrow correction: record modification time before the actual read; never establish an assumed read baseline at the first poll. New startup-before-first-poll mode fails against base and passes after correction |
| L5 | Keep once-per-second polling and stable-file debounce | First changed poll leaves settings active; second stable poll applies scale/shortcut. Failed attempts remain remembered until another edit |
| L6 | Installed patch switches remain restart-only | Existing Reload retains Get().patches and reports installed values; watcher test changes the file switch and verifies the active switch is retained |
| L7 | Preserve pending/session-only intent captured before opening the file; reject malformed candidates/output failure without replacing active settings | Existing CaptureReload/ApplyReloadSnapshot and transactional output path retained; default config-reload-tests exercises these production paths with isolated file and owner adapters |
| L8 | Retained sync readers, accepted keyboard diagnostics, shortcut replacement and queue-policy reset keep existing owners | Existing Activate and reload tests retained; no new publication or worker owner |

## Design and limits

Previously the first poll assumed its observed timestamp belonged to the file
already loaded by Get. An edit in the startup-to-frame gap was therefore silently
acknowledged without parsing its bytes. The watcher now starts with the timestamp
captured before startup parsing. Successful explicit reload also updates that
baseline. If the initial timestamp is unavailable, a later readable file follows
the normal debounce/reload path. Capturing before reading favors an extra reload
over silently losing an edit during the read.

This remains modification-time polling, not an atomic file snapshot: edits that
preserve the timestamp are outside its detection contract. Stable intervals cannot
prove an editor has completed all future writes. Automatic failed attempts retain
the existing suppression of repeated rejection messages until another edit.
No change to source defaults/aliases is claimed by this timing correction, and no
whole-catalog equivalence is inferred from the lifecycle tests.

## Validation and disposition

Windows native MSVC environment, Zig 0.16.0, x86_64-windows-msvc,
ReleaseFast production / Debug tests, moddbg=true, four jobs:
`build build-tests test-runtime-config test-runtime-reload test-mod-state
test-toml-editor test-config-save test-settings-models` passed 230/230 build/run
steps. All five explicit config-reload modes passed. These fixtures exercise real
configuration code on exclusive temporary files, with synthetic host boundaries;
they do not edit live settings or qualify native quit/rendering.

After formatting, the final `build build-tests test-config-save` rerun passed
197/197 steps and the new startup-gap mode passed again. The manifest matches
the final production/test bytes. No test registration was changed.

Logs, commands, base SHA, durations and exit codes are in the evidence directory.
`before.log` preserves the failing startup-gap assertion; `after-*.log` and
`reload-modes.json` preserve the corrected results. Native delivery will be recorded
after the source-bound build/cycle. Current Mac SDK is absent; this correction is
not Mac-qualified. Key-by-key defaults/alias reconciliation, CFG-WRITER,
STATE-STORE and STATE-QUIT coverage remain open. Accepted pin click/drag behavior
does not require an extended pin test round before this work can proceed.
