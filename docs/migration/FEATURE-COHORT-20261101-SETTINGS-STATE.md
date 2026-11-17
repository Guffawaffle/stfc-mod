# Settings saves and state lifecycle — 2026-11-01

Bounded source reconciliation for CFG-WRITER, STATE-STORE and STATE-QUIT in the
existing Ripper checkout. Target base is signed
`300dcab47ed2f0d0b1a0141e8f6478299b8c5399`; frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`.

## Result

No production correction was needed in these owners. The complete settings writer
implementation and config replacement implementation match frozen source bytes.
The pure TOML preparation engine also matches the frozen header. The state-store
changes are file-adapter/feature registration changes plus the existing quit-drain
APIs. The shared RuntimeConfig quit owner already drains state as well as TOML.
The inventory's statement that it drains TOML only was stale.

The new real-filesystem check in `tests/mod_state_transaction_test.cc` exercises
the production state owner, worker, OS lock, group merge and replacement through
BeginQuitDrain/QuitDrained/ResumeAfterQuitVeto. It queues three groups while an OS
lock prevents their early commit, checks closed admission, then verifies exact
Saved revisions, paired pin IDs/names and unrelated nested fields. A version-2
file refuses a queued edit, settles as Failed, remains byte-identical and permits
drain completion. Reopening admission and retrying against supported fixture data
succeeds. No native Unity quit is invoked by this fixture.

## Exact branch observations

| Role | Revision | CFG-WRITER | STATE-STORE | STATE-QUIT |
| --- | --- | --- | --- | --- |
| upstream dev | `ff179589f834b95331b4d7e38b1054fb6ff5ace9` | Named owner absent | Named owner absent | Named drain API absent |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer present | Consumer present | State drain absent |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Consumer present | Consumer present | State drain absent |
| refactor base | `300dcab47ed2f0d0b1a0141e8f6478299b8c5399` | Consumer present | Consumer present | Shared quit drain present |

These are source observations, not in-game control or runtime acceptance claims.
The frozen state worker has process lifetime but no normal-quit drain. Retain the
already implemented target improvement; do not restore that omission for parity.
The target's native installer uses typed Unity methods, CheckedEntry, HookSet and
the sole frame dispatcher, replacing manual lookup/detour installation.

## Verification and evidence

Zig 0.16.0, native MSVC environment, x86_64-windows-msvc, ReleaseFast production,
Debug tests, moddbg=true, four jobs. Final command:
`zig build test-runtime-config test-runtime-reload test-mod-state test-toml-editor
test-config-save -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast
-Dtest-optimize=Debug -Dmoddbg=true -j4 --summary all`.
Result: 37/37 build/run steps succeeded. The focused state run passed 8/8 steps.
Step counts are not test counts. Existing checks include real settings-model to
adapter/writer/editor/file/receipt/reopen integration, actual two-process state
merges, paused commits, nested/vetoed quit, rejected callers, per-key failures,
coalescing, cancellation and four bounded force-close fixture child processes.

Evidence directory:
`D:/dev/stfc-workspace/artifacts/settings-save-state-20261101/`.
It contains exact frozen files, source diffs, branch observations, changed/test
input hashes, engine hash comparison and command/cwd/base/duration/exit-code logs.
Production inputs are unchanged; only the state transaction fixture and migration
records changed. No new CI registration or game cycle/deployment was needed.
The installed code checkpoint remains fb615d48; its Windows boot was verified in
the preceding settings-load slice. No live settings or state files were edited.

## Remaining scope

Contracts: [CFG-WRITER](features/CFG-WRITER.md),
[STATE-STORE](features/STATE-STORE.md), [STATE-QUIT](features/STATE-QUIT.md).
Exact-client native quit/UI delivery and current Mac execution remain unqualified.
Normal drain settles failed receipts; it does not turn failures into Saved or
promise crash/power-loss durability. Force termination remains a separate best
effort. Full settings-key/default/alias reconciliation under CFG-LOAD-RELOAD is
the next bounded task; complete settings UI and whole-refactor parity remain open.
Existing parked scopes and the accepted mission/pin behavior stay unchanged.
