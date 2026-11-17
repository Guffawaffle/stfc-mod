# Alliance Games store â€” source-preserving extraction

Source: `study/alliance-tournament-store-20261026` at
`a815944d26047c6a7d768a7ade1b52b57d06d6ab`; migration base
`ce5494bd781bbaac3fa0e1b1bf15529c0f222897`, branch `play-dev-ripper`.
The user explicitly selected this study as an addition outside frozen play-dev.

Read the complete source `mods/src/dev/tournament_store_study.cc`, its two
claim-trace producer hooks, installer/build gates, command client, recovery and
study notes. Preserve the original study branch and diagnostic protocol there.
This production extraction must not inherit the claim-trace dependency or
file-command polling.

## Separate contracts and qualification

- **AT-STORE-RESEARCH:** native Standard-tree eligibility, native action/locale
  assets and VIEW RESEARCH navigation; initialized live plain-tooltip loader;
  paired wrapper references; description refresh; one reusable added component;
  confirmed ineligibility restores ordinary tooltip behavior. Native inventory
  action components remain owned by the game. Unknown is not ineligible.
  Preflight fields/operations before mutation and restore original wrapper state
  if setup fails. Root managed sources during native calls, qualify Unity
  liveness and bound context, and retain no borrowed pointer across frames.
- **AT-STORE-CURRENCY:** native `UpdateStoreCurrency` after store bundle readiness;
  do not hardcode resource IDs or substitute reported player balances. Avoid
  recursive refresh loops and avoid work outside the active store.
- **AT-STORE-ROWS:** preserve native position and order; inspect actual purchase
  refresh before selecting any corrective operation. Clean synthetic study
  refreshes did not reproduce the bug. A successful visibility invocation is
  not visual acceptance. The agent does not purchase anything.

Historical v7:17 eligible materials,17 automatic upgrades,zero errors; paired
wrapper references/live loader/enabled components independently observed. User
verified matching research, return navigation and preserved-position refresh.
Currency manual refresh changed one native resource to two and updated the
header. No permanent currency or row correction was implemented in that study.
Evidence: `D:/dev/stfc-workspace/artifacts/alliance-store-study-20261026/`.
Current tests, exact candidate and native results will be added after extraction.

## Typed integration boundary

Use named game bindings, checked reads/calls and the existing feature registry,
transactional hook installation and single FrameTick. Retain event-coalesced work
only for the active store; no global scene polling during ordinary gameplay.
Never detour the tiny `RefreshScrollerContents` wrapper. Use substantive native
methods with exact signature checks. Keep owned game objects bounded to their
qualified lifetime and component mutations reversible on failure.

No AAI changes, store migrations, new persistence owner, purchases, release or
whole-mod rewrite are part of this feature.

## First typed candidate — 99bb6719, 2026-10-28

Files: `src/mod/features/alliance_store.cc` and
`src/mod/game/alliance_store.h`; independent default-on startup gate
`[patches].alliancegamesstorehooks`. Missing key preserves default-on behavior.
One centrally reported feature owns five typed transactional hooks: store
bind/release, substantive shop regeneration, bundle refresh and bundle release.
Existing shop features are unchanged. No claim-trace installation is required.

The original native study operations are retained. The deliberate changes are:
checked identity/eligibility, field preflight, rooted operation-local sources,
weak scene/component identities, stable per-binding leases and generation
checks, complete original serialized-setting restoration, and cancellation on
source release. Native serialized actions remain untouched. Components added
by this feature are disabled and reused; original assets are strongly retained
only while their replacement is bound. The base Unity Behaviour enabled setter
is used explicitly, as in the qualified study; TooltipTrigger's hiding setter
has extra sibling-controller behavior and is not substituted.

Store work is event-coalesced for250ms on the existing FrameTick. An idle frame
returns before any scene query or traversal. A native template query occurs
only when eligible materials need assets; there is no global idle scan, file
polling, arbitrary scene-item limit or new dispatcher. No pointer used for
mutation is borrowed across frames. Weak references do not establish Unity
liveness; source/item identity, liveness and lease generations are rechecked.

Currency refresh is a **new candidate intervention**, not a historical verified
fix: once native bundle data is ready in the active store, call the existing
context's `UpdateStoreCurrency`. Native IDs/amounts/header notification remain
owned by the game. Reentrant currency-triggered regeneration is coalesced without
an update loop. Diagnostics record before/after soft-currency counts, native
visible-item count and native scroll position, preserving unavailable values.
Those counts do not prove a missing row was repaired.

No row repair or automatic purchase is implemented. If the symptom occurs after
an operator-chosen purchase, preserve a screenshot before scrolling and correlate
the native regeneration/settled-state logs. Clean synthetic study results cannot
substitute for that reproduction.

## Verification

Final offline log: `D:/dev/stfc-workspace/.ax/alliance-store-port-20261028/tests-7.log`.
The Windows suite passed **387/387** build/run/install steps with unchanged pinned
AAI enabled. New controlled-runtime modes execute the actual production store
consumer and bindings: successful conversion/native setup, exact Int64 resource
identity, failed eligibility, confirmed ineligibility, single-component reuse,
setup failure rollback, context replacement, failed strong/store custody,
reentrant pruning, release during eligibility/setup/enable, foreign native action
preservation, card teardown, loading-data refusal, idle/no repeated refresh and
release during the final currency-context getter. Wrong-width resource IDs and
same-sized foreign override structs are rejected independently.

