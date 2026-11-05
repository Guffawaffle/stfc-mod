# Isolated foundation and dependency-build checkpoint

Date: 2026-10-25. Scope: **M1a and M1b only**. No game deployment, runtime
migration, active-branch promotion or release-infrastructure change is authorized
by this checkpoint. Existing play-dev, AAI, Profiles and Bridge checkouts remain
the working products and reference inputs.

Follow-up: [M1c execution-environment investigation](M1C-PROFILES-ENVIRONMENT.md)
qualifies all three previously blocked Profiles suites in an ordinary Windows
terminal, for both original and migrated binaries. The historical tool-context
failures below remain valid; no fixture assertion or safety guard was weakened.

## Sources and isolation

| Input | Exact source |
| --- | --- |
| Ripper completed foundation / M1a | `ee11492c46b2d591f3c3d77c9d98ee13b4b1e628` (PR #10, 9/9) |
| Existing fork preservation baseline | `45d1deaffddc3706e5c8232cf746ca46ef67be91` |
| AAI, including STFCKitUI | `4bece2e5f1afa7c7343f009e83398742a0f86114` |
| Profiles source | `c5c4ba911f16f3376e828761714d72a26ebe2a96` |
| Shared TOML owner | Fork baseline `45d1deaf`, `shared/toml` |

M1a was built before any downstream target changes. Its source and clean tree
were checked before and after execution. `study/ripper-foundation-m1a-20261025`
retains that exact foundation. M1b is developed separately on
`integration/ripper-foundation-m1b-20261025` in the explicitly approved worktree
`D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`.

Canonical `D:/dev/stfc-mod` remains on play-dev at `45d1deaf`. The old study ref
`study/ripper-impact-20261025` still preserves that fork baseline. No other
checkout was switched or used as a substitute integration workspace.

## Reconciled settings preservation baseline

[settings-preservation-baseline.json](settings-preservation-baseline.json) is
the explicit key-level baseline for subsequent settings work. Counts refer to
canonical persisted keys, not physical native rows, menu actions or visible
controls in a particular session.

| Definition | Windows | Non-Windows |
| --- | ---: | ---: |
| Generic in-game catalog fixture | 102 | 97 |
| Separately registered Audio Alerts catalog | 15 | 15 |
| Combined in-game preservation baseline | **117** | **112** |

The 15 audio keys comprise 13 cue choices, coalescing and incoming-player-attack
notifications. The older 116/111 documentation predates the AAI toggle; its
apparent 14-key difference from the generic fixture combines 15 excluded audio
keys and one stale increment. The migration study's 102/97 count was correctly
scoped to the generic fixture, but was not the full native settings baseline.

The current inventory has 195 named rows, while the existing source-discovery
gate requires 196. It has 19 dynamic target templates versus 20 required. The
baseline retains 196 named keys (117 in-game and 79 TOML-only), 20 dynamic
templates, 115 separately counted shortcut actions, and the original metadata.

The original Python inventory test was rerun at clean `45d1deaf`: **six tests,
four passes, two failures**. Missing entries are `sync.diagnostics` and
`sync.targets.<target>.diagnostics`. They are baseline maintenance failures,
not migration regressions. Neither the active inventory nor these tests was
changed to hide them. Counts alone do not qualify defaults, aliases, Live /
Reopen / Restart timing, availability, interaction or persistence behavior.

## M1a: exact foundation qualification

Official Zig 0.16.0 archives were SHA-256 checked. Windows used the documented
installed Visual Studio fallback: MSVC/STL 14.51.36231 and Windows SDK
10.0.26100.0. This differs from the manifest's optional 14.50.35717 archive;
it is an exact-source build, not byte-for-byte reproduction of upstream CI.

- Windows ReleaseFast libraries/mod with Debug fixtures: **68/68 steps** and
  the runner's **8/8 Zig tests**, plus successful native binding, GC/tracker,
  mod/reload, sync, dependency-smoke and x64 detour executables.
- Windows proxy verification: 16 expected exports, static CRT and provenance.
  Preserved DLL: 9,394,688 bytes; SHA-256
  `eab2d74dd9af5cd96bb29887ff43d7fdd392aec7c0d1a4d4ea8d24cd1193de33`.
- Native Ubuntu/WSL x64 fixture lane: **55/55 steps**, **8/8 Zig tests**, and
  the corresponding native executables including the launcher rsync fixture.
  The first Windows-mounted cache attempt failed on a rename; unchanged source
  passed using Linux-native temporary caches. No new source checkout was made.

No local macOS SDK or Xcode qualification was available. Prior pinned upstream
CI results for macOS are external evidence, not a new local build or game trial.
The current study does not claim macOS signing, notarization or runtime safety.

## M1b: build-only products

All options default off. The ordinary install graph, mod sources and feature
table are unchanged. Optional outputs are never passed to `mod.add`; building
them registers no hooks, starts no worker, selects no profile and loads no game
library. Developer tools below are build-time requirements, not player runtimes.

| Module | Explicit selection | Named products / checks |
| --- | --- | --- |
| `zig/aai.zig` | `-Daai=true -Daai-source=<absolute pinned checkout>` | `aai-inputs`, `aai-core`, `aai-cli`, `aai-tests`, `test-aai` |
| Native AAI | Also `-Daai-native=true` | `aai-native`; full existing 16-source embed archive and lazy simdutf |
| `zig/shared_toml.zig` | `-Dtoml=true -Dtoml-source=<absolute owning fork checkout>` | `shared-toml-inputs`, `shared-toml-static`, `shared-toml-native`, `build-shared-toml-tests`, `test-shared-toml` |
| `zig/profiles.zig` | `-Dprofiles=true` with explicit source/archive/prefix/receipt | `profiles-core`, `profiles-build-tests`, `profiles-tests` |
| Profiles adapters | Also `-Dprofiles-adapters=true` | `profiles-adapters`; all three adapters and an inert full-link fixture |

Use an explicit development output prefix, for example
`--prefix D:/dev/stfc-workspace/.ax/ripper-foundation-m1-20261025/artifacts/m1b-win016`.
Do not distribute the entire development prefix as a mod release.

AAI and TOML check HEAD and non-ignored dirty/untracked state. Mismatches require
their explicit `*-allow-dirty` override and are reported as development inputs.
No sibling repository discovery or implicit private checkout occurs. Disabled
options return before their private source/Git/prefix inspection; a missing
source path does not make ordinary builds require private credentials.

AAI uses its unchanged prepared analytical implementation for three CLIs and
the complete native archive. The two directly reused fixtures are canonical
officer normalization and selected-ship evidence, with assertions enabled.
This does not port or rerun all 42 AAI suites, replace the native host adapter,
or establish a complete in-game DLL linkage/lifecycle qualification.

The TOML producer keeps header-only toml++ 3.4.0 and JSON 3.12.0 at its existing
standalone ABI boundary. It does not export the foundation's compiled C++ parser.
The external client loads the real shared producer and exercises all nine
operations plus allocator custody. The exact three-export census is a separate
artifact check. The pinned header lacks a static-import spelling: the static
fixture uses its existing `STFC_TOML_BUILD` convention. Do not link that archive
into a product before deliberately resolving the resulting Windows exports.

Profiles compiles the full pinned common/platform core and all three inert mod
adapters. Its RuntimeApi owns its existing dynamic symbol lookup; it does not
import host game-binding headers or create a second typed IL2CPP owner. The
adapter-link fixture retains the public entry points without invoking bootstrap,
hook installation, process admission or browser launch.

### Profiles C dependency provisioning

`scripts/prepare-profiles-deps.py` is a bounded, explicit build/verify helper.
It never downloads, selects accounts or activates a runtime. Its supported
producers are native Windows x64/MSVC and macOS arm64/x64 with an explicit SDK.
The latter source path is implemented, not locally qualified.

Prepare source directories and their original archives from these existing pins:

| Source | Archive SHA-256 |
| --- | --- |
| Profiles `c5c4ba9` | `f120590499bb51844c1a2cd274f5bd58dbae0626b72c2d9f800041fabd08fd63` |
| libarchive 3.8.9 | `f5a6539059cf5e597dbeda37bfa4874b1e8dea063c8d93bf85a2b44af90a5bd4` |
| xz 5.8.3 | `3d3a1b973af218114f4f889bbaa2f4c037deaae0c8e815eec381c3d546b974a0` |
| zlib 1.3.2 | `b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1` |
| simdutf 8.2.0 | `033a91b1d7d1cb818c1eff49e61faaa1b64a3a530d59ef9efef0195e56bda8b1` |

The helper checks entire extracted trees against their original archives,
builds static Release/MT zlib, liblzma and libarchive, and records commands,
compiler/tool hashes, configuration and complete installed-prefix hashes.
It preserves xz/zlib and Windows CNG, disables the other specified optional
codec/crypto/XML libraries, and records any Windows system XMLLite linkage.
The final DLL integration must reconcile this prefix's zlib with the mod's
existing zlib; no such final product linkage occurs in M1b.

Example, from the appropriate developer shell (all paths must be explicit):

```text
python -B scripts/prepare-profiles-deps.py build --target x86_64-windows-msvc \
  --sources <verified-source-directory> --output <separate-build-directory> \
  --cmake <cmake-executable> --ninja <ninja-executable> --cc <cl-executable>
```

Then select `profiles-source`, `profiles-archive`, `profiles-deps-prefix` and
`profiles-deps-receipt` when invoking the Zig target. Verification rereads the
full source/prefix hashes and target/configuration fields. This is local
receipt-bound integrity and traceability, not independent or authenticated
attestation of the recorded producer commands.

## Results and actual integration obstacles

- Zig **0.16 and 0.17** each passed the **65/65-step dependency build**: AAI
  CLIs/native archive and two analytical fixtures, TOML static/shared producers
  and both semantic/ABI clients, and compilation/linking of all Profiles fixtures.
- The TOML Windows 0.16 DLL has exactly `stfc_toml_abi_version`,
  `stfc_toml_execute`, `stfc_toml_free`; only KERNEL32 imports, no dynamic CRT.
- Seven source-gate cases passed, including disabled nonexistent inputs, exact
  pins, missing/relative AAI roots and incorrect source revisions.
- Eight independent source/prefix verification tests passed. They cover altered
  archives, extra/changed source files, prefix mutations, missing closure and
  wrong target receipts.
- Profiles identity, public consumer and full adapter-link fixtures passed.
  **Preferences, catalog and installation execution did not pass.** The full
  runtime fixture gate remains red; successful compile-only qualification does
  not replace it.

The latter failure is `root_redirected`: globally shared installation locks
are deliberately located under the default catalog even when fixture catalogs
are temporary. Read-only native probes observed neutral lock requests opening
under Codex's package LocalCache. The probe reported NO_PACKAGE and
TokenIsAppContainer=0, so process package identity alone does not explain it.
The per-file redirection is observed; its precise mechanism and every failing
fixture's exact write-open attribution remain unqualified. Do not delete locks,
invent identity overrides or weaken the guard. A supported ordinary execution
environment or separate Profiles diagnosis is needed before calling these
fixtures qualified. Existing account data was not imported or cleaned up.

Resolved build obstacles were a missing generated DEF dependency, an omitted
public JSON include on the Profiles adapter, Windows CMake path escaping and
Debug try-compile/PDB behavior, and simdutf 8.2's Clang feature handling.
The simdutf fix forwards `+evex512` only to its x64 translation unit. LLVM
comparison preserved baseline AVX/AVX2/AVX512-disabled features; the attributed
Icelake region gained the required encoding feature. All kernels remain intact;
the Unicode/invalid-surrogate smoke passed on supported kernels on an actual
non-AVX512 host. No global AVX512 requirement, source patch, pin upgrade or
fallback substitution was introduced.

## Preservation coverage and next boundary

| Capability | Demonstrated here | Still pending |
| --- | --- | --- |
| Ripper foundation | Exact-source Windows build and Linux fixtures | Local Mac builds; exact-client hooks/gameplay |
| AAI / STFCKitUI | Immutable source, core/CLI/native archive, two fixtures | Host port, complete host link, 42-suite gate and in-game lifecycle/artwork/visual qualification |
| Shared TOML | Producer/editor/external ABI + export/import census | Product consumer integration, static spelling, signing and Bridge acceptance |
| Profiles | Full core/adapters compile and inert link, safe fixture subset | Three blocked fixtures; critical bootstrap/isolation/lease runtime cases |
| Mod Settings | Reconciled 117/112 key baseline with known failures | Native models/adapters, preserving writer, reload and input migration |
| Sync / remaining features | Preservation requirements retained | Port, behavioral tests and independent account/session qualification |
| Release / launcher | Existing contracts untouched | New producer provenance, signing/notarization and consumer packaging qualification |

M2 may implement shared hooks and failure semantics only after a separate
decision. Critical Profiles and persistence contracts follow; M1b has not
activated them. Typed defaults still cannot replace checked analytical evidence,
and hook rollback failure cannot become optional isolation readiness.

Mac consumers must carry Security/CoreFoundation when linking Profiles; the
generic Lib currently cannot express those frameworks itself. Optional products
are target-specific; the existing `macos-universal` step packages the ordinary
mod, not these new archives. No Mac runtime, signing, notarization, launcher,
SQLite, settings migration, UI redesign or game deployment is claimed.

Machine-local commands, inputs, logs, failed attempts, readback hashes and recovery
receipts live under `D:/dev/stfc-workspace/.ax/ripper-foundation-m1-20261025`.
Direct builds were used because the inspected AXF fork build lane is bound to
the original Xmake checkout; invoking it would have violated source isolation.
No lifecycle deployment lane or Runner-governed execution was used or claimed.
