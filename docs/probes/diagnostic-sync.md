# Optional diagnostic sync destination

`[sync] diagnostics = false` is the default. A target can opt in with
`[sync.targets.astrometrics] diagnostics = true`, the receiver URL and token shown
in Bridge's Astrometrics tab, and other categories disabled for that target.
Existing sync target inheritance, headers and routing apply. For a local target,
set `proxy = ""` to override any global sync proxy.

This change supplies the generic `PublishDiagnosticEvent(stream, event)` sender
API. It does not attach or activate any producer, install hooks, or change game
behavior. Claim, bulk-selection and receipt attachments are supplied separately
by optional science integration; enabling diagnostics alone produces no events.

The input is an existing JSON observation with an unsigned `seq`, a `session`,
an `event` (or `phase`), and optional integer `utc_ms`. The sender wraps it in a
JSON sync array with type `diagnostic` and envelope schema 1. Envelope fields are
`type`, `schema`, process-scoped `source_id`, `producer_version`, original
`session`, `stream`, decimal-string `sequence`, optional decimal-string `utc_ms`,
`event`, and original JSON `payload`. Account identity is not inferred.

Delivery uses existing fire-and-forget workers: no outbox, retry or relay.
Diagnostic messages over 256 KiB and admission past 256 queued entries are
dropped with a log message. These limits apply to diagnostic admission, not
other sync categories. Sequence gaps and absent captures remain missing evidence.

The companion receiver runs locally inside Bridge while explicitly enabled. Its
SQLite store is separate from the claim investigation index. Bridge owns its
receiver, evidence schema and UI; the mod owns this sender contract. No game
configuration is edited automatically by this feature.
