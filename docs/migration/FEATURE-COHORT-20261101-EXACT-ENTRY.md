# Exact-value editor continuation

## Scope and source mapping

This bounded SET-VALUES/P24 assessment continues from clean signed
`572d81a7f97b76657081ba9b06f065a8ca9adc2c` on the existing `play-dev-ripper`
checkout. The existing implementation is retained; no production, test, build,
configuration or hook change is required by the observed sequence.

Frozen source remains `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Its `mods/src/settings/native/text_entry.cc` owns the native editor, validation,
explicit Apply, cancellation and dismissal-key drain. The named owner is also
present in play `cad98ef7990dd66167eb64ea5dda525c1fe2c296`. Upstream dev
`ff179589f834b95331b4d7e38b1054fb6ff5ace9` has no owner at the inspected target
path `src/mod/features/settings_text_entry.cc`; this is a named-path observation.

| Responsibility | Existing typed owner |
| --- | --- |
| Native objects, TMP input, closed callbacks, rooted custody, Back/EventSystem holds and restoration | `src/mod/game/settings_text_entry.h` |
| One editor, current page visit, focus loss, Escape/Return, shared frame and shortcut-capture drain | `src/mod/features/settings_text_entry.cc` |
| Draft validation, exactly one explicit commit, cancellation and replacement request identity | `src/mod/settings/native_text_entry.h` |
| Numeric parsing, exact original value, current setting snapshot and registered persistence | `src/mod/settings/config_settings.cc` |

The target retains full-value parsing rather than treating the convenience slider
range as the allowed range. An unchanged float draft uses the original observed
value. Invalid, nonfinite, overflow and stale drafts cannot become a successful
setting write. Shared capture owns opening/dismissal input; Return is recordable
in shortcut mode and requires its explicit Confirm, while ordinary exact entry
retains keyboard Apply. These are existing adaptations, not new changes here.

## Actual Windows check

Guff explicitly authorized brief Settings use after concurrent input was detected.
The agent then used native UI on client 273, ordinary C-game PID 27260, start
134380312972477042. Installed source is `f53b07ea94a62aad4a7364616ab6ba029cc16f9f`,
build `ax-zig-20261101T182606-4e62a1bd`, DLL SHA256
`87455393F134ACBC6D8C8186E84303CB711C347721A237A324F709C6AC07535E`.
No rebuild, deployment or cycle was needed for this assessment.

Profile -> Settings -> Mod Settings -> Camera -> Keyboard zoom speed Edit
opened with its current value selected. Typing `invalid` displayed the numeric
validation message and visibly disabled Apply. Cancel returned to Camera with
the prior value intact. Reopening showed the original selected value. Escape
dismissed only the editor, leaving Camera open; native Back then returned through
the menus to the game. Existing chat-pane layout was preserved; no chat input
or gameplay action was issued.

All four game-root TOML hashes match before/after. The interaction log delta
contains 30 lines, with zero errors, warnings or component diagnostics. This
qualifies opening, invalid feedback, Cancel, reopen, Escape and subsequent Back
on that artifact. It does not qualify a live successful Apply/save/restart,
focus-loss/held-key hardware behavior, every value family or Mac execution.

## Local checks and next boundary

Existing `test-mod` passes **152/152 build/run steps** with Windows MSVC ABI,
Zig 0.16.0, ReleaseFast production dependencies, Debug tests and moddbg enabled.
Step counts are not test counts. The actual text-entry session fixture covers
invalid drafts, exact integer range, stale setting/page, cancel, read-only mode,
reentrant commit/replacement and shortcut Record again/explicit Confirm. The
production catalog fixture additionally covers full float precision, exact
integer/float parsing and delayed page-bound numeric commits. Controlled native
host/persistence endpoints in those fixtures are distinct from the live trial.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-exact-entry-20261101/`,
including model-tests-command.json/model-tests.log, source-inputs.json,
runtime-before.json, native-observations.json, native-log-summary.json and
config-comparison.json. Source-bound prior DLL build evidence remains in
artifacts/ui-lookup-20261101; this turn adds no fresh native build claim.

SET-VALUES remains Partial and P24 remains open. Continue with the existing
audio/image picker cancellation and path-entry workflows, preserving their
process-level picker intent versus page-bound exact-entry lifetime. Do not add
the deferred Clear UX, revive parked scopes or treat basic editor cancellation
as full Settings acceptance.
