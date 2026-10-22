# Performance sweep tracker

This tracks the 24 follow-ups from the performance sweep at
`f7d36e764183d2f0c2114ad38db5b7d8450fa6fe` on
`perf/main-thread-discovery`. Twenty findings are **Implemented**;
four bounded P3 candidates remain **Profile first**. Implemented records local source/fixture validation,
not deployment or gameplay qualification. The earlier discovery cleanups do not
close these residual findings. Priorities describe the source audit, not a measured
explanation of the reported stutter or session memory growth.

## Comparison baseline

- **Downstream audit:** `f7d36e764183d2f0c2114ad38db5b7d8450fa6fe`.
  Source report: `artifacts/performance-20261021/performance-sweep.json` in the
  shared workspace. Implementation may advance beyond this pinned comparison.
- **Upstream comparison (U):** local remote-tracking `origin/dev` at
  `5e726547452576efa414a1ee87e645187fbf2af9`, tagged `v1.1.9.beta.4`.
  These are committed blobs, read with `git show`/`git grep` and compared directly
  against the downstream commit. No fetch was performed; this is not a claim
  about newer remote commits or current PR status.
- **Canonical upstream checkout:** `D:/dev/netniv/stfc-mod`, clean when inspected
  on 2026-10-22. Its checked-out branch was the unrelated
  `science/battle-log-summary-block` at
  `33994ccfcba81ee2f0ca4627915ea41cf14f306e`; that HEAD is **not** U. Local `dev`
  was `6ceb892baade31d907071db7bf72374902c8e668` and was not substituted for U.
  Remotes were `origin=https://github.com/netniv/stfc-mod.git` and
  `guffawaffle=https://github.com/Guffawaffle/stfc-mod.git`. No upstream source,
  refs, configuration, or canonical reconciliation audit were changed.

Below, **D** means the pinned downstream audit commit; **U** means the exact
upstream commit above. Line references belong to their labeled version.
**Upstream/shared** means the relevant mechanism exists in U. **Downstream-only**
means this particular feature/path or defect is absent from U, not that a missing
upstream feature needs a fix. **Mixed** separates shared machinery from fork-only
callers or extensions. None of the 24 has been established as already fixed in U.

## Sync: first implementation batch

| ID | Priority | Status | Ownership | Committed evidence and extraction boundary |
| --- | --- | --- | --- | --- |
| `sync-unbounded-backlogs` | P1 | Implemented | Mixed: ordinary machinery upstream/shared; Diagnostics extension downstream-only | D `mods/src/patches/parts/sync.cc:449,914,925,1321`. U same file `277,438,893,897,903,914` has unbounded target, dispatch and combat-ID queues. U `385-388` excludes target deadlines in `_MODDBG`; `454-491` creates the separate downloader without explicit deadlines. Extract ordinary queue count/byte admission, deadlines and observable loss policy; adapt the downstream Diagnostics-only bound separately. Preserve first-sync, delta and combat-log semantics rather than silently dropping arbitrary state. |
| `sync-per-payload-thread-fanout` | P1 | Implemented | Mixed: shared detached dispatch; fork-specific categories/order adapters | D `sync.cc:2505,2752,2782`. U `sync.cc:2484-2503` copies bytes and detaches a thread per entity group; `2691-2692` admits generic JSON without category filtering; `2710-2734` copies RTC JSON and detaches another thread without a slots gate. A bounded owned-payload dispatcher and early category admission apply upstream. Do not carry downstream Diagnostics registration, synchronous `submit_ordered`, or unrelated fork configuration into the extraction. |
| `sync-resolver-ttl-retains-unique-identities` | P2 | Implemented | Upstream/shared | D `sync.cc:688,713,763-795`. U `sync.cc:655-659,677,702` retains player/alliance maps; `749-802` expires only the identity currently looked up. TTL alone does not retire forgotten identities. Extract bounded count/byte retention and incremental expiry with session-aware ownership. Validate distinct identities that are never looked up again. |
| `sync-active-missions-race` | P2 | Implemented | Upstream/shared | D `sync.cc:1765-1790`. U `sync.cc:1763-1778` replaces the set under a lock, then checks/iterates it after unlocking; `2610-2613` can dispatch concurrent detached mission jobs. An owned emission snapshot or ordered worker fixes the shared issue. Validate interleaved generations without holding locks through serialization or delivery. |
| `sync-ordered-parse-on-callback` | P2 | Implemented | Downstream-only defect; shared processor bodies | D `sync.cc:2518-2533` adds `submit_ordered`; D `2541,2546,2665` uses it for away snapshots/updates and jobs. U `sync.cc:2509-2517,2603-2605` dispatches these asynchronously and has no `ordered_dispatch` helper. Move the fork's ordering requirement into the bounded dispatcher. Do not describe U as already blocking those callbacks. U still serializes JSON under its dispatch mutex at `914`; that shared lock-scope improvement can be extracted independently. |
| `sync-session-header-race` | P2 | Implemented | Upstream/shared | D `sync.cc:128-130,493,531-535,2814-2828`. U `sync.cc:125-129` stores shared metadata; `477-488,506,520-521` reads it on downloader work; `2754-2768` writes it from hooks without a common lock. Extract coherent immutable session/header snapshots and stale-generation rejection. Do not hold a metadata mutex across HTTP. Test relogin while work is blocked; actual overlap was not observed by the source audit. |

