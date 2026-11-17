# Reveal and warp confirmation qualification

Source: existing Ripper Downstream `play-dev-ripper`, starting clean at signed
`d19bbdcc09663ced90ec742672331aa622076d8b`. Implementation: signed
`a05b40173480baf0e41c69885fbe4dc59032ac3f`. Pinned PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296` and selected frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998` remain the behavior references.
No branch/worktree change. Settings passes remain stopped.

## Bounded corrections

Reveal and instant-warp consumers were already migrated; this is corrective
qualification, not a newly discovered missing feature. The frozen reveal owner
limits auto-confirmation to ActivatedAbilityType.RevealHostileVger86002 and
preserves original ability ID, TimeSpan and callback on fallback. Warp retains
always-ask, auto-jump, auto-warp, then default precedence; modified-click choice
and exact course-fleet recovery remain their existing owners.

The reveal hook checked its opt-in before native type lookup. That lookup could
disable the preference while the hook still invoked the action. The corrected
hook roots the callback and manager, qualifies a zero-argument void Action with
a usable native/runtime entry and instance receiver when needed, and rechecks
the live preference and bound method/target identity after lookup. Valid static
Actions remain supported. Changed/malformed/missing callbacks, unknown ability
evidence and other ability types retain native handling.

The warp popup resolved its mode/filter decision before final fleet/context
reads. A preference or filter change during those reads did not invalidate the
earlier answer. The corrected hook copies only the default and three filter
values and compares them after all managed observations. Any changed policy
leaves the native choice available; it is not reinterpreted or auto-retried in
the same callback. Final widget binding and instant-warp availability reads now
follow the final context getter, so that getter cannot invalidate availability
behind an earlier successful observation.

These are controlled reentry corrections, not established live user-reported
regressions. No new hook group, ability/travel policy, resource/cost mutation,
default, shortcut, writer, persistent owner or diagnostic probe is introduced.
Existing callback-failure native fallback remains: a callback may have partial
effects before throwing; no transactional action guarantee or retry is added.

## Verification

New explicit modes execute the actual production hooks in the existing typed
managed host. Unchanged production independently fails `reveal-confirmation`
when its type lookup disables the option, and `instant-warp-targeting` when the
final fleet lookup changes the policy (both exit1). Corrected cases pass live
mode/all/name-filter changes, final context/availability changes, stable retry,
callback shape/replacement, valid static Action, missing/unknown ability data,
full64-bit ability/TimeSpan fallback, callback failure and managed-root release.

Six explicit modes pass: reveal-confirmation, instant-warp-targeting,
instant-warp-native, instant-warp-missing-override, confirmation-targeting and
shop-claims-filter. The existing native-warp fixture includes modified-click
intent, exact course-fleet recovery, names/priority and label ownership. All
ability/travel/popup actions in these fixtures are counted substitutes.

Existing `test-il2cpp test-mod` passes 312/312 build/run steps. Clean/stable
Windows releasedbg passes 41/41 steps and proxy verification at the implementation
commit. Candidate DLL SHA256: `035DB379FB7F97BA07FC20FDDF40EA5B2C2AFA1D0353A7D50B4C453B63DC3035`.
Step counts are not test counts; no CI/build-target registration changed.

Evidence: `D:/dev/stfc-workspace/artifacts/reveal-warp-confirmation-20261101/`,
including baseline fixtures, before/final command/cwd/base/duration/exit and
executable hashes, exact final inputs, signed checkpoints, native receipt and
preserved DLL/PDB. Config defaults/shortcut definitions and the previous daily
filter source are hashed and unchanged.

## Qualification and continuation

Candidate and source checkpoints remain local/unpushed and undeployed. No game
input, cycle, TOML edit, travel/resource spend or ability activation occurred.
Installed artifact and published prerelease were not changed by this work.
Exact-candidate native workflows and Mac build/runtime qualification remain open.
Do not spend jump resources solely to qualify this correction.

PLAY-CONFIRMATIONS and PLAY-WARP-AUTO retain their overall classifications.
P19 remains in progress. Next local source slice: HUD visibility restoration
and cargo presentation/counters. Settings passes and parked work remain stopped.
