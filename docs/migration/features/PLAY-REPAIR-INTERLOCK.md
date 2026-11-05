# PLAY-REPAIR-INTERLOCK — historical repair protection follow-up

Discovery only, 2026-10-27. Guff asked to revisit the repair bug during the fleet
cohort. This does not authorize restoring an old workaround without qualifying
the current client or changing the frozen migration source.

Historical promotion `0ba8de71c459fa66f1343077afeaf1f36c996edb` contains
`mods/src/patches/parts/repair_action_interlock.cc`, its pure policy header and
`tests/src/test_repair_action_interlock_policy.cc`. Its three seams were:

- `FleetPlayerData.GetActionStatus(ActionType)`: retain coherent repair progress
  when status briefly regresses to Ready while still Repairing.
- `ActionElementWidget.GetInstantButtonContext()`: a bounded post-completion
  presentation hold.
- `ActionElementWidget.OnInstantButtonClickCallback()`: suppress stale repair
  instant clicks during that hold rather than reaching a paid/error path.

The historical policy uses a 2.5-second hold and a 16-fleet state owner. These are
historical implementation choices, not verified current-client requirements.
Performance notes are at `c6a80722a77733f22d768fde52302dc55a96b128`.

The source is an ancestor of frozen play-dev `1baa55fedf1758b19f843cc59707b8901a0b4998`,
but the interlock files, registration and setting are absent from that frozen
tree. The reconciliation merge `5beebf4bb9a69b61e224081f0ac1c69e62c34398` already
lacks these paths. Its message retains the former fork tree as a history parent
while selecting a current-dev PLAY tree; that establishes where the source
disappeared, not whether omitting this particular feature was intentional.

The current keyboard repair/help dispatch has a different purpose. Restoring its
frozen short-circuit order does not implement these three guards and must not be
reported as fixing the repair bug. No interlock runtime implementation, test,
deployment or current-client correctness claim is made by this discovery.

Next: examine the original complete feature/tests and current native repair
state/context/click seams, determine whether the original fault still exists,
and decide a bounded fix using checked bindings. Keep this separate from the
present queue-recovery repairs.
