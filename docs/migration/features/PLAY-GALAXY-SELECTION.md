# PLAY-GALAXY-SELECTION — Far-tier system tapping

Contract v1, source scan 2026-10-27. Source frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` starts at
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`. Candidate-bound build, fixture and
native qualification belong in the [map cohort](../FEATURE-COHORT-20261027-03.md).

## Complete source workflow

Read the complete frozen `mods/src/patches/parts/galaxy_selection.cc` and its
309-line `galaxy_selection.h`, shared pin graph and pin metadata helpers, native
World/Galaxy/camera/navigation/POI/visibility bindings, installation table,
configuration/defaults, Settings definition and registration, and retained
selection-pin tests. Current consumers are `features/galaxy_selection.cc`,
`galaxy_selection_policy.h`, `galaxy_selection_pins.h`, `game/galaxy.h`, the
existing feature registry/frame owner and registered Settings writer. No second
hit-test engine, scene-wide entity scan or independent frame hook is introduced.

| Inspected branch | Exact revision | Presence / evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | No selection module or `galaxy_extended_selection`/`OnTapRecognised` consumer found in source search; this is an absent custom capability, not a claim about all native game selection. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Existing `mods/src/patches/parts/galaxy_selection.{cc,h}` consumer. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Full producer, proxy/pin lifecycle, consumers and Settings inspected. |
| refactor base | `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` | Existing selection module and current Settings controls. Earlier inventory Partial is not evidence that these are missing now. |

Entry: tap a minor system in Galaxy at Far zoom. The live
`graphics.galaxy_extended_selection` setting defaults false; its hook group is
owned by the existing restart-only `patches.zoomhooks` gate. Mod Settings →
Galaxy → Extended system selection already exists and uses
`galaxy_controls::SelectionAvailable` plus the established preserving writer.
No new setting, default, path or persistence owner was added.

## Behavioral obligations

| ID | Source operation | Preserved or adapted target / verification |
| --- | --- | --- |
| S1 | Disabled/non-Galaxy/non-Far/unavailable input follows native selection. | Original OnTapRecognised is retained on ordinary refusals. Only confirmed Far Galaxy input enters proxy preparation. Checked depth/level/source observations and controlled disabled/foreign-thread original tests. |
| S2 | Obtain the viewed Galaxy's complete ID/X/Y arrays and use native IsMinorNode; minor classification is not IsNodeViewable/unvisited rejection. | Existing rooted protobuf backing-array decoder retained. It validates count, element type and backing capacity without per-node boxing. No 100000 source-node cap or 4096 owner-list cap is reintroduced. Production candidate scanner reaches index 100004 in a controlled fixture. |
| S3 | Use a click-local 64-pixel broad prefilter and native POI interaction spheres. Existing native Star POIs retain priority. | Same four coordinate transforms and native GetOfType. Failed lookup now remains unavailable, rather than observed-null permission to create a duplicate. Candidate helper tests existing/null/failure distinctions. Full world projection/hit testing remains native qualification. |
| S4 | Bound click candidates at 16 and owned proxy pool at 64; pressure must fall back, not pick a truncated target list. | Existing target completes candidate qualification before activation. Sixteen succeeds, seventeen declines before any activation. All source nodes remain eligible for scanning. Limits are proxy-work policy, not source truncation or evidence completeness claims. |
| S5 | Proxy ID is immutable; native UI owner references and recent clicks must not be recycled into another system. | Existing object/POI strong custody and per-proxy Galaxy weak identity retained. Retirement requires age ≥2 seconds, more than two generations, complete known owner coverage and no pin. Pool-pressure fixture checks incomplete owners, explicit pins, failed destruction and destruction-only retirement. |
| S6 | Cover selection list, current/pending viewer/context/parent query owners; retain incomplete pins conservatively. | Existing target owner graph additionally covers cached interactions and camera tracking. PinReader roots acquired owners and distinguishes successful null from unknown; dictionary counts/unknown keys and full graph failures retain their existing binding/pure tests. No capped list iteration or guessed owner identity. |
| S7 | Temporary POIs participate only during native tap processing; do not force a POI or enlarge hit radii. | Keep original recognizer, exact manager/position tap correlation, native ProcessInputTap and a single unforced OnSingleTap only if original did not dispatch this tap. Same/different-position and no-preparation fixtures exercise production dispatch. |
| S8 | Replaced world/Galaxy/camera/animator or disabled setting must not publish a mixed-source click. | Bounded click-local strong source custody survives through original dispatch; checked current-source readback gates preparation, activation and fallback. Recursive source validation cannot mutate the pool. Scene replacement fixture; real replacement timing remains native trial. |
| S9 | Failed enable/disable/destruction cannot silently release unsafe native objects or authorize fallback. | Existing failed-cleanup custody is preserved. A new blocked-tap bit prevents outer fallback after ProcessInputTap refused because deactivation was unconfirmed. Fixtures cover failure before mutation, confirmed inactive after a throwing mutation, retained roots and later cleanup. |
| S10 | Feature ownership, signatures and platform admission are explicit. | Existing single descriptor, two-hook transaction, frame cleanup and exact installed-thread guard retained. Vector2 qualification is shared for ProcessInputTap/OnSingleTap; all three raw entries and snapshotted CheckedEntry bodies must be pairwise distinct before staging, including the directly invoked fallback. Exact checked hook addresses are staged without another resolution. macOS still requires CI/native testing. |

