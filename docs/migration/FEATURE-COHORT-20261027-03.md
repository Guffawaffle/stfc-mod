# Proposed map and camera cohort — 2026-10-27

**Preparation only.** No feature implementation, tests, builds, deployment or
game cycle were performed for this proposal. The accepted travel/cargo build
remains running. This is a scoped next batch, not an update to migration status.

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

## Execution sequence after the Runner helper is ready

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
