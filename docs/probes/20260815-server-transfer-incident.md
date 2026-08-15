# Server Transfer Incident Diagnostic

Date: 2026-08-15
Tracking issue: [#258](https://github.com/Guffawaffle/stfc-mod/issues/258)
Concern ID: `server-transfer`

## Purpose

Capture a Scopely-ready timeline for an Infinite Incursion failure in which a
starbase relocation on the rival server was followed by an unexpected return to
the home server and later transfer failures.

This concern is observational. It does not change relocation, transfer,
polling, retry, popup, or recovery behavior.

## Activation

```toml
[advanced.diagnostics.concerns]
enabled = ["server-transfer"]
```

Restart the game after changing the allowlist. Records are written only to:

```text
community_patch_target_server-transfer.jsonl
```

under `[advanced.diagnostics.files].root`, or the standard diagnostics location
when no root override is configured. The shared policy retains at most two
1 MiB files for this concern.

## Reproduction

1. Start capture before crossing to the rival server.
2. Cross to the rival server through the Infinite Incursion event UI.
3. Record the approximate local time and the home/rival server numbers shown by
   the game.
4. Relocate the player starbase while still on the rival server.
5. Observe whether the client changes server or presents a transfer state.
6. Attempt the next relevant cross-server action, including return home or a
   second transfer to the rival server.
7. Screenshot every failure popup and record its approximate local time.
8. Exit normally or wait several seconds so queued records reach disk.

Do not repeatedly retry solely to increase the sample count. One complete
failure sequence is more useful than overlapping attempts.

## Records

`transfer-event` records UI intent, manager dispatch, polling start/completion,
the native return values and output-operation presence for temporary transfer,
recall-home, and polling calls, login-layer state and progress, successful
request/poll callbacks, and visible failure popups. `attempt_id` correlates one
transfer lifecycle. Numeric target server instance IDs are included when the
game supplies them.

`relocation-event` records ordinary relocation and cross-server starbase move
calls with numeric node and destination instance IDs. `relocation_id` identifies
the relocation call; `active_transfer_attempt_id` is present only when a
transfer lifecycle is active at that moment.

`transfer-error` records the request or poll stage plus bounded `GSError`
evidence: type, code, HTTP response code, category, message, transaction ID, and
request URL without query string or fragment. Request-handler and manager state
captures include in-progress flags, failure count, polling cadence, and timeout.

## Data Boundary

The concern excludes account/player identifiers, authentication or session
tokens, request/response bodies, `GSError.Data`, temporary transfer/tournament
references, localized server names, and URL query strings/fragments. Strings are
bounded and copied before they enter the asynchronous queue.

Before sending evidence outside the project, inspect every JSONL row for
unexpected identifiers in server-provided error messages or transaction IDs.
Transaction IDs are intentionally retained because Scopely support can use them
to correlate server logs; send them only to Scopely through the chosen support
channel.

## Report Assembly

Provide Scopely with:

- game client version, platform, and approximate local time with timezone;
- home and rival server numbers shown by the game;
- exact ordered reproduction steps;
- screenshots and popup text;
- the relevant JSONL rows from the first `ui-intent` through the final popup;
- the transfer `attempt_id`, numeric target instance ID, error codes, and
  transaction ID;
- whether the issue reproduces with no mod and with another known build.

State explicitly that the diagnostic DLL observed native lifecycle methods and
did not alter transfer behavior.

## Lifecycle

This is a temporary concern introduced in 2.1.0 with a 2.2.0 sunset. Remove or
revise it when a Scopely-ready report is assembled or the failing sequence is
understood. Promotion requires a recurring support workflow and reviewed low
overhead.
