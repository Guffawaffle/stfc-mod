# PLAY-WARP-LABEL — owned WARP/JUMP visit-label preview

Contract v1, source scan 2026-10-27. Frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` base
`a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`. Candidate-bound results are in the
[eight-feature cohort](../FEATURE-COHORT-20261027-02.md).

Read complete `mods/src/patches/parts/warp_action_label.cc`, its shared
instant-warp policy, ship-name matching, frame callback registration and native
button/localizer wrappers. Target retains this consumer in
`src/mod/features/instant_warp.cc`, sharing automatic-policy resolution and the
existing single frame owner. This is presentation only: no click handler,
resource cost, context, timer or course data is modified.

Source consumer presence is established at upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`, play
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`, and frozen play-dev above, at the
original path; the target implementation exists at the base above. Those are
inspected local revisions, not claims about moving branch heads or runtime parity.

| ID | Source behavior | Target mapping / verification |
| --- | --- | --- |
| L1 | At most ten label updates per second on the existing frame dispatcher. | Existing throttle and frame callback retained; no new UI detour or per-frame idle work. |
| L2 | Use visible/active ordinary visit button, selected fleet and native instant-warp ability. Cached popup CourseData is not a preview source. Toll/wormhole controls retain native text. | Same button path and native availability; checked field/reference acquisition and temporary roots. Fleet/card/button identity is rechecked after managed localization before publishing a preview. Real control variants require native trial. |
| L3 | Resolve through the same ask/jump/warp/default policy; modifier-held or all-ask configuration returns the native localized label. | Shared `ResolveConfirmation` retained. No separate title-based policy. Existing early skip avoids fleet lookups when no automatic decision is configured. |
| L4 | Never take over an already overridden label. Remember only one weak label handle and the text we wrote. | Existing ownership model retained; now override flag and CurrentText are checked observations. Actual production owner fixture covers acquisition, unchanged update and external override refusal. |
| L5 | Clear only if live label still has our exact override; release ownership even if cleanup fails. | Temporary strong custody spans getters/cleanup, then weak handle reset. Fixture checks external replacement preservation, failed CurrentText, failed setter and complete temporary-root cleanup. |
| L6 | Label replacement, hidden controls, unavailable assets/readings and policy disable cannot leave an action bound to another ship. | No action is bound at all; unavailable preview clears qualified owned text. Native label pooling, visibility, focus and actual frame cadence remain runtime qualification. |

Frozen and initial target read ownership/current text as default-valued fields or
methods. Failure could be confused with external-override=false or empty text.
This pass deliberately refuses mutation when those observations are unavailable.
It also acquires strong custody while calling managed label methods rather than
relying on the retained weak reference after releasing its handle. Failed setter
invocation does not manufacture successful ownership; no retry through another
method. These are bounded correctness repairs, not a new label cache/pool.

One weak handle may survive between updates. Temporary roots cover source,
widget/button, fleet/managers, label and managed string only during calculation
or binding. The process-lifetime owner avoids GC calls after runtime shutdown,
as already established in the target. Existing English WARP/JUMP literals are
preserved; no localization redesign is included.

`tests/instant_warp_bindings_cases.h` exercises the production OwnedLabel and
checked native calls using controlled metadata and method bodies. It does not
claim full InstantWarpFrame/native Canvas qualification. Native trial should
change fleet and filters, hold/release modifier, reopen pooled system cards and
visit toll/wormhole controls. Confirm only the owned ordinary label changes,
while native availability, button behavior and costs remain unchanged. Windows
player confirmation and macOS CI/Mcohall testing are independent of compilation.
