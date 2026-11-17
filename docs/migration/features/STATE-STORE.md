# STATE-STORE — versioned state merge and file replacement

Contract version: 1. Status: frozen store reconciliation and Windows real-file
qualification complete; native feature acceptance and Mac qualification open.
Exact inputs, branch observations and receipts: [settings/state cohort](../FEATURE-COHORT-20261101-SETTINGS-STATE.md).

Frozen1baa55fe inputs: `mods/src/mod_state.{h,cc}`,
`mods/src/mod_state_writer.h`, `mods/src/patches/pinned_ship_state.h`.
Target base300dcab4 retains their store/worker algorithms, adapting File paths,
feature startup and the existing normal-quit APIs. No production change here.

PrepareAsync resolves the ordinary or selected custom settings-adjacent state
path before actions and retains one process-lifetime worker. Pin and officer
consumers submit group values and observe exact receipts; pin native publication
requires Saved, while officer session ordering can remain live after a failed
save. Console-opacity group support remains in the store API; this assessment
does not establish an active native console consumer. No new profile discovery,
store format, database or generic persistence owner is introduced.

| ID | Obligation and mapping | Verification |
| --- | --- | --- |
| S1 | Read missing/malformed/non-object/future-version input as unavailable without rewriting it | Existing real-file ReadAndUpdatePolicy cases |
| S2 | Explicit Update/TryUpdate retains frozen recovery of malformed/non-object data, accepts missing/version1 and refuses unsupported versions or callbacks changing the reserved version | Actual transaction fixture, now including version2 failure through the queued owner |
| S3 | Serialize read-modify-write under the OS lock, reread latest file and preserve unrelated nested fields; same-directory temporary replacement and failure handling remain | Existing two child processes each merge 30 changes; TryUpdate refuses held lock; callback failure preserves bytes |
| S4 | Whole desired group values coalesce; paired pin IDs/legacy names travel together; group receipts name latest accepted request and only commit success becomes Saved | Existing paused worker/PendingPinOrder checks and new real-file three-group quit drain |
| S5 | Admission budgets charge moved allocation capacity as well as sizes; reject oversized/invalid groups without changing pending work | Existing StateWriter capacity, replacement and retained-byte tests |
| S6 | Officer/pin IDs keep string encoding; opacity updates preserve unrelated console fields and remove the old opacity key | Existing production transaction fixture and exact saved document assertions |
| S7 | Normal drain closes admission, flushes pending groups, preserves Failed and reopens after veto | STATE-QUIT contract and real-file drain/recovery fixture |

Update's recovery policy differs deliberately from Read's non-mutating refusal;
it is retained source behavior, not a new format migration. This assessment
qualifies file operations and receipts on isolated Windows files; it does not
prove native rendering, profile isolation, power-loss durability or Mac behavior.
The File adapter's scoped-path mapping was inspected, not exercised by the
fixture path override. Existing native pin click/drag acceptance remains separate.
