# Queue address guard — checked observation and live disable

This continuation uses the existing `play-dev-ripper` checkout, clean base
`9799f67fd172592ed36f843eb75cfcbb7d3380a2`, and frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`. The
[PLAY-QUEUE-ADDRESS contract](features/PLAY-QUEUE-ADDRESS.md) records the full
source/native workflow and retained upstream/play/play-dev local ref identities.

## Corrections

The address guard must inspect a matching nonempty queue before it can preserve
that queue by returning a deployment address. The port checked `Count() > 0`
then used the default list iterator, which reads Count again. A contained failure
of the second read became zero and let an entirely uninspected list pass.
One checked Count now governs checked item/class/target-ID reads.

An unavailable fleet dock-index getter previously became slot zero. It is now a
checked observation; a genuine index zero still works. The frozen source's live
policy checks in nested address/lookup hooks are restored, with a final policy
check after managed evidence acquisition. A disable during reentry retains the
game's own address. Outside the active comparison the nested hooks do not read
game-thread configuration.

The address policy, native handlers/results, thread-local scope restoration,
typed binding owners and HookSet installation remain intact. No new detour,
default, shortcut, Settings key, writer, queue mutation or CI registration is added.

## Local evidence

The new controlled-host modes independently fail against unchanged production:
`queue-address-scan`, `queue-address-disable` and `queue-address-index`, each
exit 1 at its intended assertion. With the correction, all three and the broader
`queue-address-native` mode pass. The latter exercises the production event,
comparison, lookup and address hooks and typed accessors with substituted managed
bodies/storage. It checks successful false/zero values, each unavailable index/
recall/state/removal/coordinate observation, failed count/item, empty queue,
foreign ownership, outside-event refusal, policy disable during reads and recovery.

Ten retained explicit modes also pass: queue-native, queue-list-pointer,
queue-field-pointer, queue-alias, queue-response-layout, queue-result-layout,
chat-pointer-primary, chat-focus-unavailable, fleet-native and fleet-mission-primary.
The existing `mods build-tests test-il2cpp test-mod` command passes **423/423
build/run/install steps**, using Zig 0.16, x86_64-windows-msvc, ReleaseFast
dependencies and Debug fixture assertions. Counts are steps, not test counts.
Formatting and `git diff --check` pass.

Evidence directory: `D:/dev/stfc-workspace/artifacts/queue-address-20261102/`.
`baseline-build.log`, `baseline-cases.json`, `final-build.log` and
`final-cases.json` bind the failing/passing modes, executable hashes and changed
source input hashes.

## Signed checkpoint and native candidate

Source checkpoint `0057b4a5347e12aeffda47b69b5b5053dfdcc597` has a verified good
EDDSA signature (the local key's trust is unknown). The clean/stable bound Windows
releasedbg build passes **41/41 steps** and verifies x86_64 proxy exports, static
CRT and embedded source identity. AAI remains pinned to
`4bece2e5f1afa7c7343f009e83398742a0f86114`, clean, with UI Lab selected.
Candidate SHA256:
`F9CDA3067BFAB1EB81D9CF576E16D0A944EEAF531FF2B10881F09407C20F50CC`.

The general, hostile and independent review lanes each found no actionable
defect against base `9799f67f` and source `0057b4a5`, diff hash
`04475385ea8ecbfc98ec8b21fc69a6f6321a4255`. The receipt reports static-persona
fallback; scoped continuity remained available separately. No source correction
was requested. Native/platform limits below remain open.

The evidence directory also holds `source-checkpoint.json`,
`review-receipt.json`, `review-reconciliation.json`, `native-build.log`,
`native-build-receipt.json` and the preserved `candidate/version.dll`/PDB.
The candidate is local, unpushed and undeployed. This later evidence-only record
does not change the source inputs qualified at `0057b4a5`.

## Remaining qualification

The controlled host does not prove native queue retention, real collection
mutation/GC, current-client hook fit or Mac behavior. No game cycle, input,
configuration edit, queue/resource action or published-preview replacement is
part of this assessment. The accepted Windows chat fix stays independently
qualified on source `46bd5c96`; no installed artifact is silently advanced here.

Guff subsequently confirms the normal-game queue workflows described in chat:
"those are confirmed as functioning properly. I've also done waves and had
recoveries so it appears to be working as intended". This accepts the normal-play
queue/recovery trial on the running build; it is not a request for another trial.
The last deployed source in this chat is `46bd5c96`/DLL `f72cbf2e`; no new runtime
identity read or deployment was performed for that report. It does not claim the
undeployed `0057b4a5` failure paths were manually injected in the game.

PLAY-QUEUE-ADDRESS remains Implemented, unverified for complete platform/failure
coverage, with normal gameplay accepted. Guff reports Mcohall is testing the
existing Mac pre-release as work proceeds; no new itemized outcome/source identity
is supplied here. AAI parity is off the active work list at Guff's direction.
Next source slice: SYNC-RECORDS category/config and per-target/session lifecycle,
starting with existing synthetic fixtures; new upstream features stay deferred.
