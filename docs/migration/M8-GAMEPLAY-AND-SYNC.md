# First gameplay cohorts and sync compatibility

This continues the [first playable cut](GAMEPLAY-FIRST-CUT.md) after the
[M4 configuration/state checkpoint](M4-CONFIGURATION-AND-STATE.md). Preservation
input is downstream `45d1deaffddc3706e5c8232cf746ca46ef67be91`, read through Git;
the active play-dev checkout is not a moving specification for this migration.
AAI, STFCKitUI and the native Settings screen remain deferred, not discarded.

The work is on `integration/ripper-foundation-m1b-20261025` in the approved
isolated worktree. Source and offline testing do not authorize deployment or
establish full gameplay parity. The original offline cohort gates below did not
cycle the game. A later authorized native trial exposed the startup frame-read
failure documented in [the focused correction](STARTUP-FRAME-PREFLIGHT.md);
its runtime evidence and corrected-trial status are separate from these gates.

## Registered gameplay settings

[The existing shortcut definitions](../../src/mod/settings/basic_gameplay_settings.h)
now provide one set of 19 registered identities to both startup registration and
the actual callbacks in hotkeys, UI scale, camera and instant warp:

- Hotkeys enabled; preview locate/recall; the five cargo visibility switches.
- UI, viewer and ship scale; the five zoom presets and explicit default zoom.
- Instant-warp prompt choice (`none`, `warp`, `jump`).
- The fleet queue toggle (`control.queue_enabled`), added in the separately
  qualified follow-on below.

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

Development checks establish the new pure pin policies, checked permutation,
registered worker behavior and sync processor/cache policies. The final gates
below supersede the evolving-input development runs for their exact candidate.
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

### Qualified source checkpoint

Implementation: **`dbc1950e0ea74e384070cec05387a98f6cf58072`**.
Final qualified candidate: **`f0f9c2db172d33e5cce18b115455b4f5091fd54a`**.
The latter changes only the test snapshot holder to use the same portable
acquire/release shared-pointer functions as production. The first Windows build
passed, but the first Linux run rejected the mock's atomic shared-pointer
specialization; that superseded failure remains preserved.

| Exact final candidate lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 196/196 steps; 8/8 reported Zig tests |
| Windows x64 Zig 0.17, Profiles host | 196/196 steps |
| Windows x64 Zig 0.16, default off | 174/174 steps; 8/8 reported Zig tests |
| WSL/Linux x64 Zig 0.16 | 158/158 steps; 8/8 reported Zig tests; 33 retained artifacts |

Every lane began/ended at the exact clean source candidate. Three inert Profiles
controls ran per host; four negative option controls ran on 0.16 only. The
original Profiles account/preferences/catalog lifecycle cases were not run in
the developer execution environment. The earlier M1c ordinary-desktop results
remain separate evidence, not a runtime claim for this DLL.

Windows sync tests used the actual production uploader and CPR/CURL libraries
against an owned temporary `127.0.0.1:0` listener with generated synthetic data.
They verified ordinary HTTP 500→retry→2xx acknowledgement, initial-sync header
clearing, best-effort diagnostic loss without retry, and a held response that
cannot acknowledge after its generation is sealed. A diagnostic tail still
delivers independently. Test planning explicitly bypasses retry deadlines when
checking pending state, so waiting for backoff cannot masquerade as an ACK.
The listener/child are cleaned up, and proxy environment is disabled for that
child. No live game data, real destination, game server or user configuration
was used. Linux qualified portable logic/publisher/ownership, not this HTTP lane.

Independent readback verified **408 unique recorded file facts with zero
mismatches**, source/tool/archive/log/artifact identities, every required marker,
and additional queue/wire markers. Static PE inspection verified exact identity,
static CRT and 18/18/16 exports. Each DLL has one unique compiled TOML provider;
host DLLs contain only the qualified derived Profiles adapter/core, and default
off has no Profiles libraries. The installed uploader fixtures have no static
GameAssembly/UnityPlayer imports. These checks do not load the product DLL.

