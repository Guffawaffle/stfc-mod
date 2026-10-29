# PLAY-WARP-MODIFIER — one-use manual warp/jump choice

Contract v1, source scan 2026-10-27; source frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` base
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`. Candidate and final checks are
recorded in the [eight-feature cohort](../FEATURE-COHORT-20261027-02.md).

Read complete frozen `mods/src/patches/parts/instant_warp_confirmation.cc`,
`patches/warp_click_intent.h`, matching policy and native wrappers, input/key
helpers and the current typed move-method binding. The original request producer
is NavigationManager.OnMoveFleetAction, before asynchronous route planning;
AboutToShow is its one-use consumer. No separate frame task or gameplay planner.

Branch consumer presence was observed at upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`, play
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`, frozen play-dev above, and target
base above. Upstream/play/frozen path is
`mods/src/patches/parts/instant_warp_confirmation.cc`; target is
`src/mod/features/instant_warp.cc` and `src/mod/warp_click_intent.h`.
Presence is not behavioral or platform qualification.

| ID | Required operation | Target mapping / verification |
| --- | --- | --- |
| M1 | Ctrl on Windows or Command on macOS plus mouse-left held or release edge captures the manual request before original dispatch. | Existing key polling and mouse availability guard retained; actual production move hook fixture exercises held/release and original exactly once. Mac runtime remains pending. |
| M2 | Retain exact positive signed 64-bit fleet and destination IDs. Resolve fleet `-1` from the selected fleet; unresolved explicit input must not auto-jump. | Typed move entry already qualifies Nullable<Vector3>, Int64 IDs and Boolean. Existing checked selected-fleet lookup retained; request stores scalars only, never a borrowed native object. |
| M3 | Every subsequent native move replaces/clears previous intent, including ordinary or invalid moves. | `WarpClickIntent::Begin` before original; actual ordinary-move reset fixture and existing pure fixture. |
| M4 | First popup consumes before managed reads, including failed/null/unmatched observations. Ctrl may have been released by then. | Existing detach-first `ConsumeWarpChoice`; exact match, positively different trip, missing identity and one-shot fixture. |
| M5 | Matched or unresolved explicit choice preserves the native prompt, while a positively different trip follows its own configured policy. | Frozen outer unmatched/exception handling is represented by the target's existing `unmatched` and `observed` state. No policy rewrite. |
| M6 | Hook installation must not expose a partially enabled automatic path when required input/move metadata is unavailable. | Existing two-hook transaction and mouse-state admission retained. The target intentionally declines the entire feature on missing input; frozen could install auto-confirm without its manual override. |

The source manually expressed platform ABI details and direct detours; target
uses the already-qualified `NavigationManager::MoveFleetActionMethod()` and one
HookSet. Its Nullable<Vector3> layout/signature fixtures remain the independent
binding evidence. No additional detour, request timeout, global history, storage
or input framework was introduced. One outstanding scalar request is bounded;
the next move or first popup retires it. No wall-clock expiration was present in
the source or is invented here.

The actual adapter fixture is `tests/instant_warp_bindings_cases.h`; the existing
pure fixture is `tests/warp_click_intent_test.cc`. They do not establish that a
particular real client's click edge or popup arrives, nor that macOS machine code
has room for a detour. Candidate-bound build and native qualification are the
cohort owner's responsibility. Do not call typed metadata success a native trial.

Native trial: start configured automatic behavior; Ctrl/Command-click, release
before delayed popup, verify choice remains; perform another ordinary move;
switch selected fleets/targets; verify no old override leaks. Observe missing or
unsupported paths without forcing a resource-consuming action. The [automatic
policy](PLAY-WARP-AUTO.md) and [label preview](PLAY-WARP-LABEL.md) retain separate
obligations even though they share one feature owner.

The [October 28 consecutive-click trial](../WARP-CHOICE-RECOVERY-20261028.md)
confirmed modifier release and one-shot consumption. The next popup instead
omitted its attached PlayerFleet; the repair resolves its exact course identity.
Retain the complete interaction sequence in regression coverage.
