# Play-dev claim capture

This science capture is for ongoing claims investigation while other features are
tested. Keep it out of clean `play` promotions. It observes claim, request, reward,
input, semaphore and offer-refresh lifecycles using anonymous session-local tokens.
It does not retry claims, purchases, reconciliation requests, or reward grants.
The separate, opt-in UI recovery below may retry one native navigation request.
The `[patches].claimtracehooks` emergency compatibility switch defaults to true.
Disabling the hook group makes capture unavailable; it is not a claim feature.

The incident families are tracked privately in `.codex/claims/incident-index.json`.
Raw incident data and account-specific findings must remain outside Git.

## Recording and health

The runtime appends to `science/claims/community_claims.jsonl` beneath the normal
mod data directory (the game directory on Windows). Status, reply and control
files live in that same `science/claims/` folder. There are two
8 MiB rotations: total retention is approximately24 MiB across sessions. Every
record has a session ID; startup identifies the source build and installed hooks.
Claim/request lifecycle records bypass the input-snapshot rate limit. Existing
bounded token/weak-object/list limits remain explicit in the trace.

`community_claims_status.json` is refreshed every30 seconds, including when no
claim is active. A live acknowledgement distinguishes a running recorder from
an old file left by another session. Partial hook installation or sink failures
make health false. Do not report capture as ready merely because a file exists.

The helper accepts the game/mod data directory or the explicit claims folder.
It finds the newest status across the new layout and legacy flat builds, so a
rollback's live capture remains reachable. It does not search archived traces.

```powershell
python scripts/claim-capture.py status --game-dir <runtime-log-directory>
python scripts/claim-capture.py mark --label natural-repro --game-dir <runtime-log-directory>
```

`mark` writes an incident marker and snapshots all retained rotations into the
checkout's ignored `.codex/claims/incidents/` directory with hashes and a receipt.
Live copies can contain open spans or rotate during copying; receipts record
detected source changes. The marker is evidence of the report time, not proof of
the onset or root cause. `snapshot` can preserve existing files without claiming
a successful live acknowledgement.

## Bulk-selection persistence journal

The capture also observes all native store, refinery and faction bulk selections;
it is not restricted to a list of reported problem items. The separate
`science/claims/bulk_selection.jsonl` journal appends across restarts, retaining
4 MiB plus four rotations (approximately 20 MiB total). Incident snapshots include
these rotations. Its health is reported separately as `bulk_selection` in status;
partial selection coverage does not disable the existing claims recovery.

Records include quantity edits (including explicit zero/deselection), temporary
and committed dictionaries, pending restored selections, confirmation/discard,
reset, native pruning, claims, and the native preference reads/writes for
`StoreSelection`, `RefineSelection` and `FactionSelection`. Cloud load/save attempts
and correlated transport observations are recorded without request bodies.
The probe never invokes extra preference getters, writes selections, submits
claims, or retries cloud saves. Each intercepted original executes once.

Catalog IDs use stable opaque aliases across sessions; localization aliases help
investigate a replacement catalog ID, but do not establish that two offers are
equivalent. Account/order/session credentials, raw IDs and arbitrary UI text are
not written. This is private account-specific evidence; do not publish traces.
Dictionary/list limits and unsupported layouts are explicit, not empty selections.

```powershell
python scripts/claim-selection-history.py --game-dir <runtime-log-directory>
```

The read-only helper lists edits, changes within observed native calls and saved
lists that differ on a later native read. Zero and omitted entries both mean
deselected. It does not infer durable cloud persistence from a successful request,
nor assume two different sessions used the same account. Nested calls can report
the same transition, and rotated-out history cannot be reconstructed.

The journal stores compact per-call dictionary changes instead of repeated full
snapshots during tab browsing. Native preference reads/writes retain their bounded
selection lists as comparison baselines. The edit observer uses the underlying
`BulkSelectionContainer` setter because UI callers can bypass the context wrapper.
A separate atomic `bulk_selection_baseline.json` retains each lane's last observed
write, or its first observed read until a write occurs. It survives log rotation
and restart and is attached to later native read records for comparison. This
diagnostic checkpoint never restores or modifies the game's preferences. Its
account scope is unverified, so confirm the same account before attributing a
cross-session difference to a reset. Incident snapshots include the checkpoint.

