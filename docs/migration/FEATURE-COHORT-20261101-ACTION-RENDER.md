# Settings action rendering with current components

This continuation starts at clean signed
`37ca2c77c3593cc6b1a2518ae8c91182a67a198d` on existing `play-dev-ripper`.
Implementation: signed `223c2534f14669ebdf01f8d270cb8b8d39a12871`.
Guff authorized a quick combined game test after local checks, lifting the prior
game-control/no-cycle hold for this test. Existing checkouts/branches were retained.

## Controlled defect and correction

The renderer previously checked widget/context ownership after managed reads,
but retained label/button pointers could have been detached in those reads.
The new explicit `settings-action-render` mode uses the actual production
renderer, hooks and frame recovery with fake typed components. On the unchanged
renderer, ordinary bind/invocation passes and a detached-label write assertion
fails (exit 1). This is controlled evidence, not an induced live timing race.

Rendering now checks the three attached label fields and button after managed
observations, checks label liveness and confirms the button's current object
before visibility/interactability work and readiness. A confirmed component
change revokes the presentation and prepares the existing deferred-binding path.
Cleanup finishes after rendering unwinds; frame recovery acquires current
components without replaying native SetWidgetData. Unknown observations leave
the row non-actionable for retry. Rechecks also reject a stale gameObject result
when its getter detached that object.

Production remains confined to settings_actions.cc. No new hook, worker,
callback schema, config/default, editor-lifetime change or writer change.
Page-owned text-entry source files are hashed and unchanged.

## Verification

Existing `test-il2cpp test-mod` passes 312/312 build/run steps. Six explicit
Settings modes pass: render, rebind, cleanup, action, choice and slider. New
cases cover detached labels/buttons/visibility objects, stale association getter,
unknown observation retry, component rebinding without native-original replay,
recovered command admission and managed roots. No new CI/build-target registration.

The clean/stable Windows releasedbg build at the implementation commit passes
41/41 steps and proxy verification. Candidate DLL SHA256: `2A66BE66B32E9C88257A7403B3529F4E23C8BC2FBB5A01EEFA54DB3D8438FF37`.
The ordinary AX Zig cycle checks the source artifact before shutdown, deploys
that artifact, restarts the game and observes fresh startup. The installed
artifact and loaded process identity are recorded separately in runtime evidence.

One authorized AX Zig cycle loaded the exact combined Windows DLL; native navigation and reused rows, keyboard zoom slider500->600->500 via exact editor, collection editor cancel, Notifications reset-confirmation cancel/help commands, and Q Trials choiceAlways->Auto->Always passed. Settings TOML is byte-identical before/after. Returned to station; no further input. No binding/presentation errors in1961 current-process log lines; only beta notice and existing deprecated-key warnings. Timing-specific replacement/deferred-cleanup races remain controlled evidence, not live-induced.

Additional observations: SHIFT-S/search-header input produced no visible effect
under automation; dragging the category list navigated to Camera. Their causes
are unestablished and manual recheck remains open. Shortcut editing was not part
of this smoke. AX session parsing returned zero lines; a bounded raw log read
from the verified process startup supplied the current-session review.

Source evidence: `D:/dev/stfc-workspace/artifacts/settings-action-render-20261101/`,
including before/final runtime receipts and executable hashes, exact final source
hashes, cycle/build receipts, native candidate, prior installed DLL/settings and
the bounded live-check record. Command receipts retain cwd, commit, duration,
exit and bounded logs. Source commits and notes remain local/unpushed.

## Remaining qualification

The combined Windows walkthrough does not induce the precise callback timing
covered by the controlled fixtures. Mac build/runtime and Command-Q retesting
remain open. SET-RESET-SAVE and P23/P24 retain their open statuses; no complete
cross-platform parity claim is made. Published prerelease unchanged.

Next source slice: presentation validity at command dispatch when components
change between frames, preserving native callback and page-owned editor custody.
Deferred file-selector Clear UX remains independent.
