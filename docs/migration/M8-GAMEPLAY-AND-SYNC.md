# First gameplay cohorts and sync compatibility

This continues the [first playable cut](GAMEPLAY-FIRST-CUT.md) after the
[M4 configuration/state checkpoint](M4-CONFIGURATION-AND-STATE.md). Preservation
input is downstream `45d1deaffddc3706e5c8232cf746ca46ef67be91`, read through Git;
the active play-dev checkout is not a moving specification for this migration.
AAI, STFCKitUI and the native Settings screen remain deferred, not discarded.

The work is on `integration/ripper-foundation-m1b-20261025` in the approved
isolated worktree. Source and offline testing do not authorize deployment or
establish full gameplay parity. The running game has not been cycled.

## Registered gameplay settings

[The existing shortcut definitions](../../src/mod/settings/basic_gameplay_settings.h)
now provide one set of 18 registered identities to both startup registration and
the actual callbacks in hotkeys, UI scale, camera and instant warp:

- Hotkeys enabled; preview locate/recall; the five cargo visibility switches.
- UI, viewer and ship scale; the five zoom presets and explicit default zoom.
- Instant-warp prompt choice (`none`, `warp`, `jump`).

Booleans and warp choice submit immediately. Graphics scalar values retain the
exact float converted to double, with the existing 250 ms save coalescing. The
registered comparison value comes from the original startup input, including a
missing key; first use cannot silently rebase it over an external edit.

Automatic preset-derived default zoom is deliberately session-only in the old
implementation. `runtime_config::RetainSessionIntent` preserves that live intent
through unrelated reloads without submitting a write or fabricating a failed
save. Older pending/completed receipts cannot erase it; a newer explicit default
save supersedes it normally. Rejected foreign-thread/quit-reentrant callers do
not create protected intents. Failure to retain live intent prevents an unsafe
reload rather than substituting a guessed value.

The [real-worker fixture](../../tests/basic_gameplay_settings_test.cc) uses the
same mutation helpers, RuntimeConfigWriter and TomlEditor against owned temporary
files. It covers registration, exact values, clamps/native-zero exemptions,
comments/unknown fields, absent keys, session-only zoom and real external-edit
conflict. [Native adapter tests](../../tests/runtime_config_test.cc) separately
exercise owner/reload/receipt behavior. Neither fixture drives live key dispatch
or proves Unity rendering.

Two suspected rendering gaps were withdrawn after exact source review. Old
`Config::Get()` owns a static object; `adjust_scale_res` has no initializer,
reader or assignment and remains false by zero-initialization. Its resolution-
height branch was not a supported active setting. Both implementations skip
existing-model rescaling for nonpositive multipliers; the old fallback of one
only affects newly loaded models. No speculative restoration was made.

## Stable ship pins and checked native observations

The [dock owner](../../src/mod/features/dock.cc) uses the individual Ship/BaseShip
positive 64-bit instance ID. Fleet ID, hull, display name and zero are not
substitutes. Declared interface resolution and runtime assignability are checked.

The shared IL2CPP `TryCall`/`CallResult` seam distinguishes a successful zero,
false, null or void result from unavailable metadata, type drift or a contained
exception. It shares existing resolution, virtual dispatch and reporting. The
full generic-sharing runtime exception slot remains visible to checked callers;
ordinary Call and delegate behavior retain their established contracts.

Saved IDs, legacy matching and explicit saved order reuse the preserved
[order policy](../../src/mod/pinned_ship_order.h). Desired rapid edits compose,
but only the exact latest Saved state receipt changes the applied order. State
paths/schema/worker ownership remain M4's; no pin action rewrites the whole
primary TOML. Unresolved legacy names remain available.

The managed display list keeps its identity and size. A rooted snapshot and
[checked permutation](../../src/mod/checked_ship_order.h) require observations
before mutation and complete readback afterward. Failure attempts restoration
and verifies it independently of setter return values. Further pin editing is
disabled after a failed reorder. `Unrestored` means restoration could not be
proved, not that the original list was repaired. A separate presentation-list
identity would need another native lifecycle qualification.

[Badge input](../../src/mod/features/ship_pin_input.cc) is separate from tech
artwork and officer ordering. It uses weak UI custody and the actual badge
double-click/held-drag contract before native tile selection. Touch/pen and
ordinary scroll stay native. Gesture, input epoch, source generation, context,
focus and drop geometry are independent checks. Missing geometry/identity cannot
become a pin action. Receipt callbacks revalidate the matching live context;
no managed object goes to the file worker.

Root badge, group band and first-pin cue are excluded from native layout using
a checked LayoutElement setter and readback before activation. An independent
preservation review found this missing from the first draft. Glyph pieces stay
ordinary children of their own badge. Actual clipping/layout-group geometry
still needs a native visual trial.

