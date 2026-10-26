# SPUD-only PLAY upgrade checkpoint

## Scope and exact inputs

Branch: `upgrade/spud-only-play-20261025`.
Base: `cad98ef7990dd66167eb64ea5dda525c1fe2c296` (PLAY).
SPUD snapshot: `e4d0c78be89ef2587af33dd634ae5e8071935be7`, retained vendor tree
`b443aa0ef4ce1a43049fcb840478206c7e569d10`.
The private SPUD repository was not fetched; source is the supplied snapshot.
See [vendor provenance](../xmake-packages/packages/s/spud/spud-src/SOURCE.md).

This replaces the detour library underneath PLAY. It does not port product code
from the abandoned Ripper-foundation migration, change Settings models/UI,
change defaults, migrate stores, or adopt typed IL2CPP or Zig.
The earlier migration checkpoint and its implementation stash remain untouched.

## Implementation

- Exact replacement of vendor `include/`, `src/`, and MIT license. The CMake
  recipe is downstream-owned. Fresh Xmake package `v1.0.0-play1` prevents reuse
  of the old package identity. Existing external package pins are unchanged.
- New source list and MSVC context MASM helper; no obsolete AsmJit dependency.
  SIMD kernels use their own target attributes without raising the CPU baseline.
  Static CRT, protected patching, unwind support and tracing-off ABI retained.
- Object-tracker signature result: `.get(0)` becomes `[0]`, preserving the
  existing empty guard, patterns, selection and finalizer lifecycle.
- Mac hook-fit adapter keeps independent LC_FUNCTION_STARTS extent discovery,
  existing 32-byte/minimum floor and exact target, and checks both landing sizes
  against that proven extent through `inspect_detour`.
- [Installation adapter](../mods/src/patches/spud_install.h) preserves the vendor
  macro's permanent, per-site cached success/null-failure behavior. Installation
  results are reported outside the hot call path. Every refusal includes source
  site, error, OS status, reason and diagnostic address when available.
  `rollback_failed` aborts rather than continuing with indeterminate patch bytes.
  Bootstrap null/exception is visible. Summary counts attempts, not features or
  behavioral parity. Lazy installations may occur after the startup summary.
- The 49 other changed product files contain only an include substitution.
  Existing feature logic, Settings construction, cleanup and persistence remain
  PLAY's implementation. No transactions or replacement registry are introduced.

## Offline qualification, 2026-10-25

- Complete Windows x64 release DLL built through PLAY's Xmake/MSVC path.
  Initial clean build: 122.188 seconds. This includes compilation and link,
  not a game performance measurement.
- Extended [relocation regression](../tests/spud_relocation.cc) passed through
  the same Xmake package/MASM build: RIP-relative operands, relative call,
  both branch paths, removable call-through/restoration, permanent call-through,
  repeated cached success and cached failure with a later different target.
  One deliberate null-target refusal is expected in this fixture.
- `tests/run-settings.ps1`: all local fixtures passed, including shortcut
  editor/hints, Settings models, persistence, ACLs, disk reload, preservation,
  conflict handling, failed/short writes and force-close deadlines.
  Output: `build/settings-play-spud.log` (local build evidence).
- Compared copied source with the retained snapshot: `include/`, `src/`,
  `LICENSE` identical. Verified the 49 include-only files against pinned PLAY.
- `git diff --check` passed.

## Runtime qualification and boundaries

Offline passes do not prove acceptance of exact-client targets or player workflows.
First trial should check boot and all logged installation refusals, then original
Mod Settings navigation/search/chord editing/header and selected styling, Galaxy
layers/labels, travel modifiers, chat portraits and presets. Retain the real TOML.
Use an exact source-identified prebuilt DLL, build before stopping the game,
and record source/hash/session/config preservation in the recovery evidence.

macOS build/runtime, ARM64/Rosetta hook fit, signing/notarization and release
packaging remain unqualified. No promotion, release or functional-parity claim.
The newer engine can reject a target that old SPUD accepted; investigate the
actual reason rather than disabling guards or silently counting a feature ready.

Current worktree and local receipts are under
`D:/dev/stfc-workspace/worktrees/play-spud/Downstream` and
`D:/dev/stfc-workspace/.ax/play-spud-20261025` respectively.
