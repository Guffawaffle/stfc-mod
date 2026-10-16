# Play-dev claim capture

This science capture is for ongoing claims investigation while other features are
tested. Keep it out of clean `play` promotions. It observes claim, request, reward,
input, semaphore and offer-refresh lifecycles using anonymous session-local tokens.
It does not retry claims, purchases, reconciliation requests, or reward grants.
The separate, opt-in UI recovery below may retry one native navigation request.
The `[patches].claimtracehooks` emergency compatibility switch defaults to true.
Disabling the hook group makes capture unavailable; it is not a claim feature.

The incident families are tracked privately in `.codex/claims/incident-index.json`.
Raw incident data and account-specific findings must remain outside Git.

## Recording and health

The runtime appends to `science/claims/community_claims.jsonl` beneath the normal
mod data directory (the game directory on Windows). Status, reply and control
files live in that same `science/claims/` folder. There are two
8 MiB rotations: total retention is approximately24 MiB across sessions. Every
record has a session ID; startup identifies the source build and installed hooks.
Claim/request lifecycle records bypass the input-snapshot rate limit. Existing
bounded token/weak-object/list limits remain explicit in the trace.

`community_claims_status.json` is refreshed every30 seconds, including when no
claim is active. A live acknowledgement distinguishes a running recorder from
an old file left by another session. Partial hook installation or sink failures
make health false. Do not report capture as ready merely because a file exists.

The helper accepts the game/mod data directory or the explicit claims folder.
It finds the newest status across the new layout and legacy flat builds, so a
rollback's live capture remains reachable. It does not search archived traces.

```powershell
python scripts/claim-capture.py status --game-dir <runtime-log-directory>
python scripts/claim-capture.py mark --label natural-repro --game-dir <runtime-log-directory>
```

`mark` writes an incident marker and snapshots all retained rotations into the
checkout's ignored `.codex/claims/incidents/` directory with hashes and a receipt.
Live copies can contain open spans or rotate during copying; receipts record
detected source changes. The marker is evidence of the report time, not proof of
the onset or root cause. `snapshot` can preserve existing files without claiming
a successful live acknowledgement.

## One-shot UI failure test

While a chest summary is open, refresh health, then arm explicitly:

```powershell
python scripts/claim-capture.py status --game-dir <runtime-log-directory>
python scripts/claim-capture.py arm-ui-failure --game-dir <runtime-log-directory>
python scripts/claim-capture.py cancel --game-dir <runtime-log-directory>
```

The arm is tied to the current session and summary generation. For60 seconds at
most, it can reject one `TriggerSectionChange` call made synchronously inside
that summary's `DoContinue`. It is consumed once, cancelled on hide/rebind, and
does not affect unrelated navigation. The injected event is explicitly marked.
It never manufactures a network failure, changes rewards, clears locks, or
automatically presses a claim or navigation button. The next human action runs
normally. This tests evidence collection; it does not reproduce server-side
delays, stale offers, or prove the cause of naturally stuck input.

## Rejected navigation and experimental recovery

Capture records the section manager's current/next/previous sections, transition
busy flag and coroutine identity before and after summary navigation. Rejections
include bounded native caller locations. A shared navigation ID joins the inner
transition result to the outer reward-return result, explicitly marking cases
where the inner call rejects but its parent returns success.

The feature defaults off and does not control hook installation:

```toml
[ui]
claim_navigation_recovery = false
```

When enabled, exactly one rejected transition inside one successful native reward
return can schedule a retry for that summary generation. The recorder must be
healthy, the same summary/context and origin must remain bound, and native section
state must be known. It waits at least 250 ms and observes 100 ms of idle navigation,
with a 5-second deadline. Changed context, new input/navigation, a message box,
disabled configuration or a changed section cancels it. A consumed/cancelled
budget cannot be replenished by another click on the same summary.

The retry calls the game's existing return-from-rewards routine, retaining its
own back-navigation gates. The transition hook also rejects any recomputed
destination or flags differing from the initial request. No forced transition,
lock clearing, claim submission, reward mutation or retry loop is added.

`navigation_recovery_*` events record scheduling, observations, cancellation and
the one attempt. An accepted transition is distinguished from eventual summary
hide, which records the recovery's navigation ID. `cancel` cancels both a pending
recovery and the artificial rejection arm. Status includes feature/pending state.
The UI fault test is suitable for checking this path: with recovery enabled, one
human DONE press should reject once, then close through the guarded retry.

## Qualification

The65 current Windows x64 client272 entries have inspected native space for the
SPUD overwrite. Runtime resolution checks callback ABI and rejects ambiguous
overloads. macOS shares the implementation, with platform-specific process,
caller-stack and focus handling. Native fit and player smoke on ARM64/Intel Mac
remain unverified until those exact binaries and testers are available. A build
or successful Windows test does not establish Mac hook fit.

Local policy test: compile `tests/claim_ui_fault.cc` with `mods/src` on the include
path. It covers one-shot consumption, unrelated contexts, expiry and cancellation.
Run the Python helper tests locally; these tests are not added to CI.
`tests/claim_navigation_recovery.cc` exercises delayed one-shot retry, idle waiting,
expiry, changed/unknown context and cancellation without a game client.

## On-screen incident notice

The play-dev capture displays a persistent, click-through box below the console
area after a recorded summary-navigation rejection, native claim timeout, or
manual incident marker. It stays visible across screen changes until cleared or
restarted. Rejections are amber. A green "Claim recovered" notice requires the
same summary to hide after an accepted automatic retry within five seconds;
manual navigation and new summaries cancel that attribution. It reports UI
recovery, not server-side reward ownership. Artificial rejections show `(TEST)`.

The notice uses the existing native overlay without waking the debug console or
its diagnostic producers, including in release builds. No new game hooks or
claim requests are added. `preview-notice` tests only the box and explicitly
labels it as a display preview; `clear-notice` removes it. Both commands use the
same current-session control protocol as `status`, for example:

```powershell
python scripts/claim-capture.py preview-notice --game-dir "C:/Games/Star Trek Fleet Command/default/game"
python scripts/claim-capture.py clear-notice --game-dir "C:/Games/Star Trek Fleet Command/default/game"
```

Compile/run `tests/claim_notice.cc` with `mods/src` on the include path to check
accepted-vs-hidden attribution, synchronous/asynchronous hide order, generation,
manual cancellation, rejection, expiry and test labeling. Visual placement still
needs an exact-artifact check after a cycle.
