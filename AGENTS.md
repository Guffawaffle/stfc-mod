# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

Community mod for Star Trek Fleet Command (STFC) — a desktop game that runs via Unity/IL2CPP. The mod hooks into the game's IL2CPP runtime to add QoL features (UI scaling, zoom controls, hotkeys, chat improvements, cargo viewers, data sync, etc.). Supports Windows (DLL proxy injection) and macOS (dylib injection).

## Build System

This project uses **XMake** (not CMake). All build configuration is in `xmake.lua` files. Language standard is C++23 with multi-threaded static runtime (`/MT`).

### Build Commands

```bash
# Configure and build (command line)
xmake                              # Build default target
xmake f -p macosx -a arm64 -m debug --target_minver=13.5   # Configure for macOS ARM debug
xmake f -p windows -m release         # Configure for Windows release

# Generate Visual Studio solution
xmake project -k vsxmake -m "debug,release"

# Clean
xmake clean -a

# macOS dev script (build, run, debug, crashlogs)
scripts/mac-build-test-debug.sh [build|run|debug|crashlogs] [-m debug|release|releasedbg]
```

### Build Modes
- `debug` — development
- `release` — production
- `releasedbg` — release with debug info, enables `_MODDBG` define

### Reset Build
Delete the `build/` folder to reset. Also delete `.vs/` for a full Visual Studio reset.

## Repository Expectations

- Keep changes scoped. Do not stage unrelated dirty files or generated artifacts unless the user explicitly asks.
- Before finishing C++ or patch work, run `git diff --check` and the narrowest relevant xmake build.
- For macOS core mod changes, use `xmake f -p macosx -a arm64 -m debug --target_minver=13.5 -y && xmake -y mods`.
- Review the final diff for risky hooks, platform guards, config default mismatches, and missing example config updates.
- If a subtree such as `macos-launcher/` needs specialized guidance, prefer a nested `AGENTS.md` near that code instead of overloading this root file.

## STFC Workspace

This workspace is intentional. Related STFC roots may be open nearby, but they are not interchangeable. Start in the repo/root named or implied by the task and treat that checkout as the task home.

### Paved path

Use AXF/Lex first for workspace navigation, repo selection, task context, validation, deploy, and runtime workflows.

Preferred discovery order:
- AXF MCP/capability router, if available
- AXF CLI commands beginning with `axf`, using `axf inspect <capability-id>` before
  `axf run <capability-id> [--kebab-case-arg value]`
- normal repo-local discovery only after AXF/Lex is unavailable or insufficient

Treat `.ax/ax.ps1` as an AXF provider adapter, not as the operator-facing command
vocabulary. User-facing instructions and examples should begin with `axf`, never
`ax` or a direct `.ax/ax.ps1` invocation.

If AXF/Lex command discovery is unclear, inspect available entrypoints before guessing.

### Runtime/process permission

During STFC development sessions, agents may cycle or restart the STFC game client and the sidecar when needed for build, deploy, or runtime validation/testing. This is a default permission unless the human prompt revokes it for the session.

Use AXF/Lex or repo-provided lifecycle commands when available, keep the scope to the STFC client and sidecar, do not touch unrelated processes, do not wipe stores, logs, or configs unless explicitly asked, and report when a cycle was performed. A reasonable game/sidecar cycle in service of the task is normal workflow.

### Game config permission

Agents may edit TOML configuration files under the workspace folder named `game` when a task requires mod/runtime configuration changes. In the current STFC workspace, `game` points to `C:\Games\Star Trek Fleet Command\default\game`.

This permission is limited to TOML/config edits. Use AXF/Lex or repo-provided config flows when available, do not modify game binaries, assets, packaged data, executable files, or unrelated install contents unless the human prompt explicitly authorizes it, do not wipe stores, logs, or configs unless explicitly asked, and report any TOML/config changes made.

### Nearby roots

Common workspace roots may include:
- `stfc-mod` — primary community mod repo in this checkout
- `stfc-mod-guffa` — primary community mod repo in a parallel checkout when present
- `stfc-mod-ax-private` — private AX/automation support
- `STFC Diagnostics Logs (read-only)` — logs and evidence only; do not edit
- `game` — local game install/runtime files
- `majel` — Majel/service-related work
- `stfc-mod-sidecar` — sidecar companion service/viewer
- `netniv` — related upstream/reference repo

### Working agreement

- Use the intended checkout. Only create a new clone, worktree, or sibling checkout when the task explicitly calls for it.
- If that checkout is already dirty in unrelated or unclear ways, report the branch and dirty files before implementation.
- Read-only roots stay read-only.
- Outside approved TOML/config edits under `game`, only mutate non-config game install/runtime files when the task explicitly needs deploy, copy, or runtime changes.
- For cross-root work, name the roots you will touch before editing.

