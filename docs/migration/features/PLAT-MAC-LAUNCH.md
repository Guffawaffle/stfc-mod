# PLAT-MAC-LAUNCH — C++ launch and injection source contract

Partial contract v1, 2026-11-02. This assessment covers the standalone C++ loader,
Foundation directory adapter, injected entry and bootstrap/build registration.
The catalog also includes the Swift updater, repair, rollback, bundle/signing and
interactive launch flows; those are not qualified by this bounded comparison.
No product code, setting, build registration or packaging is changed here.

Frozen source is `1baa55fedf1758b19f843cc59707b8901a0b4998`. Its macos-loader,
macos-dylib and mods/src/folder_manager.mm inputs have no diff from either pinned
PLAY `cad98ef7990dd66167eb64ea5dda525c1fe2c296` or pinned upstream
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`. The typed source examined is
`4e7c052a255aa3a0a559471c99c346bf6b181fc6`.

| Frozen operation | Existing typed owner and behavior |
| --- | --- |
| macos-loader/src/main.cc reads ~/Library/Preferences/Star Trek Fleet Command/launcher_settings.ini, General/152033..GAME_PATH, then appends the app's Contents/MacOS executable. | src/platform/macos/loader/main.cc preserves those names and adds checked missing settings/path errors and doubled-leading-slash normalization. |
| Resolve the loader's own directory and inject its adjacent libstfc-community-mod.dylib. | _NSGetExecutablePath remains checked; realpath resolves the loader's symlink before selecting that library directory. This does not change the game's separate bootstrap path lookup. |
| Prepend library path and dylib path to existing DYLD_LIBRARY_PATH and DYLD_INSERT_LIBRARIES. | Same colon-separated prepend and existing environment retention. The loader still does not check setenv's return; environment/injection failure is not runtime-qualified here. |
| execvp receives the constructed game path as argv[0], with no further arguments. | execv receives the same slash-containing executable path and argv shape; no PATH-search behavior is lost. Both main() implementations omit arbitrary loader argument forwarding. Do not introduce profile/custom arguments as an incidental refactor change. |
| macos-loader/src/folder_manager.mm returns a Foundation file-system representation for the user Library directory. | loader/library_dir.mm copies into an owning std::filesystem::path within an autorelease pool and falls back to NSHomeDirectory()/Library if no URL is returned. src/mod/file_macos.mm has the same owning directory adapter for mod files. |
| macos-dylib/src/main.cc logs injection and calls ApplyPatches from a constructor. | platform/macos/dylib/main.cc logs injection and calls StartMod from the corresponding constructor. Ordinary startup failure reporting uses syslog before file logging exists. Constructor argument/native loader assumptions remain untested. |
| ApplyPatches resolves ../Frameworks/GameAssembly.dylib from the game executable, loads it globally and detours il2cpp_init. | mod.cc::StartMod preserves the game-relative lookup and original-init flow, with checked path length, typed API initialization, shared HookSet/install ownership and explicit profile/indeterminate-state failure handling. These shared policies are not a new native Mac qualification. |
| XMake links the shared mod and Foundation loader. | zig/mod.zig registers the corresponding dylib and Foundation/inifile loader, retains .mm mod adapters and builds thin aarch64/x86_64 targets. macos-universal combines dylib/loader outputs; Profiles universal remains explicitly unqualified. zig/launcher.zig separately requires a Mac/Xcode host for Swift. |

No proven source regression was found in this slice. Preserve the existing checked
error paths, owning Foundation copies and loader-path improvements. Legacy unused
Application Support lookup and dependency-specific allocation overloads do not
establish additional reachable player workflows to restore.

Source hashes are recorded in the camera/confirmation evidence directory's
mac-source-inputs.json. This is static comparison only. Both current Mac core
requests stopped before compilation because the configured SDK is missing;
Windows fixtures do not validate Foundation, POSIX exec, actual DYLD injection,
current-client hook fit or signed/universal packaging. Mcohall's pre-release
reports remain attached to that tested artifact.

Next source reconciliation is the existing Swift install/update/cancel/repair and
rollback workflow, followed by its assembly/signing contract and actual Mac build
and runtime evidence. PLAT-MAC-LAUNCH remains Implemented, unverified; native
startup, profile isolation and overall platform parity are not closed. AAI and
new upstream features remain deferred.
