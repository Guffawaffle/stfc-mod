# Side-chat mouse primary routing

Guff reports that right-clicking a user in the open left-side chat window to
choose native "Chat" also executes an available primary action. The ordinary
client 273 installation was observed with source `777a6336` on disk, while the
existing clean Ripper worktree started at `d0fd730e`. Loaded memory was not hashed.

Side chat leaves the map section active. Text focus can legitimately be false
after clicking a message/user row, so neither `Hub::IsInChat()` nor the text-focus
guard establishes mouse ownership. The frozen `1baa55fe` shortcut matcher and
hotkey dispatcher also have no chat-pointer exclusion. This establishes a matching
unsafe source path, not which game/mod patch first exposed the reported behavior.

The correction checks native pointer ownership only when a mouse alternative of
`action_primary` matches without a matching keyboard alternative. It uses the
game's first sorted cached raycast hit, then looks for native chat list, preview
or full-view owners in that hit's hierarchy. An unrelated top hit is not claimed
by chat underneath it. Confirmed chat ownership and unavailable observations
reserve that mouse press; they do not consume native UI delivery. A newly blocked
click also discards an older deferred fleet action. Keyboard alternatives retain
their prior eligibility, including a simultaneous keyboard/mouse press.

The existing camera-wheel guard's checked cached-raycast acquisition is shared
through `game/ui_pointer.h`. Its handler/pinch policy remains with `scroll_input.h`.
Both consumers retain the same qualified count/item/target reads and temporary
GC custody. The shared observation also rejects nonfinite pointer coordinates.
There is no new hook, input latch, scene scan, native menu interception, setting,
default or TOML edit. The earlier unavailable-text-focus guard remains intact.

## Verification

Windows `mods build-tests test-il2cpp test-mod` passed 423/423 build/run/install
steps with ReleaseFast dependencies and Debug fixtures. The existing real matcher
fixture checks bare/modified mouse exclusion, retained Space and simultaneous
keyboard/mouse alternatives. New local mode `chat-pointer-primary` executes the
production cached-hit binding, owner classifier and `FleetActionShortcuts`, with
controlled raycast/parent bodies and an available primary-action sink. It checks
three chat owners, first-hit occlusion, keyboard/world eligibility, managed/custody
failure and recovery. This is not native menu or resource-action qualification.

Explicit `chat-focus-unavailable`, `fleet-mission-primary`, `hotkey-screen-order`,
`camera-native` and `chat-native-navigation` also pass. The camera fixture controls
the ownership result; its pass qualifies wheel/pinch consumer ordering, not the
whole native pointer query. The new mode is local only; no CI registration changed.

Evidence is retained outside the checkout under
`artifacts/chat-pointer-primary-20261102/`. No chat message is sent for automated
validation.

## Windows deployment and player confirmation

The normal AX cycle deployed clean/stable source
`46bd5c969970bda91bedbef0845bc22ce3633340` with the selected AAI/UI Lab host
retained. Ordinary client 273 at
`C:/Games/Star Trek Fleet Command/default/game` restarted from PID 13784 to 17004
at 2026-11-02 20:22 America/Chicago. Copy/readback matched DLL SHA256
`f72cbf2e0146f24e0c9a27f4de9d453abcfe6c74cd12648b1bea5919cd58f3d2`.
The source identity, loaded proxy path and startup were observed; memory bytes
were not hashed. All 73 patch-group lines and FrameTick readiness were present,
with no fresh startup error/unavailable/not-installed lines in the captured log.
The legacy lifecycle parser's count of two is not the complete registry count.
All five observed settings/state hashes were unchanged; no agent TOML edit occurred.

Guff confirms the exact updated Windows trial: right-clicking a user in the open
left-side chat produces the native menu only, without primary_action. Guff also
confirms ordinary world/mission-marker right-click still works. This qualifies
the reported interaction and retained world right-click, not every chat/menu,
keyboard alternative or platform. Keyboard eligibility is additionally covered
by the local matcher/dispatcher fixtures. Mac build/runtime remains unqualified;
no local Mac SDK was available. No source push, PR, tag or release change was made.
