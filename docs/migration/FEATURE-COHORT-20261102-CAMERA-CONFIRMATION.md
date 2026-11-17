# Camera pan/backdrop and confirmation continuation

Existing `play-dev-ripper`, starting head
`f9e66f2428944c09f24ae338252de1eeb5dc1479`; frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Contracts:
[pan](features/PLAY-CAMERA-PAN.md), [backdrop](features/PLAY-CAMERA-BACKDROP.md),
[FC confirmation](features/SET-FC-CONFIRM.md), and
[queue switch](features/PLAY-QUEUE-SWITCH.md). AAI, new upstream features and parked
orbit/science diagnostics remain deferred.

## Corrections and retained behavior

Pan now qualifies the static block and instance fields before mod movement,
including Unity's Vector2 identity. Missing mouse/touch calls remain unavailable,
rather than idle. Shortcut capture blocks the native keyboard route even without
a held arrow. Small copied settings replace the Config reference retained across
managed gates; policy replacement invalidates that frame. Pointer/tracking/block
observations are checked again after the gates. Finite movement is required, and
post-call policy/tracking changes do not publish stale momentum. The normal native
route still calls original once; the mod still uses one movement path per frame.

Backdrop scale observation uses the existing qualified Vector3 reader, including
declared and returned-box identity; its setter reports success before caching an
applied value. Missing/throwing reads and nonfinite scales do not write a zero or
invalid transform. Weak transform identities replace the clear-all cache limit,
so more than 256 live objects retain their own observations and recycled addresses
do not inherit them. Expired entries retire on subsequent scaling work. Newly
authored and cached loader workflows remain distinct. No live restoration to
authored scale, new graphics setting or movement-speed feature is introduced.

The next two small owners require evidence closure rather than product changes:
FC confirmation retains readiness, noncreating/noninserting reads, inversion,
generation rejection and native manager writes; queue enable retains live
short-circuiting, persisted toggles and recovery-identity reset. Existing accepted
queue/waves/recovery play remains attached to its tested build; no repeat trial
is requested.

The [FT ship-name provenance assessment](features/PLAY-FT-SHIP-NAME.md) corrects
a catalog attribution: this feature already came from upstream and is absent from
the frozen PLAY source. Its existing typed implementation is retained. It is not
new work or a frozen restoration in this slice.

## Falsifiable local evidence

The unchanged production source builds 191/191 test-build steps. An initial
fixture run exposed four control failures caused by fixture input resolution and
inherited text focus. Those test boundaries were corrected without changing the
production source or expectations. The verified baseline then has twenty intended
failures (twelve pan, eight backdrop) and thirty controls passing, including all
fifteen FC cases. Retain the initial and verified results separately.

Corrected Windows `mods build-tests test-il2cpp test-mod` passes 423/423
build/run/install steps with Zig0.16, ReleaseFast core/dependencies and Debug
fixtures. All fifty explicit new modes pass:24 pan,11 backdrop,15 FC. Ten retained
camera/queue/fleet/window/scaling modes also pass. Existing config/model/file
checks remain in the combined suite. No new test target or CI registration is
added. Changed code is formatted and `git diff --check` passes.

These fixtures include production callbacks/backend and typed bindings, with
controlled metadata, native bodies, boxed values, GC APIs, input and shortcut
boundaries. They qualify dispatch/failure/ownership decisions, not native device
input, actual Unity collection/rendering, visible camera displacement, Settings
presentation, real account preference persistence or current-client ABI/hook fit.
Matched pan momentum/glide feel remains open; source double movement is not
silently restored. Existing zoom, station, pointer and original-call improvements
are retained.

Evidence is under `D:/dev/stfc-workspace/artifacts/camera-confirmation-20261102/`:
before-build/before-cases retain the first fixture setup; before-verified-build
and before-verified-cases bind the corrected baseline; final-tests, final-cases,
retained-cases and local-verification bind final inputs and the executable.
Both Mac core requests stop before compilation because the configured SDK is
absent; per-architecture logs and mac-builds.json retain this limitation. The
implementation is shared across platforms, with no Windows-only fallback added.
Mcohall's earlier Mac pre-release testing does not qualify these later local bytes.

Signed source checkpoint `4e7c052a255aa3a0a559471c99c346bf6b181fc6` verifies with
a good signature (local signer trust unknown). General, hostile and independent
review lanes found no actionable findings against base `f9e66f24`, diff hash
`3493935673bce17523442095ac1d785e19e5c2d5`. All24 recorded source/test inputs and
the executable hash match. The review receipt uses static-persona fallback because
its CLI memory lookup was unavailable; scoped AXF continuity works separately.

The next [Mac C++ launch slice](features/PLAT-MAC-LAUNCH.md) has also been
source-compared without a proven regression or product change. Its eight source
hashes are separate from the Windows executable manifest. That partial contract
does not close Swift update/rollback, packaging or native Mac acceptance.

No game input/cycle, live TOML/config edit, account/cloud access, deployment,
pre-release replacement or remote write occurred. Classifications and overall
migration parity remain open. Next bounded source owner: the existing Swift
install/update/cancel/repair and rollback workflow, followed by bundle/signing and
actual Mac build/runtime evidence.
