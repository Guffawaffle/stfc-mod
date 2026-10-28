# PLAY-FLEET-TOW — Discovery Shift+ship towing

Contract version 1, 2026-10-27. Source and controlled-adapter qualification,
Windows native construction and boot passed at `b36d2837`; a deliberate live tow
remains pending. See [cohort 04](../FEATURE-COHORT-20261027-04.md) for the exact
382-step suite, binary and runtime evidence. Earlier fleet/queue trials do not
establish player-verified towing.

## Inputs and scope

- Frozen behavioral source: `1baa55fedf1758b19f843cc59707b8901a0b4998`
  (`frozen/play-dev-20261026`, also the inspected local `play-dev`).
- Target: `play-dev-ripper`, assessed starting base
  `557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8` in the existing
  `worktrees/ripper-foundation/Downstream` checkout. Later production changes and
  test results belong to the cohort's final candidate-bound receipt.
- Source scan includes the entire frozen `mods/src/patches/discovery_tow.cc`
  and header, SelectShip and frame/input call sites in `parts/hotkeys.cc`,
  `prime/DeploymentManager.h`, `prime/CallbackContainer.h`, the corresponding
  typed target, policy fixtures, delegate implementation and controlled host.
- Product scope: existing Discovery Shift+ship action, owned request lifecycle,
  exact native planner/callback/scheduler boundary and safe cancellation.
  No automatic resource-spending trial, new settings, new task framework or
  historical repair-interlock work.

| Inspected branch | Exact revision | Presence and meaningful difference |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Consumer in `mods/src/patches/parts/hotkeys.cc`: Shift selection finds Discovery and submits through DeploymentManager; no separated frozen request owner. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer in the same hotkeys path, including `SetTowRequest`; do not describe that entrypoint as the frozen asynchronous implementation. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Reachable `discovery_tow::Begin/Update/Cancel` module and frame/input cancellation; seven-argument planner wrapper. |
| refactor | `557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8` | Existing `BeginDiscoveryTow/ObserveDiscoveryTow/DiscoveryTowFrame/CancelDiscoveryTow` and typed `discovery_tow_binding`. Implementation predates this cohort; needs qualification rather than replacement. |

Presence is source evidence, not proof that a particular client exposes every
binding or that a server accepts a tow request.

## Source workflow and ownership

The source's eight configured ship shortcuts are handled only outside focused
text. A ship selection first cancels an earlier tow. Shift requests towing;
ordinary selection retains its existing selection behavior. Frozen discovery
enumeration scans native fleet slots 0–9 and chooses the first Discovery hull
`1307832955`. The source module owns the asynchronous request, checks native
eligibility, plans to Discovery's address, waits for completion and submits.

Frame processing advances the source's request owner before the disabled/native
hotkey returns. Settings/capture ownership, explicit hotkey disable, debug-console
handling and replacement actions retire pending publication. Text focus alone is
an initial activation guard, not a separately established cancellation condition
for an already-owned request. There is no separate tow preference or persistence
store to port; configured ship shortcuts and common hotkey mode remain the owners.

The target intentionally retains the selected fleet, Discovery, DeploymentManager,
cloned destination, baseline planned course, real callback container, iterator and
returned Coroutine using strong managed handles. Unity owns coroutine advancement
and any nested yield instructions. The mod only observes the qualified iterator's
state; it must not run a second `MoveNext` loop. The lifetime is the immutable
request's lifetime, **not** the lifetime or visibility of the original fleet bar.
Current fleet identities, section, address and eligibility still require checks
before commands and publication. Hiding the bar by itself is not evidence that
the request should be cancelled.

`CancelDiscoveryTow` detaches the old request and advances its generation before
calling native Stop. This prevents Stop reentry from cancelling a replacement.
Cancellation of publication is certain once retired; successful cancellation of
the game's planning computation is a separate checked outcome. A failed Stop is
reported and must never make an old request eligible for later submission.

## Behavioral obligations

