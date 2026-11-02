# Mac Command-Q: retained managed handles at process exit

Mcohall reports that Command-Q produces macOS's unexpected-quit warning on Apple
Silicon. Guff places the regression after the typed IL2CPP migration and before
`ripper-preview-20261101.1`. The exact affected build, crash exception, termination
reason and crashed-thread stack are unavailable. The screenshot establishes the
report's timing; it does not identify the failing shutdown path.

## Concrete source defect and correction

`il2cpp::GcHandle` releases its managed handle in its destructor. Three retained
owners were C++ process statics: Alliance Store's deferred work, its tooltip cache,
and Forbidden Tech's ship-name row cache. Entries can survive until process exit.
Their static destructors could therefore call `il2cpp_gchandle_free` after the
managed runtime stopped. This is a concrete lifetime hazard and a plausible
explanation for a quit crash, not a confirmed diagnosis of Mcohall's report.

Signed fix `b72e11991d0d1df73c4009cda885dab2de17655c` gives these three owners the
same explicit process lifetime already used by other retained UI owners. They
are not destroyed during C++ static teardown. Normal UI pruning, cache clearing
and work reset still release handles while the runtime is available. Remaining
owners are reclaimed with the process. The shared GC API, existing quit/drain
owner, shortcuts, hooks and settings are unchanged; this does not force an exit.

## Controlled exit evidence

`tests/managed_owner_exit_cases.h` exercises the actual production owners in
three fresh processes: `owner-exit-store`, `owner-exit-store-work` and
`owner-exit-ft`. Each first proves that explicit cleanup releases all live
handles, then repopulates the owner and returns through normal process exit.
An exit callback marks the controlled managed runtime unavailable before C++
static destructors run. Any subsequent managed-handle release exits with code 86.

With the old owners, all three cases return 86 and report a late release. With
the corrected owners, all three return 0. This recreates the lifetime condition
against a controlled API; it is not a native Mac crash reproduction. The modes
are run explicitly and add no CI registration or workflow changes.

The existing Windows `test-il2cpp` suite passes 163/163 build/run steps. The clean,
stable native Windows AX releasedbg build at the fix commit passes 41/41 steps
and proxy verification (16 exports, static CRT, matching provenance). Clean
AAI 4bece2e5/UI Lab remain enabled in this local build. Candidate DLL SHA256:
`8C32D8B038E2EF6051D8133FCC7D9BFB0EA11F3A65C26DE34E4E4CE0CF658AFF`.

Evidence is under `D:/dev/stfc-workspace/artifacts/mac-command-q-20261101`:
`before-all-exit-cases.json` and `final-exit-cases.json` bind commands, cwd, base,
executable hashes, durations and exits; corresponding logs retain the failure
messages. `final-tests-command.json`, `final-inputs.json`, `fix-checkpoint.json`,
`native-build-command.json` and `native-build-receipt.json` bind the source and
verification. `native-candidate` preserves the Windows DLL/PDB.

## Qualification and delivery

Apple Silicon compilation and native Command-Q behavior remain unqualified.
When available, test a Mac build containing this fix and obtain the crash report
if the warning persists. Existing state/TOML quit-drain qualification remains as
recorded in [STATE-QUIT](features/STATE-QUIT.md); this correction concerns later
managed-owner teardown and does not close its native-quit obligations.

The fix is a local signed checkpoint. No game deployment, cycle, config edit,
push or new release occurred. The published prerelease and its immutable source
039a6204 are unchanged and do not contain this correction. The refactor queue
remains paused at Guff's request.
