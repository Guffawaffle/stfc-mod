# Contained UI component lookup error

## Current investigation — 2026-11-01

User requested investigation of whether this is a race or inconsequential noise.
Base536b907e on existing play-dev-ripper is clean. Installed source0f9ad5a5,
Windows client273, ordinary C-game, records the same exception message at
11:19:35.888 local during native Settings use (log566), after publication179
pages at11:19:27.367. Search typing/result/Back and close/reopen still worked.
The typed helper's caller/type diagnostic is absent, although its format string
is present in the installed DLL. Direct callers and nested native behavior remain
possibilities; identical text is not evidence of identical cause or harmlessness.

A bounded Windows `_MODDBG` diagnostic captures up to24 native frames at this
specific contained error through the existing error handler. It logs module
basenames and relative offsets once, without changing invocation, exception
containment or object lifetime behavior. Symbol resolution uses the exact DLL/PDB
offline; no debugger/symbol session is initialized inside the game. Pending next
occurrence, no root cause, race, frequency or negligible-impact conclusion follows.
Evidence: `D:/dev/stfc-workspace/artifacts/ui-lookup-20261101/`.

## Earlier investigation

The local `ee112729` Windows startup log records a contained
`UnityEngine.GameObject.GetComponent` `System.NullReferenceException` at
2026-10-31 13:20:16.191 America/Chicago, after frame delivery became ready at
13:19:15.242. UI button audio surrounds the error, but does not identify its caller.
Earlier cohorts recorded the same managed method diagnostic; identical cause
has not been established. The original log is retained outside the checkout in
`artifacts/integration-audit-20261031/startup-log-snapshot.txt`.

The IL2CPP library reports exceptions once per managed method. All component
types and native callers therefore shared one suppression entry. Existing
`GameObject::GetComponent<T>` callers include keyboard text focus, anomaly timer
UI, loading artwork, Galaxy controls and indicators. Source inspection alone
cannot select the failing caller. The frozen keyboard implementation also
contained component lookup exceptions; restoring its catch-all would hide evidence.

The typed Unity helper now uses its existing checked call result and emits one
additional diagnostic per requested component type, recording the native source
file, line and function. A successful lookup with an absent component stays silent;
failure still returns null and later successful calls can recover. The change
does not add hooks, a scene scan, a new lookup path or a speculative gameplay fix.
Direct calls outside this helper retain the library's method-only reporting.

The local `component-lookup-diagnostic` fixture forces an actual bound lookup to
throw and checks caller/type reporting, containment, repeat suppression, absent
components and recovery. It does not reproduce the live failure. Root cause and
runtime resolution remain pending a fresh occurrence on the instrumented artifact.
The running user test and signed `ee112729` downloads have not been replaced.

Windows compile and existing binding/mod/sync/SPUD suites passed 432/432 steps;
the explicit diagnostic mode passed alongside station and earlier timer/notification
regressions. Exact input and executable hashes are retained in
`artifacts/integration-audit-20261031/station-lifecycle-verification.json`
outside the checkout. No runtime disappearance claim follows from these fixtures.