## Source preservation and deliberate corrections

The frozen Windows recognizer bypassed its original at Far tier; macOS used a
thread-local process counter. The existing typed target instead preserves the
original on both platforms and correlates manager plus exact position before an
extra native tap. This stronger established target behavior is preserved.
The frozen source's weak-only proxy handles, unconfirmed cleanup, source count
cap and eager per-candidate activation are not resurrected.

This pass fixes bounded failures in the existing target: swallowed native POI
lookup failure, incomplete scene/animator source validation, and a fallback that
could still run after cleanup refusal. It does not redesign pool admission,
selection geometry or pins. Private candidate/dispatch helpers contain the same
production operations so fixtures can exercise them without a second selector.
Pairwise entry admission also deliberately declines a shared body on Windows:
invoking OnSingleTap at an address detoured for another arity could enter that
incompatible callback. Frozen source did not establish this safeguard. Refusing
that metadata/code layout preserves native selection rather than guessing ABI
compatibility; it is not a claim that a live client exhibits this alias.
The same admission covers distinct ARM64 entry thunks that CheckedEntry resolves
to a shared branch body. A private resolver parameter lets the controlled fixture
prove all three resolved-alias pairs without pretending to run a macOS detour.
Production supplies CheckedEntry directly. A missing/undersized resolved entry
declines the feature; no unknown body is treated as proof of independence.

Each fully constructed proxy retains object and POI roots plus a weak Galaxy
identity. Partial construction keeps custody before fallible native mutations.
Failed cleanup remains owned and blocks unsafe hit testing; the existing frame
callback retries deactivation. A different thread leaves native dispatch alone
and does not mutate this pool. Click-source roots and thread-local correlation
pointers expire after that synchronous dispatch. Process-lifetime owners avoid
late GC API calls during C++ static destruction.

## Qualification boundaries and recovery

`tests/galaxy_selection_bindings_cases.h` includes the actual production adapter.
Its controlled runtime replaces native method bodies, current scene objects,
input/config and object creation state. It exercises source-span enumeration,
native lookup failure, current-source identity, dispatch/original counts,
cleanup and pool ownership. It does **not** run Unity GameObject creation,
coordinate projection, actual POI hit testing, real view pooling or SPUD native
installation. The alias case proves refusal before a transaction, not successful
game detouring. Existing array, pin/dictionary, policy and Settings tests retain
their distinct layers. No test result is claimed before the coordinator's run.

Native checklist: enable the existing setting; compare Near/normal and Far taps;
tap existing and minor systems repeatedly; rapidly change Galaxy/camera/selection;
keep a system card or camera tracking reference while selecting others; verify
native overlap choice and density fallback. Disable/re-enable and close/reopen
the view; no proxy should become another system or remain a selectable stray
target after cleanup. Do not force failed cleanup or destroy player data for a
trial. Windows startup, player behavior, both macOS architecture builds and
Mcohall's native testing are separate qualification claims. AAI and previous
fleet/queue/travel/cargo behavior remain outside these edits.
