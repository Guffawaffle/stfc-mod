# Action Queue Skip Investigation Log

> Current status: the manual refresh and ghost-hostile investigation work recorded below has been removed from the
> live branch and is now out of scope. This log is preserved as historical evidence; the active branch goal is the
> narrow Kir'shara queue-skip repair only.

## Goal

Track queue-skip experiments in one place so we can correlate code changes with live behavior and avoid repeating dead ends.

## Baseline

- Baseline before this session: skips happened occasionally.
- Current repo branch for experiments: `feat/manual-nav-refresh-hotkey-guffa`.
- Queue repair strategy to preserve:
  - keep Guffa's narrow postcondition-based queue repair in `mods/src/patches/parts/misc.cc`
  - do not migrate NetNiv queue behavior
  - do not introduce `BlockFirstArrival` / `ReleaseHeldArrival`

## High-Confidence Findings

- `fleet-slot-combat-ended` and `deployment-battle-end-event` can arrive before the queue state is actually resolved.
- `RemoveActionFromQueue` is often where the queue is finally proven to have removed a target.
- `fleet-slot-arrived-at-destination` is also being used by the game in head-turnover sequences that still represent a real queue advance.
- The queue can temporarily lose `LastEngagedTargetId` even while `IsEngaging == true`.
- In some runs, combat starts while the queue instance is still `is_engaging=false` and `last_target=0`, so there is no authoritative exact target id latched yet.

## Experiments

### 1. Preserve orphaned engaging during early combat-end window

- Change:
  - `HandleStall` stopped clearing orphaned engaging immediately after `fleet-slot-combat-ended` / `deployment-battle-end-event`.
- Outcome:
  - helped avoid one duplicate-reengage path
  - still exposed real stalls

### 2. Restore `LastEngagedTargetId` from live head when already engaging

- Change:
  - if `IsEngaging == true`, `LastEngagedTargetId == 0`, and a live queue head exists, restore the native last-target field from the live head
- Outcome:
  - kept some stalled queues coherent
  - still not enough when the game never latched an exact engaged target before combat ended

### 3. Infer engaging state from recent combat-start

- Change:
  - temporarily restored both `IsEngaging=true` and `LastEngagedTargetId=head` from recent `fleet-slot-combat-started`
- Outcome:
  - regressed skip frequency
  - rolled back
- Conclusion:
  - combat-start alone is not authoritative enough to infer the exact queue head safely

### 4. Sticky engaged-target latch

- Change:
  - add a mod-side sticky engaged target per action-queue instance
  - set it only from queue-authoritative points:
    - successful `TryPlanPathAndEngageTarget`
    - native nonzero `LastEngagedTargetId`
    - stall-time repair when native state is already engaging and we restore the last target
  - clear it only from queue-authoritative points:
    - confirmed remove of that target
    - clear-queue / clear-queue-and-move
    - queue empty after remove
- Intended effect:
  - do not let the exact engaged id drop just because the game sends an early combat-end style signal
  - prefer restoring native `LastEngagedTargetId` from sticky exact id when it still matches live queue contents

### 5. Defer arrival-triggered manual removal repair

- New data point after sticky latch:
  - skipped run showed all queued ids eventually removed, but the first head was manually removed by our repair immediately after `fleet-slot-arrived-at-destination`
  - no later `fleet-slot-combat-started` was observed for that removed head before the queue advanced
- Change:
  - when native `RemoveActionFromQueue(target)` returns true but leaves the target in the live queue, defer our manual `RemoveAt` repair if the latest trigger is `fleet-slot-arrived-at-destination`
  - keep logging the deferred case as `defer-arrival-remove-repair`
- Intended effect:
  - avoid treating arrival as proof that the hostile is dead
  - let the same queue head proceed into combat and rely on combat-end/remove for authoritative cleanup

## Important Live Signatures

### Early end signal before authoritative queue removal

- Sequence seen in logs:
  - `fleet-slot-combat-started`
  - queue still unstable or unlatching target
  - `fleet-slot-combat-ended`
  - actual `RemoveActionFromQueue(target_id)` happens later

This means combat-end is not sufficient proof that the queue head should be forgotten.

### Arrival-turnover remove

- Sequence seen in logs:
  - queue is engaging target A
  - `fleet-slot-arrived-at-destination`
  - `RemoveActionFromQueue(target A)` runs
  - queue advances to target B

This path can be real, but the 2026-05-31 15:53 run showed it can also remove the head before combat starts for that target.
For now, arrival-triggered manual repair is deferred so we do not advance the queue solely on arrival.

## Current Risks

- If sticky target latching helps the exact-id problem but skips remain, the remaining issue is likely that the game sometimes reaches combat without any authoritative queue-target latch at all.
- If sticky target latching regresses behavior, the likely failure mode is holding a target id across a genuine head transition longer than intended.

## Useful Data Points To Capture

- exact target ids at:
  - `TryPlanPathAndEngageTarget`
  - `RemoveActionFromQueue`
  - `HandleStall`
