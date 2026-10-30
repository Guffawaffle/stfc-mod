# Recovery — 2026-10-29 retained-base primary-action investigation

Later alignment continuation archived the exact experimental source again as
`D:/dev/stfc-workspace/artifacts/sona-migration-fit-20261029/parked-primary-source.zip`
and removed the diagnostic getter hook/primary dispatch changes from ordinary
source before service-cohort builds. Original bedtime snapshots and all traces
remain intact. This cleanup does not fix or resume the raid investigation.

## Historical parking and bedtime record

**Parked by Guff on return, 2026-10-29.** No further primary-action debugging
or raid trials until explicitly resumed. Preserve this document, recovery frame
`98ce99f6-2125-4742-a344-046145ea7069`, and the artifact source snapshot/traces.
At that parking point the experimental override remained uncommitted. The later
continuation above removed it before ordinary builds, preserving the snapshot.
Parking did not accept or promote the diagnostic. No cycle or source change
was made when recording the original parking status.

User requested a bedtime stop. Do not continue implementation or cycle until
work resumes. Current checkout is the existing
`D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`, branch
`play-dev-ripper`, HEAD `df56d9893baf5b3c19fd952f5e9564cfd32c9e74`.
This pass is **uncommitted**, not promoted or pushed. Preserve unrelated dirty
canonical `D:/dev/stfc-mod/AGENTS.md`.

## Hard product boundaries

Keep the enemy base card open through recall. Guff uses `primary_action` to
attack while docked; native Attack being non-green is normal and is not the
shortcut acceptance test. Guff explicitly confirms that repeat attacks after
recall have never worked with this feature: inherited limitation, not migration
regression. No new timer/polling workaround. No more live trials tonight.

## Latest live finding — read carefully

Installed diagnostic SHA256:
`2525cc52ad8acad9cc4b7daf8a7bfb6e4badf539d8ca35595c8733d6d20ebe59`.
AX built first, then cycled the exact main ordinary session. All73 groups and
FrameTick ready observed. Client273; GameAssembly SHA256
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
Current source is the uncommitted diagnostic candidate described below.

Guff: **no attack** on the latest trial. Trace proves:
- shown card eligible; buttons enabled, context available;
- base hull is Any (-1), target ID2; deferral accepted;
- exact next-frame replay matches fleet/card/context/target and remains shown;
- native OnEngageButtonClicked is invoked, `_ignoreEngageClick=false`;
- native IsEngagementViable before the action is false after recall (initial
  attack had true);
- diagnostic scope DOES enter, matches exact target and fleet, native fleet
  state2 (Docked);
- **NO false-to-true getter override message occurs inside the action.**

Therefore do NOT claim that forcing eligibility true was executed and failed.
The scoped override was installed and its scope entered, but native dispatch
apparently does not query that hooked getter for the same target during the
call (or uses a different/inlined route). The pre-call diagnostic getter runs
outside the override by design. Next investigation should trace actual current
native OnEngageButtonClicked -> FleetsManager.EngageTarget/planned-course/state
and determine what ship switching resets. Avoid more guessed refresh cycles.

## Experiments and outcomes

1. Recall logging: both preview and navigation native course-response handlers
   are suppressed for matching retained recalls. That is intentional retention;
   correlation alone does not identify the missed action state.
2. SetButtonStates refresh: native call succeeded; no attack. Removed.
3. ShowWithFleet same-ship rebind: native call succeeded; identity retained;
   viability stayed false, no attack. Removed.
4. RetrieveBattleTargetDataForStarbase(exactFleet,exactUser,false) followed by
   ShowWithFleet: identity retained; no attack. Removed. Saved prototype patch.
5. First scoped getter override trial: setup guard rejected before scope entry;
   no override observed. Not a valid forced-true test.
6. Corrected scoped diagnostic: identity based on foreign-base/user/current
   fleet, independent of stale IsValid, runs only in explicit primary action.
   Scope enters; no getter override fires; no attack. Current installed build.

Current diagnostic is `_MODDBG` only, staged in existing BaseCard transaction.
It calls original getter first, can return true only for exact thread-local
rooted target during synchronous native action, and restores prior scope on
exit. It is NOT an accepted production fix. Remove/replace diagnostic before
promotion. Primary dispatch tracing remains temporarily at info level; no
background scans were added. No persistent model flags were written.

## Other findings and original batch

[Feature cohort](FEATURE-COHORT-20261029-02.md) and four contracts under features:
PLAY-SHORTCUT-HINTS, PLAY-FOCUS-SEARCH, PLAY-BASE-CARD, PLAY-DOCK-SYNC.

- Frozen play-dev remains1baa55fedf1758b19f843cc59707b8901a0b4998.
- Hints: removed port's initially-unbound install omission; actual fixture
  reproduced stale prior-label writes after native rebind; local GC custody
  and current-owner checks correct it. Visibility/call failures remain unknown,
  not guessed writes. No new public registry or polling.
- Search: inventory/officer/ship precedence retained; live enable/input guards,
  temporary roots and replaced-field checks. Existing void native focus attempt
  is consumed once, not claimed successful.
- Base-card inventory name was wrong: cited source retains matching foreign
  station card, not name/level formatting. Actual matcher fixture qualified
  exact IDs, native fallback and recall, then reproduced a stale preview-data
  identity hazard; minimal current-data recheck added.
- Dormant dock synchronization: production code UNCHANGED. Existing100ms
  throttle and last-fleet-only acknowledgement documented. Larger prototype
  parked locally for future event-driven redesign per user. Do not resurrect.
- Missions: one dependent-template keyword is compile-only correction.
- Controlled test host required a real method-name metadata lookup stub to
  avoid accidentally importing GameAssembly.dll. Boxed-value setup and cached
  synthetic context issues were fixture errors; earlier mass load failures were
  not gameplay regressions.

Original machine-readable inventory is NOT updated for these four entries yet;
PLAY-PARITY has a continuation link. Finish reconciliation when the batch is
qualified; do not imply complete parity or player validation for hints/search.

## Validation and evidence

Latest suite:422/422 Windows assertion-enabled build/run/install steps,
`eligibility-diagnostic-tests-3.log`. Native cycles built successfully, groups
installed, main game running. Scope policy checks cover exact/other/null targets,
nested restoration and ordinary native true; identity checks cover stale-valid,
wrong-fleet and own-station cases. These do not prove native gameplay success.
Mac runtime/build not newly qualified. No CI/push/merge work this pass.
Five tracked store hashes matched at earlier cycle comparison; no config edits
were made. Later ordinary player activity can change stores; don't claim an
end-of-session hash comparison not performed. Known contained GetComponent
NullReference recurred at03:43:11; attribution remains unknown, don't turn this
into unrelated performance work.

Artifact directory:
`D:/dev/stfc-workspace/artifacts/navigation-ui-20261029/`.
Key logs: engage-trace-observations.log, same-ship-refresh-observations.log,
fresh-scan-observations.log, eligibility-diagnostic-observations.log,
**eligibility-diagnostic-observations-2.log**, all cycle receipts/test logs.
Failed fresh-scan implementation is retained in
`unsuccessful-fresh-scan-prototype.patch` (untracked fixture source also saved
in current source snapshot). Dock research: DOCK-FOLLOWUP.md and prototype files.
Older disassembly differs in GameAssembly hash; RVAs are not runtime bindings.
Current-viable-candidate.asm is exact current disk bytes at a candidate method
RVA from the newer dump, not independently proven managed-method attribution.

No Runner execution/fanout was used. AXF scoped recovery is the continuity path.
Source diff, changed-file manifest, and zip snapshot accompany this recovery.