## Architecture

### Build Targets (xmake.lua files)

| Target | Type | Platform | Description |
|---|---|---|---|
| `mods` | static lib | all | Core mod logic — patches, config, IL2CPP bindings |
| `stfc-community-mod` (win-proxy-dll) | shared DLL | Windows | Proxy DLL (`version.dll`) that loads into the game process |
| `stfc-community-mod` (macos-dylib) | shared dylib | macOS | Injected dylib equivalent |
| `stfc-community-mod-loader` | binary | macOS | Loader that injects the dylib into the game |
| `macOSLauncher` | Xcode app | macOS | Swift GUI launcher app |

### Source Layout

- **`mods/src/`** — Core mod code (the main codebase)
  - `config.h/.cc` — Singleton `Config` class, loads TOML settings, controls which patches are enabled
  - `patches/patches.cc` — Entry point: hooks `il2cpp_init`, then conditionally installs each patch
  - `patches/parts/` — Individual patch implementations (zoom, hotkeys, chat, UI scale, sync, etc.)
  - `patches/key.h`, `mapkey.h`, `modifierkey.h` — Keyboard input mapping system
  - `prime/` — Header-only IL2CPP type definitions mirroring the game's C# classes
  - `prime/proto/` — Protobuf definitions for game data sync
  - `il2cpp/` — IL2CPP helper functions for resolving methods, classes, and icalls at runtime
- **`win-proxy-dll/src/`** — Windows DLL proxy entry point
- **`macos-dylib/src/`** — macOS dylib entry point
- **`macos-loader/src/`** — macOS loader (finds game, injects dylib)
- **`macos-launcher/`** — Swift macOS GUI app
- **`third_party/libil2cpp/`** — IL2CPP SDK headers
- **`xmake-packages/`** — Custom xmake package definitions (e.g., `spud`)

### Key Patterns

**Hooking pattern** — All game function hooks use `spud` (function detour library) via `SPUD_STATIC_DETOUR`. Each hook function takes `auto original` as the first parameter to call through to the original:
```cpp
void SomeFunction_Hook(auto original, SomeClass* _this, ...) {
    // custom logic
    original(_this, ...);
}
```
macOS does not tolerate repeated hooks of the same function. If multiple features need to intercept the same game method, consolidate the behavior behind one detour or add platform guards instead of installing overlapping hooks.

**IL2CPP class resolution** — Game classes are resolved at runtime using helpers:
```cpp
static auto class_helper = il2cpp_get_class_helper("Assembly.Name", "Namespace", "ClassName");
static auto method = class_helper.GetMethodInfo("MethodName");
```

**Adding a new patch** — Create a `.cc` file in `mods/src/patches/parts/`, write an `InstallXxxHooks()` function, declare it in `patches.cc`, add a `bool installXxx` to `Config`, and register in the `patches[]` array in `patches.cc`. Patch toggles are only read from TOML in `_MODDBG` builds, so update both the `_MODDBG` config parsing path and the non-`_MODDBG` release defaults in `config.cc`.

**Config** — User settings are in TOML files. The `Config` singleton (`Config::Get()`) is loaded once during `il2cpp_init_hook`. Add new settings to `config.h`, add defaults in `defaultconfig.h`, and load them in `config.cc`. For user-facing settings, update `example_community_patch_settings.toml` unless the setting is intentionally internal.

### Dependencies (via xmake packages)

- `spud` — Function hooking/detour library
- `eastl` — EA's STL replacement
- `spdlog` — Logging
- `toml++` — TOML config parsing
- `nlohmann_json` — JSON handling
- `cpr` / `libcurl` — HTTP requests (for data sync)
- `protobuf` — Protocol buffers (game data)
- `simdutf` — UTF encoding
- `libil2cpp` — Local package pointing to `third_party/libil2cpp`

## Code Style

- Uses `.clang-format` — 2-space indent, 120 column limit, Linux brace style, aligned assignments/declarations
- Version is defined in `mods/src/version.h` (VERSION_MAJOR/MINOR/REVISION/PATCH)
- Prefer narrow platform guards such as `#if _WIN32`, `#if !_WIN32`, and `#if __APPLE__`; do not assume every non-Windows path is macOS.
- Logging via `spdlog::info()`, `spdlog::debug()`, etc.

## Branches

- `main` — default branch, working integration base, and source of fork releases
- Create fork feature branches from a current local `main`, then return validated work through a PR targeting `main`.
- The legacy `guffa-dev` and plain `dev` branches are not part of the active fork workflow. Do not select either branch merely because a stale local or remote ref exists.
- Prepare upstream contributions on fresh branches based on `upstream/main`; never fall back to `upstream/dev` when another expected branch is missing.