- whether native `LastEngagedTargetId` dropped to `0`
- whether sticky target matched the live queue head
- whether remove happened on:
  - `fleet-slot-combat-ended`
  - `deployment-battle-end-event`
  - `fleet-slot-arrived-at-destination`
- whether queue count changed only after manual repair

## Next Questions

- Does sticky-target preservation reduce skips back toward the old occasional baseline?
- When a skip still happens, did we ever have an authoritative exact target id latched before the skip?
- Does deferring arrival-triggered manual repair reduce skips back toward the old occasional baseline?

## Current Status Addendum

### 2026-05-31 / 2026-06-01: manual navigation refresh follow-up

- `Ctrl+Shift+G` reachability is fixed on `feat/manual-nav-refresh-hotkey-guffa` by commit `2822544`
  (`fix: dispatch hotkey table actions on their registered layer`).
- Current live investigation focus is no longer queue skip first. We observed that manual nav refresh can route the
  player to home/system-history state instead of reloading the currently visible system.
- This likely invalidates any ghost-clearing conclusion from the latest refresh repros until the reload target is fixed.

### Strong live evidence

- In the current repro, manual refresh captured and selected the current visible system node:
  - `current_node_id=847108551`
  - `reload_node_id=847108551`
  - `reload_node_source=NavigationManager._solarSystemNodeId`
- But the reload primitive logged:
  - `reload_primitive='SectionManager.TriggerSectionChange(saved Navigation_System state)'`
- The actual load that followed went to a different node:
  - `LoadSolarSystem phase=before node_id=79929`
  - `SetWatchNodeId phase=before node_id=79929`
- That is consistent with restoring saved `Navigation_System` state or a default/home system rather than explicitly
  reloading the visible system node.

### Natural navigation contrast

- In the same session, later system re-entry loaded the visible node directly:
  - `LoadSolarSystem phase=before node_id=847108551`
  - `SetWatchNodeId phase=before node_id=847108551`
- So the live evidence currently points at the manual refresh reload method/ordering, not the captured node id.

### Current conclusion

- Treat the current home-system jump as the primary bug ahead of permanent-ghost classification.
- The current manual refresh implementation prefers saved `Navigation_System` state and `ChangeNavigationSection(...,-1)`
  semantics before it falls back to explicit `LoadSolarSystem(reload_node_id)`.
- Any future ghost investigation should first confirm that manual refresh stays on the current visible node and that
  manual and natural S->G->S are targeting the same node.

### Follow-up after explicit reload-node patch

- After changing manual refresh to prefer explicit `ChangeNavigationSection(Navigation_System, reload_node_id)`,
  the old home-system misroute appears to be gone.
- New live behavior:
  - `Ctrl+Shift+G` drained the screen and ended with:
    - `phase=complete source=hotkey reload_node_id=903675622`
    - `repopulated=false`
    - `navigation_confirmed=false`
    - `timed_out=true`
    - `last_load_node_id=0`
    - `last_setwatch_node_id=0`
    - `reload_primitive='NavigationSectionManager.ChangeNavigationSection(Navigation_System,reload_node_id)'`
  - This means manual refresh cleared state but never produced a real `LoadSolarSystem` / `SetWatchNodeId` cycle.
- In contrast, natural `system -> galaxy -> system` immediately afterward did the normal live reload:
  - `SetWatchNodeId node_id=-1`
  - `OnViewChanged depth=1`
  - `SetWatchNodeId node_id=-1` in system view
  - `SetWatchNodeId node_id=-1` in galaxy view
  - `LoadSolarSystem node_id=903675622`
  - `OnViewChanged depth=2`
  - `SetWatchNodeId node_id=903675622`
- Practical takeaway:
  - current manual refresh no longer appears to be loading home, but it still does not mimic a real `S->G->S`
    transition
  - natural `S->G->S` remains the path that actually clears the ghost in this repro

### Current patch direction

- Manual refresh is now staged to use a real leave-and-return flow instead of same-section system re-entry:
  - leave primitive: `Navigation_Galaxy`
  - return primitive preference: `NavigationManager.LoadSolarSystem(reload_node_id)`
- The refresh will only attempt the return leg after leave evidence is observed:
  - `SetWatchNodeId(-1)`, or
  - `OnViewChanged depth=1`, or
  - active section becomes `Navigation_Galaxy`
- The next live check should confirm the manual path now produces the same core evidence as natural `S->G->S`:
  - `phase=drain-start`
  - `phase=reload-start ... reload_primitive='NavigationManager.LoadSolarSystem(reload_node_id)'`
  - `LoadSolarSystem phase=before node_id=<target>`
  - `OnViewChanged depth=2`
  - `SetWatchNodeId phase=before node_id=<target>`
  - `phase=repopulation-started`
  - `phase=complete ... timed_out=false navigation_confirmed=true`

### Natural re-entry watch: arbitrary system repro

- A later UI-driven natural `system -> galaxy -> system` repro used a ship located in a non-home system and confirmed
  that the tracked target node can be arbitrary:
  - `LoadSolarSystem phase=before node_id=2099507268`
  - `OnViewChanged phase=before depth=2`
  - `SetWatchNodeId phase=before node_id=2099507268`
