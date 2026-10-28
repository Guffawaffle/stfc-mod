# PLAY-CHAT-CHANNELS — channel tabs and preview filtering

Contract v1, source scan 2026-10-28. Implementation and controlled-consumer
qualification are in progress; execution and exact artifact receipts are owned
by the sprint coordinator. This file records source obligations, not a new
overall parity or current native gameplay claim.

## Inputs and ownership

Frozen source: `frozen/play-dev-20261026` at
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Target: `play-dev-ripper` from
`5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` in the existing Ripper Downstream
worktree. Existing inventory ID: `PLAY-CHAT-CHANNELS`. Scope is channel filtering
and its existing native controls; portraits, titles and shortcuts have separate
contracts. No chat sending, account changes or persistence migration is added.

| Inspected branch | Exact revision | Presence and evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Channel/tab/preview consumer in `mods/src/patches/parts/chat.cc`. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer and native Settings control. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete `parts/chat.cc` plus supporting headers, config, registrations and native Settings inspected. |
| refactor input | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | Already implemented in `src/mod/features/chat.cc` and `src/mod/game/chat.h`; this pass reconciles and qualifies it. |

Read complete frozen `mods/src/patches/parts/chat.cc`, `prime/ChatManager.h`,
`ChatPreviewController.h`, `FullScreenChatViewController.h`, their tab/button
accessors, `config.cc`, `defaultconfig.h`, `settings/upstream_settings.cc` and
`patches/patches.cc` registrations. The source computes positions from Cadet and
Veil availability, modifies full-screen tab contexts after native show, rejects
disabled tab changes, redirects the preview and suppresses preview messages.

`[patches].chatpatches` defaults true and controls installation at restart.
`[ui].disable_galaxy_chat` and `disable_veil_chat` default false. Existing Mod
Settings > Chat controls retain their Restart timing and registered save path.
Callbacks read the current configuration; no Config reference or native object
is cached between callbacks. Windows and macOS share the behavior. Native tabs,
their list and the preview scroller remain game-owned.

## Behavioral obligations

| ID | Frozen operation | Target mapping and verification |
| --- | --- | --- |
| CC1 | `GetChatTabIndices`: Cadet shifts Galaxy; optional Veil follows Galaxy; Alliance follows available public tabs. | Checked `ChatTabs::Current` retains manager and observes both availability methods. Four availability combinations covered by actual-consumer fixture; failed observation preserves native handlers. |
| CC2 | `DisableButtons` after `AboutToShow`: disable only requested available Galaxy/Veil contexts. Missing view/list/row is benign. | Existing field chain retained with local roots for view, bar, list and each context; runtime context class checked. Fixture drives 16 availability/preference combinations and verifies native show once first. |
| CC3 | Disabled selected full-screen tab must not call the native selected-tab handler. Allowed selections retain index/tab. | Existing guard, now with typed `void(int32_t, void*)` lookup. Fixture drives each index in every combination; unknown availability calls native once. |
| CC4 | Preview `_focusedPanel`, native panel callback and swipe scroller must agree on Alliance when redirecting. | Existing selective redirect retained as the explicit decision below. Fixture drives show and swipe, including allowed private/Cadet panels and changing preferences. |
| CC5 | Global/Regional preview message callbacks independently obey the corresponding live preference. | Existing original-or-suppress callbacks retained. Fixture checks both message producers in every combination. No message is sent by the test. |
| CC6 | Unavailable children or accessors must leave the game usable. No persistent UI ownership is introduced. | Optional availability prevents guessing a tab. Roots are callback-local and checked failures preserve native selection/swipe. Controlled host tests missing manager/root refusal; real pooling remains native qualification. |

## Explicit source differences

The frozen preview `AboutToShow` always redirects to Alliance whenever either
disable flag is set, even if the remembered panel is Private or Cadet. Its
`OnPanelFocused` captures both flags in function statics and redirects any panel
when either cached flag is enabled. That can make later preference changes
ineffective and prevent allowed panels from staying selected.

The target intentionally retains its existing correction: read current flags
on each callback and redirect only a currently disabled panel. This preserves
the channel-disable purpose without restoring the cached-setting/broad-redirect
defect. Native Settings still labels those preferences Restart; this decision
does not promise an immediate refresh of an already rendered full-screen bar.
Reopening runs native reconstruction before the disable decorator.

Missing/throwing availability previously became false/false and could identify
the wrong tab. The new checked observation refuses that inference and follows
native selection/swipe instead. This is a deliberate failure-path correction,
not newly discovered absent channel functionality.

## Verification and recovery

`tests/chat_bindings_cases.h::ChatNativeCases("chat-native-channels")` includes
the actual production callbacks. It uses controlled managed metadata/method
bodies, Config and native UI storage. It qualifies callback ordering, routing,
live reads, known/unknown observations and temporary custody. It does not run
Unity tab reconstruction, real swipe animation, regional account entitlement
or native message delivery. Child worker ran no builds; coordinator owns the
combined suite, binary and runtime receipts.

Native procedure: on the chosen account, test available Galaxy/Veil disable
combinations, reopen full and side chat, swipe onto permitted and disabled
panels, and confirm received preview messages follow each preference. Preserve
the player's chosen settings. Cadet/Veil combinations unavailable on that
account and macOS remain separate. Rollback uses the ordinary patch/feature
settings; no native controls, settings keys or stores were replaced.

Disposition: consumer present and source reconciled with explicit corrections;
exact candidate execution and native/platform qualification remain pending.
