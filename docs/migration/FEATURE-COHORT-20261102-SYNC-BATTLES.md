# Battle sync cancellation and retry ownership

This continues from clean `89258418eb32f03a83a6226f294b412107197af6` on existing
`play-dev-ripper`, retaining frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`. The
[SYNC-BATTLES contract](features/SYNC-BATTLES.md) maps source acquisition, names,
headers, caches, retry/pending ownership and accepted stronger delivery semantics.
AAI and new upstream work remain deferred.

## Behavior correction

The worker observed battle interest before backoff, but not afterward; its wait
predicate and each request checked only session health. Removing the last battle
destination could therefore still start a journal request and further profile/
alliance requests. Frozen acquisition refused requests when no targets remained.

The correction uses the existing immutable `options.battlelogs` category union,
which reflects actual target settings. It rechecks at job entry, in the existing
wait predicate, after waiting, before every journal/player/alliance POST and
before scheduling another failed attempt. A resources-only destination does not
authorize battle acquisition. The existing frame owner already wakes the waiter.

Canceled work releases only its own pending generation/ID key and leaves ordinary
sync healthy. Re-enabling interest lets a later header admit that uncached ID
again. Successfully acquired journals stay cached. Already-issued requests may
finish, with the existing session fence; optional names keep their established
empty/ID/null-name fallback when further requests are no longer wanted.

One private ProcessBattle function factors the former loop body so the existing
transport fixture can exercise a complete item without starting an infinite
detached worker. This adds no scheduler, public lifecycle, hook, configuration,
default, persistence, target or CI registration. The fixture links the existing
Windows RPC library for the real UUID helper; other platforms keep their existing
product UUID implementation. No game code or account store is loaded by the test.

## Verification

Before the correction, the unchanged loop behavior was factored and the test host
built 191/191 steps. Five isolated modes independently fail at their intended
assertions: battle-request, battle-prepost, battle-retry, battle-journal and
battle-profile. The baseline records bind this extraction, fixture and executable
hash; it is not mislabeled as the untouched parent Git tree.

The corrected Windows core/binding/mod/sync/local-uploader command passes
431/431 build/run/install steps with Zig0.16, x86_64-windows-msvc, ReleaseFast
dependencies and Debug fixtures. The retry barrier was then hardened to assert
that its observation occurred before the deadline; final checks bind that fixture
separately. The same combined command passes 431/431 steps again, and all six
isolated transport modes plus seventeen retained sync/input/queue/fleet modes
pass. These are step counts, not test counts.

The existing actual-CPR fixture now includes production battlelogs.cc request,
retry, pending and resolver code. Test-only session/store wrappers replace the
game-bound host; thread-local observers expose deterministic wait/setup barriers.
No production test callbacks or real worker lifecycle API are added. Generated
local-only session credentials authenticate the owned ephemeral loopback listener.

The retry case drives a real synthetic HTTP503, schedules the actual retry, drops
battle interest during the observed wait, verifies zero subsequent requests and
released pending ownership, then re-enables and fetches/resolves the same ID.
An unrelated resource target remains configured, and direct controls also cover
a genuinely empty target set. Held journal/profile responses verify no subsequent
name request and inspect the exact retained journal/name records through the real
Delivery/Upload wire path. Already-issued responses are allowed to finish.

Evidence: `D:/dev/stfc-workspace/artifacts/sync-battles-20261102/` contains
before-build.log, before-cases.json and individual before logs, followed by final
build/test logs, isolated transport cases and exact final source/executable hashes.
Review/signing checkpoints are recorded after the final verification.

## Qualification and continuation

This closes the identified source/controlled-transport cancellation gap. It does
not qualify actual native callback scheduling, production FollowSettings, real
receivers or the newer local source on Mac. Mcohall's testing remains attached to
the pre-release he has. Guff's ordinary queue/waves/recovery acceptance remains
recorded separately; no repeat normal queue trial is requested.

No game cycle/input, TOML edit, live account/game-server request, deployment or
published-preview replacement is part of this work. The legacy attempted-ID file
remains untouched. SYNC-BATTLES stays Implemented, unverified for full native/
platform acceptance; overall migration parity remains open. Continue the remaining
P21 ordinary/custom/profile path and persistence reconciliation in existing owners.
