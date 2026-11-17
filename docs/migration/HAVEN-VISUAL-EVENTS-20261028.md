# Haven visual refresh: native events

## Report and cause

Guff's Windows client273 trial of `1c3dba98` found temporary road-depth loss
when visiting another Haven and returning home. The inherited policy applied
once per second and rediscovered the scoped visual holder every five seconds.
Native scene reset or material replacement could therefore leave an old cohort
in use, or a new road material untreated, until the next timed discovery.

This correction changes discovery scheduling, not road classification, camera
math, configuration, restoration ownership, or the requested depth bias. The
existing shared road/water owner remains the only implementation.

## Native evidence and integration

Exact Windows client273 GameAssembly SHA256:
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
Metadata and native call sites were inspected together; method names alone were
not treated as proof of event ordering or safe detour size.

| Native method | RVA | x64 entry extent | Role |
| --- | --- | --- | --- |
| PlanetaryStarbaseManager.ResetInitialVisualLoadState | D4CCE0 | 126 bytes | Initialization, visitor initialization and visitor restoration invalidate membership |
| PlanetaryStarbaseManager.ClearMapModuleVisualState | D4D560 | 589 bytes | Owner/module rebuild invalidates membership |
| PlanetaryStarbaseManager.get_AreInitialVisualsReady | D47AB0 | 669 bytes | Checked readiness observation while an update is pending; not detoured |
| AssetLODStreaming.RefreshMeshAndMaterial | C46F70 | 816-byte entry segment | Notify after native mesh/material replacement completes |

These three new detours join the existing Haven camera HookSet transaction.
The asset streamer is in the global namespace. Its generic activity does not
authorize modifying foreign scenes: after original execution, a rooted payload's
actual transform ancestry must lead to the current Haven holder. Unavailable
custody triggers conservative invalidation, never a guessed foreign-scene write.
Outside active Haven intervention, streaming calls original once without Unity
lookups or payload roots.

OnLodModulesLoaded does not cover later quality changes. Lighting registration
occurs before material assignments and includes a tiny forwarding handler, so
that callback was rejected as the material-completion boundary.

## Scheduling and ownership

- Native reset withdraws membership authority immediately. New discovery waits
  for checked native initial readiness and a later Unity frame.
- Completed holder-descendant material updates invalidate the ready generation.
  Bursts coalesce; scan/write reentry postpones work to a later frame and cannot
  acknowledge a stale generation.
- Successful discovery and verified application remain idle until another event
  or desired-state change. There is no recurring successful full-scene scan.
- Only failed discovery retains a five-second recovery backoff. Failed mutation
  or restoration retains a one-second retry. New reset/material generations
  bypass old deadlines when ownership can be qualified.
- Existing weak caches and independent restoration records remain. Native calls
  use temporary strong custody. Invalidation does not clear vectors while a
  reentrant consumer may still hold spans. Publication checks generation again.
- Failed/not-ready replacement discovery still restores old owned road/water
  interventions. It does not strand them just because the camera remains active.

If failed discovery has no retained holder custody, streaming ancestry cannot
be qualified; those callbacks cannot bypass the failure backoff. A subsequent
native reset/clear can. This is explicit degraded recovery, not normal polling.
Readback proves sequential desired-state observations, not an atomic scene snapshot.

## Verification

Production sources: [haven.cc](../../src/mod/features/haven.cc),
[haven.h](../../src/mod/game/haven.h), and
[haven_visuals_policy.h](../../src/mod/haven_visuals_policy.h).
Tests: [haven_visuals_test.cc](../../tests/haven_visuals_test.cc) and
[haven_visual_events_cases.h](../../tests/haven_visual_events_cases.h), dispatched
through the existing controlled IL2CPP executable.

`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/events-tests-4.log`
passed **417/417 Windows build/run/install steps**, with Debug assertions and
ReleaseFast dependencies. This includes actual production consumer tests, mod
regressions and the unchanged AAI host. It is a build-step count, not a claim of
417 independent behavioral tests.

