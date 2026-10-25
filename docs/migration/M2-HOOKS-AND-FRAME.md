# M2: hook outcomes and shared frame ownership

Completed 2026-10-25 on the isolated integration branch. No deployment, game
restart, push, active-branch promotion or Profiles activation was performed.

## Exact checkpoints

- Starting M1c checkpoint: `210df408229446507fa2bb6eb366d499e29ca743`.
- M2 implementation: `623919c6a15248309c58a54950ec395d9ab52872`.
- Final source and tested candidate: **`3edcdc2b474d3abba6df61793f836fdb0ab00913`**,
  tree `534e1c67be8fcd6b92fcc0788ab9562acb8ab11a`.
- Branch: `integration/ripper-foundation-m1b-20261025`, approved worktree
  `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`.
- Existing product `D:/dev/stfc-mod` remains separate on `play-dev`,
  `45d1deaffddc3706e5c8232cf746ca46ef67be91`.

The final correction prevents an early shared-anchor request from dispatching
callbacks or reloading Config while feature initialization still borrows its
startup configuration. All three gates were rerun on that correction. Earlier
passing gates are preserved as earlier-candidate evidence, not substituted for it.

## Implemented boundaries

### Transaction evidence and failure handling

[`HookSet`](../../src/libs/il2cpp/include/il2cpp/hook.h) retains the existing
SPUD transaction and native adapters. It now returns a typed, cached
`HookInstallResult`, separating not-attempted, installed, missing target, empty,
rejected and indeterminate states from the underlying SPUD error.

`CommitDetailed()` preserves the historical result and never retries. `Commit()`
keeps ordinary bool compatibility, but throws `HookInstallError` on unsafe state,
including when another set later latches the process unsafe. Further staging is
rejected before taking caller custody.

Rollback failure, protection failure, suspension failure, unknown vendor errors,
commit/query/ownership exceptions and mismatched post-failure handle state are
conservatively indeterminate. The latter two vendor error codes are ambiguous:
protection failure can follow a target-byte write/cache-flush failure, and
suspension failure can follow partial thread-IP redirection. Matching installed
flags alone cannot establish safe native state in those cases.

The process safety latch is published before fallible reporting or cleanup.
Already allocated owners are quarantined intact; no new allocation, detach or
destruction is required to retain them. Later sets cannot apply patches. Ordinary
refusals preserve verified pre-existing hooks rather than implicitly uninstalling
them when custody ends. Stage allocation failures also protect earlier installed
or unqueryable custody before unwinding.

This is **not a vendor repair or stronger atomicity guarantee**. SPUD may release
failed-install resources or resume redirected threads before returning the error.
The wrapper avoids additional cleanup and deliberate continuation; it cannot undo
those actions or guarantee that execution survives to the host's fatal boundary.
Vendored SPUD was not modified. Thread-local diagnostics are labeled
post-transaction, because rollback may replace the original failure detail.

### Capability owners and critical policy

[`InstallGuard`](../../src/mod/installation.h) is a small bootstrap/game-thread
owner, not a task scheduler or global name registry. Repeated and reentrant
requests do not repeat initialization side effects. Skipped, unavailable,
installed and indeterminate outcomes remain distinct.

The host uses it for bootstrap, setup, the feature table, each feature and the
shared frame anchor. Ordinary optional failure may leave independent features
available. A required capability's incomplete state, or any indeterminate native
state, invokes the nonthrowing fatal startup boundary. Registration/reporting
exceptions cannot bypass the required-completion check. Required frame work
also requires successful registration and actual anchor readiness.

[`FeatureInstallations()`](../../src/mod/features/features.h) exposes capability
state, frame registration and frame readiness separately. Existing feature
descriptors remain optional by default and preserve their three/four-field
initializers. This is not a claim that every optional sub-capability in a feature
is present: object tracking still has separate per-class/finalizer limitations.
AAI's observer/Back/discovery statuses must remain independent during its later port.

### One frame anchor and process-lifetime callbacks

[`frame.h`](../../src/mod/frame.h) exposes `EnsureFrameTick()`, its observational
installation result, `RegisterFrameCallback()` and `FrameDeliveryAvailable()`.
[`mod.cc`](../../src/mod/mod.cc) remains the only `ScreenManager.Update` owner.
Settings, AAI and later consumers must join it rather than add competing detours.

[`FrameCallbacks`](../../src/mod/frame_callbacks.h) preserves the old downstream
contract: stable registration order, duplicate callback deduplication, owned
diagnostic names, rejection of null/during-dispatch registration, recursive
dispatch suppression, and containment of callback/reporting exceptions. It
borrows the stable registry directly; it does not allocate a callback snapshot
every frame. Registration may allocate and can fail without throwing through the
public native adapter.

Registration is distinct from physical anchor installation, and both are
distinct from completed host initialization. Until the host and anchor are ready,
the detour forwards the game's original Update before input, reload, subscribers
or shortcuts run. Nested native Update similarly avoids recursive host services.
Normal input -> settings reload -> installed feature callbacks -> shortcuts ->
unconsumed original ordering is preserved.

All installation/registration/readiness APIs require their documented
bootstrap/game-thread use. The atomic unsafe latch does not serialize concurrent
commits or qualify worker-thread/dynamic runtime patching. No such caller was added.

## Qualification on the final candidate

