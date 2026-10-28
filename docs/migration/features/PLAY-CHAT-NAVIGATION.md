# PLAY-CHAT-NAVIGATION — full/side chat and channel shortcuts

Contract v1, source scan 2026-10-28. Source:
`frozen/play-dev-20261026` `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target: existing Ripper Downstream `play-dev-ripper` from
`5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0`. ID:
`PLAY-CHAT-NAVIGATION`. Scope is the existing open/focus/channel workflow;
message sending and unrelated shortcut actions are excluded.

## Source, activation and host

| Inspected branch | Exact revision | Presence and evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Chat open/focus/channel routes in `mods/src/patches/parts/hotkeys.cc`. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Same reachable routes and native shortcut Settings. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Relevant complete `ScreenManager_Update_Hook` dispatch/early-return context, MapKey/config/native mappings, ChatManager and view/input headers inspected. |
| refactor input | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | `features/chat.cc::OpenChatShortcut/ChatChannelShortcuts` called by `features/hotkeys.cc::HandleShortcuts`; present consumer. |

The single frame/input owner remains `mod.cc`; no new polling hook is added.
`[patches].hotkeyhooks` defaults true; control hotkeys default enabled and native
Scopely hotkeys default off. Capture, disable/native-hotkey mode and text/section
ownership remain in the existing dispatcher. Outside chat and text input,
opening shortcuts run after fleet shortcuts and before screen/settings actions.
Within chat, channel shortcuts retain their dispatch branch and Global,
Alliance, Private precedence; this can run with focused chat input as in source.
Escape focus clearing still precedes channel dispatch.

Configured defaults in `shortcuts.def`: `show_chat=C`, `show_chatside1=ALT-C`,
`show_chatside2=backtick`; channels Global/Alliance/Private are Ctrl-1/2/3.
Existing Controls/shortcut settings own edits and persistence. Opening/selecting
chat sends no text and writes no preference or account store.

## Behavioral obligations

| ID | Frozen operation | Target mapping and verification |
| --- | --- | --- |
| CN1 | No open shortcut means no manager lookup/action; either side shortcut requests Side, otherwise full requests Fullscreen. | Existing consumer retained; controlled production-consumer fixture covers all three actions and exact requested mode/category. |
| CN2 | Existing side chat focuses its newest tracked full-screen view's message input rather than reopening. Missing view/list/input is benign. | Same tracker and field chain, now locally rooted. Fixture tests focus and missing list; real native tracker readiness is separate. |
| CN3 | Closed chat sets requested native ViewMode, then opens Alliance with native empty parameters string. | Existing binding retained; unavailable side-state no longer becomes closed, and a failed mode setter cannot open an unintended view. Fixture covers both failure paths and exact native request count. |
| CN4 | In-chat channel shortcuts request Global, Alliance or Private and consume only the first match. | Existing ordered loop preserved. Fixture exercises every channel and conflicting actions. No manager/root means no request or consumed channel. |
| CN5 | Host exclusions and dispatch order retain native Update where required; text must not open a new chat screen. | Existing `HandleShortcuts` unchanged. Earlier dispatcher fixtures use explicitly named chat sinks, so new consumer fixture does not claim combined native host qualification. Inspect existing guards and exercise them in native trial. |
| CN6 | Disabled-channel policy remains owned by Chat callbacks, not a second shortcut-specific state machine. | No new filtering here. `PLAY-CHAT-CHANNELS` records preview/tab semantics and unknown-availability fallback. Full/side/channel interplay remains a joint native trial. |
| CN7 | Source declarations without installation are not newly activated. | Frozen `ChatMessageListLocalViewController_AboutToShow_Hook`/`SendOnFocus` occurs only in its declaration/original pointer/definition; no registration/caller found. It remains dormant. |

## Adaptation and evidence

Native manager, tracked view, message list and input custody are callback-local.
The only behavioral corrections are refusing unknown side-open observations and
withholding OpenChannel after a failed requested mode setter. These failures
previously defaulted into a possibly different screen. Successful known-state
workflow, shortcut priority, bindings, settings and storage ownership remain.

`tests/chat_bindings_cases.h` mode `chat-native-navigation` executes the real
exported consumer bodies, renamed only in the fixture translation unit to avoid
the existing dispatcher test's chat sinks. Managed native methods, current
tracked objects, Config and pressed-action sets are controlled substitutes.
It verifies full/side requests, focus/missing input, channel precedence and
failure paths; it does not qualify real key layouts, focus events, Unity view
pooling, the combined dispatcher or a live native chat manager. No child builds
were run; coordinator owns candidate/test/build/runtime receipts.

Native procedure: use full and both side shortcuts from a nontext screen; repeat
with side chat open to focus its input; use channel chords within chat; exercise
Escape, capture, ordinary text focus, disabled-hotkey/native mode and disabled
channel interplay. No chat message needs to be sent. macOS/native layouts remain
separate. Existing controls and patch gate provide rollback.

Disposition: reachable source workflow reconciled and present, with narrow
failure-path corrections; exact candidate execution/native/platform results
pending. No complete parity claim.
