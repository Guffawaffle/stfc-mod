# PLAY-CHAT-AVATARS — pooled chat portrait retention

Contract v1, source scan 2026-10-28. Source is frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`play-dev-ripper` `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` in the existing
Ripper Downstream worktree. Existing ID: `PLAY-CHAT-AVATARS`. This pass qualifies
the existing portrait owner; it introduces no image cache or new UI framework.

## Inputs, activation and ownership

| Inspected branch | Exact revision | Presence and evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | No `parts/chat_avatar_retention.cc` translation unit or retention consumer in its chat source. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | `mods/src/patches/parts/chat_avatar_retention.cc`; native cache-retention consumer. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete portrait source, Chat registration and profile/message headers inspected. |
| refactor input | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | Present in `src/mod/features/chat.cc` and checked binding in `src/mod/game/chat.h`; previously restored. |

Frozen `InstallChatPatches` calls portrait installation under the default-true
`[patches].chatpatches` gate. Target keeps a separately reported
`ChatAvatarRetention` descriptor under that same gate. There is no portrait
setting or persistence writer. Both platforms share the managed field contract.

Before native `ChatMessageWidget.SetWidgetData`, retain the profile and admin
`FrameAndAvatarWidget` assets by setting `_keepInCache=true`. Leave that flag
set for asynchronous reloads. The game owns widgets and downloaded assets;
the mod retains operation-local roots, with no persistent portrait references,
new background task or frame callback.

## Behavioral obligations

| ID | Frozen operation | Target mapping and verification |
| --- | --- | --- |
| CA1 | Install only on the widget's own substantive parameterless instance-void `SetWidgetData`; reject drifted metadata. | Existing `AvatarRetentionTarget` checks declaring class, signature and generic/static flags. Existing fresh-process `chat-avatar-method-*` fixtures exercise refusal. |
| CA2 | Exact instance fields/namespaces/Boolean type and object bounds must match before reading/writing. Older clients without `_keepInCache` retain native behavior. | Existing `RetentionFields` and `AvatarRetentionStatus` preserved. Existing metadata-rejection/native-behavior processes cover incompatible or absent metadata. |
| CA3 | Read profile avatar and admin avatar; nulls are benign, aliases written once, wrong runtime avatar class refused. | Existing checked `RetainAvatars` preserved. `ChatAvatarRetentionObservations` tests both flags, alias deduplication, nulls, wrong classes, neighboring bytes and roots. |
| CA4 | Set retention before native row binding; never restore it on return. | Actual `ChatMessageWidget_SetWidgetData_Hook` fixture asserts both flags are true inside native original. No production algorithm change is needed. |
| CA5 | Optional retention failure cannot suppress or repeat native row binding. | Existing contained hook preserved. New consumer fixture executes success, wrong-class, failed-root and empty pooled-row paths and counts exactly one original each. |
| CA6 | Keep native cache and cleanup ownership; do not cache stale row pointers. | Local strong handles only; existing fixture and new consumer assert no remaining roots. Real async/pooling behavior remains a native trial. |

## Existing evidence and present boundary

[CHAT-AVATAR-RETENTION.md](../CHAT-AVATAR-RETENTION.md) records the earlier real
Windows/client-273 scroll/reopen trial: Guff confirmed **“Portraits stay visible.”**
That record names the installed DLL
`be5f2890e8d4ec5399f03474becc25509024528e4b75642e1c330bbf072e2821`, its
truthful local source marker and its native session. Preserve that historical
basic acceptance; it is not exact-current-candidate verification.

`tests/chat_bindings_cases.h` mode `chat-native-avatars` now executes the actual
production hook rather than inspecting its ordering alone. Existing binding
fixtures remain authoritative for the full field/metadata matrix. Both layers
use controlled runtime storage, not Unity downloads or real garbage collection.
The child ran no builds; final execution, toolchain, candidate/artifact identities
and runtime evidence are coordinator-owned.

Native procedure: scroll enough to reuse regular/admin message rows, leave and
reopen chat, then observe asynchronous portrait reloads. No chat message needs
to be sent. Older-client absent-switch behavior, long-session native cache
pressure and macOS remain separate. Existing patch gate is the ordinary rollback.

Disposition: existing implementation retained; source contract complete;
production callback qualification added, with exact execution/native results
pending. No overall migration parity claim.
