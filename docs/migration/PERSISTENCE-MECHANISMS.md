# Settings and local-state persistence — bounded extraction assessment

2026-10-27; scoped to the `play-dev-ripper` preset integration. This is an
inventory of the current mechanisms, not a new storage API or authorization to
move stores. Logging remains independent.

## Existing owners and actual duplication

| Concern | Settings TOML | Local JSON state |
| --- | --- | --- |
| Path ownership | `File::Config()` resolved through `File::Path`, captured by `runtime_config::Configure`. | `mod_state.cc::state_path`: custom-config stem plus `.state.json`, otherwise the existing ordinary state filename. No new account/instance inference. |
| Read and validation | `config.cc` loads typed config; runtime `TomlEditor::Save` rereads exact bytes and uses the pinned preserving TOML editor. Invalid/unsupported edits are rejected. Missing values differ from defaults. | `mod_state::Read` parses JSON and checks shape/version. Update rereads under its lock. Existing malformed/non-object content is replaced by the established update policy; a future schema version is refused. Do not silently impose either policy on the other consumer. |
| Concurrency | One process-local writer and per-key expected values. Text is rechecked before replacement, but an arbitrary external editor can race that check: this is not a filesystem compare-and-swap or cross-process locking protocol. | Process-local group queue plus an OS-exclusive sidecar lock (`CreateFileW` sharing exclusion / `flock`). The actual JSON transaction rereads while holding that lock, composing unrelated groups. Cooperating writers must use the same lock. |
| Queue semantics | Latest pending value per registered section/key; startup/acknowledged comparison values, delayed submissions, same-key retry and per-key outcomes. | Latest pending value per fixed group; pin IDs/names are one transaction. Explicit entry/retained-byte bounds and group revisions. Accepted session order and successful persistence are separate facts. |
| Temporary file mechanics | `config_save.cc::ReplaceConfigText`: sibling file, exclusive creation, checked writes and checked close before commit. | `mod_state.cc`: sibling filename with PID/sequence; `ofstream` truncating creation, checked write/flush/close. This is not equivalent exclusive creation. |
| Windows replacement | `ReplaceFileW` with backup and missing-destination non-overwrite fallback; recoverable partial replacement files retained. | `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`; removes temporary file on reported failure. Backup/recovery and metadata preservation are different. |
| macOS replacement | Copies existing permission bits, then rename. | Rename, without the same explicit permission-copy step. Neither implementation is qualified here as universally power-loss durable. |
| Failure reporting | Structured `Outcome` distinguishes conflict, invalid document, unsupported, I/O error and cancellation; UI save status/retry/reload protection use the corresponding revision and value. | Transaction returns success/failure; the group writer publishes Pending/Saved/Failed by revision. A successful queue admission is not a saved receipt. Existing warning logs retain contextual details. |
| Shutdown | One shared native quit owner drains TOML and local-state work; native veto resumption is already supported. | Participates in that same quit owner. No preset-specific quit hook or second shutdown manager. |

Source links: [config_save.cc](../../src/mod/config_save.cc),
[toml_editor.cc](../../src/mod/toml_editor.cc),
[runtime_config.cc](../../src/mod/runtime_config.cc),
[runtime_config_writer.cc](../../src/mod/runtime_config_writer.cc),
[mod_state.cc](../../src/mod/mod_state.cc),
[mod_state_writer.h](../../src/mod/mod_state_writer.h).

The meaningful duplication is checked file reads, staging sibling temporary
files, checked write/flush/close, platform replacement, and failure cleanup.
The queues, expected-value comparison, JSON locking/schema, preserving TOML
syntax, destination selection and ownership are consumer behavior, not
duplicated generic I/O.

## Follow-up: extract only the byte-file mechanisms

A bounded follow-up is justified, particularly because the JSON staging path
does not currently have the TOML writer's exclusive creation and Windows
replacement/recovery treatment. Do not route JSON through `ReplaceConfigText`:
that function deliberately parses TOML before writing.

1. Preserve/document the existing per-consumer tests and policies above.
2. Extract checked byte reads and exclusive sibling-file staging, with explicit
   close/commit/cleanup outcomes. Keep format validation outside the helper.
3. Specify replacement, symlink, permissions, backup and durability behavior
   explicitly. Moving JSON onto stronger replacement semantics is an intentional
   behavior change requiring its own failure tests, not a cosmetic deduplication.
4. Retain the state lock around its whole read/modify/commit transaction and
   retain TOML's expected-value/text conflict checks. Do not invent a shared queue
   or claim these become an external-editor compare-and-swap.
5. Map mechanism failures into each existing consumer's outcome/receipt model.
   Do not introduce a generic persistence framework or alter store schemas/paths.

No part of that extraction is implemented in this preset slice. In particular,
do not reuse this work as authorization for SQLite, settings migration or an
asynchronous logging conversion.

## Current verification and its limits

`settings_persistence_integration_test.cc` executes the production value model,
persisted definition, runtime adapter, background writer, preserving editor and
checked filesystem replacement against an exclusively created fixture directory.
It tests pending versus persisted outcomes, comment/unrelated-key preservation,
external conflicts without overwriting, same-intent retry, queued-save quit
drain and recreation of the owner/model from disk. Two bounded, deterministic
save gates separate admission from completion without relying on a fast disk or
timed sleeps. The native startup acquisition, frame/quit delivery and separate
state-owner drain are simulated; it is not a native Settings UI or real-profile
trial. Existing catalog, state transaction, state-writer, replacement-failure and
quit/veto fixtures cover their own boundaries separately.

Preset display order is queued through `mod_state::QueueOfficerOrder`; the
feature reports an accepted local move, pending disk save and exact successful
receipt separately. A failed or unadmitted save leaves session order usable and
does not claim persistence. Game preset contents remain a third boundary:
`OnSaveSlotsSuccess` is forwarded once by the native adapter, not implemented by
either file writer. No network/server-save guarantee follows from a fixture.
