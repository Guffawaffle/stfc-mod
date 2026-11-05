# Settings action-row cleanup and reuse

This bounded native Settings continuation starts at clean signed
`0a20d452bff77eb57b4b9b0058100d6c675029fe` on existing `play-dev-ripper`.
Guff's game-control/no-cycle holds remain active. No game UI, deployment, cycle
or configuration edit was performed; no automatic cycle follows the wave.

## Controlled defect

The existing `RestoreAction` checked context at entry, then cleared retained
labels, button interactability and visibility overrides. It did not verify that
the retained components were still attached to that widget after native reads.
A changed field or reentrant context/view replacement could leave cleanup acting
on components now used elsewhere. Completed restoration claims also remained
armed when a different component required retry. Release could restore overrides
while the row was still rendering or invoking its command.

The new controlled `settings-action-cleanup` mode calls the actual production
`ReleaseAction`/`RestoreAction` against typed native metadata. Normal release and
an already-replaced context pass on the unchanged base. The detached-label case
then fails: old cleanup clears the retained label even though the widget now
references a different label (exit 1). This is a controlled lifetime defect, not
an observed live Unity pooling failure.

## Correction and ownership

Signed fix `0a20b50e3c8047f793fc1fd4a76b916a0316290a`:

- Rechecks the registered view and native context after managed observations and
  before restoration. A known replacement ends old cleanup; unknown observations
  retain the revoked, non-actionable view for existing frame-driven retry.
- Restores only the label/button currently attached to the widget and the
  matching button object. A detached component is no longer an eligible target.
- Retires completed or superseded restoration claims. A retry cannot reacquire
  a label override already released, including identical text supplied later.
- Revokes input immediately but defers cleanup while rendering, invoking or
  already restoring. The existing frame service retries after callbacks unwind.

Page visit is deliberately not required for cleanup: a departed page can still
own the same pooled component until native replacement. A null released context
retains the existing cleanup policy. Restoration is not transactional rollback;
earlier eligible restoration can finish before a later observation becomes
unknown or confirms replacement. No new hook, worker, model, setting, serializer
or callback schema is introduced. Release does not close a page-owned editor;
the editor/page owner retains its own cancellation and custody policy.

## Evidence

The corrected mode passes normal release, already-pooled context, detached label
and button, context replacement during text/button/visibility observations,
unknown-context retry, partial cleanup with later identical replacement text,
release during render/invocation, replacement view registration during observation
and managed-root release. Component getters/setters and liveness are controlled;
no actual native editor, publication, rendering or Unity pool is constructed.

Existing `test-il2cpp test-mod` passes **312/312 build/run steps**. Explicit cleanup,
action-dispatch, choice and slider consumer modes all return 0 on the same hashed
executable. Baseline `test-il2cpp` passes 163/163 while the new cleanup mode fails
at the detached-label assertion. Step counts are not test counts. No build-target
or CI registration was added; the new mode runs explicitly.

Clean/stable Windows AX releasedbg at the fix commit passes 41/41 steps
and proxy verification, retaining clean AAI 4bece2e5/UI Lab. Candidate DLL SHA256:
`C9AAF51C2DB146E3EA32D7DEA764EA83C3EFDA01F2B5E7698E8952343CDDDDCD`.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-action-cleanup-20261101/`,
including before/final command/cwd/base/duration/exit and executable hashes/logs,
final-inputs.json, fix-checkpoint.json, native-build-command.json/receipt and the
preserved DLL/PDB. Changed production remains confined to settings_actions.cc;
the page-owned text-entry files are hashed and unchanged.

## Qualification and next slice

Candidate is local and undeployed. Native Windows interaction and Mac build/runtime
qualification remain open for this correction. SET-RESET-SAVE and P23/P24 stay
open; component-model evidence does not establish complete live pooling/editor
acceptance. No installed artifact or published prerelease was changed by this work.

Next local slice: render/rebind recovery after deferred cleanup, including a
newly bound row following retirement. Keep the control/no-cycle hold until Guff
lifts it. Any later player check is operator-driven after separately requested
deployment. Mac Command-Q retesting and deferred file-selector Clear UX remain
independent.
