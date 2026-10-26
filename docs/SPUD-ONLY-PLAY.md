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

First native Windows trial ran source `1defe4b29680b5e6f5341ddc77665dd7e3da7d51`
on client 273, ordinary installation, PID 21708. DLL SHA-256:
`c36c201fcb38076c662fc9d98057629168c12783d1f131c0ff348e772bd80cb6`.
It booted, recorded 191 installed permanent hooks and zero refusals, registered
the original Settings adapter with 177 pages, and remained responsive. The
TOML hash before deployment, after launch and after startup was unchanged:
`78c248ce73fad72394405765db17fb95b5ee454c8be51fc8eddcf482b7f4f6f1`.
Receipts: `candidate.json`, `deploy.json`, `cycle.json`, `first-runtime.log` in
the local recovery directory below. No configuration changes were made.

The logged installation phase took about 23.6 seconds (23:58:44.527 to
23:59:08.150). This is startup installation cost, not sustained UI/frame cost.
No controlled old-SPUD timing comparison or per-operation attribution was run.
Successful installation does not establish each player workflow's equivalence;
Settings, Galaxy, chat, travel and presets still need player qualification.

Initial player response after trying the requested Settings/workflow check:
"seems just fine - scroll doesnt spill into ui -- looking good".
This confirms a favorable native trial and contained scrolling; it is not an
independent exhaustive assertion about every shortcut, preset or gameplay path.

The later CMake CRT-selection correction honors Xmake's supplied runtime for
debug configurations too. The release trial already used MultiThreaded; this
does not retrospectively qualify a debug or macOS build.

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
