# M3: required Profiles host and platform bootstrap

Implemented in the isolated integration worktree on 2026-10-25, following
[the gameplay-first sequencing decision](GAMEPLAY-FIRST-CUT.md). This is an
offline integration checkpoint, not permission to launch a migration build.

## Preserved inputs and boundaries

The embedded Profiles source remains exactly
`c5c4ba911f16f3376e828761714d72a26ebe2a96`, archive SHA-256
`f120590499bb51844c1a2cd274f5bd58dbae0626b72c2d9f800041fabd08fd63`.
Its core, session/catalog/preferences contracts, RuntimeApi, process admission
and browser guardian are unchanged. Downstream host behavior is compared with
`45d1deaffddc3706e5c8232cf746ca46ef67be91`; moving `play-dev` is not an input.
Ripper's foundation remains pinned at `ee11492c46b2d591f3c3d77c9d98ee13b4b1e628`.

The host-derived [isolation adapter](../../src/profiles/profile_isolation.cc)
preserves preference semantics and exact managed signatures, including the
native name `TrySetSetString`. It stages the ten preference and three OIDC
targets in one M2 transaction, after signature, native-entry and canonical
target-identity checks. Missing or aliased targets, clean refusal and
indeterminate installation all stop a named launch. Enrollment completion and
session readiness follow successful installation; reporting cannot turn a
required failure into continuation. Native extent safety remains platform and
exact-client dependent, not proven by the signature inventory.

## Startup and ownership

[ProcessArguments](../../src/mod/arguments.cc) makes one owned argument snapshot.
[ProfileBootstrap](../../src/mod/profile_bootstrap.h) supplies a pure, terminal
phase policy; [profile_launch](../../src/mod/profile_launch.cc) owns the real
process-lifetime leases and delegates store semantics to the pinned library.

1. Before loading GameAssembly, resolve the explicit immutable ID, acquire the
   installation lease and check installation readiness. Ordinary host-enabled
   launches also acquire this installation exclusion. Named launches additionally
   install process admission.
2. Before file/configuration access and original runtime initialization, prepare
   the named session and isolated preference store. Preserve existing/new/resume
   enrollment selection. No authenticated account or last-used profile guessing.
3. After successful original initialization, install required preference/browser
   hooks and publish readiness before optional features. Failed named setup,
   bootstrap, original initialization or isolation cannot fall back to ordinary
   preferences. Ordinary original-initializer failure does not install features.

The lifetime is the existing injected-mod process lifetime. Dynamic unload,
concurrent bootstrap and worker-thread hook installation are not supported.
No hook owner or store is moved across that boundary. The existing M2 unsafe
latch and quarantine rules still apply; they do not repair a vendor rollback.

## File and platform compatibility

[File](../../src/mod/file.cc) gives an admitted named profile precedence over
`-ccm`: `config.toml`, `runtime.toml`, `logs/community-mod.log` and
`logs/battles.log` under the immutable profile directory. The existing derived
state filename remains `config.state.json`. Ordinary and custom filenames are
preserved. An unavailable named directory is refused, never replaced by an
ordinary path. These are routing changes; the M4 state owner is not integrated yet.

Logging uses the same resolved path. Creating the log directory first must not
suppress the existing macOS legacy-preferences migration: `File::Path(..., true)`
performs that existing migration before creating a destination. No new storage
location/schema migration is introduced.

The Windows entry admits the complete case-insensitive `prime.exe` basename,
not a prefix. Other hosts, including the browser guardian, skip mod startup.
Static-CRT thread notifications remain enabled; process-only path allocation is
behind a noinline boundary. A host-enabled DLL declares the existing sixteen
proxy exports plus the live `STFCProfilesExplicitLaunchContractV1` data marker
and `STFCProfilesBrowserLaunchW`. The guardian command spelling is unchanged.
The old unused non-version export is not added to the foundation proxy surface.

## Explicit build closure

`-Dprofiles-host=true` requires `-Dprofiles=true -Dprofiles-adapters=true` and
verified absolute source/archive/dependency-prefix/receipt inputs. The final
mod links the host-derived adapter, never also the original isolation adapter.
The original optional producer remains independently buildable for control tests.
Core/libarchive and curl use one exact verified zlib archive. External source
and static-prefix hashes are rechecked at configuration and build time.

Default-off builds remain sixteen-export foundation builds and do not inspect
private dependency roots. They explicitly reject `-stfc-profile`; they do not
claim ordinary installation exclusion or the explicit-launch contract.
Host-enabled universal macOS packaging is refused until its child builds can
carry and qualify the same dependency inputs; no silently unprofiled output.