- This removes the earlier ambiguity where `79929` could be explained by home-system navigation semantics alone.

### Natural re-entry watch: current limit

- In this arbitrary-system repro, the watch still completed without a `phase=first-drop` event:
  - counts ramped `0 -> 1 -> 4`
  - then stayed stable through tick `20`
  - final state:
    - `layer_items=4`
    - `layer_fleet_entities=4`
    - `movement_list_count=4`
- So the visible ghost clear is still not directly represented by the currently tracked fleet/runtime totals in every
  repro. The earlier late-drop case remains real, but it is not universal.

### Deeper reflective census result

- The added `RuntimeSystem[]` inspection showed that the first exposed system-manager entries are not leaf runtime
  systems. They are wrapper groups:
  - `system_managers._runtimeSystemList.Systems array_length=18`
  - `Systems[0..3].element_class=RuntimeSystemGroup`
  - each `RuntimeSystemGroup` currently exposes:
    - `_runtimeSystemList`
    - `_systemNames`
- The nested `navigation_manager._waitToCompleteViewChangeRoutine` object was `null` in this arbitrary-system repro at
  the logged milestones, so it is not presently the best explanation target for the observed clear.
- A different coroutine field was present:
  - `navigation_manager._JumpZoneGoToCoroutine value_class=Coroutine`
  but this is only observational so far and not yet tied to the ghost-clear moment.

### Next smallest diagnostic

- Recurse one level deeper into the `RuntimeSystemGroup._runtimeSystemList` contents rather than stopping at the
  top-level `RuntimeSystem[]` wrapper array.
- Keep the diagnostic read-only and bounded:
  - log subgroup `Systems` array lengths
  - log first few leaf element classes/pointers
  - only expand at existing `pre-load` / `post-load` / `post-depth2` / `post-setwatch-target` / `first-drop`
    milestones

### RuntimeSystemGroup expansion result

- The bounded subgroup expansion finally exposed leaf runtime systems rather than only wrapper groups.
- In the latest natural re-entry repro:
  - `target_node_id=79631`
  - re-entry still completed without `phase=first-drop`
  - counters again stabilized at `4/4/4` through tick `20`

### Leaf systems now visible

- `system_managers._runtimeSystemList.Systems[2]._runtimeSystemList.Systems` exposed UI-facing leaf systems:
  - `EntitySystem`
  - `UIEntitySystem`
  - `UIEntityMappingSystem`
- `system_managers._runtimeSystemList.Systems[3]._runtimeSystemList.Systems` exposed entity/culling leaf systems:
  - `CelestialObjectDataSystem`
  - `EntitySystem`
  - `NavigationEntityCullingSystem`
  - `CullingGroupSystem`
- Several of these leaf systems reference sets that are plausible owners of the visible ghost state:
  - `EntitySystem._entityTotalSet`
  - `UIEntityMappingSystem._entityTotalSet`
  - `NavigationEntityCullingSystem._entityTotalSet`
  - `NavigationEntityCullingSystem._entityVisibleSet`
  - `CullingGroupSystem._cullingGroupSet`

### Strongest next target

- `NavigationEntityCullingSystem` is currently the best next candidate because it is explicitly about navigation
  entity visibility/culling and exposes both:
  - `_entityTotalSet`
  - `_entityVisibleSet`
- `UIEntitySystem` / `UIEntityMappingSystem` remain secondary candidates for widget-side retention, but the culling
  layer is the better first stop because the user-facing symptom is a visible ghost ship that later clears.

### Recommended next step

- Stop broad recursive reflection here.
- Use AX dump search plus one narrow read-only reflective extension focused on:
  - `NavigationEntityCullingSystem`
  - `EntitySystem`
  - `UIEntitySystem`
  - `UIEntityMappingSystem`
- Goal of that next pass:
  - identify any direct runtime sets/lists or cache fields that distinguish visible vs total navigation entities
  - then watch those specific fields across `post-load` -> `post-depth2` -> `post-setwatch-target` -> tick window

### SGS follow-up after targeted leaf-system counters

- A later natural `system -> galaxy -> system` repro produced another `phase=first-drop` and finally separated the
  useful systems from the red herrings.
- The strongest result:
  - `NavigationEntityCullingSystem` still looked empty at the drop moment:
    - `_entityTotalSet runtime_set_items=0`
    - `_entityVisibleSet runtime_set_items=0`
    - `_cache list_count=0`
    - `_toVisible list_count=0`
    - `_toInvisible list_count=0`
    - `_unknownState list_count=0`
  - So the ghost-clear event is not currently explained by the culling/visibility system.

### Stronger candidate than culling

- The actionable signal moved to the fleet/entity subgroup:
  - `system_managers._runtimeSystemList.Systems[1]._runtimeSystemList.Systems[3]`
  - class: `EntitySystem`
- At `phase=first-drop`, that system showed:
  - `_entityTotalSet runtime_set_items=21`
  - `_entityTotalSet runtime_set_nav_fleets=21`
  - `_cache list_count=23`
  - `_dataUpdatedSet runtime_set_items=1`
