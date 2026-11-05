# PLAY-CHAT-TITLES — hide titles within chat rows

Contract v1, source scan 2026-10-28. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` begins at
`5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0`. Existing ID:
`PLAY-CHAT-TITLES`. Scope is existing chat title display and its checked native
boundary, excluding player-profile redesign or title configuration changes.

## Inputs and workflow

| Inspected branch | Exact revision | Presence and evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | `mods/src/patches/parts/chat_titles.cc` consumer. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Title consumer and native Settings control. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete `parts/chat_titles.cc`, profile/message headers, native-hook extent helper and registration inspected. |
| refactor input | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | Existing `HideChatTitle`, profile/title callbacks and installer in `src/mod/features/chat.cc`. |

Default-true `[patches].chattitledisplayhooks` is restart-only installation.
`[ui].show_player_title` defaults true. Existing Mod Settings > Chat retains
Reopen View timing and its registered persistence path. When false, hide only
the title object of a profile currently under a chat message. The same profile
widget outside chat retains native title behavior.

Frozen callbacks run native `UserProfileWidget.UpdatePlayerTitle` or substantive
`PlayerTitleWidget.SetIdentifiers` first, then apply hiding. Its preferred native
parent lookup includes inactive ancestors; unavailable API falls back to a
32-hop walk. No parent/row cache survives pooling or reparenting. Source also
contains exact-client wrapper-size history: tiny shared title bind wrappers
must not become new hook targets. Title-clear callbacks are left native.

## Behavioral obligations

| ID | Frozen operation | Target mapping and verification |
| --- | --- | --- |
| CT1 | Native profile title update runs once before optional display change. | Existing callback retained; actual-consumer test observes native restoring title then mod hiding it. |
| CT2 | Substantive title bind also hides the chat row, without detouring tiny shared wrappers or clear callback. | Existing `SetIdentifiers` owner retained with typed `void()` lookup and `CheckedEntry`; same-address deduplication remains. Native extents are exact-client/platform qualification. |
| CT3 | Hide only a profile under `ChatMessageWidget`, including inactive row ancestry. Other player profiles remain unchanged. | Restore frozen preferred `GetComponentInParent(Type,bool)` in `game/chat.h::chat_title_binding::Parent`; native result is checked/cast, known-null is not a reason to search globally. Actual-consumer fixture verifies native chat/nonchat paths. |
| CT4 | If scoped parent API is absent/failed, preserve bounded hierarchy fallback; no stale parent cache. | Existing 32-hop semantics retained with temporary roots for source/type/transform/object. Fixture exercises thrown lookup, ordinary fallback and cyclic hierarchy termination. |
| CT5 | Missing profile/title/object or failed custody does not change unrelated objects or suppress native callbacks. | Operation-local roots and current title-field read after chat qualification; test covers root refusal and native original count. No persistent title ownership added. |
| CT6 | With show-title true, leave native display intact; reopening rebinds existing rows. | Existing live conditional retained; fixture tests enabled path. Actual reopen/recycled-row layout remains native qualification. |

## Adaptation and verification boundary

The target previously always walked parents and borrowed the title pointer
across that traversal. The bounded adaptation restores the frozen primary
scoped lookup and roots its current objects, while preserving the fallback.
No generic shared Unity helper, new hook registry or scene scan is introduced.
Managed binding failures remain contained and the native callback remains
outside optional bookkeeping. The installer keeps one transaction and existing
alias handling; a build does not prove Windows/macOS overwrite extents.

`tests/chat_bindings_cases.h` mode `chat-native-titles` includes real production
callbacks and the chat binding. Native parent/transform/GameObject bodies and
managed storage are controlled substitutes. Assertions cover original order,
scoped inactive lookup, enabled preference, nonchat profiles, fallback, bounded
cycles and root refusal. They do not render titles, exercise real Unity pooling
or install native SPUD detours. Child execution is intentionally absent;
coordinator supplies final suite/build and exact artifact receipts.

Native procedure: hide titles, reopen chat, scroll/rebind several rows and
check a nonchat player profile. Restore/show and reopen, verify titles return,
then clear/rebind a row and inspect adjacent profile UI. macOS hook extents and
native layout remain separate from the Windows result. Rollback is the existing
feature setting or restart gate; no settings schema/store was changed.

Disposition: present consumer reconciled with source-preserving scoped lookup
and checked custody; final candidate/native/platform qualification pending.

## Coordinator qualification checkpoint — 2026-10-28

Implementation `d61b383d9e3b83d5c8ea4852c8c495e35d1a9084` passed the combined Windows assertion suite
(399/399 build/run/install steps), native AAI-enabled build (41/41), and
main-game startup with all 73 groups and frame delivery ready. See
[the cohort receipt](../FEATURE-COHORT-20261028-01.md#native-qualification) for
exact artifact, store differences and diagnostic limits. This supersedes earlier
execution-pending notes above only for the exercised fixture and startup layers.
Current player workflow and macOS acceptance remain pending.
