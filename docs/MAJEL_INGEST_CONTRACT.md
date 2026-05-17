# Majel Ingest Contract

This mod can send optional fire-and-forget telemetry to Majel directly, or to a local sidecar broker that later uploads
to Majel. Both paths are opt-in sync targets.

## Target Modes

`[sync.targets.<name>]` supports:

- `mode = "legacy"`: existing sync behavior. Sends the original sync JSON body with `stfc-sync-token`.
- `mode = "sidecar_broker"`: sends a Majel-safe ingest envelope to a local sidecar endpoint with `stfc-sync-token`.
- `mode = "majel"`: sends a Majel-safe ingest envelope directly to Majel with `Authorization: Bearer <token>`.

Sidecar broker mode remains the preferred durability/retry path. Direct Majel mode is best-effort from the mod process.
Neither mode enables callbacks, remote settings writes, or gameplay commands.

When sync starts, Majel-envelope targets receive one redacted `stfc.mod.capability_snapshot.v1` event. It lists the mod
version, platform, enabled sync categories, target modes, and supported schemas. It does not include target URLs, tokens,
callbacks, remote settings, raw game payloads, or endpoint credentials.

## Envelope

Majel-mode and sidecar-broker-mode targets POST one event envelope per queued sync item:

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
- Fleet runtime snapshots emit as `stfc.fleet.runtime_snapshot.v1`.
- Battle sync targets map to `stfc.battle.summary.v1` until a narrower battle projection is split out.
- Majel-envelope targets also receive `stfc.mod.capability_snapshot.v1` once at sync startup.

## Privacy

Do not send raw Scopely auth/session headers, cookies, coordinates, raw battle token streams, opaque protocol dumps, raw
logs, or gameplay command data. Target tokens are credentials and must stay redacted in logs, runtime snapshots, JSONL,
and diagnostics exports.
