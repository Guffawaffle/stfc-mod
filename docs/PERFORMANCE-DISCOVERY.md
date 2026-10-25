# Main-thread discovery audit

Periodic scene-wide discovery can stall the Unity owner thread. Retrying a
missing object at a short interval also makes the cost persist during ordinary
gameplay. A client-272 capture identified this pattern in AAI report discovery;
it did not establish why older performance issues worsen with session age.

This cleanup changes these active paths:

| Path | Discovery boundary |
| --- | --- |
| AAI report entry, in its owning repository | Native report section and visible-canvas component lookup; no scene-wide fallback. |
| Galactic Anomaly timer | Discover the HUD only in system navigation and retain the live Unity object; retry missing-owner discovery at most once per second. Find the drawer only when creating the panel. Native HUD canvases can live outside the top interaction canvas and its transform root. Read the existing anomaly singleton rather than searching for it. |
| Swap-ship technology indicator template | Prefer the already tracked Ship Management owner's serialized token selectors. Reuse a weak, liveness-checked template; cold global fallback is limited to once per selection context. Preserve the tile-source fallback and cheap late-owner retries. |
| Active navigation-background experiment | Reuse the selected loader while valid. Scenery bounds refresh is spread across frames rather than reading the whole pool in one frame. Rediscovery after loader loss is limited to once per five seconds; clear artwork owned by a destroyed loader. |
| Haven road depth and water | Share a weak owner and bounded relevant material/renderer cohorts. Steady passes avoid hierarchy discovery. A shared scoped refresh every five seconds catches streaming and material replacement; missing-owner global lookup uses the same backoff. |
| Object tracking | Index class membership by object identity, reject duplicate registration, remove only from owning class buckets, and compact a liveness-removal batch in one pass. Preserve insertion order and the existing native finalizer ABI. |
| Claim diagnostics | Keep native snapshots/actions on their owner thread. Queue owned diagnostic data into one bounded worker for serialization, journal flushes, checkpoint persistence and control-file I/O. Recovery/escape actions wait for the exact flushed event receipt and revalidate native state. |

Native destruction is checked even when a managed wrapper remains. Unity work
remains on the main thread; moving Unity APIs to workers is not a safe remedy.
Temporary Haven discovery arrays and mutation inputs are strongly rooted across
native calls; persistent scene cohorts retain only weak handles.
No new native hooks are installed. Haven discovery retains no more than 256 road
materials and eight water renderers. Object-tracker membership contains only
identity/metadata pointers and leaves with the object. The claim worker charges
queued and in-flight payloads against both item and memory limits; queue loss
or sink failure is visible and closes diagnostic-dependent action gates.

Five-second Haven refresh is a correctness tradeoff, not a claim that discovery
cost is zero: late streamed content may wait for the next scoped pass. Cold
artwork discovery is still possible when no supported native owner is available.

The audit also found calls intentionally left outside this change:

- The claim cinematic snapshot searches only when recording an explicit stalled
  presentation observation, rather than on every claim-monitor tick.
- AAI thumbnail-index discovery occurs on an explicit artwork request, not in
  its per-frame loading poll.
- `navigation_environment.cc` is a separate, unreferenced implementation; the
  active orbit integration calls `navigation_environment_poc`.
- Existing native singleton access elsewhere may have its own discovery cost.
  Changing the shared singleton getter without caller-specific evidence could
  change initialization behavior and is not part of this patch.

Validation should include report open/close/reopen, anomaly display, swap-ship
indicators, Haven water restore, and system/camera/loading transitions with the
background experiment active. Compare the same gameplay and artifact identities
before claiming reduced stutter. Windows builds, portable fixtures, Mac builds,
and actual runtime measurements are separate evidence.

The isolated development build selects the sibling AAI cleanup worktree using
the existing `stfc_aai_source` option. This host branch does not change the
published AAI pin or include another agent's uncommitted evidence work.
The second cleanup and feature sweep exclude AAI implementation work.

## Bounded sync work after the sweep

The follow-up sync cleanup replaces a detached parser thread per entity/RTC
payload with one ordered owned-payload worker. Jobs and away-assignment parsing
join that FIFO, so their ordering no longer requires parsing on the native
response callback. Disabled categories bypass payload copies. Native profile
reads remain on their original callback; worker tasks retain owned bytes only.