Evidence root:
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/candidates/f0f9c2db172d33e5cce18b115455b4f5091fd54a`.
Independent proof `independent-sync-readback-final.json`, SHA-256
`d24962ccbc0a345442a955caa324864907d3a573d76ac50549816c1efec97060`.

| Product DLL | SHA-256 |
| --- | --- |
| Host 0.16 | `961be5837ead1b794c347c937388dd519a8ef9d1bbde8261680297ef94c4cd33` |
| Host 0.17 | `c6a8a9dd7bdd85caf4f78435955e5dad67c395a86bf34c722fe762e44b09e28a` |
| Default off 0.16 | `be5af5d7cb88969d12455c6653e78cb5bb04c38a1778430348d8b0a5c611066b` |

No product DLL was loaded, signed, deployed, pushed or promoted; no game cycle
occurred. Later source changes need their own appropriate checks. A later
documentation commit does not change which source identity these binaries carry.

### Qualified queue-saving follow-on

Source **`5fbb1ec915acad9e242af3eb2cfe948a2e8c78b9`** adds the nineteenth
registered identity. FleetShortcuts uses the same immediate Boolean save helper
as the initial cohort and still resets queue-recovery tracking immediately after
the toggle. It restores old downstream's Ctrl-Q persistence without changing
the queue's decision policy or restart semantics.

The real-worker fixture covers both saved values, external deletion conflicts
and an explicitly supplied protected reload value. Generic owner/reload/receipt
tests separately cover the native adapter; the fixture does not synthesize live
key dispatch or claim to drive automatic Unity reload.

| Exact queue candidate lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 162/162 steps |
| Windows x64 Zig 0.17, default off | 140/140 steps |
| WSL/Linux x64 Zig 0.16 | 158/158 steps; 8/8 reported Zig tests |

These are separate, narrower Windows gates: native product/fixture compilation,
mod/configuration and gameplay-save tests, exact PE identity/export/static-CRT
checks, one compiled TOML provider and the expected Profiles dependency closure.
They did not rerun the synthetic HTTP listener, inert Profiles controls or
negative build-option controls. The full earlier sync proof remains attached to
`f0f9c2db`; it is not silently relabeled as a later full-suite run.

All three gates began and ended at the clean queue candidate. Independent
readback verified **318 file facts with zero mismatches**. Evidence is under
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/candidates/5fbb1ec915acad9e242af3eb2cfe948a2e8c78b9`;
`independent-queue-readback.json` has SHA-256
`a63d657aaa569e529f9c2d47dcf9f60de05438742c8e6b68894b2d91b8b670f5`.
The Linux compiler digest is a captured shell observation, not an independent
binary rehash by the readback auditor. No native gameplay, macOS, deployment,
signing or store-lifecycle qualification is added.

### Next preservation deltas

Officer preset ordering needs
its own canonical-source/presentation separation: SlotId zero is valid, empty
rows stay fixed, and its immediate session order differs from pin publication
on Saved. Old controls are Shift/Ctrl (Command on macOS) plus rename, not drag.

Gift drawer/daily filtering/chest/cooldown/reveal/artifact suppression already
have concrete foundation equivalents; do not port duplicate owners. Remaining
differences need their own boundary work: donation maxima and owned-value
semantics; interstitial original/close lifecycle; HUD Auto restoration/live
refresh; chat avatar cache retention; typed toast-context fallback. Exact-client
Windows ChatTitles target extent has an offline SPUD planning qualification on
two hash-verified retained client images: ten plans passed, with short and far
landings fitting before the adjacent callback. The historical 24-byte warning
does not describe the new landing sizes; no source correction was earned.
The images remained private and nonexecutable, with unchanged digests and no
hook application or client execution. Live attachment, unwind publication and
gameplay behavior remain unqualified. Private receipts are in
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/chat-hook-study`.

Claim recovery remains held separately. Its old default-false recovery setting
does not make the old default-true science hook TU safe to activate wholesale.
Preserve policies/receipts without enabling probes incidentally.

The [presentation follow-on](M8-PRESENTATION-PRESERVATION.md) restores HUD Auto
restoration/Refresh and checked named toast fallback, with its own exact final
source gates. Its pure officer policy does not activate native reordering.

Further preservation cohorts still include officer presentation ordering,
galaxy/orbit/Haven extensions, OPC/ETA/tech artwork, guarded recovery and custom
fleet/incoming-player notifications. Do not call this first cohort full
downstream parity merely because builds pass.

Private exact gate inputs/results and recovery notes are under
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025` and the earlier
`ripper-foundation-m4-20261025/sync-study`. Direct collaboration and repo-scoped
AXF continuity are not Runner-governed execution. No deployment or game cycle
is established by these source changes.
