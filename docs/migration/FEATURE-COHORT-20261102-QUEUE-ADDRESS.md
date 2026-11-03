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
source input hashes. A clean signed source checkpoint and native build receipt
are recorded separately after implementation.

## Remaining qualification

The controlled host does not prove native queue retention, real collection
mutation/GC, current-client hook fit or Mac behavior. No game cycle, input,
configuration edit, queue/resource action or published-preview replacement is
part of this assessment. The accepted Windows chat fix stays independently
qualified on source `46bd5c96`; no installed artifact is silently advanced here.

PLAY-QUEUE-ADDRESS remains Implemented, unverified. A later identified native
trial should check ordinary queued actions/toggle, moving or recalling another
ship while the affected deployed fleet keeps its same-system targets, genuine
recall/removal and a disabled guard. Do not force gameplay actions merely to
manufacture the race. Mac qualification and full optional-host parity remain open.
Next source slice: SYNC-RECORDS category/config and per-target/session lifecycle,
starting with existing synthetic fixtures; new upstream features stay deferred.