Cloud-load observation shares the existing native Mod Settings detour; it never
installs another hook on that method. This observation is available when that
native-settings hook group is installed. The additional 13 Windows x64 client272
entry fragments were checked for the 24-byte maximum SPUD overwrite; matching
Mac native fit/runtime remains unverified.
Run `python -m unittest discover -s tests -p 'test_claim_selection_history.py'`
locally to exercise deselection/reappearance, lane separation and unknown evidence.

### Long-term archive on another disk

The optional external collector copies complete selection-journal records into
immutable compressed batches outside the game. It adds a client label, resumes
after restart, follows file identities across rotation, and records missing
sequence ranges explicitly. Atomic batches and checkpoints recover a collector
crash without duplicating a published batch. This does not increase game RAM use
or change native hooks. Collection is independent of game starts and exits.

```powershell
python scripts/claim-selection-archive.py --game-dir <runtime-log-directory> --archive-dir <private-archive-directory> --client home
python scripts/claim-selection-history.py --game-dir <private-archive-directory> --since 2026-10-17
```

Defaults are a 50 GiB compressed archive budget, a 10 GiB free-space reserve and
30-second collection intervals. `--max-gib`, `--reserve-gib` and `--interval` change
these limits; `--once` performs one collection pass. At either capacity limit the
collector pauses, preserving existing evidence. It never deletes old logs. Its
`collector_status.json` heartbeat reports progress, capacity and known gaps.
If collection is unavailable long enough for the local ring to expire, that
history cannot be recovered. A client label identifies a recording installation,
not an account. Use separate destinations for different clients. Archive reader
`--session` and `--since` filters bound the analyzed timeline; `--limit` only limits
displayed findings. Register an external collector with the host's user startup
facility if it should resume automatically after login.

### Named Augment offer tracking

`claim-offer-watch.py` preserves a separate, private name catalogue for **Priority 1
Mega Claim**, **Forbidden Tech Mega Claim**, and **Planetary Charts**. It matches
complete English names after removing rich-text markup and normalizing whitespace;
Planetary Charts' numeric cap may vary. Descriptions mentioning those names do not
match. Different IDs remain separate variants, including ambiguous name matches.
This tool observes data only; it never restores selections or invokes purchases.

The existing selection archive already records offer encounters, pending/selected
changes, edits and native preference writes/reads. The new watcher preserves each
changed English localization catalogue's matching IDs, names and associated text
(including available cost/reward/tier descriptions). Historical locale payloads can
seed IDs no longer present in the current catalogue. Immutable snapshots retain old
variants; an atomic index recovers an interrupted update. Repeated bytes add no new
snapshot. A corrupt/partial live locale leaves the last good catalogue intact and
reports an error for retry. The catalogue budget is100 MiB with a10 GiB free-space
reserve; reaching either limit preserves evidence and reports an error.

Use a Python environment with `protobuf` and the matching generated
`Digit.Client.Localization_pb2` modules. The narrow decoder reads the structured
bundle localization category; it does not scrape arbitrary strings or depend on
the generic research-corpus index. No new native hooks or game restart are needed.

```powershell
python scripts/claim-offer-watch.py watch --locale <locale_en.bin> --proto-dir <generated-proto-directory> --output <private-catalogue-directory> --seed-locale <optional-historical-locale>
python scripts/claim-offer-watch.py report --output <private-catalogue-directory> --archive-dir <selection-archive-directory> --limit 30
```

`watch` polls every30 seconds (`--once` captures once).
`offer_watch_status.json` reports its heartbeat, process, catalogue version count
and variant count. This is separate from the selection collector's heartbeat; both
must be running for future name/encounter evidence. The report joins catalogue
aliases to archived native bundle/localization identities and summarizes actual
availability, edits, selection changes and faction preference read/write quantities.
It streams the archive twice to correlate differing bundle/localization IDs without
loading the archive into memory. Recent detail is bounded by `--limit`; counts and
first/last availability cover the retained archive. Known archive gaps are included.

Catalogue membership is not proof that an offer was available to the player.
Localized descriptions are not live prices, eligibility or authoritative reward
values. Same-title variants are candidates for investigation, not interchangeable
purchases. Recording on one client does not cover another Windows login or establish
account identity. The source archive is never modified by the watcher or report.