| Stage | Retention budget, including copying reservations and in-flight work |
| --- | --- |
| Owned payload parsing | 128 entries / 32 MiB, maximum 16 MiB charged per entry |
| Ordinary delivery dispatch | 256 entries / 32 MiB, maximum 16 MiB charged per entry |
| Each configured upload target | 256 entries / 32 MiB, maximum 16 MiB charged per entry |
| Combat downloads | 4,096 fixed-size ID/generation jobs |
| Diagnostic dispatch | At most 64 observations / 4 MiB within the dispatch budget; observations also retain their existing 256 KiB size guard |

These are retained-payload budgets, not a total process RSS cap. Parsing,
serialization, HTTP response storage and transport copies use temporary memory.
Upload bodies are shared between targets and each target conservatively charges
the full body. CPR's retained upload body is cleared before its in-flight charge
is released. Both upload and game-server download sessions use a three-second
connection timeout and ten-second overall request timeout, including debug builds.

Admission reserves a FIFO position and memory before copying bytes. Copies occur
outside the queue metadata lock; the consumer waits for earlier reservations
even if a later copy completes first. A slow HTTP target cannot hold this lock
or block the game callback. The number of parser/download/dispatch workers is
fixed, plus one upload worker per configured target. Workers and their queues
have process lifetime; this introduces no live-DLL-unload or loader-lock join.

Ordinary state includes incremental updates and producers' de-duplication state,
so arbitrary coalescing or dropping an old snapshot would corrupt the stream.
An admission failure visibly latches ordinary input closed, while already
admitted work drains. A target backlog failure closes only that target. A fresh
game process is required to resume the affected stream; it is not resumed merely
because the queue shrinks. Diagnostic overflow drops observations and leaves
ordinary admission intact. Warnings include the affected stage or target;
target warnings report retained/high-water counts, bytes and oldest age.

Resolver caches opportunistically prune expired identities during background
activity and retain at most 4,096 player/alliance records per cache. Retained
IDs/names/tags are limited to 256 bytes per field; oversized values may still
appear in the current response but are not retained in the cache. LRU eviction
keeps identity churn bounded even when records have not reached their TTL.

Game-server metadata is copied under a short publication lock, released before
HTTP. Queued payloads, combat jobs and upload bodies carry a login generation;
stale work is rejected before it can use the next login's headers. A login change
after ordinary sync begins explicitly quarantines ordinary sync until restart,
because its process-lifetime de-duplication/first-sync state has no full replay
reset. Diagnostics remain best effort. An HTTP request already in progress cannot
be recalled; an obsolete game-server response is discarded after completion.

Transport delivery remains best effort. This is not a durable outbox, an HTTP
acknowledgement/retry protocol, or a guarantee that restarting replays historical
battles. Battle IDs are marked seen only after combat-queue admission, but their
existing persisted seen history still precedes successful fetch/upload.

The queue fixture covers reservation order, active-work accounting, failed
copies, handler exceptions, stalled independent consumers and stop/drain. It
does not invoke native game callbacks or a real HTTP endpoint. Actual gameplay,
HTTP timeouts and session transitions require artifact-specific runtime testing.
The [sweep tracker](PERFORMANCE-SWEEP-TRACKER.md) records upstream attribution and
the other findings separately.


## Remaining sweep cleanup

The next batch bounds typed preference saves and selection capture, retires stale
galaxy weak entries independently of zoom, and replaces borrowed tracker spans
with rooted bounded snapshots. Provisional native objects have failure cleanup;
interior range ownership is weak, bounded and restorable. Discovery tow uses
Unity coroutine scheduling rather than draining yields in one update. Image
checks and pure sky generation use fixed bounded workers; native allocation/upload
stays on the owner. Artwork qualification uses ordered five-candidate frame
budgets; unchanged debug history is not recopied.

The tracker preserves all 24 IDs and upstream extraction boundaries: 20 source
fixes and four bounded P3 candidates needing profiling. Builds/fixtures do not
replace gameplay, Mac architecture builds or measured frame-time evidence.
No AAI changes or runtime deployment are part of this cleanup integration.
