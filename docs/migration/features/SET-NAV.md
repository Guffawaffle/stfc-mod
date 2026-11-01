# SET-NAV — native menu creation and navigation

Contract version: 1. Status: existing typed owner assessed; two bounded corrections.

## Inputs and scope

Frozen source: `1baa55fedf1758b19f843cc59707b8901a0b4998`,
`mods/src/settings/native/page_navigation.{cc,h}` and `settings_search.cc`.
Target base: `d65616bc1a75c12be50960a04a2249ec770ad402`, existing
`play-dev-ripper` checkout. Inventory: SET-NAV, supporting SET-SEARCH.
Evidence: `D:/dev/stfc-workspace/artifacts/settings-native-navigation-20261101/`.
No new menu framework, hook owner, configuration, persistence format or product scope.

| Inspected branch | Exact revision | Named native menu/navigation presence |
| --- | --- | --- |
| upstream dev | ff179589f834b95331b4d7e38b1054fb6ff5ace9 | absent at named target path; not a repository-wide absence claim |
| play | cad98ef7990dd66167eb64ea5dda525c1fe2c296 | control in native/page_navigation.cc |
| play-dev/frozen | 1baa55fedf1758b19f843cc59707b8901a0b4998 | control in native/page_navigation.cc |
| refactor base | d65616bc1a75c12be50960a04a2249ec770ad402 | control in features/settings_pages.cc |

## Source workflow and ownership

Frozen InstallPages builds the catalog and registers category/text/selection/
destroy hooks. The existing Boolean owner calls AddPages after native General
Settings creation. AddPages inserts categories and rows under the native root,
checks identities, and removes its added root on failure. Native contexts own
children; panels own pooled widgets. PageSelectedHook keeps visit-local collapsed
sections and calls native navigation once. Search results defer transitions;
only a search-opened visit's native parent Back redirects to the search context.
ClearSectionPage closes search/editors and invokes the page leave callback.

Target game/settings_pages.h qualifies native category/text signatures, checked
query/parent/children reads and the untyped Widget virtual IList bind. Features/
settings_pages.cc stages the entire tree off-screen, retains exact query custody,
publishes only after successful construction, and checks parent/membership before
enabling navigation. CategoryPublication retains failed/unknown cleanup work.
This intentionally strengthens frozen incremental publication; incomplete staging
does not expose a partial menu. Existing control adapters keep their own hooks.
Native SelectedOption, Back, release/bind and event delivery stay native.

## Behavioral obligations

| ID | Frozen operation | Target mapping and verification |
| --- | --- | --- |
| N1 | InstallPages/AddPages publish one identified root, preserve other native settings | Existing shared appender and off-screen construction; publication state fixture covers partial return, unknown observations, duplicates and exact-root cleanup. Real installed 9cb2f6bf publishes 179 pages and displays one Mod Settings root. |
| N2 | AddHeadingRow/control adapters retain native parent/child ownership and catalog order | Existing typed appenders, ordered presentation and IList rebind. Actual Camera and Controls rows rendered. No native ownership list rewriting. |
| N3 | PageSelectedHook calls original once; heading toggle stays on current selected parent | Existing typed navigation; restore missing same-tree and exact checked-parent refusal before section mutation. Recheck current visit after the parent call. Old/reparented heading refusal is source-qualified, not live stale-event injection. |
| N4 | Visit-local sections begin collapsed; refresh failure restores accessible controls | Existing PageSections and panel repair, no second native selection. Model/publication/typed fixtures cover their layers. Selected frozen/target catalog producers register no collapsible headings; this adapter branch is support code, not a current player-facing fold workflow. |
| N5 | Category/title overrides are released before native pooled rebind/destroy | Existing weak widget/context/label identity, checked restoration and native hooks. Real category/Back transitions render correct titles; extended replacement/failure injection is not claimed. |
| N6 | Native child Back returns to parent; search Back returns to retained query without reparenting | Existing visit-local weak search return and deferred same-tree queue. On 9cb2f6bf Camera/Controls Back and search result/Back/query retention work. Search's first result click after typing was lost. |
| N7 | Search input owns gameplay through focused input release while result actions remain usable | Frozen InputRelease retains search activity through pointer-up. Target released its capture immediately on field blur, revoking the result exception during the click. Reuse InputRelease to retain that exact search token through held input and the first free frame; shared capture drain still owns gameplay after release. Escape/close continue releasing through the shared guard. |
| N8 | Destroy/session invalidation revoke visits, editors and pending transitions | Existing weak contexts, generation token, invalidators and close-before-callback logic. Source and controlled fixture coverage; live reconnect/session replacement remains unqualified. |