Fixtures cover home/visitor/home at the same clock value, same-holder material
replacement, next-frame deferral, readiness failure/recovery, original-once
delivery, foreign payloads, payload/parent custody failure, stale reset during
native original, failed replacement discovery with old-state restoration, scan
and write reentry, and repeated successful idle calls without native scans,
material/property observations, allocations of shader strings or writes.

Earlier `events-tests-2.log` exposed same-frame retry after reentrant invalidation;
the production scheduling rule was corrected. Final tests include subsequent
custody and restoration corrections; earlier passing checks do not qualify those
later edits. Input hashes, executable identity, native build and runtime trial
results belong to this correction's own artifact receipts.

Actual road rendering, the native delivery order across real Haven transitions,
and macOS hook fit/build/runtime remain separate qualifications. The current
player report establishes a defect in the prior build, not acceptance of this fix.

## Runtime trial

After the candidate's own native build and authorized cycle: enter home Haven,
visit another Haven, return home, and repeat quickly at expanded zoom. Leave the
scene settled for fifteen seconds. Check road depth immediately after native
loading and verify publication logs follow native changes without recurring idle
discovery. Also check ordinary zoom and water behavior. Preserve pending/failed
restoration evidence if a transition still flickers.

Execution is direct tooling and independent collaboration, not Runner-governed
execution. No new persistence, timer framework, feature descriptor or settings.

### Windows candidate checkpoint

Signed implementation: `113f13ce277c5acf0f8f69bcc400417bb00be61f`.
Its own native build passed **41/41** steps with source clean and stable. AX
built before deployment; the previous main process had already exited, so no
running game was stopped. It installed the matching artifact and launched the
main ordinary client273 session, PID31468 / creation FILETIME134377131876503804.
All 73 feature groups installed and FrameTick reported ready. Startup log
inspection found no fresh error/critical entries. Session identity is historical
after another restart; this does not establish rendering or player acceptance.

DLL SHA256:
`4c88a57cca7a72f48b74d32f700142141d3dcdbf8318166a1a87a2909f21d784`.
Build invocation: `ax-zig-20261029T020324-f4bfe794`.
AAI remains clean at `4bece2e5f1afa7c7343f009e83398742a0f86114`, UI Lab enabled.
All five tracked TOML/state hashes were unchanged; no agent configuration edits.

The artifact directory above contains `events-source-inputs.json`,
`events-offline-qualification.json`, `events-tests-4.log`,
`events-build-receipt.json`, `events-native-build.log`, `events-cycle.log`,
`events-stores-before.json`, `events-stores-after.json` and
`events-runtime-qualification.json`. They distinguish source, compiled fixture,
native DLL, installation and process evidence. Earlier cohort receipts are
historical and do not qualify this correction. No push or promotion.

### Focused Windows player trial

Guff completed the requested home/visitor/home switching at expanded zoom and
answered **"yup, leaving open"** to whether the roads retained their correct
appearance immediately after each load. This accepts that narrow Windows workflow
on the exact DLL above; it is not full feature/platform parity.

Four successful discovery publications were logged at local21:09:18.881,
21:09:36.782,21:09:47.215 and21:10:08.451. All were native map-module-reset
generations. Their scoped scan times were 1220,1522,2526 and2025 microseconds;
these are discovery callback durations, not full-frame costs or end-to-end loading
latencies. Road counts were7,0,7,7, with one water renderer in each. The visitor's
zero-road cohort alone does not prove missing discovery; no visual defect was
reported. Real streamed-material callback coverage beyond these reset paths
remains unqualified, although its production adapter is exercised offline.

At local21:10:36.831, the final scene had no further publication for over28seconds,
consistent with no recurring one/five-second successful scans. A separate
contained/rate-limited GameObject.GetComponent NullReferenceException occurred at
21:09:35.534. The same diagnostic predates this correction; caller and cause
remain unattributed. It is preserved rather than claiming an error-free trial.
