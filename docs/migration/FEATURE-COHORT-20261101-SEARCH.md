# Settings search and contextual navigation — 2026-11-01

Source: frozen `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target base: `d2851fee96858bcef1d01a5fecb8b3a9f89fa350`, existing Ripper
play-dev-ripper checkout. Contract: [SET-SEARCH](features/SET-SEARCH.md).

## Result

The complete SettingsSearch metadata/matching model matches frozen source bytes.
The current native root field, result actions, queued page transition and
contextual Back path are mapped to their existing typed owners. This is bounded
search/navigation assessment, not complete SET-NAV publication/menu acceptance.

One migration difference was reproduced: SetSettingsSearchQuery truncated a UTF-8
string to128 bytes before retaining it. A query that fits the native128-character
input limit can need more bytes; truncation can store a partial codepoint. The
stored query is written back when the native field opens after navigation.
Frozen SetQuery stores the complete text supplied by the native field, with no
extra byte truncation. Its matching model bounds token matching separately.

The correction removes the extra byte resize and restores that source behavior.
The native field's limit, model matching, ranking, result cap and query revision
rules are unchanged. No new UTF-8 decoder, UI owner, setting, hook, format or CI
registration. No settings are applied/saved by query changes.

Final controller tests retain complete2/3/4-byte input that fits the native field
while exceeding the former byte cap. Repeating the same query preserves revision;
the128-character ASCII case remains accepted. Existing metadata-only matching,
key/name ranking, deduplication, hidden sections, result queue/Clear, failure
Dismiss and read-only presentation checks remain in use. These tests substitute
native navigation; actual search typing/result click/Back remains unqualified.

## Source and owners

Frozen inputs: mods/src/settings/settings_search.h,
mods/src/settings/native/settings_search.cc and search-specific portions of
native/page_navigation.cc. Target: settings/settings_search.h,
settings/maintenance_pages.cc, features/settings_search.cc,
features/settings_pages.cc and game/settings_search.h. Search input/output uses
the existing ToUtf8/NewString conversion through the typed surface.

Frozen root actions defer navigation until the clicked row's callback returns.
The target queues a page/item with its source visit token and weak target; frame
processing checks controller/context, publication, current selection and same-tree
custody before native navigation. A search-opened visit retains a weak return
context; only its native parent Back transition redirects there. The target
reveals collapsed target sections and focuses the item using existing native rows.
No native option reparenting or duplicate navigation owner is introduced.

Exact local observations: upstream dev ff179589f834b95331b4d7e38b1054fb6ff5ace9
has no named search control at the inspected path; play cad98ef7990dd66167eb64ea5dda525c1fe2c296,
frozen1baa55fe and refactor base d2851fee have the named controls. These are
source observations, not successful native menu or player acceptance.

## Verification and evidence

Windows native MSVC environment, Zig0.16.0, x86_64-windows-msvc, ReleaseFast
production / Debug tests, moddbg=true, four jobs. Final
`zig build test-settings-models test-il2cpp -Dtarget=x86_64-windows-msvc
-Doptimize=ReleaseFast -Dtest-optimize=Debug -Dmoddbg=true -j4 --summary all`
passed204/204 build/run steps. Step count is not test count. Typed adapter cases
use a controlled runtime, not live Unity interaction.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-search-navigation-20261101/`.
query-before.log records the isolated partial-UTF-8 failure against unchanged
production code. query-final-tests.log/query-final-command.json qualify final
full-query retention and record command/cwd/base/duration/exit. Earlier after
logs are intermediate boundary-only checks, not the final candidate. Source
files/diff, branch observations and final production/test hashes are retained.
Mac SDK is absent; this correction is not Mac-qualified. Native delivery will be
recorded after the clean source-bound build/cycle. Other native Settings
publication, rendered interaction, actual Back/caret/focus and platform checks
remain open; accepted mission/pin behavior and parked scopes remain unchanged.
