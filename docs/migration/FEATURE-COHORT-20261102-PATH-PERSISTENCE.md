# File namespaces, session logs and persistence routing

Existing `play-dev-ripper`, clean starting head
`f65fa9af1219ea1e5e9adbfc42202d93bafdb9f9`; frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Contracts:
[PLAT-PATHS](features/PLAT-PATHS.md), [PLAT-MAC-LOG](features/PLAT-MAC-LOG.md).
AAI and new upstream work remain deferred.

## Corrections and retained owners

The ordinary Mac working-directory fallback opened a log without rotating its
history. Frozen startup reused the same rotate/open operation after selecting
either location. The existing File owner now supplies a small native-path session
opener used by startup. Each attempted path rotates its own five backups before
append-opening; rotation errors accumulate, both-open failures retain a console-
ready logger, and a missing fallback option propagates the open failure. Startup
still supplies fallback only for ordinary Mac launches, adds stdout/levels and
installs the existing error reporter. Explicit custom/admitted paths stay confined.

The refactor also changed duplicate -ccm selection from first to last. Restore
the first complete occurrence, including an empty operand, while consuming later
operands and keeping the target's improved debug/trace scanning. Preserve Unicode
Windows paths, corrected relative Mac custom paths, immutable admitted precedence
and selected-directory cleanup. Do not restore source path/cleanup defects.

Settings reload/save, state and local-media consumers already follow the selected
File path. Existing CFG-WRITER/STATE-STORE/STATE-QUIT owners and schema remain
unchanged. Supplied-context routing verifies admitted names, state derivation and
empty/relative refusal without ordinary/custom namespace activation. It does not
execute real admission, catalog/preference stores or native isolation hooks.

The retained -ccm filename/reserved profile-selector ambiguity is explicitly
recorded in the path contract; it existed in frozen source. No profile parser or
silent ordinary fallback is introduced as an incidental logging correction.

## Verification and evidence

The fixture-first extraction preserved preferred-only rotation and the unchanged
selector, then built 191/191 steps. Four cases independently fail: log-fallback,
custom-repeated, custom-empty-first and custom-empty-later. Six control cases pass.
The captured source hashes identify this extraction, not an untouched parent tree.
A function-try-block ensures expected assertion failures unwind the registered
logger and privately owned temporary directories.

Corrected Windows core/binding/mod checks pass 423/423 build/run/install steps,
Zig0.16, x86_64-windows-msvc, ReleaseFast dependencies and Debug fixtures. Nineteen
explicit production File cases cover ordinary/custom/admitted names, repeated/
empty/missing selectors, relative admission refusal, actual Unicode log files,
fallback/history, rotation failure with append, both-open failure, no-fallback
confinement and custom/named cleanup. Two existing bootstrap/hook-policy fixtures
also pass. No new test target or CI registration is added.

The shared opener is exercised with real spdlog/filesystem operations; temporary
directories and process working-directory changes belong exclusively to each
standalone fixture. Cleanup removes selected obsolete files while comparing all
unrelated CWD and selected primary/runtime/log/cache/state bytes, including a
second idempotent pass. Real stores/settings/logs are never used.

Execution friction is retained: two Unicode log rotation cases failed under the
restricted tool process, then passed with the identical executable in authorized
normal execution. All nineteen filesystem cases and two policies pass there.
This is an environment-dependent result, not evidence of a product regression.
The restricted comparison is retained separately rather than relabeled as a pass.

Evidence directory: `D:/dev/stfc-workspace/artifacts/path-persistence-20261102/`.
Before/final build logs, before-cases.json, restricted-cases.json, final-cases.json
and the final input/executable manifest bind the exercised layers. Final formatting,
signing and independent review are recorded against their exact checkpoints.

## Qualification and continuation

Windows qualifies the common file opener and supplied-context policy, not native
Mac Foundation/startup/migration, actual argument acquisition, current-client
profile isolation or real persistent-store equivalence. Existing synthetic Mac
migration coverage and M3/M4 evidence stay distinct. Mcohall's pre-release testing
does not silently qualify these newer local changes.

No game cycle/input, live TOML/configuration edit, account/catalog access,
deployment or preview replacement occurred. Both capability classifications and
overall migration parity stay open. Continue remaining window/profile-title and
cursor source reconciliation; preserve parked diagnostics/orbit/AAI decisions.
