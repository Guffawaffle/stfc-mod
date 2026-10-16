# Play-dev claim capture

This science capture is for ongoing claims investigation while other features are
tested. Keep it out of clean `play` promotions. It observes claim, request, reward,
input, semaphore and offer-refresh lifecycles using anonymous session-local tokens.
It does not retry claims, purchases, reconciliation requests, or reward grants.
The `[patches].claimtracehooks` emergency compatibility switch defaults to true.
Disabling the hook group makes capture unavailable; it is not a claim feature.

The incident families are tracked privately in `.codex/claims/incident-index.json`.
Raw incident data and account-specific findings must remain outside Git.

## Recording and health

The runtime appends to `community_claims.jsonl` beside the mod log. There are two
8 MiB rotations: total retention is approximately24 MiB across sessions. Every
record has a session ID; startup identifies the source build and installed hooks.
Claim/request lifecycle records bypass the input-snapshot rate limit. Existing
bounded token/weak-object/list limits remain explicit in the trace.

`community_claims_status.json` is refreshed every30 seconds, including when no
claim is active. A live acknowledgement distinguishes a running recorder from
an old file left by another session. Partial hook installation or sink failures
make health false. Do not report capture as ready merely because a file exists.

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
