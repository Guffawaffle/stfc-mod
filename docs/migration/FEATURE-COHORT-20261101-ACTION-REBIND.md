# Settings action rendering and rebinding after deferred cleanup

This bounded continuation starts at clean signed
`032795f810defa926514281d3e5bb5e631596587` on existing `play-dev-ripper`.
Implementation: signed `bb5fef8bba733e0c36dd45cb2231315cdd77ef09`.
Guff's game-control/no-cycle holds remain active. No game UI, deployment, cycle
or configuration edit was performed, including no automatic cycle after the wave.

## Controlled defect

The previous release correction revokes a row immediately and defers component
cleanup while its render or command callback is active. A native SetWidgetData
inside that callback still calls the game original once, but the old registered
view prevents the mod from binding the replacement. The later frame retires the
old view without reconstructing the new one. Labels and commands consequently
remain unavailable until another native bind.

The controlled `settings-action-rebind` mode calls the actual production hooks,
RenderAction, InvokeAction and ActionsFrame over fake typed components and owned
native callback metadata. On the unchanged implementation, ordinary bind/render
and invocation pass; the post-cleanup replacement assertion fails (exit 1).
The initial fixture omitted the Context property; that setup error was corrected
before the recorded defect run. This is component-model evidence, not a live
Unity pooling reproduction.

## Change

The retiring view retains a pending binding with exact request, registered row,
page visit and adapter epoch identities. An inert view reservation supplies the
same ordering for an initial bind. The hook still calls the native original once.
After cleanup and callback unwind, the existing frame path acquires the current
components, installs a fresh view and renders it without replaying the original.

Unknown cleanup/component observations retain revoked, non-actionable custody
for retry. Completed cleanup is not repeated while waiting for components.
Newer nested binds supersede old requests. Native release, page departure and
adapter invalidation cancel recovery; a throwing native original propagates
normally and cannot leave an unprepared request. A frame whose ownership read
installs another view cannot subsequently release that newer view.

Production changes stay within settings_actions.cc. No new hook, worker,
callback schema, config/default, writer or editor-lifetime change is introduced.
The existing renderer supplies presentation after recovery; broader render-time
component ownership remains a separate next slice. Page-owned text-entry source
files are hashed and unchanged.

## Local verification

Existing `test-il2cpp test-mod` passes 312/312 build/run steps. Explicit
rebind/cleanup/action/choice/slider modes pass. The rebind cases cover active
rendering and invocation, unknown-read retry, late replacement components,
superseding/nested requests, a stale frame snapshot, native-original failure,
release/page/epoch cancellation, no replay and managed-root release.
Step counts are not test counts. No CI or build-target registration was added.

Clean/stable Windows AX releasedbg at the implementation commit passes 41/41
steps and proxy verification. Candidate DLL SHA256: `76F1D8B89EAC8B0742CC8FD8E886A184D9331A361E97AAD0891AEAC1D5D44E5D`.
The native Windows build passed; live native Windows interaction and Mac
build/runtime qualification remain open.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-action-rebind-20261101/`.
It retains the baseline fixture, before/final runtime command/cwd/base/duration/
exit and executable hashes, exact final input hashes, signed checkpoint,
native build command/receipt and preserved DLL/PDB. The baseline incremental
compile duration was not captured and is explicitly null in its receipt.

## Qualification and next slice

The candidate and signed commits remain local and undeployed. No installed
artifact or published prerelease was changed. SET-RESET-SAVE and P23/P24 remain
open; these fixtures do not establish complete native pooling/editor acceptance.

Next local slice: check that render-time label/button observations still own
the attached components after managed reentry. Keep page-owned editor custody
and the no-control/no-cycle hold. Later live checks are operator-driven after
separately requested deployment. Mac Command-Q retesting and deferred
file-selector Clear UX remain independent.