Run `python -m unittest discover -s tests -p 'test_claim_offer_watch.py'` locally.

### Cross-client selection differences

A choice can differ between two clients using the same account. Keep this
separate from a choice reverting within one client: compare the same item and
quantity, client versions, the last confirmed edit, native saved-list write,
cloud-load state, save attempt and later native read on each client. Local
fallback or migration, an older in-memory snapshot and competing cloud saves
are hypotheses until that timeline distinguishes them. The existing probe sees
the manager's native read/write result and cloud flags, not an independent
authoritative server snapshot. Do not force a cloud save, reset local preferences
or resolve the conflict automatically while collecting evidence.

Static inspection of Windows client272 confirms that `GetStringList` tests
`_isCloudFileLoaded`: the loaded path calls `MigrateStringListFromPlayerPrefs`
before returning the in-memory saved data; the other path uses local `UserPrefs`
when available. The migration checks a legacy bare key, then an account-prefixed
key, reads a nonempty local list, and calls `SetStringListData` even when the
destination preference already exists. That is a concrete way local state can
replace the loaded in-memory list. It does not prove this path caused any reported
reset or that the resulting value was subsequently stored on the server.

## One-shot UI failure test

While a chest summary is open, refresh health, then arm explicitly:

```powershell
python scripts/claim-capture.py status --game-dir <runtime-log-directory>
python scripts/claim-capture.py arm-ui-failure --game-dir <runtime-log-directory>
python scripts/claim-capture.py cancel --game-dir <runtime-log-directory>
```

The arm is tied to the current session and summary generation. For60 seconds at
most, it can reject one `TriggerSectionChange` call made synchronously inside
that summary's `DoContinue`. It is consumed once, cancelled on hide/rebind, and
does not affect unrelated navigation. The injected event is explicitly marked.
It never manufactures a network failure, changes rewards, clears locks, or
automatically presses a claim or navigation button. The next human action runs
normally. This tests evidence collection; it does not reproduce server-side
delays, stale offers, or prove the cause of naturally stuck input.

## Rejected navigation and experimental recovery

Capture records the section manager's current/next/previous sections, transition
busy flag and coroutine identity before and after summary navigation. Rejections
include bounded native caller locations. A shared navigation ID joins the inner
transition result to the outer reward-return result, explicitly marking cases
where the inner call rejects but its parent returns success.

The feature defaults off and does not control hook installation:

```toml
[ui]
claim_navigation_recovery = false
```

When enabled, exactly one rejected transition inside one successful native reward
return can schedule a retry for that summary generation. The recorder must be
healthy, the same summary/context and origin must remain bound, and native section
state must be known. It waits at least 250 ms and observes 100 ms of idle navigation,
with a 5-second deadline. Changed context, new input/navigation, a message box,
disabled configuration or a changed section cancels it. A consumed/cancelled
budget cannot be replenished by another click on the same summary.

The retry calls the game's existing return-from-rewards routine, retaining its
own back-navigation gates. The transition hook also rejects any recomputed
destination or flags differing from the initial request. No forced transition,
lock clearing, claim submission, reward mutation or retry loop is added.

`navigation_recovery_*` events record scheduling, observations, cancellation and
the one attempt. An accepted transition is distinguished from eventual summary
hide, which records the recovery's navigation ID. `cancel` cancels both a pending
recovery and the artificial rejection arm. Status includes feature/pending state.
The UI fault test is suitable for checking this path: with recovery enabled, one
human DONE press should reject once, then close through the guarded retry.

## Qualification

The65 current Windows x64 client272 entries have inspected native space for the
SPUD overwrite. Runtime resolution checks callback ABI and rejects ambiguous
overloads. macOS shares the implementation, with platform-specific process,
caller-stack and focus handling. Native fit and player smoke on ARM64/Intel Mac
remain unverified until those exact binaries and testers are available. A build
or successful Windows test does not establish Mac hook fit.

Local policy test: compile `tests/claim_ui_fault.cc` with `mods/src` on the include
path. It covers one-shot consumption, unrelated contexts, expiry and cancellation.
Run the Python helper tests locally; these tests are not added to CI.
`tests/claim_navigation_recovery.cc` exercises delayed one-shot retry, idle waiting,
expiry, changed/unknown context and cancellation without a game client.

