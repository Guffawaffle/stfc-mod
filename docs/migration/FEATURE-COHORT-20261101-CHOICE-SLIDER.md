# Choice/slider ownership and save/reset presentation

This SET-VALUES/P24 continuation starts from clean signed
`2d03621d92adeb4f0ab68e6e6998953a55b8a11d` on existing `play-dev-ripper`.
Guff requested code/local work and retained control of the game. No game UI,
deployment, cycle, configuration edit or live player test was performed.

## Correction at the typed native boundary

`SameValue` in `features/settings_values.cc` checked the page/view before managed
context and callback-ownership observations. Those observations can reenter native code.
The old predicate could therefore return true using a context read before a
replacement, or a page/view check made before retirement. The owned choice
callback then accepted an input and scheduled a preference save for the stale row.

An added controlled production-consumer case demonstrates this failure: the
callback-list observation arranges replacement during the next context getter;
the getter returns its earlier context. Against unchanged production at the base,
the old choice changes and persists despite the replacement (fixture exit 1).
This establishes a controlled reentry defect, not a reproduced live game race.

Signed fix `83369f3ba89facfa7bce2a1fc27ecb54469519f2` rechecks native context,
page visit, revocation, managed custody and the registered view after ownership
reads. Both choice and slider consumers use the same predicate. Normal selected
value, snapping, precision, defaults, serializers, writer policy and hook ownership
are retained. There is no additional detour, platform-specific branch or menu.

The manual `settings-choice-consumer` mode uses the actual production callback
and typed ownership metadata. It covers single selection, harmless radio uncheck,
one verified save, render suppression, departed page, reentrant context replacement,
page departure and view retirement during ownership reads, multi-choice refresh
suppression/whole-mask save, and foreign pooled-row native delegation. It releases
its admitted managed roots. Native publication/rendering and real IL2CPP/game
scheduling are substituted. No build-target or CI registration was added; the
new mode runs explicitly.

## Retained value and persistence contracts

Existing model fixtures preserve whole-value stale conflicts, finite/range checks,
slider snap only on explicit input, full-precision loaded values and presentation
rounding. Existing metadata checks distinguish Pending/Saved/Conflict/Failed/
Unavailable/SessionOnly, requested restart state and active state. A failed or
unverified application cannot claim a successful save; explicit retry persists
the current verified value without replaying the feature mutation.

Maintenance still deduplicates aliases by TOML key, prefers the full-range exact
numeric owner over its convenience slider, prepares guarded individual/page
resets, and rejects cancellation/stale observations before applying. Page reset
is not transactional rollback. These shared model/save/reset owners are retained;
the production change is confined to native row identity. These six model/view/
maintenance headers match frozen 1baa55fe byte-for-byte (model-comparison.json).

## Local evidence and limits

The baseline binding suite passes 163/163 build/run steps, while the new explicit
choice mode fails at the stale-context assertion. With the correction, existing
`test-il2cpp test-mod` passes **312/312 build/run steps** and the explicit choice
mode returns 0. Step counts are not test counts. Existing controlled slider
change/label cases and Settings metadata/maintenance/persistence fixtures also
run in that combined suite.

The clean/stable Windows AX releasedbg build at the fix commit passes
41/41 steps and proxy verification. Local AAI/UI Lab are retained.
Candidate DLL SHA256: `6A5C55427FFEBA9E621C452718B0B1FB9DB9A54AF978B4BEDEED2C22A1D6F61A`.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-choice-slider-20261101/`,
including before/final command/cwd/base/duration/exit and executable hashes,
final-inputs.json, model-comparison.json/pinned source models, fix-checkpoint.json,
native-build-command.json/receipt and preserved candidate. The baseline failed
consumer executable is retained by its cache path/hash; final source inputs bind
the expanded consumer cases to the signed correction.

No native Windows interaction or Mac build/runtime qualification is claimed for
this correction. The installed DLL is not replaced. Guff's game-control hold
remains active; any subsequent native check is operator-driven. SET-VALUES stays
Partial and P24 stays open. Mac Command-Q and deferred file-selector Clear UX
remain independent.

## Short operator check after deploying the candidate

1. Search Mod Settings for **Major system labels** and note the current mode and
   **Major label zoom threshold** value.
2. Choose **Threshold**, move that slider slightly, and check that exactly one
   mode stays selected and the slider/Edit readout stays clean. Defaults & Help
   should show the actual save status; pending must not be presented as saved.
3. Leave/reopen the page and check the selected mode/value. Open that setting's
   Details/Reset dialog, then Cancel; the value must remain unchanged.
4. Restore the original threshold using Edit and restore the original mode.

Do not use the whole-page Defaults reset for this bounded trial. Report any
unexpected selection, value change, save message or dismissal. This normal-flow
check cannot prove all reentrant ownership paths covered by the controlled case.