- Nearby systems in the same subgroup were also non-empty:
  - `EntityRemovingSystem._rawDataTotalSet runtime_set_items=21`
  - `EntityRemovingSystem._bufferedDataTotalSet runtime_set_items=21`
  - `FleetHostileHighlightSystem._dataTotalSet runtime_set_items=21`
- In contrast, the starbase/UI subgroup remained empty at the same moment.

### Revised hypothesis

- The ghost state likely persists in the fleet/entity data-removal path rather than in a late visibility/culling layer.
- More specifically, the likely next owners are:
  - `EntitySystem`
  - `EntityRemovingSystem`
  - `FleetDataSystem`
  - `FleetHostileHighlightSystem`
- `NavigationEntityCullingSystem` is now a weaker candidate and should not be the next primary focus.

### Skip note to preserve for later

- This same session also showed action-queue skip/reappearance markers that should be revisited later:
  - `ActionQueueProbe phase=before hook=RemoveActionFromQueue ... in_queue=true any_queue=true`
  - `ActionQueueProbe phase=result hook=RemoveActionFromQueue ... result=true`
  - `ActionQueueProbe phase=after hook=RemoveActionFromQueue ... in_queue=true any_queue=true`
  - `DeploymentLifecycleTrace event=navigation-fleet-reappearance-watch ... source=\"action-queue-remove\"`
- That is a strong breadcrumb for the separate skip thread, but it should remain out of scope for the current SGS-only
  ghost investigation.

### UI-init crash note on pool ownership diagnostics

- A later repro still crashed after `LoadSolarSystem` once station/UI view initialization began.
- This was after backing out the explicit `PoolingStateChanged(...)` detours, so the remaining risky seam is the
  periodic live-owner snapshot itself.
- Strongest current suspicion:
  - sampling `UIEntitySystem._navigationPools` entries and calling `object_class_name(...)` on live pool items during
    early UI construction is too aggressive
  - similarly, milestone-time live-owner snapshots (`pre-load` / `post-load` / `post-depth2` / `post-setwatch-target`)
    are more likely to touch half-initialized UI systems than bounded `update-tick` snapshots
- Mitigation applied:
  - remove per-pool item/class sampling from `ReentryUILive`
  - keep only pool/root counts and raw owner pointers
  - restrict `ReentryUILive` / `ReentryVisualLive` snapshots to `phase=update-tick`
- This preserves the shared-root / separate-pool signal while avoiding the hottest UI-init reads.

### Visibility-mode split confirmed

- A later stable natural `SGS` repro on node `607346503` survived with the reduced sampler and confirmed that the
  retained visual owners are not just “extra systems”; they split into explicit `Widget` and `Entity` visibility
  stacks under separate mapping owners.
- Key live shape at tick-window completion:
  - `ReentryWatch`: `layer_items=1`, `layer_fleet_entities=1`, `movement_list_count=1`
  - `live.ui_entity_mapping_system[3]`: `entity_total_count=2`, `entity_to_widget_pair_count=2`, list backing count `30`
  - `live.ui_entity_mapping_system[4]`: `entity_total_count=4`, `entity_to_widget_pair_count=4`, list backing count `4`
  - `live.ui_entity_mapping_system[0]`: `entity_total_count=1`, `entity_to_widget_pair_count=1`, list backing count `24`
- The corresponding visibility systems now prove paired mode ownership:
  - mapping `0x22c1a207ae0` had:
    - one `NavigationVisibilityToggleSystem` in `mode=Widget`
    - one `NavigationVisibilityToggleSystem` in `mode=Entity`
  - mapping `0x22c1a1fbde0` also had:
    - one `mode=Widget`
    - one `mode=Entity`
- So we now have evidence of parallel widget/entity visibility pairs attached to distinct mapping owners during re-entry,
  not merely follower or cache drift.
- Additional note:
  - `_prePoolCount` on `UIEntitySystem` did not read cleanly in this pass (`-1`), so it is not currently useful.

### Revised next step

- Aggregate by `mapping_system_ptr` rather than reading six loose live systems by eye.
- For each live mapping owner, compare:
  - mapping entity/widget/pair counts
  - attached follower counts
  - attached `Widget`-mode visibility cache counts
  - attached `Entity`-mode visibility cache counts
- This is now the smallest safe way to classify which parallel owner stack is active, retained, or partial during the
  `SGS` handoff.

### Aggregated mapping-owner result

- A later grouped-owner repro on node `304751397` produced the clearest classification yet.
- Tick-window completion still ended at:
  - `layer_items=1`
  - `layer_fleet_entities=1`
  - `movement_list_count=1`
- The mapping-owner aggregates were stable across ticks `9` through `20`:
  - owner `0x29070c70900`
    - `mapping_entity_count=3`
    - `mapping_widget_count=3`
    - `mapping_pair_count=3`
    - backing `mapping_list_count=30`
    - `follower_entities_count=30`
    - `widget_mode_systems=1`
    - `widget_mode_cache_count=30`
    - `entity_mode_systems=1`
    - `entity_mode_cache_count=0`
  - owner `0x29071bea840`
    - `mapping_entity_count=3`
    - `mapping_widget_count=3`
    - `mapping_pair_count=3`
    - backing `mapping_list_count=3`
    - `follower_entities_count=3`
    - `widget_mode_systems=1`
    - `widget_mode_cache_count=3`
    - `entity_mode_systems=1`
    - `entity_mode_cache_count=0`
  - owner `0x29071b07b40`
    - `mapping_entity_count=1`
    - `mapping_widget_count=1`
    - `mapping_pair_count=1`
    - backing `mapping_list_count=23`
    - `follower_entities_count=21`
    - no attached visibility-mode systems in this bounded slice
