# Pinned TOML preparation engine

These files are copied without changes from downstream
`45d1deaffddc3706e5c8232cf746ca46ef67be91`. Original paths, Git blobs,
SHA-256 hashes and production/test scope are recorded in `SOURCE.json`.
The enclosing repository's GPL-3.0 license applies; original dependency
notices remain in `THIRD_PARTY_NOTICES.txt`.

Production uses only `include/stfc_toml/editor.h`, the pure preparation engine
already shared by the mod and standalone adapter. The host owns acquisition,
file replacement, conflicts and lifecycle. Zig supplies the existing separately
compiled toml++ 3.4.0 provider; there is no second parser implementation or
private source-root requirement.

`test_support` preserves the entire original engine/ABI fixture and the exact
C ABI implementation/header it exercises. These files compile into the fixture
executable only. They do not enter the product DLL/dylib, add C exports, or alter
the separately selected M1b standalone producer.
