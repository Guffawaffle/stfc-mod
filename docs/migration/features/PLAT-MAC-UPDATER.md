# PLAT-MAC-UPDATER — Xsolla game update source contract

Contract v1, 2026-11-03. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`, principally
`macos-launcher/src/XsollaLib.swift`, `UpdaterFileOperations.swift`, `rsync.swift`,
`rsync.cc` and `ini.swift`. Typed starting head is
`80bc425c7c06b36d22566d6e863531c3740fbbc5`; corresponding owners are under
`src/launcher/macos/`. This contract covers the Xsolla game updater. Distribution
packaging and operational mod signing have separate contracts.
[PLAT-MAC-LAUNCH](PLAT-MAC-LAUNCH.md) owns C++ launch/bootstrap.

## Existing update workflow

| ID | Frozen operation | Retained typed behavior or deliberate correction |
| --- | --- | --- |
| U1 | Read user Library/Preferences/Star Trek Fleet Command/launcher_settings.ini, General keys 152033..GAME_PATH and GAME_TEMP_PATH; strip one doubled leading slash. | XsollaUpdater.launcherSetting preserves paths, keys and normalization. readIni preserves section/key trimming, hash comments and key/value parsing; missing settings throw a descriptive error. |
| U2 | Read .version as &game=n; unavailable data means installed version zero. Fetch the platform/project update plan from the Xsolla endpoint using that installed version. | installedVersion preserves valid values and unavailable-zero behavior, while checking the split shape instead of indexing a possibly missing component. Version text has surrounding whitespace trimmed. The endpoint/project/platform inputs are unchanged. |
| U3 | XML action order is torrent_download, extract, patch, wait_actions and version; extracted_size is ignored and unknown actions are logged. | UpdatePlanReader retains that dispatch and final version selection. Missing required or nonnumeric attributes now fail the whole plan instead of force-unwrapping. XML parser errors remain failures. HTTP status and payload size are logged, as in frozen source; no new status/size acceptance policy is claimed. |
| U4 | Clear the configured Xsolla temp directory, make a private staging root, then execute the plan sequentially. | Same order and configured roots. URLSession download data moves into the substituted $temp_path target. The typed entry and progress callback are explicitly main-actor isolated; network waits suspend but synchronous file work blocks that actor. The current private staging URL is captured by deferred asynchronous cleanup after the updater exits, replacing TemporaryFolderURL plus explicit withExtendedLifetime around final copy/deletes. |
| U5 | Extract 7z payloads into the substituted target. | extractUpdateArchive now invokes the system /usr/bin/tar with separate archive/output arguments and --no-same-owner, creates the target directory, waits and rejects nonzero or abnormal termination. This replaces the old PLzma runtime; native format/metadata compatibility needs the existing archive fixtures on Mac. |
| U6 | Normalize patch rule paths to slash-separated relative components, reject empty/dot/dot-dot components and require a lexical child of staging. Skip _CodeSignature rules because launch signing regenerates signatures. | Same accepted path shape, lexical staging guard and signature skip. These are source policies, not proof of physical filesystem containment or native archive behavior. |
| U7 | A patch uses an earlier staged result if present, otherwise the installed source. Write to .patching, retain the basis POSIX mode, replace the staged target and remove failed partial output. | applyPatch preserves all steps. The binaries plan attribute remains diagnostic; both implementations use gamePath for installed basis files. The C++ rsync bridge now checks opens, uses RAII and propagates a final output-close failure as RS_IO_ERROR. |
| U8 | create makes a missing empty staged file; copy replaces a staged target with the patch payload; delete only records a path until finalization. | Same rule dispatch, unknown-rule logging and deferred deletes. Multiple patch actions can reuse already-staged content. |
| U9 | Merge staging into the live game, preserving symlinks as links and recursing through actual source directories. Then delete obsolete paths except those with staged replacements. | copyContentsOfDirectory and deleteGamePaths preserve the same order and behavior. Symlink classification still precedes directory recursion. Existing file-operation errors propagate. |
| U10 | Write pending .version atomically only after final copy and deletes succeed; plans without a version action leave the installed version unchanged. Temp cleanup failure is logged without replacing successful completion. | Same version-last and cleanup behavior, with the same &game=n bytes. Neither implementation makes the whole live installation replacement atomic. Mid-copy/delete failure can leave mixed files while retaining the old version, requiring another update attempt. There is no game-update rollback transaction to restore here. |
| U11 | Publish stage status and action counts to the ActionView delegate. | The typed main-actor callback retains the visible Updating current of total text and Planning/Downloading/Extracting/Patching/Waiting/Finalizing/Version/Cleanup/Complete labels. Frozen completion/per-patch enum cases were no-ops in ActionView; its assigned progress Float was not rendered. Removing those unused payloads/events loses no established visible status. |
| U12 | On success, set forceEntitlementReapplication; the launch path always checks the real signature before considering re-signing, clearing that flag if already valid. | The unused flag is removed. PatchEntitlements still checks actual loader entitlements at every launch and only requests signing when they are missing. This preserves the decision without retaining an advisory preference as authority. |

The installed-game update button is available only when the launcher observes an
installed game and a newer version, with update/running exclusion in ActionView.
Neither Xsolla updater has a repair button, update cancellation command or a
rollback transaction. Cancellation of an entitlement file panel belongs to the
launch/signing flow. Do not infer an existing cancellable game updater from the
Swift task type, or add a new repair/rollback feature as migration work.

## Checks and limits

No proven updater source regression was found by this comparison. Preserve the
current malformed-plan/version parsing corrections, output-error reporting,
owned staging lifetime and actual-signature observation. The existing source
limitations above are not silently described as new regressions or completed
transactional recovery.

`tests/launcher_archive.swift` compiles the production UpdaterFileOperations
extractor and tests LZMA/LZMA2/PPMd fixture content, Unicode paths and corrupt/missing
archive rejection. It does not exercise the XML plan reader, patch rule sequence,
staged copy/deletion/version ordering, visible status callback or real install.
`tests/launcher_rsync.cc` links the production bridge and generates a real delta,
checking reconstruction, missing basis/delta, invalid delta and output errors;
its Linux-only /dev/full case covers buffered-close failure. Those are the actual
fixture boundaries, not a full Swift updater or native-launch test.

Current Windows source inspection does not compile or run Swift/Foundation/
AppKit, invoke tar/codesign, access launcher settings, contact update services,
write a real game installation or qualify a current Mac artifact. The configured
Mac SDK and native Swift/Xcode host remain unavailable here. Native download,
staged finalization, failure/retry, metadata/signature compatibility and actual
launch acceptance remain open; Mcohall's existing pre-release reports stay bound
to that artifact. AAI and new upstream functionality remain deferred.
