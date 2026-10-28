# Map and camera cohort — 2026-10-27

**Offline implementation qualified; native trial pending.** The initial proposal was documentation-only at
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`. Guff then authorized continuing
without Runner's native execution layer. Three directly coordinated workers
own the lanes below, and the coordinator owns shared integration and builds.
The accepted travel/cargo build remains the runtime baseline until this batch
passes its own checks. No governed Attempt or Runner-managed worker execution
is claimed; helper implementation remains another agent's task.

The implementation base is `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`;
the older planning base below is retained as provenance, not the built candidate.

## Inputs and selection

- Frozen source: `frozen/play-dev-20261026` at
  `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target planning base: `play-dev-ripper` at
  `6e7776f0277c2228edae4f9c546d9c1b9878dd9e`, existing worktree
  `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`.
- Running implementation: `10477fd1a05df0c06ea3921dc56843fb8296fdbd`, qualified in
  [the preceding cohort](FEATURE-COHORT-20261027-02.md).
- Procedure: [migrate-play-feature](../../.agents/skills/migrate-play-feature/SKILL.md).
- Inventory: [existing capability IDs](play-functional-parity.json). Old Partial
  and Implemented/unverified dispositions are discovery inputs, not evidence
  that the current implementation lacks the feature.

This batch follows naturally from travel and target inspection: it qualifies how
players find systems, see their map information, and control the camera. Eight
existing capabilities divide into three source ownership lanes without workers
competing over the same implementation file.

| Lane | Capability | Primary implementation |
| --- | --- | --- |
| Camera | PLAY-CAMERA-ZOOM: extended system range and clipping | `src/mod/features/camera.cc` |
| Camera | PLAY-CAMERA-PRESETS: five presets, stored default and reload behavior | same |
| Camera | PLAY-CAMERA-KEYBOARD: pointer-centered keyboard zoom and input precedence | same |
| Camera | PLAY-CAMERA-WHEEL: UI wheel ownership with native pinch intact | same |
| Galaxy presentation | PLAY-GALAXY-DETAIL: independent major/minor label thresholds | `src/mod/features/galaxy.cc` |
| Galaxy presentation | PLAY-GALAXY-OVERLAYS: simultaneous layers and native selection state | same |
| Galaxy presentation | PLAY-GALAXY-HOUSING: housing-capable markers and cleanup | same |
| Galaxy selection | PLAY-GALAXY-SELECTION: far-zoom system tapping | `src/mod/features/galaxy_selection.cc` |

The coordinator owns shared bindings (`game/navigation.h`, `game/camera.h`,
`game/scroll_input.h`, `game/galaxy.h`), `features/galaxy.h`, shared hook owners,
Settings registration, test registration and inventory integration. Workers
propose shared-boundary changes rather than independently editing these files.

## What the scoping inspection established

Current implementations exist for all eight. Full frozen-source obligation
contracts are still the first execution milestone; this planning inspection
does not substitute for those complete scans.

The earlier [M8 Galaxy record](M8-GALAXY-CONTROLS.md) describes missing native
Settings controls at its historical checkpoint. Current
`src/mod/settings/config_settings.cc:619–683` already registers extended
selection, housing, multiple overlays, all four overlay flags, and independent
major/minor detail choices and threshold sliders. Do not reimplement them or
carry the old missing-control claim into a current assessment without checking
their availability and actual consumer behavior.

The shared camera update has one owner in
`camera.cc::EnsureNavigationZoomUpdates`; the LOD update and pooled fleet
lifecycle have their own owners in `fleet_labels.cc::EnsureNavigationLodUpdates`
and `EnsureNavigationFleetLifecycle`. The camera callback also notifies Galaxy
consumers. Preserve these owners and transaction outcomes; do not introduce
competing frame or LOD hooks merely because multiple lanes need observations.
Coordinate any edits to shared functions with their file's assigned writer.

The camera scan identified two matters to qualify explicitly:

- Source and target zoom normalization divide by `maximum - minimum` without
  fully qualifying a finite positive span. Failed native observations may also
  become default zero. Check availability, zero/reversed/nonfinite ranges and
  storage outcomes before deciding on a narrow correction. If confirmed, this
  is a preserved-source correctness issue, not a newly introduced migration bug.
- Current entering-system default zoom applies even when text is focused;
  frozen behavior applied it within the unfocused path. Record and review that
  existing decision rather than silently reversing it while porting shortcuts.

No demonstrated implementation blocker was found in this scoping pass. Native
hit testing, pooled object lifetimes and platform qualification remain work to
perform, not reasons to declare a feature missing.

## Original execution sequence and current adjustment

