# Majel Ingest Contract

This mod can send optional fire-and-forget telemetry to Majel directly. Local sidecar delivery is moving onto the
dedicated `[sidecar.*]` namespace and is no longer configured through sync targets.

## Target Modes

`[sync.targets.<name>]` supports:

- `mode = "legacy"`: existing sync behavior. Sends the original sync JSON body with `stfc-sync-token`.
- `mode = "majel"`: sends a Majel-safe ingest envelope directly to Majel with `Authorization: Bearer <token>`.

`mode = "sidecar_broker"` is now invalid in the config parser. Local sidecar settings belong under `[sidecar.sync]`.
Omitting `mode` preserves legacy compatibility. An explicit mode must be exactly lowercase `legacy` or `majel`; any
other value rejects the complete target rather than falling back to raw-capable Legacy delivery.
Direct Majel mode is best-effort from the mod process. Neither supported mode enables callbacks, remote settings
writes, or gameplay commands.

When sync starts, Majel-envelope targets receive one redacted `stfc.mod.capability_snapshot.v1` event. It lists the mod
version, platform, enabled sync categories, target modes, and supported schemas. It does not include target URLs, tokens,
callbacks, remote settings, raw game payloads, or endpoint credentials.

## Envelope

Majel-mode targets POST one event envelope per queued sync item:

```json
{
  "protocolVersion": "majel.ingest.v1",
  "eventId": "uuid-or-stable-fallback",
  "source": "stfc-community-mod",
  "sourceVersion": "2.0.1-guffa.1",
  "installId": "not_configured",
  "sessionId": "launch-local-uuid",
  "sequence": 1,
  "observedAt": "2026-05-17T22:00:00Z",
  "schema": "stfc.fleet.runtime_snapshot.v1",
  "classification": "cloud_private",
  "payload": {}
}
```

Payload rules:

- Payloads that already carry `schemaVersion` or `schema` keep that schema and payload shape.
- Existing legacy sync arrays are wrapped as `stfc.sync.delta_batch.v1` with `syncType` and `items`.
- Fleet preset slot deltas also emit Majel-only `stfc.fleet.assignment_snapshot.v1` events.
- Fleet runtime snapshots emit as `stfc.fleet.runtime_snapshot.v1`.
- `Battles` and `BattlelogsRealtime` are not accepted by Majel targets. This prevents raw journals and capture tokens
  from entering a Majel request body under a misleading summary label. Configured `battlelogs = true` or
  `battlelogs_realtime = true` values are reported and normalized to `false` in the runtime snapshot. Legacy non-Majel
  delivery and the canonical local Sidecar Battle stream are unchanged.
- Majel-envelope targets also receive `stfc.mod.capability_snapshot.v1` once at sync startup.

## Privacy

Do not send raw Scopely auth/session headers, cookies, coordinates, raw battle token streams, opaque protocol dumps, raw
logs, or gameplay command data. Target tokens are credentials and must stay redacted in logs, runtime snapshots, JSONL,
and diagnostics exports.