## Tests and qualification status

Final source candidate: **`adc61c190119d94b586e027007a2f47bf1970512`**, tree
`aca88839bbe40d28e1e7fe2504f036399de43d0e`. Implementation checkpoint:
`024f39862240b881444862dc1b4541199c8237c3`. The correction uses the existing
build-root compatibility helper; the initial direct call compiled on 0.16 but
not 0.17. All final lanes below cover the correction and clean source identity.

| Lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 128/128 steps; 8/8 reported Zig tests; all requested native fixtures; 18-export/static-CRT/exact-identity verification |
| Windows x64 Zig 0.17, Profiles host | 128/128 steps; all requested native fixtures; the same PE/identity checks |
| Windows x64 Zig 0.16, default off | Full foundation build and fixtures pass with nonexistent private inputs; sixteen-export verifier passes and host-export verifier refuses it |
| WSL/Linux x64 Zig 0.16 | 94/94 steps; 8/8 reported Zig tests, including portable routing/bootstrap/target fixtures and existing launcher/sync/binding tests |
| Non-game Windows DLL loading, both host binaries | Fatal-only private GameAssembly stub control exits 197; real candidate child exits 0 after attach, 64 threads with 64-KiB reserved stacks, and detach |

Both host lanes also pass three explicitly inert Profiles tests (identity,
consumer and original-adapter address linkage), four build-option rejection
controls and opposite-export-mode rejection. Preferences/catalog/installation
fixtures are compiled, not executed. Windows uses installed MSVC
14.51.36231/SDK 10.0.26100.0 fallback, not the pinned cross-SDK archive.

Exact host DLL SHA-256 values:

- Zig 0.16: `2f5cba9c2f9156c1c8a4160565cd8ad5c5349c682ad8675e8c5866f274e74723`.
- Zig 0.17: `ee051ecff03510761a594332c52d225cd47704630c33709731af37a77f98a394`.

Independent read-back checked both enabled Windows source/tool/prefix/artifact
and log identities. The actual DLL link command has one derived host archive,
no original adapter archive, the exact verified archive/lzma/zs paths and no
second Zig zlib provider. Standalone original-adapter fixture linkage is separate.
The 0.17 graph-cache rejection policy was source-reviewed, not fault-injected.

The non-game loader checks use a private dependency directory, restricted
System32/private-directory resolution and fatal-only exports for the actual
forty-three imported IL2CPP names. WinHTTP, newly imported by the Profiles core,
was checked against the signed Windows OS component and SDK before adding that
specific name to the test allowlist. No real game module, profile session or
guardian is invoked. This does not audit arbitrary filesystem activity or prove
that game-host startup/hooks work. No installed files are changed.

Initial 48/48 narrow and 55/55 producer development runs remain earlier feedback,
not substitutes for these candidate-bound results.

New fixtures cover owned argument/ID validation, exact executable admission,
ordinary/named ordering, immutable mode, repeated/reentrant calls, terminal
failures, all thirteen target absences and all seventy-eight alias pairs.
The actual File router is tested with supplied contexts for ordinary, relative
custom, absolute custom, named and missing-admission cases. A macOS-only
disposable Library fixture checks that opening the log first preserves legacy
settings/log bytes and the existing compatibility link; it requires a Mac run.
These fixtures never open a real profile, installation or browser store.

The unchanged preferences/catalog/installation suites retain their M1c result:
both original and migrated binaries passed from Guff's ordinary terminal; this
execution context refuses redirected global locks. Those guards and assertions
remain unchanged, and those host-integration suites are not rerun here as though
that environmental limitation had disappeared. See [M1c](M1C-PROFILES-ENVIRONMENT.md).

Still unqualified: actual game initialization and all thirteen detours on the
current client; native enrollment/ordinary exclusion and browser child lifetime;
macOS compilation/runtime/signing/notarization; gameplay/store parity. The
existing macOS loader/browser environment inheritance also needs platform review.
The qualified offline loader/TLS results establish only non-game loading and
small-stack thread behavior.

## Next integration boundary

M4 restores checked/preserving configuration writes, registered-key worker/quit
semantics, original startup disk values and the state owner before gameplay
cohorts are promoted. Deferring Settings UI does not defer those persistence
guarantees. Sync journals/session behavior remains a separate preservation task.
AAI, STFCKitUI, Settings UI, probes, SQLite and settings-location changes are
not activated by M3. No deployment, cycle, push or active branch replacement.

Private development evidence is under
`D:/dev/stfc-workspace/.ax/ripper-foundation-m3-20261025`. Direct collaboration
and AXF continuity are used; this is not a Runner-governed Attempt.