The following was the proposed governed path. The current authorization removes
helper readiness as an implementation dependency: use direct workers and the
existing local build/verification workflow. Supported planning or evidence
interfaces may still be used, but do not probe or modify the native helper to
force execution. Candidate binding, tests, source preservation and lifecycle
requirements below are unchanged.

1. Verify the actual installed Runner/helper versions and rerun containment
   preflight for this repository and authorized existing worktree. Presence of a
   helper file is not proof that governed execution works. Respect refusals.
   Do not create new checkouts or weaken boundaries to manufacture readiness.
2. Materialize the supported plan with the eight existing IDs, exact source/base,
   three exclusive lanes, shared dependencies and explicit acceptance criteria.
   Bind coordination operations to the workspace coordination database where
   their actual interface supports it. Do not invent PRs or execution authority.
3. Fan out three workers plus coordinator. Each reads the complete pinned source,
   headers, callers, defaults, Settings, callbacks, cleanup and tests; writes its
   feature contracts; and maps retained logic before making bounded changes.
4. Integrate shared boundary changes under one owner and freeze the combined
   candidate. Review cross-lane camera/LOD, Settings and input interactions.
   Reconcile meaningful operations, not only feature names or hook installation.
5. Run candidate-bound regression/native gates with verified artifact read-back.
   Use the supported merge/weave path only within its actual authority. If
   workers are directly coordinated, label that honestly. Do not merge PLAY,
   publish, or change release state as part of local batch integration.
6. Build before game downtime, then perform one explicit-target main-game trial
   through the established lifecycle flow. Keep native readiness, user trials,
   retained evidence and macOS qualification separate. Save the final inventory
   and scoped recovery checkpoint.

If the helper is not yet qualified, the plan and source contracts can be
prepared independently. Another agent owns helper implementation; this batch
must not turn into a tooling-development project.

## Verification contract

Reuse the existing Galaxy policy, housing-data, selection-policy/pins, controlled
binding, keyboard and Settings fixtures. Add production-consumer tests where the
current tests stop at a pure model. Test substitutes must be named explicitly.

| Area | Required checks |
| --- | --- |
| Zoom | System changes, valid range/clipping, unavailable/zero/reversed/nonfinite observations, default application and reset |
| Presets | Five store/invoke paths; shortcut precedence; explicit durable writes versus session-only preset-as-default; reload/reopen |
| Keyboard and wheel | Capture/text/native-hotkey modes; held input; pointer position; nested/occluded/disabled scrollers; failed ownership query; native touch pinch remains intact |
| Labels | Major/minor independence; within-tier threshold changes; camera replacement; native mode restoration; Settings disable/reload |
| Overlays | All 15 nonempty combinations; last-deselect Default fallback; runtime selection, native highlight/representative mode and saved flags agree; pooled layout restores |
| Housing | Known housing/nonhousing nodes; marker follows visibility and identity; disable/rebind/release/failure cleanup; do not imply free slots |
| Selection | Native and far-tier taps; pinned systems; overlapping candidates; complete source enumeration; density fallback; immutable POIs; cleanup failure and owner coverage; pool pressure retains native behavior |
| Shared consumers | Camera/LOD ownership; Settings availability and actual consumer changes; source replacement; AAI, queue, travel/cargo and preset regression checks |

The Windows trial should combine a normal system entry/preset, keyboard zoom,
chat scrolling without background zoom, Galaxy layer combinations, major/minor
label thresholds, housing markers and a far-zoom system click. Use a chosen
preset for persistence checks; do not overwrite player preferences merely to
obtain a green test. macOS needs both architecture builds in CI and Mcohall's
actual native trial. A successful Windows launch establishes neither.

## Explicit exclusions and follow-ups

Preserve adjacent behavior, including backdrop scale, pan/glide/momentum,
station/interior camera controls, fleet labels and Haven, when shared code is
read or adapted. They are not additional completed feature scans in this batch.
Pan/momentum remains a named follow-up so Guff's earlier request is not lost.
System orbit remains explicitly excluded.

Fleet selection, ability recovery, screen shortcuts and Discovery towing are a
later coherent cohort. Towing deserves its own careful native lane: it depends
on a seven-argument planner, iterator fields, Unity coroutine scheduling and
closed callbacks; a pure policy test alone does not qualify that workflow.

Historical repair protection, the contained/unattributed GetComponent exception,
Settings visual rework, toolkit expansion and persistence remain separate work.
No source behavior, installed artifact, user settings or player store changed
while preparing this proposal.

## Implemented source reconciliation

