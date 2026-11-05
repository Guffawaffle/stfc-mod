# PLAY-FLEET-ACTIONS — contextual fleet actions

Contract v1, 2026-10-27. Inputs and final qualification are in the
[fleet cohort](../FEATURE-COHORT-20261027-01.md). Source is frozen `1baa55fe`;
target starts at `fa3860a7`. This reassessment preserves the older inventory.

Read source: `mods/src/patches/parts/hotkeys.cc` (dispatch, ExecuteSpaceAction,
DidExecuteFleetAction and viewer helpers), mapkey/key, ActionQueueManager,
FleetBar/Local/PlayerData, viewer/button/context headers, base-card and retained
planned-system-warp consumers. Queue switch lives in `parts/testing.cc`; its
Settings/save ownership is in preview/upstream settings and runtime_config.
Target maps to `features/fleet_actions.cc`, `hotkeys.cc`, typed game bindings and
the existing registered writer. Discovery tow/retained warp are supporting paths
already qualified separately; this batch does not rewrite them.

| Obligation | Preserved/adapted operation | Evidence / limit |
| --- | --- | --- |
| F1 | Queue clear precedes recall cancellation, then pre-scan/mining/star-node/navigation/armada, then recall/repair. Native methods retain eligibility/commands. | Complete source/call-order comparison; full native workflow pending. |
| F2 | Primary visibility preference is independent of secondary scan. Open mining overlay takes priority; visible armada joins from its own card. | Existing native binding paths retained; delayed replay cannot transfer to a different mining card. |
| F3 | Queue press requires native unlock, capacity, button/listener and appropriate target. | Native unlock control tested; target button/capacity interaction remains native trial. |
| F4 | Recall only for idle/impulse/mining/capturing; cancel for charging/warping. Repair only docked/destroyed; already repairing can ask help. | Actual RequestFleetAction tested for repair success, rejection, unsupported state and help. No extra help after successful normal repair. |
| F5 | Preserve base-card recall retention and existing checked retained-course travel instead of calling a hidden destination handler. | Supporting source and binding readback; no travel implementation changes. |
| F6 | Target-loading retry cannot cross ship/card/context, mutate a queue request into attack, or survive skipped frames. | Bounded three-root request, exact next-frame gate, immutable IDs and liveness/visibility checks. Actual helper/custody tests; native race trial pending. This deliberately fixes inherited force-flag behavior. |
| F7 | Queue toggle uses existing live setting and registered save, resets correlations, and cannot unlock native-locked queues. | Production shortcut/unlock fixture substitutes only input/save sink; real Settings writer tested separately. |

No new combat or repair mechanics. The historical stale-instant-repair interlock
is a separate [discovery](PLAY-REPAIR-INTERLOCK.md). C++ dependent-template
disambiguation added while compiling the complete adapter into the fixture is
syntax only; it does not change retained warp behavior.

## Mission-marker primary action — 2026-11-01

An in-system mission-location popup owns a separate native GO button. Primary
action previously tested the ordinary set-course widget and called its handler,
so a visible mission popup with that widget hidden could do nothing. The typed
consumer now reads the navigation controller's mission widget and presses its
native button only while the mission widget is shown and active. The button
retains native eligibility; an unavailable or rejected mission action cannot
fall through to an ordinary destination. Secondary action is unchanged.

The current Windows client 273 assembly, metadata and dump match their recorded
hashes. The production-dispatch fixture reproduces the failure before correction
and passes afterwards, including hidden/inactive cards, missing buttons, native
rejection and ordinary navigation. Binding/gameplay checks pass 312/312 build/run
steps. This is local controlled-runtime evidence; live mission-marker travel is
pending. Both local Mac builds stop at missing SDK discovery. No hook, setting,
configuration or CI registration was added.