The old downstream accepted `pinnedshipclicktoggle` but had no consumer. It
remains accepted; badge installation follows the actual `pinnedshiphooks` owner.
The obsolete Shift-click documentation is corrected without inventing a new
meaning for that compatibility key. One shared native tile click owner serves
pin input and double-click assignment.

## Sync: completeness, transport and names

The foundation's canonical RecordStore and per-target revision/2xx acknowledgement
remain the ordinary delivery implementation. Existing record builders are not
replaced by the old serialized sync engine.

Owned input uses the preserved reservation/FIFO/lease queue: 128 entries,
32 MiB admission charge and 16 MiB per item. Admission precedes managed string
conversion/copy; an in-flight lease retains its charge. Known required input
loss, malformed supported evidence or processor failure seals that generation's
ordinary publication/delivery/ACK path. Deliberate unsupported filters and
optional player/alliance enrichment are distinct. A dead processing worker
cannot recover merely through a new login.

Supported JSON sections reject null/scalars before claiming a full baseline;
valid object/array forms and valid empty containers remain accepted. Resource
keys must parse completely, so `7junk` cannot alias resource 7. Unknown sections
remain deliberate filters. Both owned worker loops contain terminal exceptions:
queue/bookkeeping failure in the journal thread cannot escape a detached thread
and terminate the game. The small terminal boundary is tested independently;
that does not claim an injected allocation fault in a running game.

Session bookkeeping catches only the mod's owned work. Allocation/metadata
failure stops ordinary sync; it cannot skip or retry the game's original call.
A complete successful native session reset establishes the new generation.
An old delayed failure cannot poison a newer generation. These are admission
and publication checks: they do not retract HTTP already authorized or sent.

HTTP deadlines stay finite in debug and release. Request bodies have explicit
cleanup after each synchronous operation. Queue charges are not total process
memory bounds: canonical JSON, per-target old versions, fetched responses,
serialization temporaries and HTTP library ownership remain separate costs.

Legacy nonempty `sync.url` plus `sync.token` supplies an in-memory default target.
A valid explicit default wins; inactive placeholders do not suppress the legacy
pair. Category/proxy/SSL defaults and per-target overrides survive. Pure parsing
does not rewrite the primary input or activate workers.

Diagnostic observations retain the old schema-1 producer boundary and are
independent best-effort append-only delivery, not coalesced canonical entities.
Diagnostics are default-false globally/per target and do not activate probes.
Restoring the transport in source was separately authorized; testing must use
synthetic local-only payloads, never live game/session data or real destinations.
Exact sequence/UTC encoding, bounded serialization, target/session revisions,
queue ownership and failures require their own fixture evidence.

Resolver caching reuses the preserved 4096-entry LRU primitive, with the new
foundation's generation fence. Name, tag and string identity limits are 256
bytes. Zero/negative TTL disables retention; a successful fetch may still use
its own fresh result. Missing/oversized names remain optional enrichment, not
lost ordinary evidence. These bounds do not eliminate upstream decoding costs.

The old sent-ID file records download-queue admission, not successful delivery,
and has no target/account identity. Guff delegated the choice: keep its bytes
and paths untouched, never infer ACKs or suppress acquisition from those IDs,
and retain the stronger in-process per-target semantics. Restart can re-fetch
and re-send. No new persistent history, account inference, migration or exactly-
once promise is introduced. Live remote-target behavior remains unqualified.

## Verification and remaining gates

Development checks already establish the new pure pin policies, checked
permutation, registered worker behavior and sync processor/cache policies.
They do not substitute for a clean final source/binary-bound candidate gate.
The initial legacy-config assertions incorrectly used `value_or("")` followed by
string-literal comparison; toml++ returned `const char*`. Their string fallback
types were corrected while retaining all behavior assertions. This was a test
defect, not permission to weaken the target-preservation contract.

Before any normal-game trial: finish current code/fixtures, inspect the exact
candidate, run both supported Windows compiler lanes plus portable fixtures,
verify exports/dependency closure/artifact identity, and retain the results.
Actual native method/field resolution, hook extent, pin geometry/input, save-
and-reopen behavior, profile/store behavior, macOS, signing and external sync
remain distinct qualification claims.

Further preservation cohorts still include officer presentation ordering,
galaxy/orbit/Haven extensions, OPC/ETA/tech artwork, guarded recovery and custom
fleet/incoming-player notifications. Do not call this first cohort full
downstream parity merely because builds pass.

Private exact gate inputs/results and recovery notes are under
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025` and the earlier
`ripper-foundation-m4-20261025/sync-study`. Direct collaboration and repo-scoped
AXF continuity are not Runner-governed execution. No deployment or game cycle
is established by these source changes.