| Lane | Result |
| --- | --- |
| Windows x64, Zig 0.16 | **93/93 steps**, all requested native fixtures, 8/8 reported Zig unit tests; proxy verification passed |
| Windows x64, Zig 0.17 | **93/93 steps**, all requested native fixtures; proxy verification passed |
| Native WSL/Linux x64, Zig 0.16 | **81/81 steps**, all requested fixtures including launcher rsync, 8/8 reported Zig unit tests |
| Formatting / whitespace | Changed C++ clang-format checks, Zig formatting and `git diff --check` passed |
| Independent review | Ownership/failure/caller review and correction-delta review completed; final receipts independently read back |

Windows used the existing installed MSVC 14.51.36231 / SDK 10.0.26100.0 fallback,
not a new pinned cross-SDK reproduction. Libraries/mod were ReleaseFast and native
fixtures Debug. Windows gates included install/build-tests, toolchains, codegen,
dependency smoke, IL2CPP, mod, sync and SPUD. Both DLLs verified 16 proxy exports,
static CRT and the exact candidate/build provenance marker. Nothing was signed.
Nonexistent optional source paths with default-off options still built successfully.

The new qualification covers:

- Missing/empty sets, missing Nth member, successful detach and cached results.
- Deterministic reverse rollback, pre-existing custody and terminal mutation rejection.
- Eight fresh-process HookSet invocations: safe cases plus seven unsafe modes
  (rollback, install throw, mutation-before-error, protection, suspension, unknown
  error and detach throw), with no reset of the real process latch.
- Real `InstallGuard` behavior under that latch, blocked later work, critical vs
  optional policy, historical success vs current safety, and quarantine lifetime.
- Real native rollback of an added chain member, preservation of existing hooks,
  intentional same-target chaining and repeated-commit idempotence.
- Callback ordering/mutation/recursion/exception rules and early-anchor readiness.
- Existing binding/GC/tracker, configuration/reload/shortcut, sync and relocation
  fixtures. Linux also retained the existing launcher rsync tests.

The first narrow development run found an unnecessary heavyweight dependency in
the pure installer fixture (native headers/SPUD include closure). The owner model
now needs only the latch declaration. No assertions were weakened. Initial Linux
setup needed the already verified compiler archive restored into the existing
Linux-native cache; setup failures are recorded separately from test results.

## Limits and work still parked

- No in-game Update/reload/input trial, exact-client detour fit, native startup
  termination trial, macOS build/runtime/signing/notarization or release qualification.
- Staging-allocation and installed-state-query exception paths were inspected,
  but lack deterministic allocator/query fault-injection fixtures.
- Native rollback coverage restores an added chain member on an already-hooked
  target, not original bytes of a previously unhooked target. Generic fake
  transactions do not prove a native shared suspension window.
- Full downstream Settings, AAI/STFCKitUI and Profiles integration is not active.
  M1b's optional-product results remain attributed to their earlier candidate;
  this is not a rerun of every AAI/Profiles suite or a complete host port.
- Platform bootstrap differences remain M3 obligations: exact `prime.exe`
  admission, static-CRT thread notification behavior and truthful explicit-profile
  launch contract export. Do not advertise the marker until its behavior exists.
- Preserve Profiles Initialize before assembly load, Prepare before original
  runtime initialization, and required hook installation after successful runtime
  initialization before optional features/login. Ordinary installation leases and
  explicit-profile fail-closed behavior remain mandatory. No pin upgrade occurred.
- The 117/112 Settings preservation baseline and its two known inventory failures
  remain visible; settings/persistence semantics were not ported or redefined.
- Toolkit expansion, SQLite, persistence changes, launcher work and UI redesign remain parked.

The Codex execution-context issue is **parked for a separate follow-up, not
closed**. [M1c](M1C-PROFILES-ENVIRONMENT.md) preserves the original-vs-migrated,
ordinary-vs-tool-host evidence. Identifying the redirecting layer and establishing
a supported automated execution lane remain future work. Do not weaken guards,
relocate locks or manufacture an ordinary identity to make those tests pass.

## Evidence and recovery

Private root: `D:/dev/stfc-workspace/.ax/ripper-foundation-m2-20261025`.
Final receipts/artifacts are under
`candidates/3edcdc2b474d3abba6df61793f836fdb0ab00913/`:
`windows-016-receipt.json`, `windows-017-receipt.json`, `linux016-receipt.json`,
their logs and separate `artifacts/win016`, `win017`, `linux016` outputs.
Earlier root-level receipts/artifacts preserve `623919c6` independently.

Windows DLL SHA-256:

- Zig 0.16: `f0e6268b975d46aa1b46dbfcaf6f8a1da9465315fe18dd95b30acfc33f8d881e`.
- Zig 0.17: `cec43e5e82c061ef8347600ee4a150a3be282dfedab7f660c566e5c54ac3be0e`.

Direct tools and independently coordinated agents performed this work. No
Runner-governed Attempt or Runner-created checkpoint is claimed. Git source,
candidate-bound receipts and explicitly scoped AXF continuity are distinct records.

**Next boundary: M3 Profiles bootstrap integration**, retaining the embedded pin,
physical installation exclusion, three-phase ordering and fail-closed contract.
Its runtime activation/qualification is a separate step; M2 does not authorize it.
