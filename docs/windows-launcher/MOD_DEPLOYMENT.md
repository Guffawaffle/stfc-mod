# Windows Launcher Mod Deployment

Status: WL-004 transaction core implemented; Home action wiring and installed-client smoke remain in progress.

## Ownership boundary

The launcher manages only `version.dll` plus its transaction-scoped stage and
rollback names listed in
[`GAME_DIRECTORY_FILE_ALLOWLIST.md`](../GAME_DIRECTORY_FILE_ALLOWLIST.md).
It never treats the selected game directory as launcher-owned. Existing manual
`version.dll` files require an explicit `AdoptAndPreserve` decision; the prior
bytes are retained under launcher-owned rollback state.

## Verified transaction

`ModDeploymentService` is UI-independent and enforces this order:

1. validate an exact game directory containing `prime.exe`;
2. reject mutation while the game is running or another process holds the
   operation lock;
3. fail closed on an incomplete or unreadable persisted journal;
4. require HTTPS plus bounded artifact metadata;
5. verify HTTP status, declared length when present, actual length, and
   SHA-256 before writing beside the game;
6. write and re-verify a same-volume stage file, then require Windows
   `WinVerifyTrust` and the configured Authenticode publisher identity;
7. journal `Committing`, preserve any existing artifact, and replace the
   target;
8. re-verify target size, SHA-256, and the expected embedded numeric file
   version;
9. persist managed ownership before journaling `Committed`.

The expected embedded version is deliberately separate from the release tag.
For example, release `2.1.0-guffa.8` carries numeric Windows file version
`2.1.0.8`; release discovery must perform that explicit mapping instead of
comparing the descriptive tag directly to `FileVersionInfo`.

The HTTP downloader refuses to buffer more than 128 MiB. Unknown journal
schemas, corrupt state, invalid metadata, non-HTTPS artifact URLs, wrong
targets, and externally changed managed DLLs fail closed.

## Recovery and uninstall

Every phase is persisted through an atomic state-file replacement. An
incomplete transaction blocks new mutations. `RecoverAsync` is deterministic
and idempotently restores the preserved artifact or removes a partially
committed fresh install, restores the previous installed-state record, and
removes transaction-scoped files.

Uninstall verifies that the live DLL still matches launcher-managed state. It
then uses the same operation lock and journal boundary. A fresh managed DLL is
removed; an explicitly adopted prior DLL is restored. Configuration, logs,
runtime snapshots, and unrelated game files are untouched. If the managed DLL
changed outside the launcher, uninstall refuses to guess ownership or delete
it.

## Automated evidence

The core test suite covers:

- invalid game targets and game-running denial before download;
- explicit adoption of a pre-existing DLL;
- non-200 HTTP, declared-size, actual-size, and SHA-256 rejection;
- bounded HTTP reads;
- mandatory Authenticode rejection before commit;
- embedded-version mismatch rollback;
- injected failure after every persisted deployment boundary;
- concurrent mutation denial;
- corrupt persisted state;
- startup recovery from an interrupted commit;
- allowlist-only uninstall, adopted-artifact restoration, external-change
  refusal, and uninstall rollback.

Installed-client mutation smoke is intentionally deferred until release
discovery and Home confirmation present the exact target, version, and effect
to the user. Unit tests use isolated synthetic game/state directories and do
not modify the real STFC installation.
