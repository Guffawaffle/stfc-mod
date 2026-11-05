# Native search, shortcut hints and station-card review

Current continuation: the unsuccessful raid experiments below are historical.
Their exact source and traces are archived, and the experiments were removed
from the ordinary candidate in [cohort03](FEATURE-COHORT-20261029-03.md).
The inherited primary-action limitation remains parked, not fixed.

Continue source-preserving work from accepted `play-dev-ripper` checkpoint
`df56d989`. Frozen play-dev remains `1baa55fedf1758b19f843cc59707b8901a0b4998`;
historical PLAY is `cad98ef7990dd66167eb64ea5dda525c1fe2c296`, inspected upstream
dev `e884efa7c6ff66d0776b4cd4d6b76ce193296be5`. Current user direction keeps
dormant dock-sync work minimal and parks its eventual event-driven redesign.

| Capability | Contract | Boundary |
| --- | --- | --- |
| PLAY-SHORTCUT-HINTS | [Hints](features/PLAY-SHORTCUT-HINTS.md) | Restore initially-unbound installation, current native label and qualified visibility |
| PLAY-FOCUS-SEARCH | [Search](features/PLAY-FOCUS-SEARCH.md) | Preserve search precedence and shared input guards; reject replaced fields |
| PLAY-BASE-CARD | [Station card](features/PLAY-BASE-CARD.md) | Source means enemy-card retention, not station name/level formatting |
| PLAY-DOCK-SYNC | [Dormant dock sync](features/PLAY-DOCK-SYNC.md) | Source review only; production unchanged, polling redesign deferred |

Full relevant frozen implementations were read: focus_search.cc, base_card.cc,
hotkeys.cc's dock sync and hint mapping/text/visibility/install/callers, plus
supporting typed bindings, shortcut defaults, Settings and config aliases.
Existing native components and the single frame/input owner remain in place.
No new UI layout, polling system, persistence engine or account identity is added.

## Findings and bounded corrections

Frozen hints install independently of the current HOME/toggle binding, allowing
Settings to assign it later. The target's early HasBinding return was an actual
port omission and is removed. Shared shortcut formatting remains unchanged.
The actual production badge fixture reproduced a stale write to the prior label
when native UpdateText rebound it. Temporary GC custody and current-owner checks
now prevent it. Native disabled-mode text restoration, side-chat fallback and
unknown-action dash presentation are preserved. Unknown dock indices and hint
visibility cannot become guessed zero/false actions. Native gate refusal again
returns unhandled, as in the original source.

Search preserves inventory → officer assignment → ship assignment precedence.
The outer dispatcher retains chat/input/capture ordering; the helper also checks
live enablement and input focus. Temporary owner/field roots and a final identity
check prevent focusing an old field after a rebind. The existing void native
Focus attempt still consumes the shortcut; it is not a promise of completed focus.

The inventory's old Base Card name/level description was not what its cited
source implemented. Correcting the description does not invent or remove a
separate station-label feature: this capability owns only matching enemy-card
retention and native fallback.

## Dock work explicitly parked

Guff asked not to invest further in the mostly dormant dock feature and wants
to get away from polling. Production dock.cc is unchanged from the accepted
source, including its existing100ms throttle. The larger ownership prototype
and unqualified fixture were set aside locally under the artifact directory:
`DOCK-FOLLOWUP.md`, `dock-selection-prototype.patch`,
`managed-dock-selection-prototype.h`, `dock-selection-prototype-cases.txt`.
They are research artifacts, not shipped or qualified code.

The original last-fleet-only acknowledgement may skip a replacement bar. This
remains a source-level finding, not a player-observed regression or approval to
retain a new polling design. A future redesign should first qualify native
selection/context/bar lifecycle notifications and preserve real dock-index
semantics, disabled behavior and native selection refusal. Do not assume a
property setter observes every way the game changes selection.

## Qualification

Actual badge/search/station-binding cases extend the existing controlled
IL2CPP host; values/native callbacks are substitutes, not game rendering or
installed detours. Export-name aliases isolate the real badge implementation
from the earlier top-level hotkey fixture's unrelated badge sinks. No analytical
implementation or duplicate UI engine is introduced.

The new station-binding fixture initially imported GameAssembly's method-name
lookup, causing the shared executable to fail at load, not a gameplay failure.
The existing controlled API now supplies that lookup from its own metadata;
no game DLL is copied into tests. Duplicate synthetic navigation-context
metadata and cached test-body interposition were corrected in the fixture.
One dependent-template annotation in the existing mission fixture's production
helper is compile-only, with unchanged behavior. Intermediate failing logs remain.

