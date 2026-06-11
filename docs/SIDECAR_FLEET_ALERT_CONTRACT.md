# Sidecar Fleet Alert Evidence Contract

Branch: `docs/sidecar-fleet-alert-contract`

This document defines the first contract shape for sidecar-driven fleet alerts. It
is a design checkpoint only: it does not add hooks, sidecar transport, config
keys, TTS providers, or gameplay behavior.

## Problem

Fleet attention loss is a user-facing safety problem. A fleet arrives, mines out,
or becomes threatened while the operator is focused elsewhere. The mod should be
able to help the sidecar alert the operator without turning alerting into
autoplay.

The boundary is strict:

- Native observes gameplay seams and emits copied evidence.
- The sidecar consumes evidence, applies user alert rules, and emits alert
  intents.
- Alert providers fulfill intents through UI, sound, TTS, or later channels.
- No sidecar alert path may scan, attack, recall, defend, repair, start queues,
  or send gameplay commands back to the game.

## Current Seams

These current seams already cover the first fleet-alert use cases:

| Use case | Current seam | Current owner | Evidence today | Gap |
| --- | --- | --- | --- | --- |
| Fleet arrives, docks, starts mining, repairs | `Digit.Prime.HUD.FleetStateWidget.SetWidgetData` | `parts/fleet_arrival.cc` | fleet id, fleet state transition, cached display ship name, cargo/mining context, fleet runtime sync request provenance | system/location id is not carried; ship/runtime id is only available later through fleet runtime snapshot probes |
| Incoming attack materializes | `Digit.Prime.HUD.ToastFleetObserver.QueueNotifications` | `parts/fleet_arrival.cc` | target fleet id, target type, attacker fleet type, attacker identity when quick scan provides it | attacker identity may be opaque; system/location id is not carried; target ship facts depend on fleet-bar cache |
| Fleet Watch current state | deferred `fleet.runtime` snapshot from `ScreenManager.Update` | `fleet_runtime_sync.cc` and `sidecar_local_ingest.cc` | copied fleet/slot state, hull spec id, hull/display names, ship identity probe id when available, sidecar provenance, bounded/coalesced delivery | ids are not consistently string-safe; system/location id is missing |

These seams are enough to design the alert contract. They are not enough for a
high-quality spoken template such as "Your Squall has arrived in Sol" without
adding or preserving better location identifiers.

## Evidence, Intent, Provider

Do not model sidecar alerting as a final notification string produced by native.
Use three layers:

1. Fleet alert evidence: copied facts from a gameplay seam or safe runtime
   snapshot.
2. Alert intent: a sidecar decision that an operator should be alerted.
3. Alert provider: an output adapter such as desktop UI, local sound, Windows
   TTS, Piper, or a later device/channel.

This keeps native focused on evidence preservation and keeps sidecar policy,
acknowledgment, escalation, and provider selection out of gameplay hooks.

## Why Not `sidecar = true`

A simple `sidecar = true` flag under the current notification policy is not the
right primary abstraction.

Current notification policy answers whether native should emit system/audio
delivery for a named event. Sidecar fleet alerting needs different questions:

- Should this evidence be exported to the sidecar?
- Which sidecar rule should consider it?
- Should the sidecar create an alert intent?
- Which provider should fulfill the intent?
- How should acknowledgment and escalation be handled?

A `sidecar = true` flag can exist later as a compatibility shortcut or narrow
export toggle, but it should not be the center of the model.

Preferred future shape:

```toml
[sidecar.alerts]
enabled = true

[sidecar.alerts.rules.fleet_arrived_in_system]
enabled = true
providers = ["desktop", "sound", "tts"]
ack_timeout_seconds = 30
escalate_after_seconds = 90
```

The exact TOML is intentionally not implemented in this branch. The important
rule is that sidecar alert config should describe alert rules and providers, not
raw hook mechanisms.

## Native Evidence Shape

The next native-to-sidecar branch should introduce a sidecar evidence event or
event family, not final alert text. A minimal fleet arrival evidence event should
look conceptually like this:

```json
{
  "protocolVersion": "stfc.sidecar.ingest.v1",
  "kind": "fleet.alert_evidence",
  "payload": {
    "schemaVersion": "stfc.fleet.alert_evidence.v0",
    "eventType": "fleet.arrived_in_system",
    "observedAtUnixMs": 1770000000000,
    "source": {
      "owner": "FleetArrivalHooks",
      "seam": "Digit.Prime.HUD.FleetStateWidget.SetWidgetData",
      "reason": "fleet-slot-arrived-in-system",
      "effect": "publish-fleet-alert-evidence"
    },
    "fleet": {
      "fleetId": "12345678901234567890",
      "slotIndex": 2,
      "state": {
        "previous": 3,
        "previousName": "Traveling",
        "current": 4,
        "currentName": "ArrivedInSystem"
      }
    },
    "ship": {
      "shipId": "9876543210987654321",
      "hullSpecId": "12345",
      "displayName": "Squall",
      "hullName": "USS Enterprise"
    },
    "location": {
      "systemId": "778899",
      "displayName": "Sol"
    }
  }
}
```

Fields that are unknown at capture time should be omitted or set to null only if
the schema says null is meaningful. Do not invent display names in native just to
make a template read well.

## Identifier Requirements

The sidecar is expected to have access to local catalog data such as stfc.space
data. Native should preserve identifiers so sidecar enrichment can resolve names
later.

Carry these identifiers when available:

- `fleetId` as a string.
- `shipId` or runtime ship id as a string.
- `hullSpecId` as a string, with numeric compatibility only if needed.
- `systemId` as a string.
- `nodeId`, `destinationId`, or location target id as strings when a seam exposes
  them.
- `slotIndex` as a number.
- current and previous fleet state values plus readable fallback names.
- display fallbacks such as ship name, hull name, and system name.
- capture timestamp and provenance context.

Runtime ids should be string-safe by default because JavaScript, JSON stores, and
sidecar databases can lose precision on 64-bit numeric ids.

## Recommended MVP Branch

Recommended next branch:

`feature/sidecar-fleet-alert-evidence`

Scope:

- Add a provider-neutral sidecar evidence kind such as `fleet.alert_evidence`.
- Emit copied evidence from existing fleet arrival and incoming attack evidence
  surfaces only.
- Preserve exact ids as strings.
- Reuse existing provenance structs.
- Keep delivery bounded and sidecar-offline safe by using the existing
  sidecar-local enqueue/backoff pattern.
- Add tests for serialization, id string preservation, and config-off behavior.

Explicitly out of scope:

- TTS provider packaging.
- Piper integration.
- Windows TTS integration.
- alert acknowledgment UI.
- escalation timers.
- config namespace migration.
- new raw gameplay hooks.
- gameplay commands from sidecar to game.

## Legacy And Probe Classification

Encountered paths remain classified as follows:

| Path | Classification | Reason |
| --- | --- | --- |
| Direct `SPUD_STATIC_DETOUR` hooks in `parts/fleet_arrival.cc` | temporary legacy exception | Grandfathered by the seam scanner baseline; do not add new raw hooks for alerting. |
| Disabled live-debug deployment-event detours in `parts/live_debug.cc` | quarantine/probe-only | Duplicate deployment-event family; do not re-enable for fleet alerts. |
| `sidecar.sync.fleet_runtime_mode` diagnostic modes | quarantine/probe-only | Useful for boundary testing; not a user-facing alert model. |
| Existing native OS/audio notification delivery | migrate-by-subscription later | Keep current behavior; sidecar alerts should become a separate subscriber over copied evidence. |

## Validation Expectations

Docs/design branch:

- `git diff --check`
- direct gameplay seam scanner if source paths are touched
- `global.stfc-mod.review-contract` for normal branch readiness

Future code branch:

- `git diff --check`
- direct gameplay seam scanner
- `global.stfc-mod.review-contract`
- focused serialization tests proving 64-bit ids remain strings
- sidecar-offline validation proving alert evidence does not hot-loop
- sidecar-running validation proving evidence arrives without changing Fleet
  Watch, queue behavior, notifications, or battle ingest

## Parked Provider Work

Provider work should be a later sidecar branch after evidence exists:

- Compare Windows/system TTS and Piper from the sidecar side.
- Define alert acknowledgment and escalation state in sidecar, not native.
- Keep provider failures isolated from native gameplay and sidecar ingest.
- Keep final spoken text as a sidecar template over enriched evidence.
