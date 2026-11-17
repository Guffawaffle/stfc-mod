# Fleet and navigation cohort - 2026-10-27

Implementation, offline suite, Windows native boot and basic selection/screen trial qualified.
Guff authorized continuing after the accepted map/
camera batch. The existing worktree is `play-dev-ripper` at
`557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8`. Frozen play-dev remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`; do not advance that source ref.
The running map/camera binary remains the baseline until this candidate passes
its own checks. No result from the previous batch qualifies new source changes.

Use [migrate-play-feature](../../.agents/skills/migrate-play-feature/SKILL.md):
read the complete source, supporting bindings/callers/tests and current target;
preserve implementation and adapt only incompatible boundaries. The existing
[inventory](play-functional-parity.json) is the discovery index, not a behavioral
specification. Record newly discovered capabilities immediately without silently
expanding implementation scope.

## Ownership and scope

| Capability | Source / target | Assigned work |
| --- | --- | --- |
| PLAY-FLEET-SELECT | frozen hotkeys / fleet_actions.cc | Complete slot-selection and repeat-locate workflow; preserve native selection |
| PLAY-FLEET-ABILITY-RECOVERY | frozen hotkeys / fleet_actions.cc | Expired cooldown button repair, availability and parallel-array custody |
| PLAY-SCREEN-ACTIONS | frozen hotkeys / screens.cc | Existing navigation/arrow workflow and delayed ship/officer/preset buttons |
| PLAY-FLEET-TOW | frozen discovery_tow + hotkeys / fleet_actions.cc + game/fleet.h | Native planning, iterator, scheduler, callback and command-boundary ownership |

Three directly coordinated workers investigate selection/ability, screens and
towing respectively. Only the first edits the fleet implementation initially;
the coordinator integrates later towing corrections after that writer freezes.
The coordinator owns shared game bindings, fixture registration, the inventory,
serial builds and runtime qualification. No competing writers own a file at
once. Workers do not build, cycle, change branches or mutate Git.

Runner's native execution remains held at Guff's request; no governed Attempt,
Runner fanout or helper modification is claimed. Existing scoped AXF continuity
and normal build/lifecycle commands remain available. No new worktree, push,
merge, PLAY replacement or release is part of this cohort.

## Evidence and acceptance

- Pin branch-presence observations to upstream dev e884efa7, PLAY cad98ef79,
  frozen1baa55fe and the actual refactor base/candidate. A similarly named API is
  not proof of equivalent behavior.
- Selection must preserve occupied/empty slots, configurable repeat timing,
  focus/capture/native-hotkey exclusions, preview-location preference and native
  request behavior. A replacement bar/fleet must not inherit an old repeat click.
- Ability repair must retain the 250 ms cadence, source capture exclusion,
  eligible fleet/effect/timer rules and distinction between enabling a button
  and activating an ability. Unknown observations must not authorize mutation.
- Deferred buttons retain the existing navigation table and 300-frame policy;
  cancellation, replacement, hidden/pooled controllers, missing buttons and
  reentrant completion must not advance the wrong request.
- Towing must use the existing seven-argument planner and Unity coroutine
  scheduler, real closed callbacks, immutable destination and fresh qualified
  course. Never advance MoveNext in a loop. Qualify actual adapters in addition
  to the existing pure policy tests. Distinguish computation retirement,
  unconfirmed native stopping and permission to submit a tow request.
- Run relevant Windows assertions-enabled fixtures and legacy Settings/presets/
  queue/travel/cargo/map/AAI-host regressions, then one native build-before-stop
  exact-target cycle. Preserve settings/stores; no resource-consuming game action
  is needed merely for a passing check. Real towing uses an operator-chosen case.
- Record local fixtures, native construction, boot readiness, player results and
  macOS build/runtime gaps separately. Mac builds require CI and later Mcohall
  testing; no local success implies platform qualification.

AAI analytics/UI, camera/map behavior, historical repair interlock, pan momentum,
Settings visual work, toolkit expansion, persistence and Runner helper work
remain independent. Do not turn this batch into infrastructure work.

## Implemented and source-qualified

All four complete contracts are under `features/`. Existing consumers were
present at the target base: this is source-preserving reconciliation, not four
features presumed missing. No new capability was discovered outside existing
inventory IDs. The three workers read full source and supporting paths; their
cross-checks were direct collaboration rather than Runner orchestration.

- Selection keeps native RequestSelect, eight configured slots, repeat timing
  and preview behavior. One weak identity record now distinguishes replacement
  bars, contexts, arrays and fleets; location rechecks the source after hiding
  a target. Unknown slot evidence still permits ordinary native selection but
  cannot authorize repeat-location. No persistent fleet pointers are added.
- Ability recovery retains the 250 ms cadence, exact -1-second TimeSpan
  threshold, shorter parallel-array traversal and source capture exclusion.
  It now requires checked state/effect/interactability/readiness, signed Int64
  array storage and actual System.TimeSpan return identity. Rooted source,
  cooldown and button context are rechecked before enabling Interactable.
  It never presses or activates the ability. Existing APIs remain compatible.
- Screen routes and navigation/arrow order are preserved. Existing 300-poll
  button waits now own their request episode and source, with conditional
  completion so reentrant cancellation/replacement wins. Failed immediate
  attempts and pooled-controller iteration also respect that revision. Actual
  Press success is distinguished from a nonnull button. Manage Ships returns
  to its frozen late position after the setting-shortcut else-if chain; a
  shared chord no longer opens Manage and changes a setting together. Native
  Update is still not consumed by that late route. Config is reacquired after
  awaited native callbacks rather than retaining a reference across reload.
- Towing retains the existing seven-argument planner, Unity scheduler, immutable
  destination, real closed callbacks and fresh-course interpretation. The
  extracted SourceMatches predicate reuses existing identity/eligibility rules
  before Plan, before Start and at final submission. Completion explicitly uses
  the new retirement ticket. Invalid sources are rejected before course reads.
  Computational stopping and publication cancellation remain distinct: a failed
  native Stop cannot revive a retired request, and a terminal fresh calculation
  can submit only after its current source is qualified again.

Towing's cached bindings now distinguish metadata presence from callable code,
including instance entries and shared invokers. Invocation-only targets use the
existing call resolver rather than the detour-only lookup, which deliberately
rejects shared generic code. Qualified virtual Equals and closed/shared calls
remain supported. No binding-library rewrite, parallel callback engine, native
iterator driving or new feature/frame/hook owner was added.

The exact retained client273 dump, GameAssembly and six disassembly files were
rehashed against `.ax/play-p09-study/native-oracle/receipt.json`. The current
main installation's GameAssembly independently matches
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
That supports the ABI study; it does not establish successful live towing.
Both frozen and target towing choose the intended fleet via manager index;
ordinary selection passes the requested ordinal to native RequestSelect. The
unsorted fixture qualifies Discovery enumeration and native indices, not an
equivalence between every fleet-bar array ordering and native manager ordering.
That relationship belongs in the deliberate native tow trial; no silent remap
to array position was introduced.

## Executed offline qualification

The frozen combined candidate passed **382/382 build/run/install steps**, not
382 independent tests. Command: `zig build test-mod test-il2cpp test-aai-host
build-tests -Daai-host=true -Daai-source=D:/dev/stfc-mod/third_party/stfc-aai
-Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -Dtest-optimize=Debug -j4
--summary all`, Zig0.16.0 in the established native MSVC environment.

Log: `D:/dev/stfc-workspace/.ax/fleet-navigation-cohort-20261027/tests-4.log`,
SHA256 `af6b465105abdedbfd9318814b04786a27e5c1f5053345d89413a1061d5afa56`.
Fixture executable `zig-out/bin/il2cpp-bindings-tests.exe` SHA256:
`e3e6b062a9174aeebcfaa5ea531b57e5ac194e125d3aa21d15fc257d66ca9a29`.
AAI remains pinned at `4bece2e5f1afa7c7343f009e83398742a0f86114`.

Twenty-four fresh-process modes add actual selection, ability, screen, complete
shortcut-order and towing consumer coverage. Tow cases run real acquisition,
callback construction/readback, native-call wrappers, begin/observe/frame,
waiting/completion/cancel/reentry and refusal; positive virtual/shared routes
and malformed pointer/byref/arity/type/closed-generic modes remain distinct.
Existing Settings persistence, presets, queue, map/camera, travel/cargo and AAI
host checks passed in the same suite. Policy and source comparisons do not
stand in for final consumer fixtures.

Managed metadata, method bodies, GC handle storage, scenes and scheduler are
controlled substitutes. Test-host per-class allocation/static-field/invocation
seams let the real callback code run without fabricating delegate fields in
production. The fake runtime was corrected to resolve copied array descriptors
like the SDK, and focus cases now perform the real Key::Poll lifecycle step.
Native UI visibility, actual collection, pooling, server acceptance, resource
effects, full-frame costs and macOS are not established by those cases.

Earlier logs remain failed evidence: initial template syntax and a missing
adjacent chat sink prevented compilation/linking; run3 then exposed the copied
array/focus host omissions and the production detour-lookup/shared-call mismatch.
They were corrected without relaxing assertions. Cross-review additionally
closed the failed-immediate-Press replacement gap and native callable-entry
admission before the passing fourth run. Later source edits need their own
appropriate verification; documentation-only qualification does not change the
compiled candidate.

## Windows deployment qualification

Implementation: `b36d28377318953e532597fb1191a47f114ee92c`, tree
`d4d56234963b931b89390a4091b137e0279fa273`. The normal AX Zig cycle built the
clean/stable existing worktree before stopping main ordinary PID53260. Native
construction passed **41/41 steps**; binary verification confirmed x86_64,
16 proxy exports, static CRT and the expected embedded marker. AAI's pinned
source and UI Lab remain enabled. No second native build was invoked.

Installed/copy-verified DLL SHA256:
`ff4ebe50268e98bf230c10e693fe1e6dd9ba12dfeaac6ae1046f7371bf391612`.
Build: `ax-zig-20261028T041356-29052797`, mode `releasedbg`. Main ordinary
client273 reopened as PID48004, creation FILETIME `134376347152882556`.
The explicit source/build/embedded marker agree on b36d2837; canonical checkout
metadata in the nested deploy receipt is not this Zig worktree's source.
Disk/module-path observations do not independently hash in-memory instructions
or establish authenticated-account identity.

The fresh complete log confirms all **71 groups**, followed by FrameTick ready;
the initial generic boot parser's earlier partial count was not used as complete
proof. The initial startup snapshot had zero error lines. Five tracked TOML/
state files were byte-identical immediately after the cycle. No agent config/
store edits or migrations occurred; later operator changes remain independent.
The post-trial snapshot includes one contained/rate-limited GameObject.GetComponent
NullReference at local23:22:35, the same reported method/symptom seen before this
cohort. Caller and cause remain unknown; this is not proof of identical causation
or an error-free runtime. Keep the existing diagnostic follow-up separate.

Player checks were requested for number selection/repeat locate, Ship Swap,
Officer Swap, Presets and Back/Escape. Guff answered **"looks good to me"**.
That accepts the ordinary combined Windows workflow. Naturally expired ability
recovery and a deliberate operator-chosen live tow were not specifically
confirmed; no resource expenditure was requested merely to pass a trial.
Boot/basic shortcut success is not rare recovery or server delivery proof.
macOS builds and Mcohall's native testing remain open.

Qualification receipt:
`D:/dev/stfc-workspace/artifacts/fleet-navigation-cohort-20261027/qualification.json`,
SHA256 `bdec1bc7dd4e1c0c2a4875f35050c080d20f06ad6ab9db9602058011050decc0`.
The exact DLL is retained beside it as `b36d2837-version.dll`. Ten source/player/
executable/build/cycle/runtime/store evidence files and their hashes are recorded
there; logs and receipts live under
`D:/dev/stfc-workspace/.ax/fleet-navigation-cohort-20261027/`.

## Recovery

Scoped Downstream offline frame: `db6c87f8-c791-48c9-b421-43ad7caa3a74`, explicit
reference `play-dev-ripper@b36d2837`. AXF uses the existing canonical Downstream
memory scope; its unrelated branch metadata is not the source selector for this
worktree. This was not a Runner-created frame or governed execution.

The inventory/catalog preserve historical assessments and distinguish current
basic selection/screen acceptance from still-unqualified native cases. The next
step is choosing another source-grounded cohort while retaining focused native
checks when their situations occur naturally. Keep native tow/index ordering,
rare ability recovery, macOS, historical repair protection, pan momentum,
Settings visuals and the GetComponent diagnostic explicit rather than folding
them into an overall parity claim. No push, merge, PLAY replacement, release,
new checkout or Runner native execution occurred.
