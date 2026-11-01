# CFG-WRITER — preserving registered-key saves and shared quit

Contract version: 1. Status: frozen worker reconciliation and Windows fixture
qualification complete; exact-client native quit/UI and Mac qualification open.
Exact inputs, branch observations and receipts: [settings/state cohort](../FEATURE-COHORT-20261101-SETTINGS-STATE.md).

## Source and ownership

Frozen1baa55fe: `mods/src/patches/parts/runtime_config.cc`,
`mods/src/patches/runtime_config.h`, `mods/src/runtime_config_writer.{h,cc}`,
`mods/src/toml_editor.{h,cc}`, `mods/src/config_save.{h,cc}` and
`shared/toml/include/stfc_toml/editor.h`.
Target base300dcab4 uses the corresponding src/mod owners and pinned engine under
third_party/stfc_toml. Complete worker/config-save implementations are unchanged;
the engine header is byte-identical. No production change in this assessment.

Startup Configure owns the semantic disk baseline and File-selected path. Setting
definitions/gameplay keys register independently of lazy worker startup. Settings,
shortcut and gameplay consumers submit owned values; the worker performs I/O and
publishes per-key revisions. Status callbacks run on the established owner frame.
PrepareFrame, typed quit installation and shared dispatch retain their existing
ownership. Pending/session-only reload protection is the retained target improvement.

| ID | Obligation and mapping | Verification |
| --- | --- | --- |
| W1 | Missing startup keys differ from defaults; registration is idempotent and never rebases external conflicts; unregistered submissions reject | Existing worker, gameplay and disk-integration fixtures |
| W2 | Preserve comments, unknown data, line endings and unrelated keys; reject unsupported/non-finite/invalid values and compare actual bytes before replacement | Existing production editor/checked-replacement tests and pinned engine fixture |
| W3 | Latest queued same-key value coalesces; successful own writes rebase later queued intent, failures do not; another key's success cannot clear a failed key | Existing deterministically paused worker fixture, per-key status and retry cases |
| W4 | Live application and disk acknowledgement remain distinct; failed/unavailable/session-only intents survive unrelated reloads | Existing settings model -> runtime adapter -> actual file -> receipt -> reopened model test; capture/reload fixtures |
| W5 | Normal quit closes admission, flushes debounce and defers while work remains; nested/latest veto wins; resume only after observed worker exit on deferred quit | Existing real worker and controlled native-adapter tests; state coordination in STATE-QUIT |
| W6 | Rejected foreign/nested/force callers do not invent reload intent; status presentation cannot kill shutdown | Existing owner/quit/status fixture paths |
| W7 | Force-close cancellation and independent Windows deadline remain bounded best effort | Existing four disposable fixture-child cases; no game terminated by these tests |

An external editor can still race the final native replacement after the recheck;
this is the frozen optimistic contract, not an arbitrary-editor atomic transaction.
Retry preserves the acknowledged comparison base and does not authorize overwriting
an external conflict. Idle quit and deferred quit have distinct worker-exit paths.
The game retains its owner until process exit; no worker join was added to teardown.
Disabled/missing optional persistence hooks do not imply working native quit
coordination. Mac guards and checked entry remain, but current Mac execution is
not qualified. See the cohort for exact scope and commands.
