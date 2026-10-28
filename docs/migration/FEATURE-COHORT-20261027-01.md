# Fleet and queue workflow cohort — 2026-10-27

Source-preserving reassessment of PLAY-FLEET-ACTIONS, PLAY-FLEET-PREVIEW-GATES,
PLAY-QUEUE-RECOVERY and PLAY-QUEUE-THIN. Queue enable/save and address handling
were read as supporting dependencies, not counted as four newly missing features.

## Exact inputs and method

Frozen play-dev and local play-dev are both
`1baa55fedf1758b19f843cc59707b8901a0b4998`. PLAY remains
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`. The local upstream-dev source observed
in this pass is `e884efa7c6ff66d0776b4cd4d6b76ce193296be5`; these four consumers
exist there too. This is source presence, not equivalent runtime behavior.
The existing target is `play-dev-ripper` at `fa3860a733f675161907ccdcd39d1afa6c114e95`.

The complete relevant frozen action, dispatch, viewer, retry/store, prune,
configuration and Settings paths were compared with the existing target. Pure
queue predicates and the eight-slot/30-second weak request store were retained.
Existing typed bindings, transactional hook sets, native entry qualification,
registered Settings writer and the one shared frame owner remain authoritative.
Implementation was continuous in the existing checkout. An independent read-only
source/correction review helped identify missing safety checks; it performed no
implementation or Runner-governed execution. LexRunner was not used.

Contracts:

- [Fleet actions](features/PLAY-FLEET-ACTIONS.md)
- [Preview and Escape](features/PLAY-FLEET-PREVIEW-GATES.md)
- [Vanished-target recovery](features/PLAY-QUEUE-RECOVERY.md)
- [Native prefix-prune recovery](features/PLAY-QUEUE-THIN.md)

## Repairs and deliberate differences

1. Queue collection acquisition validates managed reference-vector shape and
   element identity. List acquisition rejects native-pointer metadata before
   invoking Count/Item. Each scan uses its one checked count and checked items;
   a later failed Count cannot manufacture an empty secondary queue.
2. Restored live post-native policy checks, destroyed-head identity/count/latch
   confirmation, exact response field layout and the Int32 EngageResult contract.
   Incompatible callbacks cannot share one native entry within either hook set.
3. Accepted live-file changes to either queue policy flag retire correlations.
   Parsing, rejected reloads and unrelated accepted settings do not clear them.
4. Restored frozen repair/help short-circuit order. This is not the historical
   [repair action interlock](features/PLAY-REPAIR-INTERLOCK.md), which predates the
   migration but is already absent from frozen play-dev. Its current need remains
   a separate investigation requested by Guff.
5. A delayed action now owns at most three managed roots, keeps the original
   action and immutable ship/target IDs, and is eligible on exactly the next
   Unity frame. Current source, visibility, native liveness and IDs are checked
   before replay. Rebound/replaced sources, overlapping mining cards, fresh input
   and expired requests cannot redirect it into another action. The existing
   frame callback retires roots even when normal shortcut dispatch is skipped.
   No elapsed-time combat model, pool or generic task framework is introduced.
6. Frozen source's global force flag could turn an unknown-target queue request
   into a primary action; the earlier target's unbound optional could survive
   skipped dispatches. Those defects are explicitly not reproduced. A queue press
   on incomplete target data gets one same-action retry only when identity and
   custody are available. Unknown identity consumes no guessed action.
7. Recovery/thin installers now share the existing implementation on macOS and
   use CheckedEntry before staging. macOS can decline an insufficient native
   entry. Windows entries are unchanged and retain their earlier client checks;
   CheckedEntry does not independently measure Windows extents. Mac CI and
   Mcohall runtime qualification remain pending. No claim of local Mac compilation.

## Verification layers

`tests/action_queue.cc` retains policy/store tests. New fixture fragments compile
the actual production queue and fleet adapters into the existing controlled
IL2CPP runtime, replacing Config/native method bodies/tracker input/save sinks.
They cover actual response scoping, originals, cross-fleet absence, managed
failures, pruning, disable/teardown, destroyed-head revalidation, malformed
arrays/lists/ABI, aliased callbacks, repair/help order, queue toggle persistence
submission, source-bound retry ownership and nested viewer dismissal.

`tests/config_reload.cc` drives production file reload/activation against an
isolated temporary directory. Its reset sink is counted, not a native queue.
Native file writes, native game collections and real input/scene timing are not
qualified by those substitutes. No tests touch player presets or game settings.

Local evidence, commands, executable hashes and final build/runtime disposition
are recorded below at the completed candidate. Earlier failing fixture wiring
logs remain under `D:/dev/stfc-workspace/.ax/fleet-queue-cohort-20261027/`; they
are not passing-candidate evidence. Existing AAI source/pin, analytics and UI
remain unchanged. No push, release, PLAY replacement or whole-product parity.

## Focused native trial

1. In normal gameplay, queue two hostiles; verify queue clear, toggle and normal
   native availability. No forced network failures or gameplay mutations.
2. When naturally available, observe another player removing the first queued
   target while the ship is selecting/travelling. Verify the remaining queue
   resumes without duplicate travel/attack; this race cannot be proven on demand
   by a normal startup or uncomplicated two-target queue.
3. Press the ordinary scan/engage/mining/travel shortcuts, switch selected fleets
   promptly, and dismiss a target while its data is loading. No later action
   should land on the new fleet or unrelated card.
4. With nested viewers, Escape dismisses the first viewer type, then the next;
   focused text/capture and golden-reward behavior retain their separate paths.
5. Repair/help can be checked only on an operator-chosen ship in an appropriate
   state. Do not spend speedups/resources merely to qualify the historical bug.
6. Check an ordinary AAI open/close and Settings navigation after these changes.
   Existing user confirmation remains attached to its earlier AAI candidate.

Mac trial repeats supported workflows after both architecture builds. Native
failure/race tests and missing APIs remain independent of successful launch.

## Completed candidate and qualification

Implementation checkpoint:
`94f4b3e07417430b0bf3f4683e692ec15239e366`
(Preserve fleet and queue workflows across typed native boundaries).
All source/tests were clean at this checkpoint for verification and deployment;
the later qualification checkpoint changes documentation only.

### Offline and native build

- Windows `test-mod test-il2cpp test-aai-host build-tests`, with the immutable
  AAI source enabled, completed **335/335 build/run/install steps**. This is a
  build-step count, not 335 independent test cases. Fourteen explicit fixture
  reruns also passed, with executable hashes and modes recorded separately.
- Production native DLL construction completed **41/41 steps**, with the proxy
  exports and static CRT retained. Final cycle construction preceded stopping
  the game, so build time did not become game downtime.
- Controlled adapters and local filesystem fixtures cover the behaviors listed
  above. They do not prove real native collection timing or a network recovery
  race. Independent source/correction review finished without an unresolved
  substantive finding; this was direct collaboration, not Runner execution.
- AAI's source remains `4bece2e5f1afa7c7343f009e83398742a0f86114`.
  The current host regression modes passed. Earlier 42-fixture/96-comparison
  analytical qualification is historical evidence at that unchanged pin, not
  a claim that all replay comparisons were rerun in this cohort.

Exact command output and executable identities:
`D:/dev/stfc-workspace/artifacts/fleet-queue-cohort-20261027/offline-qualification.json`
(SHA256 `49f5bf94c176594c38ae41e1d137d753e1ff49b9ed0e9b33ea9a5589f236e109`).
The final test log is
`D:/dev/stfc-workspace/.ax/fleet-queue-cohort-20261027/tests-final.log`.

### Windows native trial

The approved Zig cycle targeted only the main installation
`C:/Games/Star Trek Fleet Command/default/game`, ordinary profile, client 273.
The old PID 39960 was replaced by PID 3352, creation FILETIME
`134376187428315886`. Installed source is the implementation checkpoint above;
the embedded build identity is `ax-zig-20261027T234953-ddd0a2f0`.

Installed DLL SHA256:
`ed89a38e1bdd425dc752bbcc53e0e795829897f2567a1cb05729381463a6b5c5`.

Fresh logs confirmed all **71 feature groups**, recovery/thin/address hooks,
both AAI native hooks and **FrameTick delivery ready**. This is group reporting,
not a count of individual detours. Guff's response to normal queued
attacks/clear/toggle, scan/engage shortcuts, Escape through target cards and
obvious blank UI/unexpected actions was **“no problems”**. No forced recovery
race, resource-consuming repair experiment or automated gameplay was performed.
The five tracked TOML/state files were byte-identical before and after the cycle.
No game configuration or data-store migration was made.

One post-startup `UnityEngine.GameObject.GetComponent` NullReferenceException
was logged and contained. Its caller has not been attributed to this cohort.
Frequency is unknown because the binding suppresses further exceptions from
that method. The game remained responsive and the user reported no problems;
neither observation establishes an error-free run or resolves this log finding.

Runtime receipt:
`D:/dev/stfc-workspace/artifacts/fleet-queue-cohort-20261027/runtime-qualification.json`
(SHA256 `12a6d0dd7332661578a672111bae726ef10ce3dc595256ae61fe83556ccc2dee`).
Raw cycle receipt:
`D:/dev/stfc-workspace/.ax/fleet-queue-cohort-20261027/cycle.json`.

### Qualification and tooling limits

Natural vanished-target/network races, extended real input/scene transitions,
macOS compilation and Mcohall's native trial remain open. The historical repair
interlock is a separate discovery follow-up, not an implemented protection here.
AAI's prior player acceptance remains attached to its earlier source candidate;
this cohort confirms unchanged host/hook installation and automated regressions.

The cycle's generic boot parser returned before initialization completed and
counted only two groups. The full 71 groups and frame readiness were checked
independently afterward. Its raw nested `deploy.commit` describes canonical
checkout `a815944d`, while the explicitly selected Zig worktree, prepared build,
embedded marker and installed hash bind the actual source to `94f4b3e0`.
These raw receipts are retained; no lifecycle-tooling changes were made to hide
the attribution mismatch. Native build/deploy used the existing approved Zig
adapter because the published AXF build schema does not expose that mode.

This is implementation plus automated qualification and basic Windows player
acceptance. It is not complete recovery-race qualification, whole-product parity,
a release, or authorization to push/merge. PLAY, play-dev and the frozen source
are unchanged. The running candidate is left in place; no additional cycle is
needed to save these documentation updates.