The native patch gate remains restart-only. Browsing/search do not apply or save
preferences. Current Windows evidence does not qualify Mac, every page/control,
Shift-S routing, session replacement, Alt-Tab/caret behavior or native failure
recovery. Prior failed staging trials remain historical evidence; a successful
current menu observation supersedes their claim that no menu can be published.

## Validation and disposition

Existing Windows `test-settings-models test-il2cpp test-mod` passes 313/313
build/run steps, ReleaseFast production/Debug fixtures, moddbg=true, Zig0.16.0,
x86_64-windows-msvc. These exercise models, publication policy and controlled
typed bindings; they do not execute the live search frame or prove stale native
heading rejection. The observed first-click regression needs a source-bound
native build and the same real typing/result-click/Back sequence after deployment.
Command, base, duration and exit are in navigation-test-command.json.

Initial live trial: Windows client273, ordinary C-game, source9cb2f6bf, DLL
7BB3EBBA3AF25A0F84CDCE48BCB3A03723D004AF224981968205FC3AD8D72F49,
PID3212/start134380254273316375. Publication logged at 2026-11-01
11:08:35.357 local. Direct child navigation works. Typing `zoom` produced33
results capped to32; first Open click did not navigate, second did; Back retained
the query/results and Clear restored normal categories. No preference writes.

## Windows delivery and native trial

Signed source `0f9ad5a5f6140c366139b718e1bc163fd141fe36` passed the clean,
stable 41/41-step AX build with AAI4bece2e5/UI Lab retained. One guarded ordinary
C-game cycle stopped PID3212 and started PID48260/start134380271165629261.
Installed/build DLL SHA256:
`C3F46902F0E8E4F8FA5D91455A480D2859801C6C0E8F50F22090531DC22A4612`.
Embedded build: ax-zig-20261101T171603-fef9e375. Exact source, intended loaded
module path, disk hash and full frame readiness are observed. Command/cwd/source,
duration and exit are retained in native-cycle-command.json/native-cycle.log.

Actual corrected client trial publishes179 pages and displays one Mod Settings
root. Typing `zoom` and clicking Open **once** opens Camera. Native Back restores
`zoom` and its results; Clear restores categories. Back closes to native Settings
and then the player profile. Reopening Settings publishes179 pages again, shows
one Mod Settings entry and opens its ordinary categories. This is agent-observed
native interaction, not player acceptance or every-control/platform qualification.
No preferences were changed; all four root TOML hashes match before/after.

Startup had no fresh errors. During menu interaction the previously tracked
contained GameObject.GetComponent NullReferenceException message recurred at
11:19:35.888 local (community_patch.log:566). Its caller/cause remain unassigned;
there is no additional typed GetComponent caller diagnostic. Do not attribute it
to search or claim a clean complete interaction log. The same-message historical
[lookup investigation](../UI-LOOKUP-INVESTIGATION-20261031.md) remains unresolved;
identical text is not proof of identical cause. Search/navigation observations
above succeeded. Exact row scrolling, stale/replaced objects, session changes,
failure injection, Shift-S and Mac remain separate; they do not prevent moving
to the next bounded refactor slice. runtime-verification.json and
native-log-observations.json bind the observations to this installed artifact.

Later [lookup investigation](../UI-LOOKUP-INVESTIGATION-20261031.md) identifies
the reproduced error as the shared keyboard-focus check querying a selected
GameObject whose native object is already destroyed. Signed207becc9 skips only
confirmed destruction;163 typed-binding steps and41 clean native build steps
pass. It is built locally with AAI/UI Lab but deployment/native-after checks are
held by Guff's explicit no-cycle request during a wave. Earlier unassigned-cause
statements above describe the prior observation, not the current diagnosis.
