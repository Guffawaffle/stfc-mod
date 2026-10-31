# Contained UI component lookup error

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
