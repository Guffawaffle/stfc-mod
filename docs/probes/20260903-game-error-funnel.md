# Game Error Funnel Science Probe

## Question

Can the central managed game-error handler provide a small, actionable error-state stream without adding a general
runtime trace or increasing `community_patch.log` volume?

## Provenance

- Science branch: `science/promotion-hud-error-state-probe`
- Base commit: `c6ad5da9db4428742a8c4c6682fa0a6a6de71a5e`
- Base behavior: the exact `releasedbg` DLL deployed while the player was using the OPC indicators and native console
- Client build: 260
- GameAssembly SHA-256: `3B219F2556F677C818C892B06D091C154D7FBDA9DC459A4A0F80F95D35AC1C47`
- Metadata SHA-256: `DBFA2522C8F4FF9BC7490F8B4B2947BC988D77D7318810C1F576F0C44951F5B2`

## Seam And Risk

- One seam: `Digit.Client.Core.GsErrorHandler.OnGSError(GSError)`
- Build-260 RVA and native extent: `0x8198C0..0x819E40` (`0x580` bytes)
- Risk class: R4, native interpretation of a known managed `GSError` argument
- Behavior: log-only; the original handler is called exactly once before best-effort file/console output
- No callback families, per-frame hooks, error suppression, or game-state mutation

The payload fields are resolved by name from current IL2CPP metadata rather than hard-coded offsets. The captured
surface is limited to error type, numeric code, HTTP response code, bounded category/message, request path, and
transaction ID. This is a local science artifact intended to preserve identifiers that may be useful in a Scopely bug
report. URL queries/fragments, hosts, headers, and request bodies are not recorded.

Installation is pinned to the validated build-260 handler RVA and all 29 bytes in SPUD's instruction-aligned x64
relocation window. The probe refuses to install after a client drift changes either check, even when the managed class
and method names still resolve. An install-time negative self-test alters a byte after the common ten-byte prologue and
confirms that the complete-window fingerprint rejects it.

## Enable And Disable

The probe exists only in Windows `releasedbg` builds and defaults off. Set this environment variable in the process
that launches the game:

```text
STFC_MOD_SCIENCE_ERROR_PROBE=1
```

Remove the variable and restart with the same DLL to disable it. Reinstalling the known-good base DLL is the rollback
if the hook is unstable.

## Output And Volume Bounds

- Dedicated file: `community_patch_science_errors.jsonl`
- Rotation: 1 MiB per file, two rotated files
- Consecutive exact duplicate events inside ten seconds are collapsed
- At most 60 distinct events are emitted per anchored one-minute window
- Every enabled process writes a session-start record; subsequent errors carry the same process/timestamp session ID
- Every error row repeats the client and mod artifact provenance, so it remains attributable if rotation removes the
  session-start row or that marker cannot be written
- One bounded core/message line and, when identifiers exist, one bounded transaction-ID/request-path line are also
  published to the sleeping-by-default native dev console when it is awake; normal UUIDs remain complete, every line
  respects the overlay's 112-byte limit, control/line-separator bytes and TMP angle-bracket markup are rendered inert,
  and no console formatting work occurs while it sleeps
- Existing sync logging remains unchanged; its defaults are already `sync.logging = false` and `sync.debug = false`

Rotation bounds disk consumption, not retention time. Disabling the probe does not delete existing science logs; keep
them for the investigation or remove them manually after the evidence is no longer needed.

## Run Plan

1. Build and deploy the science branch as `releasedbg` with the environment variable enabled for launch.
2. Confirm one hook installs and the normal game boot remains clean.
3. Play normally; do not intentionally induce purchases, claim failures, or destructive actions.
4. Inspect the dedicated JSONL after a natural error or after a bounded observation window.
5. If no useful events appear, do not broaden immediately. The next candidate is the promotion-local
   `PromotionHudViewController.OnFetchBundlesError(GSError)` seam, evaluated separately.

## Stop Conditions And Exit

Stop immediately on crash, hang, input loss, repeated modal behavior, or any evidence that the original handler is not
called once. If stable, either keep this as a temporary branch-only diagnostic until enough natural evidence exists,
or delete it if the central funnel does not observe the failures of interest. Promotion into product diagnostics would
require a separate review of schema, privacy, retention, and hook support.

## First Runtime Observation

- Deployed probe commit: `60aba74`
- Deployed/build DLL SHA-256: `758777C55FB40A50F68F5D5B9F3672D977A1982156E841C00ACF7118B822477E`
- Boot result: 31 hook groups evaluated, 28 installed, three config-skipped, zero boot errors
- Health after the observation: game process responsive at approximately 1.8 GiB working set
- Dedicated stream volume: one JSONL record / 377 bytes
- Naturally observed event: network `GSError`, code `429`, HTTP response code `-1`, category `default`, message
  `Duplicate request detected`, sanitized route `//alliance/{id}`
- Suppression counters: zero duplicate events and zero rate-limited events
- No claim- or promotion-specific error was observed in this initial window

This moves `GsErrorHandler.OnGSError(GSError)` from static relationship to runtime-observed with interpreted payload on
this exact client/build pair. It does not yet establish that promotion-local bundle failures pass through the central
handler.

## Frozen Refinery Claim Correlation

While source state `f56a37c738e78cdbde58debece435cf9273acf53` was deployed, the player observed a frozen
`REFINE MATERIALS` reward screen. The probe captured the same error twice, at `2026-09-04T00:31:24.599-05:00` and
`2026-09-04T00:32:02.350-05:00` (37.751 seconds apart):

- Type `platform`, code `14`, HTTP response code `0`
- Message: `Bundle not available for purchase by selected account`
- Empty category, request route, and transaction ID
- Zero prior duplicate or rate-limited events on both rows

This is a direct runtime correlation between the frozen claim presentation and a platform bundle-eligibility rejection.
It does not yet prove which upstream state selected the unavailable bundle or why the UI failed to unwind after the
error.

## Review Correction

The first review gate rejected the initial hook because telemetry executed before the original handler and raw-byte
truncation could produce invalid UTF-8. The corrected design snapshots best-effort, calls the original exactly once,
then performs all serialization and output behind a catch-all boundary. Truncation preserves UTF-8 code-point
boundaries, JSON serialization replaces any remaining invalid sequence, and an install-time multibyte boundary
self-test fails closed. The probe remained disabled while this correction was made.
