# SYNC-BATTLES — journal acquisition, names and retry ownership

Contract v1, 2026-11-02. Existing `play-dev-ripper`, clean starting head
`89258418eb32f03a83a6226f294b412107197af6`. Frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Authorized scope is the existing
battle sync workflow and its target-interest correction. AAI and new upstream
features remain deferred. No live sync configuration or account requests are needed.

| Inspected retained local ref | Exact revision | Presence |
| --- | --- | --- |
| upstream/dev | e884efa7c6ff66d0776b4cd4d6b76ce193296be5 | Original sync journal/config domain in `mods/src/patches/parts/sync.cc`. |
| origin/play | cad98ef7990dd66167eb64ea5dda525c1fe2c296 | Same existing acquisition and participant-resolution domain. |
| origin/play-dev and frozen/play-dev-20261026 | 1baa55fedf1758b19f843cc59707b8901a0b4998 | Selected sync owner, bounded resolver helper and session header owner. |
| refactor base | 89258418eb32f03a83a6226f294b412107197af6 | `src/mod/sync/battlelogs.cc`, shared RecordStore/Delivery/InputHealth and immutable Config snapshots. |

These identities are observations of retained refs, not fresh remote-head claims.

## Source workflow and obligations

Supported battle headers queue journals only when battle sync is selected.
Each queued job captures the reporting session and owns one pending generation/ID
key until completion, cancellation, exhaustion or queue eviction. The worker
fetches the journal and optional missing player/alliance names through its existing
CPR session, then publishes a canonical journal for per-target delivery. The existing
name caches are bounded, mutex-protected and fenced by session generation.

| ID | Frozen source evidence / retained decision | Target obligation |
| --- | --- | --- |
| B1 | `sync.cc:2890–2907,2595–2601` selects battle consumers; `594–600` refuses every fetch with no target. | Use current `options.battlelogs`, the union of actual target categories from `config.cc:1042–1047`. Recheck after backoff and before each journal/player/alliance POST. A resources-only target must not authorize battle acquisition. |
| B2 | `sync.cc:609–635` and `sync_session_headers.h` retain the reporting login and reject stale generations. | Preserve captured URL/session ID and matching current version/instance, finite deadlines and ordinary session fencing. Disinterest is cancellation, not input loss or a generation seal. |
| B3 | `sync.cc:1197–1217,1325–1332` requests `/journals/get` with journal_id and emits journal/names. | Preserve endpoint/body, canonical wire record and successful journal cache. Already-issued requests may finish; disabling cannot retract HTTP. |
| B4 | `sync.cc:817–843,1224–1227` identifies fleet/cracked-station participants and excludes NPC IDs. | Preserve both participant sources, safe missing/null sides, optional name resolution and existing IDs/null-name fallback. |
| B5 | `sync.cc:856–899,1229–1322` resolves player/alliance profiles. | Keep request shapes and local results; stopping interest prevents subsequent name requests without discarding a successful journal already acquired. |
| B6 | `sync.cc:773–815` and frozen resolver primitive bound retained names and TTL. | Preserve 4096 entries, 256-byte fields, generation fence and fresh-result use when TTL is nonpositive. Passive profile enrichment stays optional. |
| B7 | `sync.cc:1534–1575` preserves header order/deduplication. Current accepted retry is stronger than frozen dropped failures. | Retain reverse admission, per-job pending key, recent-record dedup, 10 attempts and bounded exponential delay. Canceled uncached work releases its own key; a later interested header may re-admit it. Do not globally clear pending when config changes. |
| B8 | Existing M8 restart decision supersedes frozen `sync.cc:926–981,1551–1564` attempted-ID suppression. | Leave legacy file/path untouched; no queue admission becomes delivery ACK or a permanent session blacklist. New sessions and retained per-target delivery owners remain unchanged. |
| B9 | Existing worker contains terminal exceptions rather than terminating the game. | Keep process-lifetime owners and the existing ContainWorkerExit boundary. A private one-item function factors the unchanged loop body for direct local transport qualification, without adding a scheduler or public worker lifecycle. |

## Correction boundary

The port checked battle interest only before retry waiting. The predicate watched
session health alone, so target removal could wake the worker yet still leave it
waiting until the deadline, followed by a journal request. Each request itself
also checked only session health. The existing immutable snapshot/category union
and frame-owner notification are sufficient to observe the new intent.

Preserve useful successful records and names. Cancel only work that no longer
has a consumer before acquiring more data; do not seal ordinary sync, wipe caches,
invent target/account identity, or retract a request already issued. A failed
request must not keep requeueing solely because the session remains open after
interest is removed. Canceled pending ownership is released normally.

## Verification and limits

The [cohort](../FEATURE-COHORT-20261102-SYNC-BATTLES.md) binds before/final tests,
exact inputs, signing and review. The existing uploader fixture includes the
actual battle acquisition/one-item code and uses the real CPR transport against
an owned synthetic loopback listener. Test-only session/store wrappers replace
the game-bound host; no infinite detached worker or real game-server data is used.
Its deterministic wait/response barriers qualify request and ownership ordering,
not native callbacks, production FollowSettings scheduling, real receiver behavior
or current Mac runtime behavior. Keep those claims separate and overall parity open.