- Practical interpretation:
  - we now have at least two explicit paired `Widget`/`Entity` visibility stacks:
    - one large retained stack with active object counts `3` but retained backing counts `30`
    - one small stack with matching counts `3`
  - the large stack is not just “extra followers”; its widget-mode visibility cache is also retained at `30`
  - the entity-mode side currently reports `0` cache entries for both paired owners, so the retained visual weight is
    on the widget-mode path, not the entity-mode cache
- This pushes the hypothesis one step further:
  - the visible ghost path is most likely controlled by retained widget-mode visibility ownership on a parallel mapping
    owner, while the entity-mode partner remains present but not populated in the same way

### Skip breadcrumb for this repro

- The user observed one skip in this same session.
- The captured tail for this repro did not contain a strong new action-queue marker such as:
  - `RemoveActionFromQueue`
  - `navigation-fleet-reappearance-watch`
  - or a fresh `DeploymentLifecycleTrace`
- So preserve only the user report here; there is no stronger skip breadcrumb from the bounded tail to add beyond that.

### SGS follow-up after dedicated re-entry pipeline watch

- A later natural `system -> galaxy -> system` repro used the new `[ReentryPipeline]` stream focused on:
  - `FleetDataSystem`
  - `EntityRemovingSystem`
  - the matching `EntitySystem`
- Result:
  - the visible-system re-entry stabilized at `4` tracked fleets during the tick window
  - but the fleet-removal collections never activated in the way a true disposal path would suggest

### What the pipeline watch showed

- `FleetDataSystem`
  - `_dataTotalSet runtime_set_items=4`
  - `_totalHashSet list_count=4`
  - `_added=0`
  - `_removed=0`
  - `_updated=0`
  - `_invalid=0`
  - `_markedToRemove=0`
- `EntityRemovingSystem`
  - `_rawDataTotalSet runtime_set_items=4`
  - `_bufferedDataTotalSet runtime_set_items=4`
  - `_removing list_count=0`
- `EntitySystem`
  - `_entityTotalSet runtime_set_items=4`
  - `_entityTotalSet runtime_set_nav_fleets=4`
  - `_cache list_count=24`
  - `_dataUpdatedSet runtime_set_items=0`

### Strongest new conclusion

- The current ghost-clearing moment is still not presenting as an active fleet-removal/disposal transition.
- There is no evidence in this repro that fleets are entering:
  - `FleetDataSystem._removed`
  - `FleetDataSystem._markedToRemove`
  - `EntityRemovingSystem._removing`
- Instead, the system appears to settle into a stable smaller active fleet set (`4`) while `EntitySystem._cache`
  remains much larger (`24`), which suggests retained entity/cache state rather than explicit removal-queue work.

### Revised next target

- The next smallest useful read-only step should stop following remove-queue containers and instead compare:
  - `EntitySystem._entityTotalSet`
  - `EntitySystem._cache`
  - and, if accessible, the actual fleet ids / object identities represented in each
- Goal:
  - determine whether the “ghosts” are stale cached entities that no longer correspond to the active total set
  - or whether the active total set itself still contains the visual ghosts and they are being hidden elsewhere

### Entity identity crash note

- The first `EntitySystem` identity pass crashed immediately after:
  - `[ReentryIdentity] phase=pre-load root=fleet_group.entity_system._entityTotalSet source=runtime_set count=20`
- No per-sample identity lines were emitted before process death.
- Strongest current interpretation:
  - the dangerous edge was direct dereference of sampled runtime-set/cache entities through
    `NavigationAbstractEntity.get_ID` and/or class-name reads
  - the count-level and cache-count instrumentation remained safe
- Follow-up adjustment:
  - downgrade the identity watch to raw pointer/key overlap only
  - avoid calling entity methods or reading class metadata on sampled runtime-set/cache values until a safer object
    shape is proven
- Crash dumps captured for later forensics if needed:
  - `%LOCALAPPDATA%\\CrashDumps\\prime.exe.21548.dmp`
  - `%LOCALAPPDATA%\\CrashDumps\\prime.exe.40964.dmp`

### Corrected cache overlap result

- After fixing the dictionary entry offset handling for `EntitySystem._cache`, the cache overlap watch produced a
  trustworthy result.
- During stable re-entry ticks before the late drop:
  - `EntitySystem._entityTotalSet runtime_set_items=4`
  - `EntitySystem._cache valid_entries=17`
  - `shared_entries=4`
  - `cache_only_entries=13`
