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
| Swap-ship technology indicator template | Reuse a weak, Unity-liveness-checked native template. Retry a missing template no more than once per five seconds; preserve the existing tile-source fallback. |
| Active navigation-background experiment | Reuse the selected loader while valid, checking its current artwork on normal refreshes. Rediscovery after loss is limited to once per five seconds; clear artwork owned by a destroyed loader. |
| Haven water root | Reuse a weak, liveness-checked native root and traverse only its hierarchy. Missing-root discovery is limited to once per five seconds. |

These caches hold at most one object each. Native destruction is checked even
when a managed wrapper remains. Unity work remains on the main thread; moving
Unity APIs to workers is not a safe remedy. No new native hooks are installed.

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