### Sync implementation receipt

The first cleanup batch replaces per-payload detached threads and callback-side
ordered parsing with a single bounded FIFO, bounds dispatch/target/combat
retention including in-flight work, and applies finite upload/download deadlines
in every build mode. Overload quarantines ordinary input or the affected target;
accepted work drains and diagnostics remain best effort. Mission emission reads
its owned snapshot. Resolver retention is capped with expiry/LRU and field-size
limits. Session snapshots remove shared-string races; generation guards plus
explicit ordinary-session quarantine prevent silent resume with stale
first-sync/de-dup state. This does not add full relogin replay or reliable battle
upload acknowledgements.

Portable optimized checks (assertions remain active in these fixtures):

```powershell
clang++ -std=c++23 -O2 -DNDEBUG -Wall -Wextra -Werror -pthread -Imods/src tests/sync_bounded_queue_test.cc -o build/sync_bounded_queue_test.exe
./build/sync_bounded_queue_test.exe
clang++ -std=c++23 -O2 -DNDEBUG -Wall -Wextra -Werror -pthread -Imods/src tests/sync_retention_session_test.cc -o build/sync_retention_session_test.exe
./build/sync_retention_session_test.exe
```

Both passed after formatting, as did `git diff --check`. Independent source
review covered queue admission, native-byte ownership, diagnostic isolation,
worker lifetime and the session/quarantine correction. The native Windows build
receipt, exact commit/artifact identity and platform/runtime limits are recorded
in the shared workspace's `artifacts/performance-20261021/sync-cleanup.json` once
validation completes. Portable fixtures use simulated sinks, not real CPR/native
hooks. No live runtime result or Mac build is implied by these statuses.

### Sync provenance and upstream extraction