Evidence: `D:/dev/stfc-workspace/artifacts/navigation-ui-20261029/`.
Windows assertion-enabled checks passed422/422 in `tests-final-1.log`,
`primary-refresh-tests.log` and `fresh-scan-tests.log`. These are controlled
native-call fixtures, not gameplay proof. The native AX Zig cycles compiled
before stopping the exact main ordinary session. Runtime started with all73
groups and FrameTick ready. No configuration was edited; the before/after store
hash comparison was unchanged at the second cycle.
No push, merge, CI work, Runner execution/fanout or overall-parity claim.

## Retained-base primary action investigation

Guff clarified that repeated `primary_action` after recall has **never worked**
with keep-base-card-open. This is an inherited feature limitation, not a
migration regression. Keeping the card open through recall is a hard requirement.
The native Attack button can remain unavailable while docked; the mod shortcut
uses the native engage handler independently. Its color is not an acceptance test.

Live trace proved both initial and failed post-recall presses reached the same
native engage handler: exact one-frame replay matched, visible card matched,
`_ignoreEngageClick` was false. Native `IsEngagementViable` changed from true to
false. The earlier button-only refresh ran but did not resolve the failure and
was removed. A same-ship ShowWithFleet rebind also preserved the failure.

Native source inspection identifies a meaningful difference: ShowWithFleet asks
to keep previous scan information for the same selected ship, but requests fresh
information when changing ships. The current experimental correction requests
`RetrieveBattleTargetDataForStarbase(exactFleet, exactUser, false)` before native
ShowWithFleet on an explicit primary press against an unavailable foreign base
with the ship docked. It then uses the existing one-frame action path. No timer,
fake ship-selection change, viability override or new command path is added.
Changed/unavailable target identity after native rebuilding consumes the press
without replaying into another target. Ordinary already-viable and moving-ship
routing remains unchanged. Fixture cases cover these decisions and exact fresh
scan arguments. Final in-game outcome is pending; do not mark it fixed from
method invocation or passing fixtures alone.

Evidence is retained in `engage-trace-observations.log`,
`same-ship-refresh-observations.log`, the scoped cycle receipts and test logs
under the artifact directory above. Earlier disassembly is client-specific
research evidence; its GameAssembly hash differs from the current client and
its RVAs are not used for runtime bindings. Runtime methods resolve by checked
metadata through the existing typed adapter.

### Diagnostic continuation (supersedes the experiment status above)

Guff reported no attack with both the same-ship and fresh-scan experiments.
Those implementations were removed; their patch and traces remain locally for
recovery. They are not accepted fixes. At Guff's explicit request, a development
build now tests an eligibility override only synchronously inside primary_action
for the exact rooted foreign-base context and the matching docked player fleet.
The original getter runs first; ordinary calls, other target instances, nested
scopes and calls after dispatch keep the native result. Scope destruction
restores the prior context. There is no permanent model write or polling.
The getter hook is installed in the existing BaseCard transaction only with
_MODDBG. This intentionally bypasses native eligibility for that one diagnostic
call; it is not a production mechanics fix or evidence that every native
eligibility condition was stale. Native rendering/clicks remain unchanged.
In-game override outcome remains pending. No overall feature acceptance yet.

The first override trial did not exercise the override: no diagnostic getter
message appeared. Do not count it as proof that forced eligibility fails. The
second diagnostic separates exact foreign-base/current-fleet identity from the
stale validity flag being investigated, and no longer requires a particular
fleet state. It still runs only in the explicit primary call on that same
visible card; every other context remains native. Its scope entry and actual
false-to-true getter changes are logged independently. Controlled tests passed
422/422 (`eligibility-diagnostic-tests-3.log`). AX built before cycling main
ordinary PID55436; candidate disk SHA256
`2525cc52ad8acad9cc4b7daf8a7bfb6e4badf539d8ca35595c8733d6d20ebe59`.
All73 groups and FrameTick ready were observed. This is an uncommitted diagnostic
candidate; player outcome and hook execution are still pending. No production
acceptance, Mac qualification, push or promotion is implied.

### Bedtime outcome

Guff reported no attack on the corrected diagnostic. Trace confirms exact-target
scope entry (fleet state2), but **no getter false-to-true override invocation**
inside the native action. Thus forced eligibility has not been proven ineffective;
the hooked getter was not observed on that path. Stop live cycling. Full recovery,
all other findings and the next native-call investigation are recorded in
[RECOVERY-20261029-BASE-CARD-PRIMARY.md](RECOVERY-20261029-BASE-CARD-PRIMARY.md).
