# M4: configuration and state preservation

This follows [M3](M3-PROFILES-HOST.md) and the
[gameplay-first cut](GAMEPLAY-FIRST-CUT.md). The native Settings UI remains
deferred. Persistence behavior required by gameplay is not deferred with it.
No schema, store location or embedded dependency pin is changed.

## M4A: checked startup document output

The [checked writer](../../src/mod/config_save.cc) is ported from downstream
`45d1deaffddc3706e5c8232cf746ca46ef67be91`, `mods/src/config_save.{h,cc}`.
It is the existing implementation, not another TOML editor or storage layer.
[Config](../../src/mod/config.cc) uses it for generated startup defaults and
runtime vars. The bounded change does not alter settings parsing, defaults,
registered keys, hooks, live profile selection or file routing.

- Serialize and parse-check TOML before touching a destination. Values use
  toml++ encoding; supplied text is not interpolated as arbitrary TOML values.
- Stage an exclusively created sibling, check the complete write and close,
  then use the existing native replacement protocol. Preserve ordinary file
  permissions and existing symlink behavior. Windows partial replacement errors
  can retain staging/backup files for recovery instead of deleting evidence.
- Failure of optional startup output, including path preparation, reports a
  warning without preventing readable settings from becoming active. Warning
  failure itself does not turn that optional output into mandatory startup.
- Existing primary TOML is not rewritten just to emit effective settings.
  Unknown keys, comments and malformed source bytes remain available. Defaults
  may be active after parse failure; that is not permission to overwrite the
  malformed primary. Default creation is attempted only after an absent path is
  established. Generated vars remain a whole document of known effective values.
- Reload still rejects a candidate before changing settings, shortcuts or the
  published immutable sync snapshot when vars-path preparation/replacement fails.
  Checked write failures now participate in that rejection as well as path errors.

The writer does **not** promise power-loss durability, cross-process locking,
compare-and-swap with arbitrary editors or exclusive create-only startup.
Existence can change between the initial check and replacement. An optional
expected-text check narrows a conflict window; it does not make the replacement
a filesystem CAS. Those are preserved limitations, not new isolation claims.

### Qualification fixtures

- [config_save_test](../../tests/config_save_test.cc): escaped/Unicode values,
  invalid generated header, occupied destination, no leaked temporary files,
  real Windows sharing failure, and POSIX mode/symlink preservation.
- [config_save_failure_test](../../tests/config_save_failure_test.cc): test-only
  short-write/failed-close substitutions exercise production cleanup and preserve
  previous readable bytes. No runtime failure-injection switch is shipped.
- [config_reload](../../tests/config_reload.cc): actual Config startup/reload,
  settings/shortcut activation and immutable readers spanning twenty reloads.
  Separate first-use processes exercise failed startup output, malformed primary
  preservation and valid absent-primary default creation. Existing reload and
  invalid-TOML rejection checks remain.

The new writer fixtures exclusively create their own temporary root and accept
no caller-supplied settings path. The Config fixture uses its existing synthetic
File boundary. These are disposable filesystem tests, not real profile/store or
game trials. `test-config-save` selects the focused cases; `test-mod` and
`build-tests` also include them.

### Source-bound results

Implementation/test candidate: **`154b598e160acc3267925a24eb7a87b9c2afd617`**,
tree `81abec05717d53b048841919591dbab947f36f6c`.

| Lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 138/138 steps; 8/8 reported Zig tests; native and checked-writer fixtures pass; exact 18-export/static-CRT/identity verification |
| Windows x64 Zig 0.17, Profiles host | 138/138 steps; same native and checked-writer fixtures; exact PE/identity verification |
| Windows x64 Zig 0.16, default off | 116/116 steps; 8/8 reported Zig tests; missing optional inputs remain inert; exact 16-export/static-CRT/identity verification |
| WSL/Linux x64 Zig 0.16 | 104/104 steps; 8/8 reported Zig tests; real writer's POSIX mode/symlink checks and all independent Config startup modes pass |

