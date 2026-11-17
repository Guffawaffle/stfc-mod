# M1c: Profiles execution-environment investigation

Date: 2026-10-25. M1b source checkpoint:
`de0d436db127db53ee568c07a8219e55c32fe85b`.

## Outcome and decision

All three unchanged Profiles fixture suites passed for **both** the original
source control and migrated Zig build when Guff ran them from an ordinary,
non-administrator terminal. The same six binaries fail with `root_redirected`
when launched through the Codex execution context, including a repeat using the
same PowerShell executable and working directory as the ordinary run.

The immediate cause is a mismatch between the requested shared installation
lock and the private app-storage file actually opened. A diagnostic variant
observed the production-acquired handle: Win32 final-name queries and the NT
object-name query agree on the private LocalCache destination. The exact
component responsible for that redirection is **not identified**.

No product or fixture-source correction is justified by these failures. The
physical-root guard enforces the intended shared-lock contract. Keep it and the
fixture assertions unchanged. This is an execution-context limitation, with no
migration regression demonstrated in these suites. Independent M2 hook work can
proceed; this document does not implement or authorize runtime migration.

## Behavior before tests

The pinned Profiles source is
`c5c4ba911f16f3376e828761714d72a26ebe2a96`, not the newer canonical Profiles
checkout. Source references below refer to that exact pin:

- `src/catalog.cc:126`: `CheckSharedLockHandle` checks the opened handle against
  the requested neutral path; mismatch is `root_redirected`.
- `src/catalog.cc:234`: the real lock open uses `OPEN_ALWAYS`, shared-read or
  exclusive access as appropriate, and `FILE_FLAG_OPEN_REPARSE_POINT`. Ordinary
  file attributes, one hard link and final path are checked before admission.
- `src/catalog.cc:603`: the default root uses the known-folder API with
  `KF_FLAG_NO_PACKAGE_REDIRECTION`. A neutral path string does not guarantee a
  later file open will be unredirected.
- `src/catalog.cc:620`: `InstallationLease` deliberately uses the default
  shared lock directory, including when its caller supplies a temporary catalog.
- `src/catalog.cc:669`: `SessionLease` acquires an installation lease for the
  running executable's parent directory. Preferences and catalog suites therefore
  exercise real host coordination as well as disposable fixture data.
- `tests/installation_test.cc:190`: cross-catalog installation contention is
  explicitly asserted. `docs/GAME_INSTALLATION_CONTRACT.md:48` requires common
  installation exclusion across actors.

Moving each suite's catalog into a temporary directory cannot make this IPC
boundary local to that suite. Moving or overriding the global lock root would
remove an intended safety property. Accepting independently redirected lock
copies could split exclusion between actors.

Functionality alignment still comes first. A future test categorization may
separate pure logic, disposable-filesystem units and host integration. That
would clarify prerequisites; it must not turn a host refusal into a passing
integration result or replace global contention coverage. No such refactor was
needed or performed here.

## Controls and provenance

The original control uses the exact immutable c5c source and original fixture
mains, compiled directly with native MSVC 14.51.36231. It is **not an execution
of the original Xmake build**. Its receipt preserves the actual commands,
definitions, source hashes, seven core translation units, JSON headers and
pinned C dependency closure. The original Xmake graph's unrelated dependency
resolution was not introduced into this diagnostic.

The migrated executables are the existing M1b Zig 0.16 outputs, not rebuilt or
patched for the comparison. The manifest binds all six executable hashes and
lengths plus the native context probe. Source and copied bytes are checked
before and after execution; stdout/stderr and exact harness/manifest snapshots
are retained per run.

Each original/migrated pair runs sequentially at the same
`paired-slot/<fixture>.exe` path. This matters because preferences/catalog
installation identity derives from the executable directory. Their common
installation key is
`7719d1939e4e7c388e298d31c2ca870cbb84e4227db2504ae72928581cca02de`.
The installation suite creates a random internal temporary game directory; that
suite's installation identity is not identical between runs. No identity was
substituted to make it match.

| Suite | Original, Codex context | Migrated, Codex context | Original, ordinary terminal | Migrated, ordinary terminal |
| --- | --- | --- | --- | --- |
| Preferences | `root_redirected`, exit 1 | `root_redirected`, exit 1 | Pass, exit 0 | Pass, exit 0 |
| Catalog | `root_redirected`, exit 1 | `root_redirected`, exit 1 | Pass, exit 0 | Pass, exit 0 |
| Installation | `root_redirected`, exit 1 | `root_redirected`, exit 1 | Pass, exit 0 | Pass, exit 0 |