| ID | Source behavior / requirement | Target mapping and qualification |
| --- | --- | --- |
| T1 | Shift+configured ship action selects the requested native slot, cancels old towing, respects initial input ownership; ordinary ship actions still work. | Existing `SelectShipShortcuts` routes to `BeginDiscoveryTow`; do not reconstruct the dispatcher. Complete keyboard precedence remains an exact-client check. |
| T2 | Identify Discovery by hull identity, not display name; towed fleet and Discovery must be different. | Full typed fleet-array scan, individually rooted elements and exact signed identities. Lowest nonnegative native Discovery index preserves source ordering while removing the old ten-slot bound. Controlled array is deliberately unsorted and includes index 12. |
| T3 | Missing owner, context, selected fleet, address or eligibility declines safely. | Checked observations retain missing/false/null distinctions. No command-authorizing default zeros; callbacks and native metadata must qualify before planning. |
| T4 | Plan for the selected fleet at Discovery's destination using the current seven-argument client signature. | Actual `discovery_tow_binding::Plan`: selected fleet, cloned address, zero Vector3 and four null optional native targets. Typed metadata validates named types and Vector3 layout. Controlled body checks every argument. |
| T5 | Complete planning before towing; do not borrow a stale existing planned course. | Root baseline course, own iterator, let native Unity scheduling advance it, accept only terminal state and a distinct fresh matching course. Validity, selected FleetID, IsInUse=false and destination are independently observed. |
| T6 | Keep request references alive across the native scheduler and callback boundary. | Strong roots and exact iterator fields; actual delegates and native constructor, rather than source layout fabrication. Controlled constructor checks root custody and native readback. |
| T7 | Abort on timeout, unavailable/replaced owner or fleets, changed section/destination or changed eligibility/input mode. | Existing copied request and optional evidence policy; current base checks these during polling and before publication. Cohort should also qualify source reentry immediately before Plan/Start and the final command. |
| T8 | One action produces at most one correctly owned native tow command. | Retire request before native Stop/submission; generation and replacement checks after reentry; fresh course readback immediately before command. Native fixture counts exact signed IDs and one final submission. |
| T9 | Cancel/replacement never publishes stale work or retires the replacement. | Detach-before-Stop, local shared request ownership, start-in-progress flag and frame reentry guard. Actual Plan/Start/Stop reentry cases, not policy-only assertions. |
| T10 | A Start that mutates native scheduler state and then fails cannot become an apparently running request. | Checked Start result; iterator Stop overload when no Coroutine handle was returned. Native fixture models both throw-after-mutation and successful-null handle. |
| T11 | Unknown binding/runtime observations disable only this action without unsafe ABI guesses. | Fail-closed exact cached binding boundary and explicit refusal modes. Native game UI and unrelated shortcuts remain independent. |
| T12 | Cleanup does not call IL2CPP during late C++ static destruction. | Process-lifetime state object; normal frame/cancel/finish paths release roots while runtime is live. No new worker, store or shutdown owner. |

## Intentional corrections and limits

These differences should not be called accidental migration regressions:

- The old bespoke `CallbackContainer::Create` wrote the container's own address
  into both delegate fields. That is an existing source defect, not a safe
  implementation to preserve. The target constructs the real closed
  `CallbackContainer<Int32>` and two real closed delegates bound to the game's
  actual recall callback singleton, then verifies native getters.
- Discovery's destination is cloned. Retaining the same mutable address and
  later comparing it with itself would not establish destination stability.
- Full fleet enumeration replaces the source's hard ten-slot search, preserving
  source native-index selection order. No name localization is required.
- Exact missing-data rejection, stronger ownership and guarded reentry are
  existing target safety improvements. They do not establish server acceptance.
- The retained iterator's implementation-specific states are **not** a general
  IEnumerator contract. On metadata drift, retain safe refusal rather than
  guessing a new nested class, field or yield-state convention.