Independent Windows read-back verified source/tool/prefix/artifact/log hashes,
actual fixture success markers and actual DLL link commands. Both host lanes
ran three inert Profiles checks; four host-option negative controls ran on
0.16, not repeated on 0.17. Opposite-export-mode rejection ran on all three
Windows outputs. The derived host adapter and one verified zlib provider remain
the enabled closure; the default-off closure contains no Profiles/prefix inputs.

Host DLL SHA-256:

- 0.16: `34be04db47ae659c5829d93617f1f3ddee24afc319f0ffb78d3769cc0ea2accb`.
- 0.17: `5fcc6cd52c9398de6aa9812495e8aeb93e2799a493beb758ec1c2c84f6a115fc`.

The first Windows malformed-primary fixture compared LF source text with bytes
written in Windows text mode. It was corrected to capture actual preexisting
bytes and compare them after startup, preserving the assertion for CRLF and LF.
The first Linux harness used Windows-only resource-identity options, which the
portable build does not register. That setup refusal and its exact inputs are
retained under `attempts/linux-setup1`; the corrected gate uses candidate/hash
receipts without inventing PE resources for Linux fixtures.

No live configuration, preferences, browser or account store was used. Windows
uses the same installed MSVC/SDK fallback as M3. macOS and real client behavior
remain unqualified. M3's non-game DLL-loading result belongs to its exact older
artifacts; these M4A DLLs were built and PE-checked, not loaded or deployed.
The Windows load-time GameAssembly ordering limitation is documented in M3.

## Remaining M4 dependencies

M4A alone is not complete gameplay persistence parity. In particular, the
foundation dock save path has not acquired the downstream preserving per-key
writer simply because generated startup files are now checked.

1. Port the existing preserving `TomlEditor` and registered-key
   `RuntimeConfigWriter`, using the pinned shared TOML implementation. Keep one
   owner, original startup **disk** values (absence differs from defaults),
   per-key revisions, coalescing, conflicts, failures and completion receipts.
2. Adapt native quit/frame integration to M2 transaction/registration APIs.
   Preserve cancellation, drain, quit veto/resume and failure reporting. Do not
   join a running worker from Unity or DLL teardown. During installation, check
   registration and anchor installation separately from delivery readiness:
   M2 intentionally withholds dispatch until host initialization completes.
3. Reconcile automatic reload with pending/failed session edits. Restore gameplay
   settings definitions and consumers in verified cohorts; no key is advertised
   as durably saved until its real worker/quit contract exists.
4. Restore `mod_state` and its existing async receipts, adapting only File access.
   Preserve v1 JSON keys, string IDs, lock/retry/RMW behavior and future-version
   refusal. Existing policy is precise: Read ignores malformed/non-object data;
   Update may replace it with a new v1 object, but refuses future schemas. Do not
   claim all malformed files are immutable or introduce a new migration policy.

Settings UI, AAI/Kit integration and sync journal/session compatibility remain
separate dependencies. No live trial, deployment, game cycle, signing, release
or active-branch promotion is established by these source changes.

## Evidence and recovery

M3 source is `adc61c190119d94b586e027007a2f47bf1970512`; its documentation
checkpoint is `d56218f989956b4513acfc8863cfb77990b3643b`. M3 scoped AXF frame:
`afd97cee-324c-426c-8ab4-d585633df92f`. The canonical Downstream scope labels
its frames with `play-dev`; the frame text/reference explicitly identify this
isolated branch and do not mean that branch was promoted.

M4A preparation input and reviewed patch hashes are retained under
`D:/dev/stfc-workspace/.ax/ripper-foundation-m3-20261025/m4-preparation`.
Final M4A candidate-bound verification is recorded under the separate
`D:/dev/stfc-workspace/.ax/ripper-foundation-m4-20261025` evidence root.
Direct implementation, independent collaboration and scoped AXF continuity are
used. No Runner-governed execution is claimed.