- At `phase=first-drop`, the same relationship held at larger scale:
  - `EntitySystem._entityTotalSet runtime_set_items=21`
  - `EntitySystem._cache valid_entries=34`
  - `shared_entries=21`
  - `cache_only_entries=13`
- Practical meaning:
  - every active entity sampled from `_entityTotalSet` was also present in `_cache`
  - but `_cache` retained an additional `13` entities that were not members of the active runtime set
- This is the strongest current evidence so far that the ghost problem is tied to retained `EntitySystem` cache state
  rather than to active removal queues or `NavigationEntityCullingSystem`.

### Revised strongest hypothesis

- `EntitySystem._cache` is a likely ownership layer for stale retained navigation entities.
- The visible ghost clear may be the point where rendering/runtime behavior stops honoring those cached-but-inactive
  entities, even though the cache itself still retains them.
- The next smallest useful read-only step should identify those `cache_only` entities more concretely:
  - compare cache-only values against a safer identifier source
  - or inspect `EntitySystem.GetEntity(long id)` / `FleetDataSystem` interactions to learn whether the cache is queried
    for objects no longer represented in `_entityTotalSet`

### Cache-only follow-up correction

- The next `SGS` repro with cache-only lifecycle tracking did **not** support those 13 entries as live stale entities.
- Observed pattern across the bounded tick window:
  - `valid_entries=17`
  - `shared_entries=4`
  - `cache_only_entries=13`
  - but every sampled cache-only entry logged as:
    - `key=0`
    - `entity_ptr=0x0`
- The cache-only delta tracker also showed:
  - `persisted_entries=0`
  - `added_entries=13`
  - `removed_entries=13`
  on every tick, which is consistent with re-counting default/empty dictionary slots rather than tracking stable retained
  entities.
- A narrow `EntitySystem.GetEntity(long)` lookup hook produced no `[ReentryLookup]` hits against those cache-only keys.

### Revised conclusion after cache-only tracking

- The earlier “13 extra cached entities” result was overstated.
- Current evidence says those `13` entries are most likely empty/default dictionary slots, not real retained fleet
  objects.
- So we should **not** clear `EntitySystem._cache` based on the current evidence.
- The stronger remaining signal is still the active runtime/entity path itself:
  - `EntitySystem._entityTotalSet`
  - `FleetDataSystem._dataTotalSet`
  - and the late-drop change in runtime counts during natural re-entry

### Active-set and FleetDataSystem follow-up

- A later bounded repro added:
  - active `EntitySystem._entityTotalSet` pointer deltas
  - and before/after hooks on:
    - `FleetDataSystem.RemoveFleetsDestroyedOnClient`
    - `FleetDataSystem.FilterCurrentSystemFleets`
    - plus additional removal-path hooks for future captures
- In this repro there was **no** `phase=first-drop`. The re-entry watch completed with:
  - `layer_items=4`
  - `layer_fleet_entities=4`
  - `movement_list_count=4`
- The active entity set behavior was:
  - `0 -> 4` once during early tick window
  - then stable at `4` through completion
- The hooked `FleetDataSystem` methods did run, but showed no active-set removal:
  - `RemoveFleetsDestroyedOnClient` fired repeatedly with `active_entities=4` and `persisted=4 added=0 removed=0`
  - `FilterCurrentSystemFleets` fired once while `active_entities=0`, also with no delta
- Practical meaning:
  - this repro does **not** support the ghost clear being caused by active entity removal inside the currently hooked
    `FleetDataSystem` methods
  - the visible clear may instead happen in a downstream visual/widget mapping layer while the active entity set remains
    stable

### UI mapping follow-up

- A subsequent bounded repro added a downstream UI-layer watch:
  - `UIEntitySystem`
  - `UIEntityMappingSystem`
  - widget-set deltas
  - read-only `GetWidget(...)` lookup hooks
- This repro was sparse and did not produce a `phase=first-drop`, but it still added a useful contradiction:
  - `ReentryUI widgets_current=0 widgets_persisted=0 widgets_added=0 widgets_removed=0`
  - `ReentryActive current=0`
  - while `UIEntityMappingSystem.GetWidget(NavigationAbstractEntity)` was actively called and returned non-null widgets
    for several entity pointers during the same tick window
- Practical meaning:
  - the currently watched `WidgetRuntimeSet` surface is **not** the authoritative source for the visible widget state
    during this path
  - `UIEntityMappingSystem` can still resolve entity -> widget mappings while the tracked widget set reports zero
- Revised next target:
  - inspect `UIEntityMappingSystem._entityToWidget` and `_widgetToEntity` directly rather than relying on
    `WidgetRuntimeSet` totals

### UI mapping dictionary follow-up

- A later bounded repro moved the UI-layer watch from `WidgetRuntimeSet` to direct dictionary inspection of:
  - `UIEntityMappingSystem._entityToWidget`
  - `UIEntityMappingSystem._widgetToEntity`
- That repro was a low-signal drain case:
  - `phase=first-drop`
  - `reason=post-depth2`
  - `peak_layer_items=14`
  - `peak_layer_fleet_entities=14`
  - `peak_movement_list_count=14`
  - then immediate drop to `0 / 0 / 0`
