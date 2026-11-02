# Chat text-input guard: unavailable focus

Guff reports that chat intermittently stops owning typed keys: Enter stops
working and the configured faction-store shortcut opens the store. Reported
after loading the typed refactor branch. Exact full/side view, native focus
state and triggering transition have not been captured. The user is away;
this pass uses source and local controlled checks only.

Source: existing Ripper Downstream `play-dev-ripper`, starting clean at
`b79b0501bbdf5ff3d2491b331bbe426e06fff34f`, tracking the same remote head.
Signed correction: `777a6336a7e55c481472dd05de47d4015231d85c`.

## Matching source failure

The earlier `207becc9` correction avoids querying components on a confirmed
destroyed selected Unity object. That lifetime guard remains intact. The older
pre-migration focus-value/address correction is also represented by the current
typed Boolean getter; this is not evidence that its original defect returned.

The shared focus reader used default-valued calls. Failed EventSystem,
selection, component or isFocused observations could therefore become false
(not typing). Outside a full-screen chat section, `HandleShortcuts` could then
dispatch the configured faction-store action and return true, skipping the
native ScreenManager.Update for that invocation. A controlled production
dispatcher case reproduces this path with a selected text field and a failed
component lookup. Baseline exits1 at the shortcut-leak/native-delivery assertion.
This establishes a matching unsafe path, not the cause of the reported live
chat failure.

The private reader now preserves known true/false versus unavailable through
existing TryCall results. `Key::IsInputFocused` conservatively reserves keys
for native input when a focus observation is unavailable, cached only for the
current frame and retried next frame. `ClearInputFocus` clears selection only
after confirmed focus; unknown focus cannot trigger a clear-selection callback.
Known absent EventSystem/selection/component, known unfocused field and confirmed
destroyed selection retain their prior behavior. Unavailable Unity liveness
still proceeds to the existing component/focus observations.

No chat-specific latch, forced refocus, owner scan, new hook, input interception,
config/default/shortcut or Settings change is introduced. Actual loss of native
focus is not repaired by this correction. A persistent unavailable binding can
keep mod shortcuts reserved until the underlying query becomes available.

## Local verification

Explicit `chat-focus-unavailable` executes the actual shortcut dispatcher,
shared Unity focus reader and screen routing against the existing controlled
host. Corrected cases pass unavailable component/focus/selection/EventSystem,
preserved native selection, continued native Update delivery, recovery to known
focused state and normal faction navigation after confirmed focus=false.
No real text is read or sent, and no native chat Send/Enter handling is claimed.

Five explicit modes pass: chat-focus-unavailable, hotkey-screen-order,
chat-native-navigation, screen-actions-pending and screen-actions-routing.
Existing `test-il2cpp test-mod` passes312/312 build/run steps, including focused,
nontext, clear/redirect, destroyed selection, unavailable liveness and missing
EventSystem focus cases. No CI/target registration changed. Clean/stable Windows
releasedbg at the correction passes41/41 and proxy verification. Candidate DLL
SHA256: `F58B8BAA710E82D65046A77A15112329E03F19513045EADF87047FAB701C7547`.

Evidence: `D:/dev/stfc-workspace/artifacts/chat-input-guard-20261102/`, including
before/final commands/cwd/commit/duration/exit/executable hashes, baseline case,
final inputs, signed checkpoints, native receipt and preserved DLL/PDB.

## Delivery and next check

At local assessment, correction and candidate remained local/unpushed/undeployed. No game input, cycle,
TOML edit, chat message or resource action occurred. Published signed preview
`ripper-preview-20261102.1` and the installed development DLL are unchanged.
New correction has no Mac build/runtime qualification yet.

When Guff returns, establish whether this was full or side chat and whether
clicking the input restores it. If deployment is requested, check a draft with
the configured faction shortcut and Enter, leave/reopen chat and repeat. Do not
send a message merely for validation. Native reproduction/confirmation remains
open; collect narrowly scoped focus-state evidence if the symptom persists.
Settings passes and unrelated parked work remain stopped.

Guff subsequently requested the DLL in the game folder before switching PC
accounts. Read-only runtime context resolved the ordinary installation at
`C:/Games/Star Trek Fleet Command/default/game` with no running game process.
Normal AX deploy used the preserved candidate with explicit artifact path and
expected hash; copy/readback matched
`F58B8BAA710E82D65046A77A15112329E03F19513045EADF87047FAB701C7547`.
The installed DLL identifies source `777a6336`; no game stop/start, input or TOML
edit was needed. AX updated normal install/sidecar metadata. Source remains
unpushed and the published preview is unchanged. This is deployment only, not
startup or chat behavior qualification; the game was left closed.
