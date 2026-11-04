# Chat focus and Escape recovery

Guff reports that mod keybinds stopped working after using the chat input,
possibly around Alt+Tab. Recovery followed Alt+Tab or clicking the input and
then elsewhere; which action restored input is uncertain. The sequence cannot
currently be reproduced reliably, and Escape behavior during the occurrence is
unknown. This pass fixes a matching source-level Escape recovery defect and adds
a bounded diagnostic. It does not establish the cause of the reported incident
or claim that all intermittent keybind failures are resolved.

## Source and runtime evidence

Work uses the existing Ripper Downstream worktree, branch `play-dev-ripper`,
starting clean at `46af805cdf787947cf5259678ae8864d3001d4fc`. The canonical
Downstream study checkout has an unrelated dirty `AGENTS.md`, left untouched.
No branch, checkout or worktree was created or switched for this correction.

Read-only runtime observation at 2026-11-04 00:28:36 UTC selected ordinary
client 273, PID 36528, at
`C:/Games/Star Trek Fleet Command/default/game/prime.exe`. The installed proxy's
current disk SHA256 was
`f72cbf2e0146f24e0c9a27f4de9d453abcfe6c74cd12648b1bea5919cd58f3d2`,
with embedded source `46bd5c969970bda91bedbef0845bc22ce3633340` and build
`ax-zig-20261103T021931-ea3bc88e`. The pre-correction focus implementation and
hotkey dispatcher are identical between that source and the starting worktree.
This identifies current disk bytes and reported source, not in-memory bytes or
the transient focus state during the incident.

That session subsequently ended outside the agent's actions. A new log starts
at 18:32:35 America/Chicago; the prior `.1` log starts at 17:36:21. Filtering the
available prior log found no GetComponent/isFocused/Hotkeys diagnostics. No
text-focus, capture or modifier state was captured during the reported failure.
Quiet logs do not exclude an unavailable query, stale focus or a modifier issue.
No agent cycle or live UI automation was performed for this investigation.

The older Boolean-value correction is present in both frozen `1baa55fe` and the
typed implementation: both read the focus value rather than treating its
address as true. Both also retain Escape clearing of selected text input.
Source/history inspection has not established a missed prior fix or an Alt+Tab
regression. See the earlier [unavailable-focus guard](CHAT-INPUT-GUARD-20261102.md)
for the separate protection against typed keys dispatching gameplay shortcuts.

## Matching source defect and correction

The conservative guard treats an unavailable focus observation as typing. That
continues to reserve ordinary typed shortcut keys for native input and retries
next frame. Previously, Escape entered that typing branch, called
`ClearInputFocus`, and returned handled unconditionally. Unknown focus caused
the clear helper to do nothing; a failed clear-selection call was likewise not
reported to the dispatcher. Returning handled skipped native
`ScreenManager.Update`, starving the game's own Escape recovery. A persistent
unavailable observation could repeat this on every held Escape frame.

`Key::TryIsInputFocused` now exposes the same frame-cached observation with its
known/unavailable status. `IsInputFocused` retains the conservative policy.
`ClearInputFocus` takes a fresh observation and returns `CallResult<bool>`:
failure means an unavailable observation or failed clear invocation;
success/false means known absent text focus; success/true means a known focused
field received a successful checked `SetSelectedGameObject(nullptr)` invocation.
It invalidates the cached observation on every outcome. A successful clear
invocation does not establish observed blur: a selection callback can redirect
focus to a field.

The Escape branch consumes the key after a successful clear. Unknown focus or
a failed clear instead returns native Update directly, before the mod's
exit-key suppression can swallow it. Existing Escape ownership for full chat
with freshly confirmed absent text focus is preserved. That decision uses the
clear helper's qualified result rather than an earlier cached typing Boolean.
This provides a native recovery
opportunity; it cannot guarantee that the game repairs a persistent failed
binding or stale field. No forced refocus, application-focus reset, scene scan,
new hook, setting, toggle or TOML change is introduced.

## Self-review correction

The first correction returned a plain Boolean from `ClearInputFocus`, merging
known absent focus with a failed fresh observation. Self-review found that an
initially unfocused full-chat snapshot still caused the caller to consume
Escape if the helper's later observation failed. The new tenth mode,
`chat-focus-recovery-late-observation`, qualifies this transition independently
of the original nine modes. Windows `build-tests` passes 191/191 steps against
that first correction, but the new mode exits 1 at the expected assertion:
the earlier unfocused snapshot must not mask a later failed clear observation.
`late-observation-before.json`, the corresponding build/case logs and the
preserved test executable bind this correction-delta failure. It is not a
native Alt+Tab reproduction.

The qualified result above keeps known absent focus separate from failed
observation and invocation. Full chat preserves its former ownership only when
the fresh observation succeeds; a fresh failure leaves native Escape available
even if the initial snapshot was known unfocused. No broader input reset is
introduced.

## Bounded diagnostic

`_MODDBG` builds add `[Hotkeys] input-state` at debug level when Unity reports a
key-down, at most once per two seconds. A log-level guard returns before any
diagnostic probes when debug logging is off. Each sample records only frame number,
capture status, whether text focus was sampled, whether that observation was
known, its confirmed focused value, and whether Unity reports a held modifier.
The normal branch reuses the focus read at the dispatcher's existing position,
after earlier native callbacks; the capture branch does not query text focus.
It records no chat text, key identities, object pointers or account state.

