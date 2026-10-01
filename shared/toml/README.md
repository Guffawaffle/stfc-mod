# Shared TOML engine

The mod owns this pure preparation component. `include/stfc_toml/editor.h` is the
same engine used by the mod's `TomlEditor` and the offline native C adapter. It
parses the full document with toml++ and returns candidate bytes only after a
fresh parse matches the entire intended semantic tree. The host owns all I/O,
conflict admission, backups and replacement. See the repository's
`docs/SHARED_TOML_EDIT_CONTRACT.md` for ABI v1 requests, results and safety rules.

Build from this component directory and pass its exact project path. XMake can
otherwise discover the enclosing game project; its default build output is
relative to the invoking directory.

```powershell
xmake f -P . -p windows -a x64 -m release -y
xmake build -P . -y stfc-toml-native
xmake build -P . -y stfc-toml-tests
.\build\windows\x64\release\stfc-toml-tests.exe
```

For an external build directory pass `-o <absolute-path>` during configure and
keep the same component cwd/project for build. The shared binary basename is
`stfc-toml-native` (Windows DLL or macOS dylib). macOS uses the same project with
`-p macosx -a arm64` or `-p macosx -a x86_64`; both are supported build targets.
These fixtures do not access game files. Local Windows evidence does not verify
Mac builds or player runtime behavior.

Dependencies are exactly toml++ 3.4.0 and nlohmann/json 3.12.0, with immutable
XMake recipe commits in `xmake-requires.lock` for Windows and both Mac
architectures. CMake/Ninja entries are dependency installation tools, not linked
product dependencies. The native binary uses the static Windows runtime, catches
all exceptions at its C ABI and exports only ABI version, execute and free.

The component follows the enclosing repository's GPL-3.0 license. Dependency MIT
attributions are in `THIRD_PARTY_NOTICES.txt`; downstream native distributions
must include these notices and the enclosing repository license. Build caches
and generated binaries are excluded from source.
