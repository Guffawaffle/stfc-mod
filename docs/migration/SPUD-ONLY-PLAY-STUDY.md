# Updating SPUD directly in PLAY

Study date: 2026-10-25. No product implementation, PLAY edits, deployment, game
cycle, branch switch, Runner orchestration or delegated work was performed.

## Conclusion

Updating SPUD independently is a credible, substantially narrower alternative to
the Ripper-foundation migration. The available newer library retains the hook
macro used throughout PLAY. Its API does not require replacing PLAY's IL2CPP
helpers, settings, controls, feature owners, state files or build system.

This is **not a literal file-copy-only upgrade**. Three directly used API surfaces
changed, the vendored build recipe is obsolete for the new source layout, and the
new hook install/failure semantics require qualification. A full PLAY build with
the replacement library has not been performed in this study.

Recommendation: prefer a SPUD-only experiment based on pinned PLAY if the next
assignment is approved. Keep this migrated foundation frozen as a reference.
Preserve the existing product source and make the dependency/integration edits
below before considering optional transaction adoption.

## Pinned inputs and preserved checkpoint

| Input | Revision / identity |
| --- | --- |
| PLAY, unchanged throughout this study | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` |
| Stopped migration implementation | `c9c03ac21999b2e9cd056f64ecbed54c96c501fb` |
| Migration stop checkpoint | `6924faa473086a8d8cf30174be67acc9d3aee8a4` |
| New SPUD source, as recorded by its vendor notice | `e4d0c78be89ef2587af33dd634ae5e8071935be7` |
| Exact new vendored tree | `b443aa0ef4ce1a43049fcb840478206c7e569d10` |
| Exact old PLAY SPUD tree | `04154955212a7fce813f7b75c61fb972e0e5d21f` |

The newer SPUD is the locally retained snapshot under `third_party/spud`, not an
assertion about the latest private upstream revision. PLAY currently selects the
local package label `spud v0.2.0-9`; the source tree identity above is stronger
evidence than that version label. The newer tree includes its MIT LICENSE.

The existing implementation stash `0fa6d6edd6f359187ac6957b705034e4a108a19a`
remains untouched. Unrelated development-install tooling dirt is excluded from
the migration checkpoint and this study.

## What remains intact

A comment-stripped, source-level inventory found **168 SPUD_STATIC_DETOUR
occurrences in 51 PLAY files**. These are textual sites, including templates and
platform/configuration-dependent paths, not 168 verified runtime hooks.

The new library still accepts this macro and the same hook shape: original-call
pointer first, followed by the existing native arguments. Representative void,
integer and aggregate-result hooks compiled against both old and new headers,
with tracing both off and on.

Accordingly, the following should stay as existing PLAY code unless a concrete
build/runtime incompatibility is found:

- Settings catalog, native page navigation, row styling, chord capture/editor,
  reset/save flows and native pooling cleanup.
- Galaxy labels/layers, zoom/camera behavior and the existing input precedence.
- Preset ordering and its context/save/renderer behavior.
- Existing IL2CPP invocation/checked-value helpers and Unity wrappers.
- TOML parsing, registered-key saving, state ownership, sync and notifications.
- Xmake, proxy exports, artifact identity and existing release/signing processes.

This is an expected preservation boundary, **not** proof that a replacement PLAY
binary already builds or every native hook installs. None of these subsystems
needs the Ripper typed bindings merely to call the newer SPUD macro.

## Required dependency and integration edits

| Area | Concrete change | Product logic affected |
| --- | --- | --- |
| Vendored SPUD | Replace the old package's include/src coherently with the exact newer snapshot; retain the license and provenance. Do not mix new headers with an old cached static library. | Hook engine only. |
| Package CMake | Update `xmake-packages/packages/s/spud/spud-src/CMakeLists.txt`: old explicit relocator/remapper/prologue sources and assembly layout no longer describe the new tree. Include chain/import/pointer, executable pool, safe patching, unwind and platform sources. | Build integration only. |
| Dependencies/options | The old SPUD CMake fetches AsmJit; the new retained tree does not reference it. Zydis 4.1.0 and Capstone 6.0.0-Alpha5 are already represented in both builds. Keep ARM64 decoding/unwind support and uniform tracing definitions. Avoid unrelated global dependency cleanup. | Build integration only. |
| Windows assembly | PLAY's MSVC build needs the new `detour_context_support.masm` implementation. It reads r11/r10, whereas the old helper read a different context. The current Zig/Clang build uses inline assembly instead; that is not proof that omitting the MASM helper works with MSVC. | Hook entry ABI. |
| Package identity/cache | Update the local package identity in `xmake/dependencies/common.lua`, its recipe as needed and `xmake-requires.lock`; force the SPUD consumer/link closure to rebuild. | Prevent stale header/library combinations. |
| GC signature lookup | `mods/src/patches/parts/object_tracker.cc`: `matches.get(0)` becomes `matches[0]` or `front()`, retaining the existing empty-match guard. The two-argument `find_in_module(pattern,module)` remains available. | One lookup expression; retain tracking/finalizer behavior. |
| macOS hook extent guard | `mods/src/patches/native_hook_extent.cc`: `has_detour_prologue` is absent. Adapt to `inspect_detour`, retaining independently established bounds and the current conservative floor. | Native safety boundary, not feature behavior. |
| Relocation fixture | `tests/spud_relocation.cc`: replace `hook.install()` with checked `try_install()`. Keep the original call/branch/destruction assertions; carry the newer separate writable-data/executable-code layout for macOS/Rosetta qualification. | Test only. |

The available newer source snapshot intentionally has no CMakeLists.txt. Copying
it over PLAY's include/src while leaving the old CMake recipe would still fail:
that recipe names deleted sources, fetches the former emitter dependency, and
looks for the former assembly files. This is a bounded package-recipe adaptation;
it does not justify migrating the application to Zig.

The standalone `inspect_detour` probe establishes API compatibility only. A
production MacHookFits replacement must check short/far landing policy, forwarder
resolution and exact bounds; a successful planning result alone is not the old
guard's complete safety contract. Do not simply remove the guard or lower its
minimum because the newer engine can sometimes use shorter patches.

## Runtime and failure semantics that need attention

The new SPUD_STATIC_DETOUR uses `try_install_permanent` and caches its first result.
It returns null on refusal and leaves a successful hook installed for process
lifetime. Permanent stubs avoid the per-call entry counting used by removable
hooks. Preserve this distinction rather than converting every hot hook to a
removable transaction just to use a newer API.

PLAY already checks installation in several owners (including presets), but many
macro results are ignored. Its top-level ApplyPatches also ignores the bootstrap
il2cpp_init hook result. The general startup banner does not establish individual
hook success. Before a live SPUD-only trial:

- Report and handle a refused bootstrap hook explicitly.
- Record error, failure detail and target for refused installs without changing
  the feature's callbacks. The raw compatibility macro only gives callers null;
  useful diagnosis may require a small application-owned installation wrapper.
- Check multi-hook owners' complete readiness. Do not report a partially installed
  workflow as available or assume exceptions are the only failure channel.
- Preserve the default protected-patching policy. Do not enable unguarded writes
  or disable unwind checks to make a rejected target pass.

The newer engine adds protected patch windows, hook chains, unwind handling,
control-flow checks and stricter refusal reasons. Therefore successful source
compilation cannot establish identical target acceptance. Shared targets,
forwarders/tiny methods, native callbacks with nontrivial arguments/results and
Windows/macOS unwind paths need real qualification.

Transactions are useful but **optional for the first dependency update**. Adopting
them later requires live owning handles and explicit rollback policy. Permanent
hooks cannot be undone like removable transaction members; do not put the
compatibility macro into a transaction and claim atomic feature installation.
`kRollbackFailed` remains an unsafe/indeterminate outcome requiring startup policy,
not ordinary optional-feature unavailability. None of this requires replacing the
settings UI, public configuration or every feature installer at once.

No claim is made that SPUD alone caused the smoother experience observed on the
larger migrated foundation. That candidate changed more than the detour library.

## Study validation and its limits

Evidence directory:
`D:/dev/stfc-workspace/.ax/spud-only-play-study-20261025`.

- `inventory.json`: pinned source identities and source-level SPUD use sites.
- `syntax-results.json`: 11 MSVC syntax probes, all matching their expected result.
  Old/new macro controls pass. Old scanner/prologue/relocation APIs compile against
  old headers; each fails against new headers for the identified API change; the
  small study-only adaptations compile. These probes do not modify product files.
- `macro-results.json`: the representative hook shapes compile against both
  versions with tracing 0 and 1. An initial integer-address probe was invalid for
  both versions; the corrected control uses pointer addresses like the native sites.
- `new-spud-relocation.log`: the existing foundation's standalone x64 relocation
  fixture built and ran against the retained newer SPUD: **7/7 build/run steps**,
  with call-through, both branch paths and removal passing. It uses the newer
  fixture layout, not an assertion that untouched PLAY was rebuilt.

Syntax checks used the available Visual Studio MSVC compiler; the standalone
native fixture used the existing Zig Windows/MSVC-ABI build and Debug assertions.
No complete PLAY/Xmake link, MSVC assembly link, macOS build/runtime, game hook
installation, user workflow, signing or packaging trial was performed.

## Smallest next implementation, if authorized

1. Start from pinned PLAY in an approved isolated branch. Update only the vendor
   package/recipe/cache identity and the three directly incompatible source/test
   surfaces above. Keep the existing UI and all product owners intact.
2. Build through PLAY's existing Windows/Xmake path; run the adapted relocation
   regression and the existing relevant PLAY fixtures. Review the complete diff
   for accidental configuration/product changes. Qualify the macOS guard separately.
3. Add the narrow install diagnostics/bootstrap readiness handling, then build a
   source-identified candidate for an explicitly authorized live trial. Verify
   actual targets and existing workflows; do not substitute startup success.
4. Consider transactional feature installs independently after the dependency-only
   candidate works. No typed-IL2CPP/Unity/feature-ownership migration is a prerequisite.

This study authorizes none of those implementation or operational steps. PLAY
and the running game were left unchanged by the study.
