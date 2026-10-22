# Main-thread discovery audit

Periodic scene-wide discovery can stall the Unity owner thread. Retrying a
missing object at a short interval also makes the cost persist during ordinary
gameplay. A client-272 capture identified this pattern in AAI report discovery;
it did not establish why older performance issues worsen with session age.

This cleanup changes these active paths:

| Path | Discovery boundary |
| --- | --- |
| AAI report entry, in its owning repository | Native report section and visible-canvas component lookup; no scene-wide fallback. |
| Galactic Anomaly timer | System-section HUD lookup below the visible canvas; drawer lookup within the existing UI root; read the existing native singleton rather than search for it. |
| Swap-ship technology indicator template | Prefer the already tracked Ship Management owner's serialized token selectors. Reuse a weak, liveness-checked template; cold global fallback is limited to once per selection context. Preserve the tile-source fallback and cheap late-owner retries. |
| Active navigation-background experiment | Reuse the selected loader while valid. Scenery bounds refresh is spread across frames rather than reading the whole pool in one frame. Rediscovery after loader loss is limited to once per five seconds; clear artwork owned by a destroyed loader. |
| Haven road depth and water | Share a weak owner and bounded relevant material/renderer cohorts. Steady passes avoid hierarchy discovery. A shared scoped refresh every five seconds catches streaming and material replacement; missing-owner global lookup uses the same backoff. |
| Object tracking | Index class membership by object identity, reject duplicate registration, remove only from owning class buckets, and compact a liveness-removal batch in one pass. Preserve insertion order and the existing native finalizer ABI. |
| Claim diagnostics | Keep native snapshots/actions on their owner thread. Queue owned diagnostic data into one bounded worker for serialization, journal flushes, checkpoint persistence and control-file I/O. Recovery/escape actions wait for the exact flushed event receipt and revalidate native state. |

Native destruction is checked even when a managed wrapper remains. Unity work
remains on the main thread; moving Unity APIs to workers is not a safe remedy.
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
