# Settings action-button ownership

This native Settings continuation starts at clean signed
`a5f9c142ff3657b1b7249b705cbeeee762f3cb8a` on existing `play-dev-ripper`.
The preceding choice/slider fix and its notes were already committed. Guff asked
to move next, retained game control, and reinforced **no cycle while in a wave**.
No game UI, deployment, cycle or configuration edit was performed.

## Controlled dispatch defect and correction

The typed action owner already uses private closed callback schemas and a unique
managed token per row. It checks parent, query, callback list, action/index,
rendered presentation, shortcut capture and synchronous invoking-widget custody.
Controllers retain their own reset/save/editor guards; these are not replaced.

`SameAction` nevertheless checked the current page before managed ownership
observations and retained the context read before them. If a lookup reentered
code that departed the page or replaced the widget's context, the predicate
could still approve the old command. The second check in `InvokeAction`, after
reading current presentation, had the same gap.

The new controlled consumer invokes the actual production `InvokeAction` over
matching typed metadata and private callback copies. Its presentation read arms
a page departure during the final callback-list lookup. Against unchanged
production, the departed command still invokes and the explicit mode returns 1.
This is a deterministic controlled reentry failure, not a live game reproduction
or proof that a particular reset/save operation changed a player's preferences.

Signed fix `8bb7c00571aa2af9144f2d530b0ecf92bb762219` re-reads native context after
ownership observations, then checks the same row registration, page visit,
revocation, managed custody and view registration. The existing token/action/
index checks remain. Both rendering and dispatch use this predicate. No new
detour, menu, callback schema, setting default or persistence behavior is added.

## Local verification

`settings-action-consumer` covers one owned invocation, exact row index,
synchronous invoking-widget restoration, self-reentry suppression, foreign token,
page refresh, shortcut capture, page departure/context replacement/view retirement
during ownership reads, row retirement during the final context observation,
changed save-status presentation, foreign callback ownership, subsequent recovery
and managed-root release. Publication, rendering and actual Unity scheduling
remain controlled boundaries. A generic command sink counts dispatch; it does
not perform a real reset or disk retry.

Baseline `test-il2cpp` passes 163/163 build/run steps, while the explicit new action
case fails at its departed-page assertion. Corrected `test-il2cpp test-mod` passes
**312/312 build/run steps**. Explicit action, choice and slider consumer modes all
return 0 on the same hashed executable. These are step counts, not test counts.
The new mode is run manually; no build-target or CI registration was added.

Clean/stable native Windows AX releasedbg at the fix commit passes 41/41
steps and proxy verification, retaining clean AAI 4bece2e5/UI Lab. Candidate DLL
SHA256: `34EE3EB7207E31BFA065100BC9A5A144ABCA31CA12D25241474B4EF6648BCA38`.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-actions-20261101/` includes
before/final command/cwd/base/duration/exit, executable hashes/logs, final-inputs,
fix checkpoint, pinned source owners, native build receipt and preserved DLL/PDB.
Frozen producer paths are `mods/src/settings/action_setting.h` and
`mods/src/settings/native/action_widgets.{cc,h}` at 1baa55fe. The target retains
the existing typed adapter; the change is the post-observation identity guard.

## Qualification and next slice

The candidate remains local and undeployed. Native Windows interaction and Mac
build/runtime are unqualified; installed f53b07ea and preview039 are unchanged.
The game-control/no-cycle hold remains in force until Guff lifts it. A later
operator-driven smoke can open Details, leave/reopen Settings, and open an
individual Reset then Cancel. No whole-page reset or induced real-file conflict
is needed to check ordinary button behavior.

SET-RESET-SAVE and native adapter/catalog tasks stay open; controlled dispatch
coverage does not establish complete controller, rendering or pooling parity.
Next bounded local slice: action-row presentation cleanup on release/reuse,
preserving page-owned editor lifetime. Keep manual trials, Mac Command-Q and
deferred Clear UX separate; do not cycle automatically when the wave ends.
