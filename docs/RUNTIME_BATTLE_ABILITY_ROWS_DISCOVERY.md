# Runtime Battle Ability/Effect Row Discovery

Status: experimental, unstable, disabled-by-default future work.

Branch: `experiment/runtime-battle-ability-rows`

This track is separate from the static modifier catalog slice. The static catalog answers "what can happen"; runtime ability/effect rows should answer "what actually happened in this battle." Do not mix runtime discovery probes into the static catalog branch or expand static source families from this work until runtime IDs/kinds are observed and understood.

## Stella Fixture Correction

`STELLA` in the screenshot is the participant/owner ship whose battle log group contains the visible rows. It is not the source of every ability or effect.

Interpret the visible rows as:

| Owner/participant ship | Source / ability | Visible effect |
|---|---|---|
| `STELLA` | `QUASI - Adaptive Transformation` | Increases Apex Shred against non-Armada hostiles by `+25%`. |
| `STELLA` | `B'ELANNA TORRES - Knock it Down` | `100%` chance each round to apply Hull Breach to non-player hostiles or Armada for `1` round. |
| `STELLA` | `WOK SAAVIK - By The Book` | On round start, against non-Armada hostiles, increases Isolytic Cascade damage by `15%` for `1` round if the enemy has any state. |
| `STELLA` | mitigation/damage row | Mitigates `25` Standard damage. |

The runtime row model must keep participant/owner ship identity separate from source ability identity:

- `ownerShipId` / `participantShipId`: the ship whose battle log group the row appears under.
- `sourceCategory`: officer, ship ability, forbidden tech, buff, mitigation, unknown, etc.
- `sourceId`: the actual source ID when known.
- `sourceName`: source display name, such as `B'Elanna Torres`, `Wok Saavik`, or `QUASI`.
- `abilityName`: ability label, such as `Knock it Down`, `By The Book`, or `Adaptive Transformation`.
- `effectName`: effect label, such as `Hull Breach`, `Apex Shred`, `Isolytic Cascade`, or Standard damage mitigation.
- `targetShipId` / `targetScope`: affected ship or scope when known.

Do not collapse `STELLA`/participant identity into officer, ability, or effect source identity.

## Fixture Evidence To Preserve

The current fixture shows a battle UI/export mismatch:

- The UI shows Round 1 ability/effect rows under the `STELLA` participant group.
- `battle.analytics.csvParity.coverage.abilityRowCount = 0`.
- Only attack rows are emitted in CSV parity.
- Attack row 0 has `preAttackTokenCount = 30`.
- Attack row 0 `preAttackMarkers` include `[-96, -90, -88, -86, -85, -87, -84, -83]`.
- The related `catalog.snapshot` has empty `officers`, `abilities`, `buffs`, `debuffs`, and `forbiddenTech` domains.
- Current `triggeredEffectCount` is a narrow decoded-record count from the known post-attack marker grammar. It must not be interpreted as "all ability activations."
- One decoded `triggeredEffect` with value `0.12` exists, but it does not by itself explain the visible UI rows above.

## Current Decoder Boundaries

The current decoder:

- Finds the stable 16-token attack payload shape.
- Preserves tokens before that payload as `preAttackTokenCount` and `preAttackMarkers`.
- Decodes only the narrow post-attack marker shape:
  `-93, shipId, -91, refA, refB, value, -92, -94, -99`.
- Emits CSV parity attack rows only.
- Emits ability columns as placeholders.
- Hard-codes `abilityRowCount = 0`.
- Populates `catalog.snapshot` effect domains only from decoded `triggeredEffects` that already have a typed `kind` and `id`.

This suggests the visible UI rows are likely one of:

- separate records currently treated as opaque,
- pre-attack token groups inside attack records,
- post-attack token groups not covered by the narrow decoder,
- rows derived from static/localized text plus runtime state,
- or a mix of token evidence and UI/view-model state.

## Proposed Runtime Row Shape

Notes only. Do not implement as final analytics yet.

```json
{
  "schema": "stfc.runtime_ability_row.v0",
  "battleId": "string",
  "round": 1,
  "subRound": 1,
  "phase": "round_start|pre_attack|post_attack|attack|mitigation",
  "ownerShipId": "string",
  "ownerShipName": "Stella",
  "sourceCategory": "officer|ship_ability|forbidden_tech|buff|mitigation|unknown",
  "sourceId": "string|null",
  "sourceName": "B'Elanna Torres",
  "abilityName": "Knock it Down",
  "effectName": "Hull Breach",
  "targetShipId": "string|null",
  "targetScope": "non-player hostiles or armada",
  "triggered": true,
  "opportunityCount": 1,
  "occurrenceCount": 1,
  "chance": 1.0,
  "value": null,
  "durationRounds": 1,
  "stackCount": null,
  "rawMarkers": [-96, -90],
  "tokenRange": {
    "segmentIndex": 0,
    "recordIndex": 0,
    "start": 0,
    "end": 29
  },
  "confidence": "experimental_marker_decode"
}
```

Stable IDs/kinds are the first useful target. Once runtime rows carry source IDs, ability IDs, buff IDs, target codes, and trigger codes, consumers such as stfc.phd can join those rows against static catalog facts without depending on display text.

## Discovery Surfaces

Next useful work is source-surface discovery, not final analytics:

- Marker grammar around pre-attack token groups.
- Combat result UI presenter/view-model row creation.
- Localization formatting calls used for battle-result ability rows.
- Candidate officer, ability, buff, debuff, forbidden-tech, mitigation, and ship-ability IDs.
- Whether UI rows are decoded from battle log tokens, UI state, static specs, localization formatters, or a mix.

## Probe Safety Rules

Experimental probes are allowed to be unstable. A targeted read/probe may crash the game if it is contained and produces useful evidence.

Required boundaries:

- Put probes behind explicit config flags, disabled by default.
- Prefer one probe at a time.
- Use bounded logs or a ring buffer.
- Log enough context to learn from failure.
- Keep crash-prone probes out of default config.
- Do not ship unstable probes as normal behavior.
- Do not touch queue repair, Majel/cloud, release/tag/PR flow, or static catalog source-family expansion.

If a probe crashes, record:

- Exact probe enabled.
- Screen/context.
- Battle/log state.
- Last successful read.
- Suspected pointer, type, offset, or source.
- Whether the crash happened immediately, on battle open, on row render, or on battle close.

## Minimum Next Slice

1. Capture or preserve a Stella fixture bundle with raw `battle.capture`, `battle.report`, `catalog.snapshot`, `battle.analytics`, and screenshot-visible rows.
2. Add one disabled-by-default experimental probe surface for discovering battle-result UI rows or pre-attack marker groups.
3. Emit bounded diagnostic rows with raw IDs/kinds and token/view-model provenance only.
4. After IDs/kinds are proven, decide whether to expand catalog resolver domains, static officer ability source families, or marker grammar first.