The host's weak handles are simulated; this is not real Unity GC, rendered UI,
real inventory navigation, a purchase, native currency semantics, macOS, or final
native hook installation. Current exact-client273 metadata and Windows unwind
regions corroborate signatures and substantive entries. No tiny wrapper is hooked.

In-game checklist: open Alliance Games Store without visiting inventory first;
inspect multiple eligible materials and native VIEW RESEARCH; return and scroll;
check both currency types/owned amounts on first load and after returning;
inspect a non-research card; close/reopen. If already planning a purchase, user
chooses it and checks rows/position immediately before any corrective scrolling.
The agent must not make a purchase. Record exact candidate/hash and each result.

## First native trial and purchase reproduction

Candidate `99bb6719b06a8b45281943ee390dd3b68694098e` built41/41 native
steps and booted all73 feature rows plus FrameTick in Windows client273 main
ordinary PID66284, creation FILETIME134376717336578974. DLL SHA256
`89042b7faba9e4274620c6e890ae1ec47a09db5fb1ee5fb47322c305fef7dab7`.
Five tracked stores were unchanged after AX's build-before-stop cycle.

Player confirmed both currencies, visibly recovering from one to two, and
VIEW RESEARCH navigation. Native diagnostics independently recorded17 upgrades,
zero setup failures and currency context1→2.46 unavailable items were retained
as unavailable rather than treated as confirmed ineligible. After an actual
user-chosen purchase, the final row disappeared until scrolling, and Research
actions did not restore. This qualifies initial navigation/currency correction;
it does **not** qualify the purchase lifecycle. The prior contained/rate-limited
GetComponent diagnostic recurred before the store trial; caller/cause unknown.

Receipt: `D:/dev/stfc-workspace/artifacts/alliance-store-port-20261028/qualification.json`
SHA256 `7b275b6d05ecbfc4536d9f55711bcf28f90c411269b177605d9f81190ac27811`. Exact DLL and source/test/build/store/log evidence are retained.

## Purchase-refresh correction candidate

The native bind body runs SetupTooltip before SetScrollerReference. Native
regeneration may then spawn/rebind pooled cards on separate frames. A one-off
store pass plus a RefreshBundle hook does not cover every subsequent bind.
The correction adds post-`BundleDataWidget.OnDidBindContext` scheduling, reads
its checked current scroller owner after original returns, and debounces to the
latest bind. It waits for native regeneration, held-initialization data and the
spawner coroutine to finish. Release/cancellation ownership remains unchanged.

The same-position candidate invokes the native SmartScrollerBase scrolling
notification with its exact current normalized Vector2 through runtime marshaling.
It does not call a position setter, synthesize input or rebuild data. Virtual
native behavior handles the actual SmartScrollerGrid. Native visibility checks
alone only inspect existing cards and can report impressions; they do not create
missing cards or restore Research. No hard-coded non-Grid implementation is used.
This is a candidate based on the real reproduction, not a proven row fix.

Qualification must repeat the user-chosen purchase/return workflow and inspect
rows/Research before scrolling. No agent purchase is authorized or performed.

Correction-delta verification: `tests-9.log` under the same evidence directory
passed387/387 Windows build/run/install steps after the late-bind and native
same-position changes. SHA256
`3cde0b3d7aaa84258da600ebfe743d75cba138813de052dacb3052b92b60291c`;
test executable `617fc62318970f0ceff62e6b616aa4b7c503f7cc9ae9314cdddedac1312f4cac`.
Additional actual-consumer checks cover post-original card owner assignment,
late-bind scheduling, foreign position boxes, native spawner wait, exact Vector2
notification and source cancellation during notification. The test host provides
managed box storage; this is not game rendering or a real purchase.

Read-only correction review independently checked native base callback dispatch
and spawner completion clearing. The revision adds a sixth hook (card bind),
not a new feature owner, global scan or frame dispatcher. Source hashes and exact
native qualification belong to the subsequent candidate, not the first trial.

## Accepted Windows purchase trial and pause — 2026-10-28

Correction `4f9a6f3b4102e7a6aba6299d932e960284dd5246` built41/41 native steps and booted73 feature
rows plus FrameTick. DLL SHA256 `c1de2877a4604805e9f39974a124dc76c59c64f68c2cbaff20f5c166e6095ff0`;
main ordinary client273, PID54228 at qualification, FILETIME134376736362962870.
Five tracked stores were unchanged; no agent configuration or purchase action.

Guff answered **"both worked!!"** to the requested purchase-row/Research
restoration trial, then **"and the currency worked too"**. Native logs record
successful same-position notifications,17 initial and20 later upgrades with
zero setup failures, currency1→2 then2→2, and retained position2525.960205.
This accepts the combined basic Windows workflow on this exact candidate.
macOS, exhaustive store/resource cases and future clients remain unverified.
Earlier99bb6719 purchase failures remain recorded as the independent baseline.

Receipt: `D:/dev/stfc-workspace/artifacts/alliance-store-port-20261028/qualification-2.json`
SHA256 `bd6a92b5f29e29aaea2ebad587317e5952f5a1d4f2bb2da41be9c709d3f71bf2`. Eight evidence hashes and the exact DLL are retained.
The operator paused work to switch accounts. Do not restart, deploy or continue
implementation on the strength of an old PID; resolve target intent on resume.
No push, merge, PLAY/play-dev promotion or Runner execution occurred.