- Through the rest of the bounded tick window:
  - `ReentryActive current=0`
  - `_entityToWidget valid_pairs=0`
  - `_widgetToEntity valid_pairs=0`
  - mapped widget/entity counts stayed `0`
- Practical meaning:
  - the visible clear may indeed be happening during the final `S` transition itself, around or just after
    `OnViewChanged(depth=2)`
  - but this repro did **not** capture later remapping or repopulation, so it cannot yet distinguish:
    - “the actual clear happened here”
    - from “the clear happened slightly earlier and we only first observed the zero here”

### Next timing refinement

- The next smallest read-only refinement is to stop relying only on periodic snapshots and instead inspect the exact
  `UIEntityMappingSystem` instance that services `GetWidget(...)` calls during the bounded re-entry watch.
- New instrumentation goal:
  - on `UIEntityMappingSystem.GetWidget(entity)` with non-null result
  - log the live `system_ptr`
  - log current `_entityToWidget` and `_widgetToEntity` valid-pair counts from that same instance
  - log whether the requested `entity_ptr` / returned `widget_ptr` are present in those dictionaries
- This should settle whether the “drop to zero after final S” is the actual clear point, or just a gap before the
  mapping layer starts answering again.

### UI mutation / visibility follow-up

- A later SGS repro finally proved that real UI mutation is happening during re-entry, but not on the same
  `UIEntityMappingSystem` instance returned by the current periodic sampler.
- Key evidence:
  - early in the tick window, while the periodic watch still showed:
    - `layer_items=0`
    - `layer_fleet_entities=0`
    - `_entityToWidget valid_pairs=0`
  - the method-level hooks logged real mutation:
    - `UIEntitySystem.Update` added `4` widgets on one instance
    - `UIEntityMappingSystem.Update` added `4` entity->widget mappings on one instance
    - another `UIEntitySystem.Update` added `30` widgets on a different instance
    - another `UIEntityMappingSystem.Update` added `30` mappings on a different instance
- The logged pointers were materially different from the periodic `ui.ui_entity_mapping_system root_ptr=...` instance.
- Practical meaning:
  - the periodic `find_runtime_system_by_class_name(...)` snapshot is not a reliable source of truth for the live
    mapping owner during this transition
  - the real visual mapping churn is occurring on sibling/alternate runtime-system instances that the current periodic
    scan is not selecting

### Visibility-toggle result

- `NavigationVisibilityToggleSystem.Toggle(entity, true)` fired heavily during the same re-entry window.
- But every observed call was:
  - `state=true`
  - and `before_count == after_count`
- No `state=false` transitions were observed in this repro, and no count drops were attributed to the toggle caches.
- Practical meaning:
  - the toggle system is part of re-entry enable/rebuild behavior
  - but this repro does not support it as the direct “clear/removal” mechanism yet

### Revised next target

- The next smallest useful diagnostic should stop using the generic runtime-system lookup as the primary UI truth
  source and instead track the actual mutated instances seen in hooks:
  - `UIEntitySystem.Update`
  - `UIEntityMappingSystem.Update`
- The key question is now:
  - do the same mutated system instances later perform removals/unmaps
  - or is the visible clear simply the point where the correct system instance replaces an older retained visual owner

### Live-instance result

- The next bounded SGS repro confirmed that the live-instance approach is the right surface, but the current run did
  not show destruction inside the 20-tick watch window.
- The watch targeted system node `1092043499` and completed with:
  - `layer_items=1`
  - `layer_fleet_entities=1`
  - `movement_list_count=1`
- Across ticks `11` through `20`, `[ReentryUILive]` stayed stable with multiple concurrent UI owners:
  - `live.ui_entity_mapping_system[0]` / `live.ui_entity_system[0]` at `1 / 1`, with backing dictionary/cache
    slack of `24`
  - `live.ui_entity_mapping_system[3]` / `live.ui_entity_system[3]` at `2 / 2`, with backing dictionary/cache
    size `14`
  - several sibling instances remained at `0 / 0`
- Practical meaning:
  - the current repro still supports the “competing retained owners” theory, just at smaller counts than the earlier
    `4 / 4` and `14 / 14` runs
  - nothing in the current bounded window proves that the stale-looking owner is destroyed immediately after the
    correct owner appears

### Next lifecycle target

- The next narrow read-only step is to hook lifecycle on the exact owner classes we have already confirmed are live:
  - `UIEntitySystem.Destroy`
  - `UIEntityMappingSystem.Destroy`
  - `NavigationVisibilityToggleSystem.Destroy`
  - `EntityUiFollowerSystem.Destroy`
- Pair those with live-instance tracking for:
  - `NavigationVisibilityToggleSystem`
  - `EntityUiFollowerSystem`
- This should answer whether the ghost-side owner is:
  - destroyed later than our current bounded window,
  - detached/superseded without destruction,
  - or still alive while another owner simply wins visibility/precedence.

### Owner-link result

- The follow-up SGS repro with owner-link logging confirmed that the visual/follower stacks are explicitly attached to
  specific mapping-system instances, and that the stacks persist through the bounded window.