- A new requirement that the original bar remain open through the operation
  would change the source's intentionally owned request model; none is added.

The bounded issue identified at the assessed base is command-boundary freshness:
callbacks/baseline getters can change input or section after their first readings;
Plan can reenter before Start; late course getters can mutate a previously read
source observation. Generation protects cancellation/replacement but does not
alone detect every in-place context change. The parent integration lane owns any
production correction. The fixture includes focused reentry cases for these
boundaries. Do not claim arbitrary managed mutable getters provide an atomic
snapshot; record exactly what each revalidation and fixture establishes.

The correction also distinguishes metadata presence from callable native code.
`il2cpp::Method::operator bool` checks only that metadata exists; the shared
resolver intentionally does not promise a native entry. The tow-local cached
qualification now checks the actual planner, scheduler stops/start, tow command,
callback constructor/getters, recall handlers and existing Clone/Equals/CanTow/
CanBeTowed/get_Valid support methods before enabling the action. It refuses null
entries, abstract/static/definition methods and unexpected byref signatures.
Ordinary instance entries require both code routes; fully shared generic calls
retain the existing runtime-invoker route and require its invoker. No shared SDK
semantics or alternate invocation framework is introduced.

The first combined shared-route trial exposed an additional lookup mismatch:
`Class::Method<F>` is explicitly a detour lookup and calls `Hookable`, which
rejects fully shared generic code before invocation qualification. Towing does
not detour these methods. Its cached call targets now use the existing typed
`MethodCall::Resolve` directly, retain the same MethodInfo/API storage and run
through the same guarded `TryInvoke`. The callable checks remain intact. This
correction requires rerunning the shared-route fixture on a freshly built
candidate; the prior failed run is not passing evidence.

Retained NodeAddress exposes virtual `Equals(object)` before its typed overload.
The existing compatible-reference resolver can select that correct override;
virtual dispatch remains supported. Closed generic callback constructors likewise
remain supported. New fresh-process fixtures exercise both accepted routes and
the rejected null-entry/void-byref/shared-invoker cases.

## Retained native evidence

Read-only private oracle:
`D:/dev/stfc-workspace/.ax/play-p09-study/native-oracle/receipt.json`, snapshot
`game-273-a9ab2edbc143-2b586d6f23eb`. This pass rehashed both retained source
objects and all six existing disassemblies against that receipt:

- Dump SHA-256: `3c019dd684ee5e1062e758831c00be719fb53c94b4859c95ba2cc46189dc8621`.
- GameAssembly SHA-256: `a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
- `manager-plan.asm`: `117ce94e1d9636ec11a972468a2b1a67a044d538688bb5cf4478fe7ef5b95784`.
- `manager-tow.asm`: `b7b125245fd6a1414aa4658d04ad41aaf778798ecd1514412402fe2ec15bfdc7`.
- `iterator-player.asm`: `1cdcc5a722dce4a0a411993b981532847fc3d9909f4f45f6b3049434c32d9974`.
- `callback-constructor.asm`: `84becf0c2f193b8a2b617353dc5ad15a88b87ca6d2d8261f9d17c7a170d9e381`.
- `tow-callback.asm`: `8dc207483daf36aef70f80192cbd9b1bc559213df0a64a075a1b8a2cf66ca964`.
- `recall-noop.asm`: `16581bd2a9176082e420914c897db6349ed892e0ae7fc016404f76cb23f14b9d`.

The dump's DeploymentManager PlanCourse at line 4779 and service declaration
at 359250 expose all seven parameters; SetTowRequest at 4848/359211 uses signed
`long` identities and closed `CallbackContainer<int>`. The nested iterator at
358898 has Int32 state and named fleet/address fields. Existing disassembly
shows terminal -1 and retained yield-state paths; only that exact implementation
supports the qualified state convention. The native closed constructor stores
real managed delegate references with GC barriers. Dump line 1277062 defines
mode 2 as `HandleError`; the four-argument constructor declares that native
default. The retained recall success/error bodies are literal `ret` callbacks.

This is source/metadata/body evidence for retained **Windows client 273**. Hashing
disk artifacts does not prove that a current process loaded them. No macOS native
body was inspected and no game resource request was executed by this scan.

## Fixtures and current qualification

- Existing [policy test](../../../tests/discovery_tow_policy_test.cc): copied
  identity, optional evidence, iterator-state and fresh-course decision rules.
  It does **not** exercise the native adapters or actual request owner.
- Existing [controlled runtime](../../../tests/il2cpp_bindings.cc): generic
  `NewClosedDelegate` tests qualify the shared implementation, not the full
  Discovery callback graph by themselves.
- New [production adapter fixture](../../../tests/discovery_tow_bindings_cases.h):
  extends that runtime and the already-included production `fleet_actions.cc`.
  It calls the real Plan, Start, Stop, Callbacks, Submit, Begin, Observe and Frame
  paths. Only managed metadata/generated bodies, allocation and the native
  scheduler are substituted. No duplicate towing policy, synthetic task engine,
  network call or resource-spending request is introduced.
- Fresh-process modes: `discovery-tow-native`, `discovery-tow-reject-arity`,
  `discovery-tow-reject-unsigned`, `discovery-tow-reject-generic`,
  `discovery-tow-reject-vector`. Separate processes matter because qualification
  caches bind the first observed metadata.
- Additional callable-qualification modes: `discovery-tow-virtual-equals`,
  `discovery-tow-shared`; rejection modes `discovery-tow-reject-null-plan`,
  `discovery-tow-reject-null-start`, `discovery-tow-reject-null-instance`,
  `discovery-tow-reject-null-tow`, `discovery-tow-reject-null-ctor`,
  `discovery-tow-reject-null-support`, `discovery-tow-reject-byref-void`,
  `discovery-tow-reject-shared-invoker`. The shared-mode host runs the real
  invocation wrapper and callback constructor through its runtime seam, including
  argument marshaling and contained scheduler exceptions. This is controlled-host
  ABI qualification, not proof of exact-client fully shared metadata flags.
- Native mode covers full unsorted array selection beyond slot 9, real closed
  delegates, constructor/readback/allocation failures, waiting and terminal
  scheduler states, successful exact submission once, mutable destination,
  owner/section/capture/deadline cancellation, Plan/Start/Stop reentry,
  throw-after-start/null-start, failed Stop without stale publication, managed
  root failures and targeted command-boundary context changes.
- Test-host seams have no production runtime role. Per-class owned allocations
  are required because the old generic host reused one tiny TestObject for all
  non-Action allocations, which cannot represent two delegates and a callback
  container. Static singleton readback is similarly extended only in the host.
- **At this document's authored checkpoint no new fixture was compiled or run
  by this lane.** Parent build/receipt must record the final candidate, executable
  identity, modes and results. Current source review or a historical policy pass
  is not a substitute for those results.

## Exact-client acceptance and recovery

Use only an operator-selected disposable/safe tow, since a genuine tow may spend
resources. First verify disabled/native-hotkey/focused-input behavior, ordinary
ship selection and unavailable Discovery without submitting anything. Then, with
Discovery deployed and native eligibility visibly satisfied, Shift+the intended
fleet shortcut should issue one tow to Discovery's location. Check cancellation
by changing section or selecting another ship before completion, wrong/same
fleet and unavailable eligibility; ensure ordinary native UI remains usable.
Confirm live metadata/scheduler binding and native callback availability before
calling the action player-verified. Windows boot alone and this host's simulated
scheduler do not qualify server delivery, resource behavior or macOS.

Keep historical `PLAY-FLEET-TOW` inventory evidence; append this source-bound
assessment and the final cohort candidate/results. The policy and callback
corrections predate this discovery checkpoint. Remaining work is focused
production-boundary qualification and a deliberate native trial, not rewriting
the existing tow implementation.