Both ordinary installation runs report 13 recovered crash checkpoints and 12
replayed recovery checkpoints. Paired stdout hashes agree. No suite timed out.
These durations are not a performance benchmark.

## What differs between environments

The user-run native probe and harness establish an ordinary terminal lineage:
PowerShell 7.6.6 -> Windows Terminal -> Explorer. The Codex-side comparison
includes PowerShell -> Codex -> packaged desktop application ancestry. A repeat
used exactly `C:/Program Files/PowerShell/7/pwsh.exe` and `C:/Users/Guff` as the
working directory on the Codex side; all six failures remained.

Both contexts use the same Windows user, non-administrator medium-integrity
tokens, no package identity (`15700`), AppContainer false and token virtualization
disabled. The native probe is in a job in the Codex context and outside a job in
the ordinary context. These observations do **not** identify a job policy,
AppContainer, PowerShell, UAC virtualization, filesystem filter or shim as the
cause. No guard bypass, token/environment rewrite, elevation or alternate user
was used.

The actual-handle observation came from a separately labeled private diagnostic
build of the original source. It adds metadata queries immediately after the
production `CreateFileW`, preserves last-error state, and neither replaces the
open nor closes the borrowed handle. Guards and assertions are unchanged. This
variant failed as expected and is not one of the six baseline qualification
executables.

Its preferences/catalog handles identify the same private file object. The
installation diagnostic also captured its fresh internal key opening privately.
The handles have one link, no reparse tag and ordinary-file attributes. This
rules out a simple final-file symlink explanation for those acquisitions; it
does not identify the redirecting mechanism. No global lock files were deleted,
moved or replaced.

## Qualification limits

- Ordinary Windows qualification applies to the exact six executable inputs and
  observed environment. It is not a complete production account-isolation,
  game-hook, Bridge, macOS, release-signing or runtime-migration qualification.
- The harness could not read the running image path before three short-lived
  ordinary child processes exited (migrated preferences and both installation
  cases). Exact requested paths, launched PIDs, source/copy hashes before and
  after, exit status and output remain recorded. Do not claim live image-path
  readback for all six processes.
- No kernel/API-stack trace identified the responsible filter, shim or inherited
  policy. More investigation is optional if tool-host execution becomes a
  requirement; it is not necessary to begin independent M2 fixtures.
- This does not upgrade Profiles, redefine its default-root contract, or prove
  other newer Profiles revisions. No real account preferences were imported.
- No mod deployment, game restart, active-branch change or release change occurred.

## Exact local evidence and recovery

Private evidence root:
`D:/dev/stfc-workspace/.ax/ripper-profiles-m1c-20261025`.

| Evidence | Relative location |
| --- | --- |
| Original source/control build | `original-control/build-receipt.json` |
| Exact six inputs | `m1c-comparison-manifest.json` |
| Developer/tool-host trial | `comparison-results/DeveloperTool/20261025T082925380Z-4a43b94c/run.json` |
| Human ordinary trial | `comparison-results/OrdinaryClaimed/20261025T083146538Z-cbe7c245/run.json` |
| Matched PowerShell/CWD repeat | `comparison-results/DeveloperTool/20261025T083459912Z-b286e39b/run.json` |
| Production-handle diagnostic build | `diagnostic-control/build-receipt.json` |
| Actual acquired-handle records | `diagnostic-control/tool-host-paired/*.stderr.log` and `run.json` |
| Diagnostic source and context probe | `m1c_handle_observer.h`, `m1c_context_probe.cc`, `context-probe-receipt.json` |
| Windows mechanism research | `mechanism-findings.md` |
| Reproduction guidance | `HARNESS-NOTES.md`, `Run-M1c-Ordinary.ps1` |

Ordinary run receipt SHA-256:
`631ae05bb47ab01d239cd97879a2e0e533b5b0b9f80b27fc4e9089bb032bb141`.
Each run contains immutable harness/manifest snapshots. These are local
source-to-artifact receipts, not authenticated third-party attestations.

Direct tooling and independent agent review were used. No Runner-governed
Attempt, Runner-created recovery frame or application of a shadow persona is
claimed. The existing M1b source/build checkpoint and this document are the
durable implementation references. M2 has not started at this checkpoint.
