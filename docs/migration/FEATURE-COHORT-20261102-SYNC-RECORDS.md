# Sync records, checked native intake and target retirement

This continues from clean signed `5c71cf56e56aaabdabf0f48fdb9287dd4c3d30ea`
on existing `play-dev-ripper`, preserving frozen
`1baa55fedf1758b19f843cc59707b8901a0b4998`. The
[SYNC-RECORDS contract](features/SYNC-RECORDS.md) maps the complete source producer,
wire/config and delivery workflows, including accepted stronger target semantics.
AAI remains outside the active work at Guff's direction. New upstream features wait.

## Corrections and retained behavior

The Haven building stream lost the source's full-sync marker. Its stream spec
now requests Always, retaining the existing full-marked canonical snapshot
policy. Frozen source sent changed subsets with that same marker. A two-building
fixture makes this batching difference explicit; this restores the header without
claiming identical batches or qualified remote receiver behavior.

Native sync intake used default-returning getters/iterators. A contained failure
could silently drop required input, turn unknown type into UserProfiles=0, or
turn unavailable ByteString storage into a valid empty full update. Checked
bindings now qualify collection acquisition, one count and each item; type is
observed before payload selection. Byte storage must be a metadata-qualified U1
vector with no bounds, rooted through the immediate owned copy. Required loss
uses the existing generation seal. Checked RTC and local-identity reads prevent
failed observations from becoming ordinary skips or a fabricated profile level.

Successful null/no-content getters retain their existing filters. Actual empty
byte[]/collections remain valid. Unsupported types are filtered before reading
their payload, and optional name failures remain best effort. Native originals
still execute exactly once outside mod-owned exception boundaries. No new hook,
worker, default, config key, writer, test target or CI registration was introduced.

## Verification and evidence

Baseline existing sync/uploader tests pass 37/37 steps. The new Haven assertion
then fails against unchanged production at the missing marker. Five local modes
independently fail against unchanged intake: sync-intake-count, -group, -type,
-bytes and -rtc. A fixture-only name collision was corrected before capturing
those failures; its compile log is retained as superseded setup evidence.

Corrected Windows `mods build-tests test-il2cpp test-mod test-sync test-sync-upload`
passes **431/431 build/run/install steps**, Zig0.16, x86_64-windows-msvc,
ReleaseFast dependencies and Debug fixtures. Ten explicit sync-intake modes pass:
the five above plus -list-pointer, -byte-pointer, -byte-shape, -identity and -native.
They execute real sync.cc intake/Submit/queue/health with controlled managed bodies
and inert external processors; no installer, worker or HTTP starts in that host.
The positive suite checks owned copy before native mutation, real empty arrays,
unsupported/optional filters, platform offers, identity recovery and original
exception propagation. Five retained queue/chat/fleet modes pass separately.

The existing production CPR uploader fixture now holds responses across target
Configure and retirement/re-add. The replacement configuration must receive the
unchanged record as a fresh baseline; the retired worker releases its destination
after its already-issued request, while the replacement advances independently.
The owned Python listener checks captured bodies and full-sync headers. This
exercises actual Delivery/UploadTarget and simulates registry ownership; native
FollowSettings and real remote targets are separate. All data/keys are synthetic
and the listener binds only its owned ephemeral loopback port.

Evidence: `D:/dev/stfc-workspace/artifacts/sync-records-20261102/`, including
baseline-tests.log, haven-before.log, intake-before-build*.log,
intake-before-cases.json, candidate-tests.log and candidate-cases.json.
Exact final input hashes, checkpoint and review receipts accompany those logs.

## Review correction — bound reference metadata

Initial signed checkpoint `5b0ceaf2` passed the above checks. The hostile review
then found that the shared signature matcher treated a bound System.Object facade
as managed-only, but the pinned runtime makes Object assignable from unmanaged
pointer classes too (`libil2cpp/vm/Class.cpp:663–665,1858–1872`). IntPtr is a value
type and did not expose that path. Native-pointer collection metadata could
therefore execute a getter and be rooted as managed storage.

Two new modes independently fail against that initial source:
`sync-intake-list-unmanaged` and `sync-intake-platform-unmanaged`. The controlled
runtime now models Object's actual universal assignability. The shared matcher
requires managed-reference metadata kinds for bound pointers before class
assignability; intentional untyped-pointer bindings keep their existing behavior.
The new modes check zero getter invocations, one native original, generation
seal, and retained untyped native-pointer calls. This corrects the existing
documented library contract for all bound references rather than adding a local
parallel decoder. Final checks and delta review are bound separately to the
corrected source; the earlier logs are not silently relabeled.

The review's separate suggestion to register the explicit intake modes in routine
CI remains deferred under this migration's no-new-registration scope. Current
explicit executable-bound evidence is retained.

After the matcher correction, the same combined Windows core/binding/mod/sync/
local-uploader command passes 431/431 steps again. All twelve sync-intake modes
and five retained queue/chat/fleet modes pass. `pointer-before-cases.json` binds
the two additional failures; `final-tests.log`, `final-cases.json` and
`final-local-verification.json` bind the corrected source inputs and executables.

## Acceptance and continuation

Guff has confirmed normal queue play, waves and recovery on the running build;
see the updated [queue record](FEATURE-COHORT-20261102-QUEUE-ADDRESS.md).
Mcohall's testing of the existing Mac pre-release is ongoing. No new source-bound
Mac result is inferred for these later local changes.

No game cycle/input, TOML edit, live sync, user/account payload, deployment or
published-preview replacement occurred. SYNC-RECORDS remains Implemented,
unverified for full native/platform coverage. Defaults, aliases, persistence,
per-target ACK/retry and accepted restart resend decisions are preserved.

The source audit also identified a separate SYNC-BATTLES correction: after the
last target is removed during journal retry backoff, current acquisition can
still issue another journal/name request. Frozen acquisition checked target
presence before each request. Recheck current battle interest after the wait and
before each game-server POST, then prove suppression/re-enable through a synthetic
barrier. That producer gap is not closed by the uploader retirement fixture and
is the next bounded source slice. Legacy attempted-ID suppression stays excluded.
