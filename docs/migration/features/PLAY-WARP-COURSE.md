# PLAY-WARP-COURSE — retained Galaxy course execution

Contract v1, 2026-10-27. Source-preserving reassessment in the eight-feature
travel/cargo batch. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff` on `play-dev-ripper`.
No course algorithm or native movement implementation is replaced in this pass.

## Source and dependency scan

Complete frozen `mods/src/patches/planned_system_warp.{cc,h}` and the relevant
`parts/hotkeys.cc` ExecuteSpaceAction, dispatch, visibility and retained-course
branches were read with their FleetPlayerData, NavigationInteractionUIContext,
NavigationSectionManager, DeploymentManager and star-node/section bindings.
The current implementation is `src/mod/features/fleet_actions.cc`
(`TryPlannedSystemWarp`, `ActOnViewer`) with
`src/mod/planned_system_warp_policy.h` and typed bindings in `game/fleet.h`,
`navigation.h` and `app.h`. Existing pure policy and actual binding fixtures were
read, not treated as proof of the complete native consumer.

Local upstream dev `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` has only the
legacy partial workflow: `mods/src/patches/parts/hotkeys.cc:1568` invokes
`InitiateWarp`, and its lines 1593–1595 call `OnSetCourseButtonClick` with a hidden
control when visibility is not required. Its navigation bindings expose those
native handlers but do not acquire or qualify the retained planned course.
A complete patch/binding search found no `GetPlannedCourse`, position/path
agreement or `OnMoveFleetAction` recovery consumer at that revision. Therefore
upstream presence is **partial**, not equivalent to this qualified capability.
PLAY `cad98ef7`, frozen play-dev `1baa55fe` and the target contain the qualified
consumer. Presence is not behavioral qualification. Primary input is admitted
through the existing hotkey/capture,
focus and section owners. `control.primary_requires_visible_target` defaults
false; hidden Galaxy recovery is deliberately independent of that preference.
No extra setting, hook group, asynchronous planner or persistence owner is added.

## Behavioral obligations

| ID | Frozen operation | Current implementation and verification boundary |
| --- | --- | --- |
| W1 | In Galaxy, a hidden/unknown star-node view uses retained-course recovery; a shown card uses its native InitiateWarp handler. Secondary owns a simultaneous primary/secondary press. | Existing `ViewerEligible`/`RoutePrimary` order retained. Pure fixture covers all 48 routing combinations. Native hotkey precedence still needs a current-client trial. |
| W2 | A hidden Galaxy Set Course control can recover the selected unused ordinary course. A refused hidden action is consumed, never sent to a stale hidden native handler. | Existing navigation/card branches and no-fallthrough behavior retained. Pure routing/refusal fixture covers the decision. |
| W3 | Require native NavigationValidateInput, selected/course fleet agreement, Planned course, Orbit/Null target action, unused Warp, no recall/wormhole/toll, valid path, system target, matching start system/instance. | The existing `Prepare` policy and checked acquisition preserve every predicate. Observed fleet ID zero remains available; no invented positivity restriction was added to fleet/instance identity. Native validator still owns gameplay eligibility. |
| W4 | Require a finite exact destination, positive node ID, matching retained path, projected retained world position within the original exclusive 0.01-coordinate tolerance, and matching system TreeNode when present. A known-null TreeNode is distinct from an unavailable read. | Existing policy and binding paths retained; pure fixtures cover every missing observation, nonfinite coordinates, tolerance boundaries and mismatched identity. |
| W5 | Invoke the game OnMoveFleetAction once with present Nullable<Vector3>, exact signed node/fleet IDs and force_move=false. Native confirmation/network/cost behavior remains native. | Existing named binding/layout checks and runtime-invocation fixture cover ABI and managed failure. A contained failure does not retry or fall through. This is not live course execution evidence. |
| W6 | Do not use a replaced selection/source or retain borrowed native pointers beyond qualified lifetime. | The target's existing operation-local strong roots and final current fleet/context/course/path/position recheck are stronger than the old borrowed traversal; preserved unchanged. Source/context objects do not cross a worker boundary. Full consumer reentrancy and native lifetime remain separately qualified. |

## Decisions and disposition

The frozen optional path did raw reflection and value unboxing. The current
typed boundary already shares signed-value/reference qualification, Nullable
layout checks, native runtime invocation and owned snapshots; replacing it with
the old reflective implementation would lose established safety. The original
qualification predicates and movement arguments are preserved instead.

No new defect in the course policy was established by this scan. Existing
`tests/planned_system_warp_test.cc` is a pure model fixture, while the warp cases
in `tests/il2cpp_bindings.cc` exercise actual binding lookup, value layouts,
reference acquisition, managed exception handling and argument marshaling.
Neither is a real Galaxy course or resource-use trial. The coordinator's batch
record binds rerun results to the final candidate.

Pending Windows trial: select a normal system, close its card, invoke the primary
action; repeat after changing fleet/destination while planning is pending. A
refused/invalid course must not move a different fleet or act on an old target.
Verify shown-card native behavior, secondary priority, and ordinary confirmation
handling without spending travel resources merely to create a failure.
macOS compilation/ABI and native play remain pending CI/Mcohall qualification.

Migration disposition: implemented, independently qualified at policy/binding
layers; complete source scan is current, exact-client course workflow is not yet
proven. Final source/build/runtime checkpoints belong to the batch record, not
this starting-base reference. No repair interlock, new mechanics, UI redesign,
AAI change or overall parity claim follows.
