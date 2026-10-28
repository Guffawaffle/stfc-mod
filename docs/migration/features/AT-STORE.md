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

## Implemented candidate — 2026-10-28

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
