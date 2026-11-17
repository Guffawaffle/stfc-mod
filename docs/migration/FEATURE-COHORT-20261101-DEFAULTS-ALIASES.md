# Settings defaults and aliases — 2026-11-01

Frozen source remains `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target base: `4865af29ad1fbc8d5499a42a177c1a4e391c813b`, existing Ripper
play-dev-ripper checkout. Capability: [CFG-LOAD-RELOAD](features/CFG-LOAD-RELOAD.md).

## Result

The selected product defaults match the actual frozen loader. Added an independent
190-value fixture in `tests/config_defaults_frozen.toml`, derived from frozen
config.cc/defaultconfig.h/config.h and notification catalogs. Production Parse
matches it for absent settings and for wrong-type arrays in every selected field.
Coverage: patches44, control9, graphics45, audio16, ui52, sync21, config2, buffs1.
Floating defaults compare numerically across source double and target float types.
The conditional experimental momentum default is checked separately. Existing
shortcut-default, parser, example-config, model and reload checks remain in use.

Declared constants alone are not effective behavior. In particular the frozen
loader explicitly defaults sync category flags to false, despite unused true
constants in DefaultConfig::Sync. Those dormant constants were not restored.
Empty-vs-three-named faction defaults retain the existing equivalent behavior.
Unused example-only keys remain unused; no new feature is implied by a constant.

The five-pair alias matrix checks missing, legacy-only, both supplied, malformed
canonical and malformed legacy input. It includes zero/false canonical values:
graphics.ui_scale_step/ui_scale_adjust, ui.format_cargo_values/cargo_format,
ui.disable_exchange_all/hide_artifact_exchange_all,
ui.focus_search/patches.focussearch and sync.officers/officer.
It reproduced one target mismatch: the custom focus-search alias path recorded
patches.focussearch in effective settings even when absent from input. Frozen
ConfigAliasDefault resolves the preference without recording that legacy key.

The correction generalizes the existing ReadRenamed helper to cross-section
aliases and routes focus_search through it. Canonical presence wins even for a
malformed value, as before; legacy-only input remains accepted. Effective output
now records only ui.focus_search. The generic renamed-key deprecation diagnostic
also applies to this alias. Primary settings are not rewritten by the reader.
No hooks, defaults, English example, settings UI, schema or CI registration changed.

## Explicit scope boundaries

System orbit remains parked: its source true hook default is not a restoration
request; the target false default is asserted separately. The newly recorded
DIAG-CLAIM-TRACE discovery identifies frozen patches.claimtracehooks and
ui.claim_navigation_recovery plus their separate diagnostic/recovery owner.
Its complete workflow/gates are unassessed and excluded from this product fixture;
discovery does not authorize implementing or activating it. The inventory records
its CFG-LOAD-RELOAD/PLAY-CLAIM parent relationship. Whole-refactor parity, native
settings interaction, all diagnostic scopes and Mac behavior remain separate.

## Evidence

`D:/dev/stfc-workspace/artifacts/settings-defaults-aliases-20261101/` retains
frozen constants/alias helper/catalogs, literal-reader/default-expression maps,
the explicit exclusions, fixture derivation, candidate hashes and before/after logs.
The alias fixture failed against unchanged production code at the target base
with three obsolete-focussearch assertions; it passes after the correction.

Native MSVC environment, Zig0.16.0, x86_64-windows-msvc, ReleaseFast production,
Debug tests, moddbg=true, four jobs:
`zig build build-tests test-mod -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast
-Dtest-optimize=Debug -Dmoddbg=true -j4 --summary all` passed 270/270 build/run
steps. Step count is not test count. These are production parser/owner paths with
isolated files and controlled host boundaries, not native UI/quit acceptance.
Commands, cwd, base, duration and exit codes are retained with hashed inputs.
Current Mac SDK is absent; this correction is not Mac-qualified.

## Windows delivery and generated-output observation

Clean signed source `4498a8b8987e3d3263cba773c730d9838ea1ea1e` passed the
41/41-step source-bound AX build, retaining clean AAI4bece2e5 and UI Lab. Source
remained stable during the build. One guarded ordinary-game cycle stopped PID28436
and started PID47028. Embedded build `ax-zig-20261101T160322-3446894a` identifies
that source; intended loaded module path, installed/build hash and full frame
readiness are verified. DLL SHA256:
`3E845314CFCF6CCDA9F229282A5942E2A5FBE7F2113442741BD340A77260CF03`.
No fresh startup errors observed. Known-working fb615d48 DLL is retained.

The existing primary file already uses the legacy focus key without the canonical
key. Before deployment, runtime vars contained both names. After deployment, vars
contain only ui.focus_search, and its resolved preference matches the existing
legacy value. This establishes the native generated-output correction for that
observed input without editing live settings. All four root TOML hashes match the
pre-cycle values; no agent TOML edits. No configuration values/tokens are printed
in these observation records. Native Settings interaction/quit and Mac coverage
remain separate. Exact cycle command, cwd, source, duration, exit and runtime
evidence are retained in native-cycle-command.json/native-cycle.log and
runtime-verification.json. Next: existing Settings/reset and shortcut-edit workflows.
