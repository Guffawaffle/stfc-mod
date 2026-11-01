# Contained UI component lookup error

## Diagnosis and correction — 2026-11-01

The current reproduced occurrence is a stale native selection, not an established
thread race. Unity's EventSystem returns a non-null managed GameObject wrapper
whose native Unity object has already been destroyed. Keyboard focus detection
treated the non-null pointer as usable and called GetComponent<TMP_InputField>.
That throws and returns the binding's default null, making focus read false.
It is contained, but is a real failed read and possible repeated exception work;
the once-per-method log cannot establish frequency or harmlessness.

Signed diagnostic936aeb0f reproduced by selecting the Mod Settings search field
on client273 at11:58:27.517 local. Exact DLL/PDB native stack resolves to
unity::Key::IsInputFocused -> TryInvoke<TMP_InputField*,Il2CppReflectionType*>
from the shared ScreenManager frame. It does not point to the Settings tree
builder, a store/AAI caller or row scrolling. See native-trace.log and
native-trace-symbols.json under artifacts/ui-lookup-20261101. The first lifecycle
attempt built successfully but refused deployment because the game process
changed; nothing was stopped by that attempt. Retargeted guarded cycle succeeded.

Signed diagnostic17e378d0 then measured the receiver around the same lookup,
without skipping it. At12:07:53.627/.628 local, liveness succeeded with **false**
both before and after the thrown lookup, and selection identity remained the
same. lifetime-observation.log binds the report; focus-state-build.json and
focus-state-symbols retain the exact code/DLL/PDB. Thus the object was already
destroyed before lookup; destruction racing that particular call is unnecessary
to explain the exception. How long EventSystem retains it has not been measured.

The correction changes only shared input-focus detection: use existing checked
Unity liveness, and skip a confirmed-destroyed selection. Unknown liveness keeps
the previous lookup path; it is not silently classified as destruction. Live
objects and genuine lookup failures retain their existing behavior/reporting.
No binding ABI, hook, shortcut mapping, setting or native selection is changed.
Temporary stack/state probes are removed from the final code.

The controlled runtime exercises actual production Key::IsInputFocused and
ClearInputFocus. New destroyed-wrapper regression fails against17e378d0 because
GetComponent is still called; the final case requires no component lookup and no
clear-selection callback. It also checks next-frame live selection recovery,
unknown liveness preserving the lookup, and a genuine live lookup exception
remaining contained. This fixture recreates the observed lifetime condition;
the separate native trial establishes its real-client occurrence.

Final production guard/tests are signed207becc9c9c4befdbf77133ecda2e75508ec10ce.
`zig build test-il2cpp -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast
-Dtest-optimize=Debug -Dmoddbg=true -j4 --summary all` passes163/163 build/run
steps. Before/final command, cwd, base, duration and exit are retained in
destroyed-before-command.json/destroyed-final-command.json, with bounded logs.
final-inputs.json hashes the actual changed production/fixture inputs.

The clean, stable local Windows AX releasedbg build passes41/41 steps and the
proxy check, retaining clean AAI4bece2e5/UI Lab. Candidate DLL SHA256:
`C086DD0760FDC70CD52F3D4393230044DCDB38B5473B94726D1304A42B789AF3`.
Exact receipt and DLL/PDB are in final-build-receipt.json/final-candidate.
Final build command/cwd/source/duration/exit: final-build-command.json.

**Not deployed or native-after qualified.** Guff entered a wave and explicitly
requested no cycling. All subsequent work was local build/evidence only; no UI
input, deployment or cycle followed that request. The live DLL remains diagnostic
17e378d0, SHA256 F0C2720E7D51C989B6499A2FD77D0A772E8184CFC92C5D4367BEA4256C8EE145,
PID2500/start134380300047161782 at its last process observation. The on-disk live
DLL hash is unchanged after the final local build. deployment-hold.json records
the distinction. Do not resume a cycle/deploy until Guff lifts the hold. Then
recheck exact runtime identity, deploy the guarded source and repeat selection,
typing/result/Back/close-reopen with a fresh complete log; confirm the prior
GetComponent exception is absent while normal focus/navigation still work.
No Mac execution or universal elimination of all GetComponent failures is claimed.

## Initial continuation evidence — 2026-11-01

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