All eight complete frozen-source contracts now exist under
[`features/`](features/). Existing consumers were preserved; this pass did not
rebuild them from feature descriptions. Three directly coordinated workers
owned camera, Galaxy presentation and selection respectively. The coordinator
adapted shared checked bindings and integrated the existing controlled-runtime
fixture host. Cross-lane source reviews were independent collaboration, not
Runner-governed execution. Runner's native layer was not invoked or modified.

Camera conversions now require available finite observations and a positive
range. Valid formulas, explicit preset save keys, session-only automatic defaults
and shortcut precedence remain intact. A weak camera-bound pending default
replaces the process-wide flag; older reentrant completion cannot consume a newer
camera's request. A managed getter cannot leave a preset write targeting the
previous Config owner. After a successful distance setter, current scene/range/
configuration are rechecked before pointer/world data and the remaining native
zoom command are published. The setter's own effect is not rolled back.

Galaxy restoration retains layout/marker custody through weak-owner replacement,
failed strong rooting and unknown Unity liveness. Surviving layout objects are
restored even if their moved child was collected. Disabled presentation cleanup
retries through the existing Refresh/AfterZoom paths, including outside Galaxy;
separate latches retire only completed work. Re-enable cancels disabled-only
cleanup. Unknown zoom observations no longer appear as measured zoom zero.

Selection now distinguishes failed native POI lookup from observed null, keeps
click-local source custody, rechecks replaced scene/animator/configuration and
blocks the outer fallback after unconfirmed proxy cleanup. Complete source-array
enumeration, immutable proxy identities, bounded candidate/pool policy and
native hit testing remain with their existing owners. Raw and resolved checked
entries must be distinct across recognizer, process and direct fallback; the
same checked snapshot is staged. Presentation's eight mixed callback entries
also reject aliases before staging. These checks decline incompatible layouts;
they do not assert that a live client currently aliases them.

Current Settings already exposes the Galaxy controls. Historical M8 missing-page
claims are retained only as historical evidence. No setting keys, defaults,
store paths, hook groups or independent camera/frame owners were added. System
orbit remains excluded. Pan/momentum, fleet labels, backdrop and Haven are
adjacent preserved behavior rather than newly completed scans.

## Executed offline qualification

The combined Windows suite passed **358/358 build/run/install steps**, not 358
independent tests. Command: `zig build test-mod test-il2cpp test-aai-host
build-tests -Daai-host=true -Daai-source=D:/dev/stfc-mod/third_party/stfc-aai
-Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -Dtest-optimize=Debug -j4
--summary all`, with Zig 0.16.0 and the existing MSVC environment.

Evidence: `D:/dev/stfc-workspace/.ax/map-camera-cohort-20261027/tests-4.log`,
SHA256 `7ce08b06d7f927124b933500ae3f61716a79dab4212d6bf92718cd7c257faa91`.
Controlled fixture executable: `zig-out/bin/il2cpp-bindings-tests.exe`, SHA256
`c7f89722eedf445d01446dd14113d42740d9cf685e2c4c72cf95a46ede677585`.
AAI remains pinned at `4bece2e5f1afa7c7343f009e83398742a0f86114`.

Ten added fresh-process fixture modes cover camera/range/input, Galaxy layout/
marker cleanup, all 15 nonempty overlay combinations and save/highlight
composition, independent major/minor profiles, candidate/dispatch/pool behavior
and raw/resolved entry aliases. Separate fixture processes preserve realistic
metadata cache initialization for missing-member cases. Existing pure policy,
housing dataset, input, Settings persistence, preset, queue, travel/cargo and
production AAI-host regressions passed in the same suite.

Fixtures include the actual consumer source but substitute managed method bodies,
current scene/Config owners and selected service boundaries. Camera UI ownership
and pinch observations use a named facade; these tests do not run real raycast
sorting. Pan shortcut ownership, adjacent orbit/fleet-label notifications and
navigation LOD installation are named host substitutes. The Config replacement
case exercises replacement in the controlled Config owner, not actual in-game
reload delivery. Housing uses the existing production dataset generator.
Ordinary typed value-field checks still use the binding library's size/kind
qualification; this is not exhaustive wrong-value-class metadata proof.

The first three attempts exposed fixture integration errors (generator pointer
argument, inaccessible fixture Config construction, missing adjacent shortcut
stub). They were corrected without weakening production assertions; logs
`tests-1.log` through `tests-3.log` remain failed evidence. Review corrections
were applied before the passing fourth candidate, then independently read back.
No source edits followed that suite; later edits record the results/catalog.

Windows native construction/installation, real Unity layout/raycast/hit testing,
touch pinch, real pooled view behavior and both macOS builds/native trials are
separate qualifications. No current player acceptance is inferred from the suite.