## On-screen incident notice

The play-dev capture displays a persistent, click-through box below the console
area after a recorded summary-navigation rejection, native claim timeout, or
manual incident marker. It stays visible across screen changes until cleared or
restarted. Rejections are amber. A green "Claim recovered" notice requires the
same summary to hide after an accepted automatic retry within five seconds;
manual navigation and new summaries cancel that attribution. It reports UI
recovery, not server-side reward ownership. Artificial rejections show `(TEST)`.

The notice uses the existing native overlay without waking the debug console or
its diagnostic producers, including in release builds. No new game hooks or
claim requests are added. `preview-notice` tests only the box and explicitly
labels it as a display preview; `clear-notice` removes it. Both commands use the
same current-session control protocol as `status`, for example:

```powershell
python scripts/claim-capture.py preview-notice --game-dir "C:/Games/Star Trek Fleet Command/default/game"
python scripts/claim-capture.py clear-notice --game-dir "C:/Games/Star Trek Fleet Command/default/game"
```

Compile/run `tests/claim_notice.cc` with `mods/src` on the include path to check
accepted-vs-hidden attribution, synchronous/asynchronous hide order, generation,
manual cancellation, rejection, expiry and test labeling. Visual placement still
needs an exact-artifact check after a cycle.

## Pre-summary presentation stalls

The 2026-10-19 Transporter Patterns/Kang incident reached Succeeded/Granted with
52 shards in the purchase-order receipt, then remained Presenting without a
summary. Unity logged a null reference in
`ShopCinematicDirector.OnSectionEnterPrepareActivate`. The player escaped with
Q (ShowStationInterior), but the last observed claim was still Presenting.
Screen navigation therefore does not prove claim completion or inventory balance.

The existing 65 hooks now also observe navigation while the latest claim is
Succeeded/Granted/Presenting without a summary. After 15 seconds in the same
known section state, with no bound reward/chest context, capture records one
`presentation_stall_observed` event and an amber notice. This is a possible stall,
not proof of an exception: slow cinematic loading may also meet the conditions.
The snapshot reads available cinematic directors, component/context presence,
selected/transaction bundle aliases and metadata/asset presence. Missing data is
explicit. These are observation-time snapshots, not the throwing stack's locals.
No new detours or hook-fit claims are introduced.

Status exposes `presentation_watch` with the current local claim key, stall and
one-shot escape state. An explicit operator command can try the same ordinary
non-forced station-interior navigation as Q:

```powershell
python scripts/claim-capture.py status --game-dir <game-directory>
python scripts/claim-capture.py escape-presentation --game-dir <game-directory>
```

This requires enabled `ui.claim_navigation_recovery`, healthy capture, the same
claim from fresh status, unchanged section state, no summary/reward context,
idle native navigation and no capture/popup/focused text/mouse/Escape input ownership. The
budget is consumed before the native call, even on rejection or exception.
It never runs automatically, forces navigation, clears locks, submits claims or
changes reward state. Acknowledgement `ok` means the attempt ran; `accepted`,
`escaped_now` and `presentation_terminal` are separate observations. Asynchronous
navigation may not have completed when the acknowledgement is written.
`presentation_stalled_section_left` records leaving the stalled section,
including manual escape. An accepted navigation retires eligibility immediately,
even before the next sampled section observation. Presentation terminal state is
distinct from active-claim removal and inventory balance. Leaving the stalled
section permanently retires this incident's escape eligibility even if its claim remains Presenting, so later gameplay is not a
recovery target. No green recovered notice is produced for this path.

Preserve the Unity exception stack alongside claim journals when capturing:

```powershell
python scripts/claim-capture.py snapshot --game-dir <game-directory> --player-log <Unity-Player.log>
```

Local tests: `tests/claim_presentation_watch.cc` covers timed observation,
navigation changes, single-attempt consumption and retirement after escape;
`tests/test_claim_capture.py` covers command preflight and item/Unity log snapshots.
Windows build/fixtures do not qualify macOS builds or runtime behavior. The
shared implementation is enabled on Windows and macOS; Mac builds and runtime
remain unverified. Exact-artifact escape smoke remains
required before claiming that the new command recovers this failure.
