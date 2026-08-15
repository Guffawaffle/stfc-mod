# Infinite Incursion Transfer State Failure

## Summary

After entering the rival server during Infinite Incursion, relocating my
starbase caused the client to return me to my home server unexpectedly. The
event UI still offers a transfer to the rival server, but every attempt now
fails immediately. The failure popup says the request to return home failed
while also stating that the account is still on the home server.

The client and account transfer state appear to disagree about whether the
account is home or away.

## Environment

- Platform: Windows
- STFC client generation: 254
- Home server shown by the client: US 710 Alderman
- Rival target instance supplied by the Incursion UI: 703
- Observed on: 2026-08-15
- Detailed reproduction captured at: 2026-08-15 19:00:51 UTC
  (2026-08-15 14:00:51 CDT)

## Preconditions

1. Infinite Incursion was active.
2. The account had successfully crossed to the rival server.
3. The player starbase was relocated while on the rival server.
4. The relocation was immediately followed by an unexpected return to the home
   server.

## Steps To Reproduce

1. Open the Infinite Incursion event UI while the client shows the account on
   home server 710.
2. Select the action to transfer to the rival server.
3. Observe the transfer failure popup.
4. Dismiss the popup and repeat the transfer attempt.

## Expected Result

The account transfers from home server 710 to the rival server instance offered
by the Infinite Incursion UI.

## Actual Result

The request fails in approximately 120-150 ms, before transfer polling begins.
The popup says:

> Infinite Incursion Transfer Failed
>
> Something went wrong with the request to return home!
>
> Your account is still in US - 710 Alderman.

The wording is contradictory to the selected action: the player selected a
transfer to the rival server, not a request to return home.

## Native Request Timeline

The following sequence reproduced three times in one instrumented session. The
final attempt was:

| UTC time | Elapsed | Native observation |
|---|---:|---|
| 19:00:51.671 | 0 ms | Incursion UI intent: transfer to rival instance 703 |
| 19:00:51.671 | 0 ms | Transfer state changed to `Request` (`state=1`, `progress=0`) |
| 19:00:51.671 | 0 ms | `RequestTransfer(703, temporaryReference, out operation)` returned `true`; output operation was present |
| 19:00:51.821 | 150 ms | Request operation raised platform `GSError`, code 400 |
| 19:00:51.821 | 150 ms | Transfer state changed to `Failed` (`state=4`, `progress=0`) |
| 19:00:51.838 | 167 ms | Incursion-specific transfer-failed popup was shown |

The exact native error message was:

> Invalid instance transfer request. Requesting recall home instead of transfer.

Additional error fields:

- Error type: `PLATFORM` (`2`)
- Error code: `400`
- HTTP response code: `0`
- Error category: empty
- Transaction ID: empty
- Request URL: empty
- Poll requests observed: `0`

At the error callback, the request handler still reported the start request in
progress and an active transfer operation. Immediately after the error, both
were cleared and the state was `Failed`.

## Interpretation

The temporary transfer call itself is admitted: it returns `true` and creates a
transfer operation for rival instance 703. The operation is then rejected
before polling with an error that says recall-home should have been requested.

This proves the failure is in initial transfer request classification or
validation, not a transfer polling timeout. Combined with the UI stating the
account is on home server 710, the evidence strongly suggests stale or
contradictory home/away transfer state. The capture cannot determine whether
that state is held in the client account layer or Scopely's platform service.

## Diagnostic Method

A temporary observational DLL recorded the game's existing Incursion UI,
server-transfer request handler, state callback, error callback, popup, polling,
and relocation method boundaries. It did not modify arguments, return values,
callbacks, retries, polling, account state, relocation behavior, or transfer
behavior.

The capture reported zero queue drops, record-size rejections, shutdown drops,
or writer failures. The complete local evidence is in:

```text
F:\stfc-diagnostics\native-logs\community_patch_target_server-transfer.jsonl
```

The screenshot of the failure popup should be attached with this report.
