# Kir'Shara early queue clearing science

This science branch enables capture and an experimental address guard by default,
in release and debug builds.
Logs are `community_kirshara_queue_<session>.jsonl` in the game's working directory.
Three rotating files of 3 MiB each, a 4096-event asynchronous buffer, and a 50,000-span
session limit bound capture. Preserve all rotations promptly after a reproduction.
Unknown storage/layout is recorded as unknown. The log reports dropped asynchronous
events and the session limit; missing records cannot prove that a path did not run.

Capture observes native add, clear, per-instance clear, target processing, cleanup,
fleet state changes, queue enable changes, planner, watchdog, and disposal paths.
Existing recovery/protection detours own their methods and share the observer, so
the science branch does not hook those methods twice. The session lists every hook
and whether it installed or shares a feature's detour. Queue contents, retry counts,
engagement state, last attempt time, pending target, and last engaged target are
sampled before/after calls. Nested span IDs, native stack addresses, feature settings,
and explicit mod interventions distinguish native pruning from mod actions. Fleet
state inputs are sampled where metadata permits, without invoking game getters.
The address-mismatch check also records its native result, the player's exact
galaxy/system/planet/instance address, and the target lookups the native check actually
performs. The address getter and deployment lookup observers are gated to queue
operations rather than high-frequency UI availability polling. During a player-state
event for a validated nonempty owned queue, the lookup also obtains that player's
own current deployment once. A different ship being moved or recalled in another
system is the current reproduction trigger.

The captured failure is a transient base address in A's player-state data while its
queued hostile remains in A's original system. The guard substitutes the current
owned deployment's address only inside that native predicate, only when A's owned
queue is validated, and only when the owned deployment and the current target agree
on the complete address. The bad player address must be a base planet in another
system in the same galaxy/instance. Player and deployment states must be active in
space; recall planning, removal, destruction, dock and warp evidence are ineligible.
Unknown fields or a missing deployment retain native behavior. Every target still
passes the native check; a truly foreign target still invalidates the queue.
No fleet fields or queue contents are rewritten, and no previous-frame address is
cached. Explicit clear and destroyed-state clear bypass this substitution.

`STFC_MOD_KIRSHARA_ADDRESS_GUARD=0` disables substitution while keeping capture.
Logs record raw `player_address`, `player_location`, `own_deployment`, and, when used,
`address_guard_applied` plus `effective_player_address`. Reproduce the cross-system
B move/recall and confirm the guard note and a surviving queue. Then explicitly
clear A, recall A, and move A to another system to validate legitimate invalidation.

1. Build a queue with several targets. Note fleet, expected remaining targets, time,
   and what happened immediately before the early clear (combat, travel, recall,
   target disappearance, changing screens, or toggling the queue).
2. After it clears, preserve the current JSONL and both rotations. Run
   `python scripts/analyze-kirshara-queue.py <session.jsonl> <session.1.jsonl> --output report.json`
   using only files that exist from that session. The report shows verified removed
   target IDs, emptying versus partial pruning, entire queues detached from the
   manager, nested callers, and evidence gaps. A detached queue's former targets
   are recorded without claiming its inaccessible list storage was emptied.
3. Repeat with Faster Queue Recovery disabled in Mod Settings, then with Thin Queue
   Protection also disabled (`[control] thin_queue_protection = false`, restart
   required by its existing installer). These are mod-feature controls, not proof
   of behavior in an unmodified client. Keep the queue enabled and retain each
   session's logs. Restore settings after the comparison.
4. For a science-off comparison, launch with `STFC_MOD_KIRSHARA_TRACE=0`.
   This disables both capture and this science branch's experimental guard.
   A genuinely unmodified client run is a separate comparison and has no mod capture.

Current Windows client 271 method signatures and native hook extents were checked
against the installed GameAssembly and matching local dump. All new detours use
object/scalar parameters, including the substantive instance-clear body. Existing
Windows course-response hooks additionally capture response success/recall and
recovery decisions. The shared observer compiles on macOS by design, but current
Mac ARM64/Intel builds, native hook fit, and runtime capture require tester validation;
no direct Mac value-type course-response detour is added. Test this exact source on
each Mac architecture: boot, inspect hook coverage, add/clear targets, reproduce the
early clear, and analyze the session. Native stacks require the matching client dump
and loaded image base to symbolize; raw addresses alone are not causal proof.

The science branch makes no queue edits and does not restore the retired
combat-completion repair. Address-substitution provenance and unmodified-client
recurrence remain unverified; the exact native mismatch clearing path was captured.