- In the captured non-home repro (`target_node_id=1270407833`), the stable live UI owners were:
  - `live.ui_entity_mapping_system[0]` at `1 / 1` with dictionary slack `24`
  - `live.ui_entity_mapping_system[3]` at `2 / 2` with dictionary slack `30`
- The attached visual-owner graph lined up as follows:
  - `live.entity_ui_follower_system[0] -> mapping_system_ptr = live.ui_entity_mapping_system[0]`
  - `live.entity_ui_follower_system[3] -> mapping_system_ptr = live.ui_entity_mapping_system[3]`
  - `live.navigation_visibility_toggle_system[0]` and `[1] -> mapping_system_ptr = live.ui_entity_mapping_system[3]`
- Important detail:
  - the follower counts on those stacks stayed much larger than the active UI-owner counts:
    - follower `22` attached to mapping `1`
    - follower `29` attached to mapping `2`
    - visibility cache count `29` also attached to mapping `2`
- No destroy/lifecycle lines fired in the bounded window.
- Practical meaning:
  - this is no longer just “generic stale visuals somewhere in the UI layer”
  - we now have evidence of parallel retained visual pipelines bound to concrete mapping owners
  - the next question is not whether the owners exist, but why follower/visibility membership remains much larger than
    the mapping/entity totals on those owners

### Overlap result

- The next SGS repro added exact entity-key overlap between each visual owner and its linked mapping owner.
- This disproved the strongest stale-follower/cache hypothesis.
- On the high-count retained pipeline:
  - `live.ui_entity_mapping_system[3]` had `entity_to_widget_pair_count = 30`
  - `live.entity_ui_follower_system[3]` had `shared_with_mapping = 28`, `follower_only = 0`, `mapping_only = 2`
  - `live.navigation_visibility_toggle_system[0]` had `shared_with_mapping = 28`, `cache_only = 0`,
    `mapping_only = 2`
- On the small active pipeline:
  - `live.ui_entity_mapping_system[0]` had `entity_to_widget_pair_count = 4`
  - `live.entity_ui_follower_system[0]` had `shared_with_mapping = 4`, `follower_only = 0`, `mapping_only = 0`
- Practical meaning:
  - follower/visibility are **not** holding extra entity keys that their linked mapping owner lacks
  - the large raw dictionary counts (`23`, `30`, etc.) are not evidence of extra live members by themselves
  - if anything, the follower/visibility layers are a subset of mapping on the retained pipeline, missing `2` mapped
    entities rather than carrying stale extras
- Revised next target:
  - investigate why the retained parallel mapping owner still exists at all
  - or why its mapped entities survive while the correct active pipeline also exists
  - likely next surfaces: `UIEntityMappingSystem` / `UIEntitySystem` ownership and widget-pool handoff, or the
    section/view root that keeps both pipelines alive concurrently

### Shared-root / separate-pool result

- The next SGS repro finally answered the first ownership question: the parallel pipelines are **not** on separate UI
  roots.
- In the stable captured window:
  - all live `UIEntitySystem` instances shared the same `canvas_variable_ptr`
  - all live `EntityUiFollowerSystem` instances shared the same `nav_ui_canvas_ptr`
  - all live `EntityUiFollowerSystem` instances shared the same `nav_camera_ptr`
- So this is not a split-canvas or split-camera problem.
- However, the retained and active UI pipelines did have distinct per-system pool ownership on that shared root:
  - active `UIEntitySystem[0]`:
    - `widget_pool_ptr=...df00`
    - `widget_pool_count_all=23`
    - `widget_pool_stack_count=2`
    - `navigation_pools_count=2`
    - navigation pool sample ptrs distinct from the retained pipeline
  - retained `UIEntitySystem[3]`:
    - `widget_pool_ptr=...dbE0`
    - `widget_pool_count_all=30`
    - `widget_pool_stack_count=0`
    - `navigation_pools_count=2`
    - navigation pool sample ptrs also distinct
- Practical meaning:
  - two UI pipelines are alive on the same canvas root
  - but they appear to own different widget pools and different navigation-pool state
  - that makes pool / navigation-pool handoff a stronger lead than follower/visibility membership

### Pooling-hook crash note

- A direct attempt to hook `PoolingStateChanged(bool isActive)` on several `INavigationPoolingState` implementers
  crashed the client during UI/station-view initialization after load.
- Important detail:
  - there were still no `[ReentryPooling]` lines before the crash
  - that suggests the detour family itself is too fragile in this phase, rather than the log formatting inside the
    hook being the only problem
- Decision:
  - back off the live `PoolingStateChanged` hooks
  - keep the safer periodic pool/root snapshots (`_widgetPool`, `_navigationPools`, canvas/camera pointers, class-name
    sampling)

### Skip breadcrumb (same session)

- A skip happened in the same session, but the current bounded slice did not show a richer skip-specific marker than
  repeated queue clearing with no queued state:
  - repeated `ActionQueueProbe ... ClearQueue ... in_queue=false any_queue=false`
  - repeated `OnPlayerFleetStateChanged ... any_queue=false`
- Keep this as a light breadcrumb only; it does not change the SGS ownership conclusion above.
