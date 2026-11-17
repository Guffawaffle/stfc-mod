# Downstream play chat portrait retention

## Restored behavior

Primary preservation input: `play` at
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`. Its
`mods/src/patches/parts/chat_avatar_retention.cc` is unchanged from the earlier
`45d1deaf` study input. This is existing play functionality, not an AAI or
STFCKitUI addition.

The migration had omitted the portrait retention fix. Chat rows could lose
their downloaded portraits when reused. The restored, independently reported
`ChatAvatarRetention` feature uses the existing `patches.chatpatches` switch.
It sets the profile and admin avatar widgets' `_keepInCache` Boolean before
native `ChatMessageWidget.SetWidgetData` executes, and leaves it set for
asynchronous reloads. This reuses the game's asset cache; it adds no image
loader, background scan, persistent cache, setting or frame callback.

The binding preserves exact reference namespaces, Boolean type, instance-field
and object-bounds checks. Null children are benign; aliased widgets are written
once. Managed roots are local to the binding operation. Missing or incompatible
metadata leaves the native path available and reports the optional feature
unavailable. Older clients without the retention switch retain native behavior.
Optional retention failures cannot skip or retry the original native binding.

Sources: [binding](../../src/mod/game/chat.h),
[hook](../../src/mod/features/chat.cc),
[registry](../../src/mod/mod.cc),
[controlled binding fixtures](../../tests/il2cpp_bindings.cc).

## Verification and actual trial

- Windows x64, Zig 0.16, ReleaseFast with the pinned Profiles host:
  `install + test-il2cpp + test-mod` passed **163/163 build steps**.
- Actual binding fixtures cover exact Boolean writes and neighboring storage,
  both portrait sources, nulls, aliasing, wrong runtime classes, root refusal
  and temporary-root release. Twenty separate processes cover absent retention
  support and incompatible field/target metadata. Hook ordering and exactly-one
  native call were independently inspected in the real hook; the fixture does
  not claim to execute that hook.
- The first run found inconsistent synthetic method metadata: a one-parameter
  method lacked its parameter table. The fixture now supplies a real table;
  the refusal assertion is unchanged. The first failed run remains retained.
- Static Windows verification passed: 18 exports, explicit Profiles host,
  static CRT and embedded provenance.
- Deployed to the ordinary installation at
  `C:/Games/Star Trek Fleet Command/default/game`. The exact prior session
  (PID 66824, creation file time `134374285606657446`) was checked before the
  authorized cycle. The reopened process was PID 50676. All **49** hook groups
  completed, including portrait retention, and FrameTick reported ready.
- Guff explicitly confirmed **"Portraits stay visible"** after the requested
  chat scroll and leave/reopen trial. This qualifies that Windows/client-273
  interaction, not all clients, macOS, explicit-profile launches or long-session
  cache behavior.

Installed DLL SHA-256:
`be5f2890e8d4ec5399f03474becc25509024528e4b75642e1c330bbf072e2821`.
Size: 10,971,136 bytes. Its truthful local source marker is
`local:dce32e0f:play-chat-35551eac`, build
`local:play-avatar:20261025:win016`. The source was a recorded local delta;
the marker does not claim a clean committed candidate or release qualification.
The canonical AX deployment checkout reported its own orchestration revision;
that revision is not this DLL's analytical source identity.

Private recovery evidence:
`D:/dev/stfc-workspace/.ax/ripper-play-avatar-20261025/`:
`source.diff`, `final-source.diff`, initial `build.log`, passing
`final-build.log`, `stop.json`, `deploy.json`, `launch.json` and product artifacts.
Passing build-log SHA-256:
`26177a8a9dada78d76509a9fece2ef7d314d49d9f99521c188222d5002d76bc2`.

No TOML, account stores or persistence schema were edited for this port. Normal
game startup/runtime writes and installation metadata updates still occur.
No push, active-branch replacement or release promotion occurred.

## Remaining work

[The pinned play matrix](PLAY-PARITY.md) is the primary migration target. Full
native Mod Settings, its existing workflows and remaining play feature owners
must be restored before treating later play-dev, AAI or STFCKitUI additions as
the next product priority. The avatar correction does not restore that UI.
