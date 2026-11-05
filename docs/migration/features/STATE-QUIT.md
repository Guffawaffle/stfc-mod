# STATE-QUIT — pending state on normal quit

Contract version: 1. Status: retained target improvement verified through Windows
adapter/worker/real-file fixtures; exact-client native quit and Mac remain open.
Exact inputs, branch observations and receipts: [settings/state cohort](../FEATURE-COHORT-20261101-SETTINGS-STATE.md).

The separate [Mac Command-Q investigation](../MAC-COMMAND-Q-20261101.md) corrects
three retained managed owners that could release handles during process static
teardown. Controlled exit cases pass; the reported Apple Silicon quit warning
and native shutdown sequence remain unqualified. The drain owner below is unchanged.

The frozen1baa55fe process-lifetime state worker has no normal-quit drain. Its
RuntimeConfig WantsQuit/Update coordinates TOML only. Target base300dcab4 already
adds BeginQuitDrain/QuitDrained/ResumeAfterQuitVeto in mod_state and coordinates
them from the existing RuntimeConfig quit owner. Preserve this improvement.
The prior inventory statement that the target drains TOML only is superseded.
No production change or additional detour was needed in this assessment.

| ID | Obligation and mapping | Verification |
| --- | --- | --- |
| Q1 | An allowed quit closes both owners' admission before deciding whether exit is idle; outstanding state blocks exit even when TOML is idle | Existing controlled RuntimeConfig adapter fixture; real-file check queues all three state groups under an OS lock and verifies closed admission |
| Q2 | Main-thread callback neither joins nor performs state-file I/O; pending debounce is bypassed and in-flight work still blocks drain completion | Existing paused StateWriter commit and real-file wrappers; wrappers only signal/query worker state |
| Q3 | Deferred quit resumes only after TOML worker termination and state settlement; latest/nested veto takes precedence | Existing native-adapter fixture with independently controlled state settlement and worker exit |
| Q4 | Failed writes settle as Failed, never Saved; unsupported future-version file stays intact and cannot stall settlement forever | New production state-file version2 case plus existing paused failed commit fixture |
| Q5 | A resumed native veto restores both owners' admission without discarding acknowledged values/failures; later save can succeed | Existing RuntimeConfig resumed-veto and worker restart cases; new real-file state admission/retry sequence |
| Q6 | Native quit request is consumed once; force termination is a separate best effort | Existing adapter vote/callback and Windows force-close child fixtures |

Requires installed RuntimeConfig quit/frame delivery and an initialized state
owner for pending state work. Missing/disabled hooks and unavailable frame service
do not establish this native guarantee. The exact Unity callback is substituted
in adapter tests; the state-file check calls the shared APIs directly and does
not issue a native quit. These complementary tests do not prove a live game's
shutdown sequence. Failure settlement means completion, not successful saving or
crash/power-loss durability. No new player test gate is imposed before continuing.
