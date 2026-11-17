# Media picker cancellation and path-entry lifetime

## Scope and retained implementation

This SET-AUDIO-PICKER/SET-IMAGE-PICKER/P24 assessment starts at clean signed
`a4c81d9b7f6886985eef70208851a993025892b2` on existing `play-dev-ripper`.
The preceding Mac teardown fix and investigation were pushed at that head at
Guff's request. That push does not publish a Mac binary or replace the preview.

The selected media workflows already use the typed host and shared Settings
owners. No production, test, hook, build or configuration correction was required
by this bounded assessment. Frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`; its image fallback lives in
`mods/src/settings/upstream_settings.cc`, not a separate image_path_entry header.
Existing source contracts retain the broader provenance and obligations:
[audio](features/SET-AUDIO-PICKER.md), [image](features/SET-IMAGE-PICKER.md).

| Responsibility | Retained typed implementation and check |
| --- | --- |
| Native file dialog, owned UTF-8 result, empty cancellation | `file_picker_windows.cc` / `file_picker_mac.mm`; Windows COM task and Mac main-queue NSOpenPanel adapters remain distinct |
| Nonjoining future, owned background inputs | `settings/file_picker_task.h`; existing worker fixture verifies off-thread work, pending-future destruction and exception delivery |
| One audio selection/load, stale cue/reset protection | `settings/audio_alerts.cc`; existing fixture checks cancel/failure, observed replacement, unchanged reset and reset during decoding |
| Image snapshot, validation ticket, stale edit/reset protection | `settings/image_file_setting.h`; existing fixture checks cancel/failure, single admission, reset during a probe and requested versus active restart state |
| Bounded path validation, cancellation generation | `settings/image_path_validator.h`; existing worker fixture checks one active/latest pending, debounce, stale completion and failures |
| Exact fallback and page visit | `settings/image_path_entry.h`, `config_settings.cc` and existing text-entry owners; production catalog fixture checks trimmed/empty paths, validation notification, fallback routing and rejected commit after page departure |

External file pickers keep process-level intent. Closing their page alone does
not cancel an explicitly opened chooser. They retain no borrowed native view;
completion is polled on the Settings owner and cannot overwrite observed newer
intent. Exact entry is page-bound and rechecks the visit and observed value before
committing. Missing picker admission can route to the editor; missing shared frame
delivery does not establish a usable editor. No unhealthy anchor was forced in
the live check. Image regular-file validation does not prove successful decoding.

## Windows native check

Computer Use operated client 273, ordinary C-game PID 27260,
start `134380312972477042`. Installed source remains
`f53b07ea94a62aad4a7364616ab6ba029cc16f9f`, build
`ax-zig-20261101T182606-4e62a1bd`, DLL SHA256
`87455393F134ACBC6D8C8186E84303CB711C347721A237A324F709C6AC07535E`.
The inspected media adapters and native editor sources are unchanged between
that installed source and this assessment base; this is an existing-artifact
check, not a newly deployed build or a Mac teardown trial.

From Mod Settings search, Loading Screens opens its image chooser with PNG/JPG/
JPEG filters. Cancel returns to the unchanged empty path. Reopening succeeds;
Escape dismisses the second chooser without leaving a dialog behind. Battle
victory opens its audio chooser with WAV/MP3 filters, disables duplicate selection
while pending, and retains Off after Cancel with a visible cancellation status.
The chooser reopens. With it pending, the game can be brought forward and Back
leaves the audio page. The chooser remains available; Escape cancels it afterward.
Reopening the page still shows Off and cancellation status.

Search is cleared and Settings/profile are closed back to the system/chat view.
No files were selected, no audio preview was played and no gameplay action was
issued. All four game-root TOML hashes match before/after. The picker-interaction
log delta contains no warnings, errors or component lookup diagnostics. One
automation click failed because geometry was unavailable; a fresh chooser
observation and coordinate click recovered it. This was a tooling error, not a
reported game exception.

## Evidence and remaining qualification

Existing Windows `test-mod` passes **152/152 build/run steps**, not 152 tests,
including the actual catalog/audio/image/path/task fixtures above. No test
registration or CI workflow was added. No fresh native build, deployment, cycle
or preference edit was needed for this assessment.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-media-20261101/` contains
model-tests-command.json/log, source-inputs.json, pinned frozen owners,
runtime-before.json, native-observations.json, config-comparison.json and
native-log-summary.json. Runtime observation resolves the intended installation,
ordinary profile, exact process and module path; disk hashes are not an independent
measurement of every in-memory byte.

The observed cancellation/reopen sequences are qualified on that Windows artifact.
Successful live file selection/save/restart, Unicode/network paths, image decoding,
audible preview, native exact-fallback routing and Mac interaction remain separate
checks. Feature classifications and P24 stay open. Continue with existing choice/
slider value controls and their save/reset presentation; keep deferred file Clear
UX and other parked scopes deferred. Mac Command-Q retesting remains independent.