U `mods/src/patches/parts/sync.cc` has Git blob
`bba2c6a2ca51df839c367cfcbb9f908c6535ffd2`; the audited D blob is
`81209f27b46e3047666f60b0ff2946a1554158a6`. The direct U-to-D diff is 134 changed lines. The latest
commit affecting this file in U is
`fd360dea4fccc55b661d394eac5789df833cf49c`, 2026-10-15,
“Re-emit job events when start, duration or reduction changes (#327)”. These facts
identify the compared source, not authorship of every inherited helper.

| Shared upstream component | U source evidence | Fork delta to keep separate |
| --- | --- | --- |
| `TargetWorker`, target map and worker lifecycle | `sync.cc:266-413`; request string ownership, queue mutex/condition variable, per-target worker and HTTP session already exist. | D adds a Diagnostics-only 256-item rejection before ordinary target admission. Upstream has no Diagnostics category/publisher at U. |
| Ordinary admission and dispatch queues | `sync.cc:415-452,888-918`; ordinary enqueue and JSON dump under `sync_data_mtx` are inherited. | `PublishDiagnosticEvent` and its envelope/schema/size checks are downstream additions. A general queue helper must not require this feature. |
| Download session and metadata | `sync.cc:454-550`; download-session setup and worker `client_mutex` are inherited. Metadata fields and hooks are at `125-130,2749-2769`. | Preserve ordinary endpoint/auth behavior. The downloader mutex does not protect hook writes; a new session snapshot must cover both paths. |
| Per-payload parsing threads | `sync.cc:2484-2503,2691-2692,2710-2734`; byte ownership exists but admission/concurrency is unbounded. | D adds synchronous ordering for three cases. Port the shared dispatcher against U's async callers; adapt the fork's ordered callers separately. |
| Resolver/mission state | `sync.cc:655-708,749-802,1746-1783`; maps, lookup-only TTL and unlocked mission emission are inherited. | No need to import fork feature categories to repair these containers. |
| Haven local-owner validation | U `sync.cc:1559-1595` invokes profile-manager/managed methods inside `planetary_map`, reached through `submit_async` at `2589-2591`. | D deliberately captures an owned local-user ID on the native callback and passes it to the worker. Preserve that ownership correction in this branch. A focused upstream dispatcher must not move more Unity/IL2CPP work onto workers; consider the existing U owner-read defect as a separately evidenced dependency, not a reason to import all fork runtime helpers. |

Candidate upstream sync work should be prepared against a freshly verified
upstream base when requested. Keep shared queue/worker policy independent of
downstream Diagnostics, claim science, branding, isolated profiles and orbit/UI
features. Include only helper code actually needed for that fix. This tracker
does not authorize publication, revive closed PRs, or establish a current PR's
state. Local fixtures and platform validation remain necessary; do not add CI or
root XMake test registration merely for these fixtures.

## Other upstream/shared and mixed paths

| ID | Priority | Status | Ownership | Committed evidence and extraction boundary |
| --- | --- | --- | --- | --- |
| `native-tracker-borrowed-span-invalidation` | P2 | Implemented | Mixed: shared unsafe API and callers; additional downstream callers/index cleanup | D `mods/src/il2cpp/il2cpp_helper.h:374`, `mods/src/patches/parts/hotkeys.cc:484`. U helper `374-377` returns a borrowed span over mutable vector storage without a reader lock/root. U `object_tracker.cc:38-72,94,120,134-175,178-201` mutates it under writer locks; U hotkeys uses `GetAll` at `356,380,418,470,581,624,648,672,1202,1224,1287,1307,1444`. Provide a bounded protected snapshot, release its lock before UI invokes, and handle object lifetime. Extract shared consumers first; mining-preview and additional fork consumers are separate. Initial reverse-index cleanup does not repair the reader contract. Finalizer thread identity remains unverified. |
| `native-tow-coroutine-drained-in-one-update` | P2 | Implemented | Upstream/shared; checked iterator wrapper differs | D `mods/src/patches/parts/hotkeys.cc:895` drains checked `TryMoveNext`. U `hotkeys.cc:837-845` calls `PlanCourse`, then `while (plannedCourse->MoveNext())` before tow submission. Both consume a potentially yielding iterator in one update. Use supported scheduling or one rooted pending action with bounded advancement/cancellation; preserve successful completion before tow request. Do not import unrelated fork hotkey changes. Native iterator yield behavior needs validation. |
| `native-interior-range-cache-lifetime` | P2 | Implemented | Upstream/shared; downstream hook ownership differs | D `mods/src/patches/parts/interior_zoom.cc:85`. U same file `41-44,75-110` retains raw-address `g_native` entries after restoration with no retirement/cap. The relevant cache body is unchanged by D's shared Haven/interior detour adapter. Extract provider/constraint identity and restoration lifecycle repair; retain U's installer rather than bringing in the fork's Haven orbit hook. |
| `native-loading-overlay-failed-construction-retains-object` | P2 | Implemented | Upstream/shared; fork artwork fallback differs | D `mods/src/patches/parts/loading_screen_common.h:474`. U same file `433-486` constructs/root-holds a GameObject, then frees only its managed handle on post-construction failures, including after parenting. Destroy provisional native objects until publication. U-to-D changes here only add a custom-background warning and `_USE_ORIGINAL_BG` fallback guard; omit those from the cleanup extraction. Validate repeated failures, rather than claiming the success path leaks. |
| `native-haven-shared-discovery-refresh-burst` | P3 | Profile first | Mixed: current shared cache is downstream; underlying road feature is upstream | D `mods/src/patches/haven_visuals_cache.h:244` periodically traverses a live owner's hierarchy. This helper and `haven_water.cc` are absent in U. U `haven_road_depth.cc:82-131` instead repeats root `Find` and hierarchy/material reads every second. Measure/amortize the current D residual without claiming U already has this five-second cache. For upstream, extract the relevant road discovery/lifetime fix; water restoration and sharing with a nonexistent upstream water feature are optional dependencies that require justification. |
| `hidden-chat-title-repeated-reflected-ancestor-walks` | P3 | Implemented | Mixed: shared traversal; different installed bind callbacks/platform reach | D `mods/src/patches/parts/chat_titles.cc:17-105` shares U's reflected helpers and two bounded ancestor walks. U `chat_titles.cc:38-47,72,97-105` has the same work, but its Windows installer skips the unsafe bind wrappers and retains `UpdatePlayerTitle`; D changes the bind hook to substantive `SetIdentifiers`. Optimize shared qualification/metadata without reintroducing unsafe wrapper detours or overstating the Windows double-walk trigger in U. Pooled-row/reparent invalidation still matters. |
| `ordinary-window-title-retries-every-update` | P3 | Implemented | Upstream/shared, Windows path | D `mods/src/patches/parts/free_resize.cc:133`. U same file `103,133-139` retries until `File::Title()` is nonempty; U `mods/src/file.cc:76-90` caches a title only with `File::override`; U `mods/src/titlewindows.h:15-22` reads/allocates on each retry. The relevant bodies are shared. Extract an ordinary/custom-name gate and bounded window-availability retry. D's isolated-profile addition elsewhere in `File::Init` is unnecessary for U. Small bounded work, not established heavy stutter. |
| `zoom-repeats-camera-setters-in-steady-system-update` | P3 | Profile first | Mixed: shared repeated setters; fork-only orbit adapter | D `mods/src/patches/parts/zoom.cc:631,728`. U `zoom.cc:340,434` calls `EnsureSystemZoomRange` before/after original update; U `254-256,280` writes clip/clear/background each pass. D adds `ApplyNavigationOrbitDrawDistance` to this setter path. Measure and avoid redundant setters in the shared path while preserving post-native precedence. Do not import fork orbit, galaxy-label, shortcut-badge, settings-persistence or fleet-label ownership changes merely to extract this optimization. |

## Downstream-only feature paths

These implementations are absent from U's committed tree. Absence was checked
with `git cat-file -e U:<path>`, full-tree path inspection, relevant symbol
searches and the direct shared-file diffs. A feature's presence on an unrelated
local branch or in a historically submitted PR does not make it part of U.

| ID | Priority | Status | Ownership | Downstream evidence and upstream boundary |
| --- | --- | --- | --- | --- |
| `state-persistence-blocks-ui-actions` | P2 | Implemented | Downstream-only state layer/callers | D `mods/src/mod_state.cc:72,181-253`; preset reorder and ship-ID pin callers synchronously read/modify/write JSON, with up to about 490 ms of lock retry sleep for `Update`. `mod_state.cc` and the reorder implementation are absent in U. U has name pins and Shift-click pinning; this does not establish the same JSON-state defect in those different callers. Fix the fork state writer without treating all upstream pinning as absent or broken. |
| `selection-diagnostics-full-owner-thread-snapshots` | P2 | Implemented | Downstream-only diagnostic science | D `mods/src/patches/parts/claim_selection_trace.h:148-256,425-523`, `claim_trace.cc:623` builds full snapshots before worker compaction. Those files are absent in U. Keep any bounded/incremental capture correction with this science feature; no upstream diagnostic framework is needed to fix an absent feature. |
| `galaxy-stale-weak-entry-retirement-gated-by-zoom` | P2 | Implemented | Downstream-only galaxy-label controls | D `mods/src/patches/parts/galaxy_labels.cc:187-207,730-742` gates dead-entry pruning behind redraw changes. File/feature is absent in U; existing fleet-label controls in U `zoom.cc` are a separate feature. Repair bounded maintenance in the fork. Actual game-side object churn and retained counts need measurement. |
| `native-navigation-sky-cpu-generation-on-owner-thread` | P2 | Implemented | Downstream-only navigation experiment | D `mods/src/patches/parts/navigation_environment_poc.cc:251`, with deterministic artwork generation in `mods/src/patches/navigation_environment_art.h`. Active orbit integration and this experiment are absent in U. A bounded CPU-generation worker belongs to the fork feature; Unity texture creation/upload must stay on its owner thread. |
| `ship-indicator-orphan-clone-on-name-failure` | P2 | Implemented | Downstream-only technology indicators | D `mods/src/patches/parts/ship_tech_indicators.cc:538-550` misses native cleanup if naming fails after successful instantiation. This indicator implementation is absent in U. Scoped provisional-clone ownership is a fork fix, not an upstream missing-feature defect. |
| `settings-image-path-stat-on-each-edit` | P2 | Implemented | Downstream-only native settings; may also matter to a separately based settings proposal | D `mods/src/settings/upstream_settings.cc:877`, `native/text_entry.cc:153,265`, `image_file_setting.h:74` synchronously stats edited paths. The `mods/src/settings` tree is absent in U. The filename `upstream_settings.cc` identifies the settings category, not evidence of upstream integration. Fix/debounce owned validation here; compare an exact settings-PR head separately before tagging that PR. |
| `debug-console-copies-unchanged-history-per-frame` | P3 | Implemented | Downstream-only debug console | D `mods/src/dev/dev_console.cc:555`, `diagnostics.cc:135,143` copies a bounded ring before the sequence check. The dev-console tree is absent in U. Fix in the debug feature; it is not active in the audited release functionality. |
| `native-navigation-missing-loader-global-retry` | P3 | Profile first | Downstream-only navigation experiment | D `mods/src/patches/parts/navigation_environment_poc.cc:541` retains a five-second missing-owner global lookup. This experiment is absent in U. Event/owner discovery improvement belongs to the fork; preserve late loader arrival and cleanup. Already throttled work still needs measured peak cost before a larger redesign. |
| `native-navigation-procedural-artwork-pool-rescan` | P3 | Implemented | Downstream-only navigation experiment | D `mods/src/patches/parts/navigation_environment_poc.cc:589` can inspect up to 128 pool entries every 30 frames on procedural-background fallback. The corresponding experiment is absent in U. Amortize with content-change correctness; this is separate from the already amortized scenery-bounds pass. |
| `settings-eager-tree-and-repeated-native-label-lookup` | P3 | Profile first | Downstream-only native settings; separately based PR comparison still needed | D `mods/src/settings/native/page_navigation.cc:109,575`, `value_widgets.cc:39,75,85`, `interop.cc:181` eagerly creates pages and repeats bounded catalog/native label work. The settings tree is absent in U. Profile the existing timing seam first; any index/lazy-page fix belongs to this feature or its exact future PR head. No idle/age-driven growth is established. |


## Second implementation batch

Fourteen additional findings have source fixes and focused validation in this
batch. No new hooks or configuration switches are installed. Unity queries,
object creation, texture upload and native UI mutations stay on their owner
thread. Portable checks exercise production helpers; native lifecycle and visual
behavior still require artifact-specific gameplay qualification.

| Finding(s) | Implemented behavior and limits |
| --- | --- |
| `native-tracker-borrowed-span-invalidation` | Replace borrowed mutable spans with at most 128 strongly rooted snapshot entries (32 for technology-owner discovery). Release the registry lock before native calls. Report truncation/root failure and distinguish plain managed objects from Unity native liveness. Separate single-object `Get()` retains its existing raw-pointer lifetime contract. |
| `native-tow-coroutine-drained-in-one-update` | Use Unity coroutine scheduling, preserving nested yields. One pending operation, cancellation, 30-second deadline and 20 Hz completion checks. Require a fresh valid course for the exact fleet/destination before tow submission; unsupported contracts fail closed. Client272 Windows metadata/native control flow informed this contract; actual tow behavior remains unqualified. |
| `native-interior-range-cache-lifetime` | Bound weak provider/constraint/radius ownership to 16 entries. Restore only owned writes on scene exit/scale reset/replacement; share native baselines for aliased data. New providers preserve native behavior at capacity. |
| `native-loading-overlay-failed-construction-retains-object`, `ship-indicator-orphan-clone-on-name-failure` | Scoped provisional native ownership destroys successfully constructed objects/clones on failures before publication. |
| `hidden-chat-title-repeated-reflected-ancestor-walks` | Checked native ancestor query including inactive parents, with existing bounded fallback. No retained pooled/reparentable hierarchy cache or installer changes. |
| `ordinary-window-title-retries-every-update` | Skip title discovery for ordinary launches; custom-title startup retry is throttled to one second. |
| `state-persistence-blocks-ui-actions` | One worker, one replaceable pending value per three typed groups, one in-flight edit; 4,096 entries/1 MiB per edit and 4 MiB retained capacity including in-flight allocations. Preserve OS-locked current-file read/modify/write and unknown keys. Pins compose pending intent but apply only the exact latest saved receipt; failure keeps applied state. Officer/debug edits retain their prior immediate session behavior with accurate pending/failure reporting. |
| `selection-diagnostics-full-owner-thread-snapshots` | Worker-pressure admission before traversal; shared 256-row/512-slot before+after+details allowance, eight observations per rolling 100 ms. Preserve known counts and explicit omissions; partial capture never becomes empty selection or confirmed baseline. |
| `galaxy-stale-weak-entry-retirement-gated-by-zoom` | Fair maintenance independent of zoom: 32 weak checks per four registries and one layout-restoration obligation per callback. Retain failed restoration obligations; no cap on legitimate live widgets. |
| `native-navigation-sky-cpu-generation-on-owner-thread`, `native-navigation-procedural-artwork-pool-rescan` | One pure CPU worker, one active plus one replaceable pending name (256 bytes), fixed 1024x512 RGBA result (2 MiB). Stale generations cannot publish; canceled active work finishes before discarding. Allocate native shell/mesh/texture after pixels are ready. At most five artwork qualifications per frame, rooted ordered cursors across at most 64 loaders/128 pool slots, and identity checks before publication. Continuous identity churn can restart passes; native upload cost remains. |
| `settings-image-path-stat-on-each-edit` | One active filesystem probe plus one replaceable pending owned path, 250 ms debounce, 128 KiB cap and generation rejection. Picker and exact editor validate asynchronously. A blocked stat cannot create more workers; completed validation does not prevent subsequent filesystem changes. |
| `debug-console-copies-unchanged-history-per-frame` | Check ring revision before copying; copy only changed/forced history, including clear/drop changes. Debug functionality is checked separately. |

Focused fixtures: `sync_bounded_queue_test`, `sync_retention_session_test`,
`claim_selection_budget_test`, `galaxy_label_tracking_test`,
`dev_diagnostics_snapshot_test` (`_MODDBG`), `mod_state_writer_test`,
`mod_state_transaction_test`, `image_path_validator_test`,
`image_file_setting_test` and `navigation_environment_work_test`. All ten passed
with optimized `-DNDEBUG -Wall -Wextra -Werror` builds after formatting (the
existing image fixture additionally suppresses missing-field initializer warnings).
Checks remain active under NDEBUG. The transaction fixture uses two real child
processes and an isolated file to check OS locks, current-file RMW, unknown keys
and schema refusal. No CI/root XMake test registration is added.

Integrated Windows release/releasedbg artifact identities and the integration
receipt are recorded separately in the shared workspace's
`artifacts/performance-20261021/sweep-cleanup.json` after builds finish.
Gameplay and Mac architecture qualification remain separate. AAI is excluded.

### Four remaining profiling candidates

- `native-haven-shared-discovery-refresh-burst`: the scoped five-second array
  query is bounded; a streaming-safe incremental replacement needs measured
  timing and verified native semantics.
- `native-navigation-missing-loader-global-retry`: the five-second cold fallback
  supports late loader arrival; no verified cheaper owner exists.
- `zoom-repeats-camera-setters-in-steady-system-update`: pre/post-native setters
  preserve precedence. Skipping them needs timing/invalidation evidence.
- `settings-eager-tree-and-repeated-native-label-lookup`: bounded catalog work
  occurs on settings open. Lazy construction/search redesign needs measured UI
  cost rather than an assumption of idle or session-age growth.

These remain open for measurement. No fixed or deferred path is established as
the cause of the reported stutter. Upstream tags and comparison baseline remain
unchanged; extract focused fixes against a freshly verified base when requested.

## Earlier six-candidate cleanup: separate completed implementation

The first follow-up audit at `0de98a97856fe648a3e928f9a27b9bf5026969b2`
identified these six candidates. Their implementation is already committed in
`bac95842548855b5f23279d922d04699df958726`, with the Haven temporary-root lifetime
correction in `f7d36e764183d2f0c2114ad38db5b7d8450fa6fe`. “Implemented” below
does not mean deployed, runtime-qualified, or submitted upstream.

| Initial candidate ID | State at D | Attribution and extraction caveat |
| --- | --- | --- |
| `haven-road-depth` | Implemented; scoped-refresh residual remains Open above | Upstream/shared original: U `haven_road_depth.cc:82-131` repeats one-second root/hierarchy/material discovery. Extract a road-focused lifetime/content-refresh fix against U; justify any shared helper separately from downstream water behavior. |
| `object-tracker-cleanup` | Implemented; borrowed-reader finding remains Open | Upstream/shared original: U `object_tracker.cc:50-55,178-201` repeatedly scans all class vectors for each removal. The reverse membership index and batched compaction can be an upstream candidate without unrelated fork features; preserve finalizer ABI and ordering. |
| `haven-water-traversal` | Implemented; shares road cache | Downstream-only at U: `haven_water.cc` is absent. Its restoration obligations must remain intact here but are not an upstream missing-feature fix. |
| `claim-diagnostics-io` | Implemented; snapshot-capture finding remains Open | Downstream-only at U: claim science/worker files are absent. Keep bounded I/O, receipt barriers and failure gates with that diagnostic feature. |
| `navigation-bounds-refresh` | Implemented; other navigation residuals remain Open | Downstream-only at U: active `navigation_environment_poc` is absent. Do not extract the experiment as a prerequisite for an upstream performance PR. |
| `ship-tech-missing-template-retry` | Implemented; exceptional clone cleanup remains Open | Downstream-only at U: the technology-indicator implementation is absent. Native-owner discovery is a fork fix; the new clone failure finding is a separate obligation. |

The preceding `0de98a97` discovery mitigation also changed the Galactic Anomaly
timer. That feature **does** exist in U: `galactic_anomaly_timer.cc:169-172` defines
the global finder, `276` uses it for the drawer, and `340-356` retries navigation
and anomaly-manager lookup from a 250 ms update when missing. The scoped HUD
lookup/native singleton correction is another upstream-relevant extraction
candidate. It is not one of the six IDs in the follow-up audit and is not evidence
that the timer caused stutter while disabled. AAI implementation remains outside
this host tracker and comparison scope.

## Evidence and completion rules

- Preserve each finding ID in fix/test receipts. Record commit, fixture commands,
  native build/platform coverage and runtime limits before closing an item.
  Source-proven unsafe access or growth paths and unmeasured optimization
  candidates are different evidence levels.
- Keep acceptance budgets in **bytes and items**, including in-flight payloads;
  bounded queues alone do not bound detached threads, HTTP duration or memory
  held outside the queue. Make dropped/replaced work visible and preserve each
  category's state/delta semantics.
- Keep Unity/IL2CPP reads and mutations on their supported owner thread. An
  owned byte/string task can move to workers; raw managed pointers cannot simply
  be carried there. Rooting and generation invalidation are separate obligations.
- These attribution tags describe U, not all upstream history or feature branches.
  Recompare exact candidate/base blobs before an eventual focused upstream PR.
  Follow the canonical upstream scope/branding/held-PR guidance and review gate
  then; do not merge the fork play stack to obtain a small fix.
- This document is an implementation tracker, not a replacement for the canonical
  local downstream/upstream reconciliation audit. No upstream publication or
  integration status is changed by adding it.

Representative U blobs for reproducibility:

| Path | Git blob at U |
| --- | --- |
| `mods/src/patches/parts/sync.cc` | `bba2c6a2ca51df839c367cfcbb9f908c6535ffd2` |
| `mods/src/il2cpp/il2cpp_helper.h` | `16f5e06d9411e621a7569f1cbb64ae46cbc93f72` |
| `mods/src/patches/parts/object_tracker.cc` | `e6a30fc608b457086bfe63d694eb7f6ea8f0aaed` |
| `mods/src/patches/parts/hotkeys.cc` | `c9392f5be1b172ac79c7d38fea38942cbeb2c4de` |
| `mods/src/patches/parts/interior_zoom.cc` | `f782bf55a2bfdd119ff4f8a7ccdb627729d1dac6` |
| `mods/src/patches/parts/loading_screen_common.h` | `4f78890c706a7c098e84f49dbc8ec55b1b69aa0a` |
| `mods/src/patches/parts/chat_titles.cc` | `b472d533817d0df7ae9954d150a27d927d5cf812` |
| `mods/src/patches/parts/free_resize.cc` | `abc1e262f2e1a53997ad716642d48a771f7cf52c` |
| `mods/src/patches/parts/zoom.cc` | `2fb7af700f3aabcff55f29136e8e988c0dc84698` |
| `mods/src/patches/parts/haven_road_depth.cc` | `3c573fab3aee8424241fe52f6f2d6f3db7a36a21` |
| `mods/src/patches/parts/galactic_anomaly_timer.cc` | `7ed45ca320a3bf4a63ec8f21392d7e35fceaf578` |

The comparison used committed source and read-only Git/status/AXF context checks.
It performed no source builds, gameplay experiments, runtime/config changes,
AAI implementation review, upstream source writes, fetches or commits.