The dispatcher fixture explicitly scopes `_MODDBG` around its included
production hotkey implementation because the build's `moddbg` option does not
propagate that definition to the test module. The late-observation mode enables
debug logging. Its final Windows and Linux runs both contain the required
input-state sample and pass the staged focus-read failure assertion. An extra
text-focus query before the clear helper would move that staged failure and
fail the case. The diagnostic reuses the sampled focus result while retaining
the expected focus-read ordering.

`text_sampled=false` means no text observation was taken, not a failed read.
When `text_known=false`, `text_focused=false` is not proof of absent focus.
The modifier Boolean does not identify a key or independently verify physical
keyboard state. Rate limiting, debug-log filtering and dependence on Unity's
key-down observation can miss a short transition. These samples can distinguish
some blocked-input states when an incident recurs; they do not establish an
Alt+Tab cause or provide a complete input trace.

## Controlled verification and delivery status

The baseline Windows `build-tests` receipt reports 191/191 steps succeeded.
Nine explicit production-dispatcher modes then produce six expected failures:
unavailable EventSystem, selected object, component and focus getter, plus clear
setter failure before and after selection mutation. Three controls pass:
known focused clear, callback-redirected focus, and known unfocused full chat.
The baseline failures establish Escape starvation in the tested source paths,
not reproduction of the user's native incident.

The corrected cases exercise held Escape across successive frames in map and
full-chat contexts, preserve unavailable-focus suppression of ordinary faction
shortcuts, and recover to exactly one navigation after known focus=false.
Setter failure after mutation remains a failed invocation and must leave native
Escape available. Successful-clear and callback-redirect controls verify
same-frame cache invalidation without assuming blur. Configured action matching,
capture and unrelated services remain controlled fixture sinks; these are not
native chat, Enter/Send, physical keyboard or Alt+Tab trials.

Before the self-review delta, Windows `mods build-tests test-il2cpp test-mod`
with `-Dmoddbg=true` passed 423/423 steps and 16/16 explicit modes. Linux
`test-il2cpp` passed 157/157 steps and 13/13 explicit modes. Those receipts,
executables and hashes are preserved under `first-pass/`; they qualify the
superseded Boolean-result implementation, not the final qualified-result delta.

Final Windows `mods build-tests test-il2cpp test-mod` with `-Dmoddbg=true` passes
423/423 build/run/install steps and 17/17 explicit modes: ten recovery modes
plus retained unavailable focus, hotkey order, chat pointer, native chat
navigation, dock Enter, dock capture reentry and camera pan. The test executable
SHA256 is
`9e88ca1ce6e8bc793adae2768eb8f040c98123f55496b9ae81c9d346c55006b8`;
the compiled core `mods.lib` SHA256 is
`ee1829488efc8063638e35f4a69a217fc97ee0f79e8cfb20c97bcc2ffa7de541`.

Final Linux `test-il2cpp` passes 157/157 steps and 14/14 explicit modes: ten
recovery modes plus unavailable focus, hotkey order, chat pointer and dock Enter.
The preserved ELF test executable SHA256 is
`8f60cd5111a9361724115c1f4e96b45470d4f690cf2fd2a3f217f4e01b4bbdc3`.
All five changed C++/header inputs match the frozen inputs on Linux. The
refreshed full source manifest records 479 inputs with no frozen-input
mismatches. These are local controlled-runtime results, not native client
qualification. No new CI registration is added.

Enabling the production debug branch in the fixture exposed an unused fleet
probe link reference. An inert fixture-only stub asserts if called; no fleet
probe is installed or exercised. The failed link attempt is retained under
`moddbg-fixture-link-failure/`. The final passing results above include that
fixture-link correction.

Both Mac core target requests, `aarch64-macos.14.6` and
`x86_64-macos.14.6`, stop during configuration because the required Mac SDK is
unavailable; neither reaches compilation. Both requests were repeated after the
final API correction. The shared code path and Linux pass do not establish Mac
build or runtime qualification.

Source signing, review, final native proxy build and delivery are pending.
Core compilation is not an assembled proxy artifact or deployment. No corrected
DLL is claimed installed or accepted by the player, and no deployment or cycle
is planned as part of this recovered incident without the source-bound lifecycle
flow. The planned normal native build preserves the installed configuration's
clean pinned host `4bece2e5` and enabled UI Lab. The core fixtures do not use that
host; this correction changes no AAI source and makes no AAI parity claim. No
agent cycle, live UI input, deployment or game configuration change occurred.

Evidence is retained outside the checkout under
`D:/dev/stfc-workspace/artifacts/chat-focus-recovery-20261103/`, including
`runtime-observation.json`, `before-windows-build.log`,
`before-windows-cases.json`, per-mode baseline logs, the preserved baseline test
executable and `before-inputs.json`. Final receipts must bind the actual source,
commands, exit results and artifact before a delivery or runtime claim is added.
`first-pass/` retains the superseded local results. Final
`local-verification.json`, `after-windows-cases.json`,
`after-linux-results.json`, `after-linux-freeze-comparison.json` and
`final-source-inputs.json` bind the completed correction-delta results.
`mac-builds.json` retains the configuration limits.
