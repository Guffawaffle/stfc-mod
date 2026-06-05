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

## Sudo / stfc.phd Bridge Goal

The immediate consumer goal is to make battle exports useful to stfc.phd without forcing it to depend on our display
strings, UI wording, or final CSV parity work. Sudo already has substantial static exports/name lookup data, but still
needs reliable battle-log IDs and enough runtime evidence to join combat rows against research, buffs, exocomps,
officers, hostiles, and other modifier sources.

Near-term contract priorities:

- Keep stable native IDs primary: owner ship IDs, source IDs, effect IDs, component IDs, hull IDs, and battle IDs.
- Preserve source category guesses only when backed by local battle/account evidence, such as bridge officer IDs,
  component IDs, hull IDs, or ship IDs from the same capture.
- Preserve unknown IDs instead of dropping or inventing names for them.
- Include known bridge/officer IDs in the battle-scoped catalog snapshot so runtime `sourceCategory = officer` rows have
  a stable catalog join point even when display-name resolution is unavailable.
- Keep display names optional annotations. They are useful for humans, but should not be the join key.
- Keep static catalog facts separate from runtime candidate rows. Static catalog facts answer "what can happen"; runtime
  rows answer "what appeared or actually occurred in this battle."
- Treat post-attack triggered rows as occurrence evidence. Opportunity counts are still future work.
- Do not classify `effectId` values into ability/buff/debuff domains until marker grammar, static catalog rows, or UI
  presenter evidence proves the domain.

The useful bridge sample shape for stfc.phd is the experimental candidate subset:

```json
{
  "battleId": "2737488725357159795",
  "phase": "round_start|pre_attack|post_attack",
  "ownerShipId": "2731143593850127402",
  "sourceCategory": "officer|ship_ability|component|ship|unknown",
  "sourceId": "4290764940",
  "effectId": "1120204726",
  "value": 0.7,
  "triggered": true,
  "occurrenceCount": 1,
  "tokenRange": {
    "segmentIndex": 1,
    "recordIndex": 0,
    "recordStart": 25,
    "recordEnd": 29
  },
  "confidence": "experimental_marker_decode"
}
```

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

## First Probe Slice

The first native implementation slice uses the existing disabled-by-default `[advanced.diagnostics].battle_log_decoder`
gate. When enabled with battle-log enrichment, the decoder emits `runtimeAbilityRowCandidates` under the
`experimental` section of `battle.report` and `battle.analytics`.

This first probe preserves source/effect/value marker groups from pre-attack prefixes and post-attack triggered-effect
markers. It does not resolve officer names, ability names, buff names, or effect semantics. The candidate rows
intentionally keep display fields null and source categories `unknown` unless same-battle entity evidence proves a
specific category.

After the first cleanup, marker-only owner/phase prefixes are not emitted as candidate rows. A short battle that shows
only normal damage, mitigation, barrier, and shield/hull receive rows should produce zero
`runtimeAbilityRowCandidates`; always-on research/passive stat impacts are static/account-state context, not runtime
ability activations by themselves.

## Manual Cycle Evidence: 2026-06-04

Recent sidecar battle `2737473116774917542` showed the same UI/export split:

- UI owner group: `VOR'CHA`
- Visible source/effect rows included:
  - `SNW SAM KIRK - Phaser-Based Study`, value `90%`
  - `SNW ORTEGAS - Frequency Plating`, value `1,100%`
  - `SNW PIKE - Prepared For Anything`, value `11%`
  - `SNW PIKE - Quick Thinker`, value `25%`
  - `ALOK SAHAR - Augment's Sagacity`, value `1000`
  - `S31 Torpedo Pods`, value `800%`
- Stored `battle.analytics` still had `csvParity.coverage.abilityRowCount = 0` and no experimental candidate section,
  which means this cycle was captured before the native experimental candidate emission was active in-game.
- The raw capture's first pre-attack prefix had `preAttackTokenCount = 48` and markers
  `[-96,-90,-88,-86,-85,-87,-84,-82,-81,-83]`.
- The `-86` fields matched bridge officer IDs from the same capture:
  - `1426126747, 2813724537, 0.9`
  - `1689277174, 2416006584, 11.0`
  - `2241990218, 3308805436, 0.11`
  - `2241990218, 1761806598, 0.25`
  - `2100903263, 3098652344, 1000.0`
- The `-82` fields looked like non-officer or ship/equipment effect candidates:
  - `473132032, 405335503, 8.0`
  - `473132032, 2573953069, 4.0`

Working hypothesis for the next probe slice:

- `-88` / `-85` / `-84` / `-81` carry or refresh owner/participant ship context.
- `-86` carries `sourceId, effectId, value`; when `sourceId` matches captured bridge officer IDs, classify it as
  `sourceCategory = officer`.
- `-82` also carries `sourceId, effectId, value`, but source category remains `unknown` until static catalog or UI
  presenter evidence proves whether it is ship ability, forbidden tech, buff, or equipment.

Longer battles from the same cycle showed post-attack proc evidence:

- Battle `2737476041672811642`: `56` attack rows, `10` decoded triggered-effect blocks.
- Battle `2737475839280866385`: `51` attack rows, `5` decoded triggered-effect blocks.
- Stored analytics still had `abilityRowCount = 0` and no experimental candidate section, so these were emitted by the
  older in-game build before candidate emission was loaded.
- Repeating post-attack token shape:
  - `-93, ownerShipId, -91, sourceId, effectId, value, -92, ... , -94, -99`
- Observed triggered rows included:
  - `4290764940, 1120204726, 0.7`
  - `3426564736, 3426564736, 0.03`
- Interpretation for the experimental probe:
  - `-86` / `-82` rows are candidate static/runtime availability rows for "what is active or can happen."
  - `-93` / `-91` rows are candidate occurrence rows for "what actually triggered during this attack."
  - `opportunityCount` is still unknown; this slice only records occurrence evidence.

## Repro Evidence After Candidate Emission: 2026-06-04

Fresh manual cycles with the experimental probe enabled confirmed that the game is now emitting candidate rows to
sidecar `battle.analytics` while CSV parity remains unchanged:

- `battle.analytics.csvParity.coverage.abilityRowCount` remains `0`; final CSV parity still has only attack rows.
- Short combat logs that visually show only damage/mitigation rows now emit `0` `runtimeAbilityRowCandidates`.
- Longer combat logs emit stable candidate signatures with source/effect/value IDs:
  - Battle `2737488725357159795`: `63` attack rows, `70` candidates, `43` triggered occurrence candidates.
  - Candidate phases: `round_start:13`, `pre_attack:14`, `post_attack:43`.
  - No marker-only candidates were emitted in this run.

Observed candidate signatures from battle `2737488725357159795`:

| Count | Phase | Source category | Source ID | Effect ID | Value | Triggered | Marker kind |
|---:|---|---|---|---|---:|---|---|
| 25 | `post_attack` | `ship_ability` | `3426564736` | `3426564736` | `0.03` | `true` | `triggered_effect_value` |
| 18 | `post_attack` | `officer` | `4290764940` | `1120204726` | `0.7` | `true` | `triggered_effect_value` |
| 14 | `pre_attack` | `officer` | `4290764940` | `1120204726` | `0.7` | null | `source_value` |
| 4 | `round_start` | `officer` | `4290764940` | `1120204726` | `0.7` | null | `source_value` |
| 4 | `round_start` | `officer` | `182221633` | `2974230331` | `0.05` | null | `source_value` |
| 1 | `round_start` | `officer` | `1426126747` | `2813724537` | `0.9` | null | `source_value` |
| 1 | `round_start` | `officer` | `2241990218` | `3308805436` | `0.11` | null | `source_value` |
| 1 | `round_start` | `officer` | `2241990218` | `1761806598` | `0.25` | null | `source_value` |
| 1 | `round_start` | `unknown` | `473132032` | `405335503` | `8` | null | `secondary_source_value` |
| 1 | `round_start` | `unknown` | `473132032` | `2573953069` | `4` | null | `secondary_source_value` |

The latest short-battle screenshot showed owner ship `SERENE SQUALL` versus `Suliban Stealth Cruiser` with only
damage, mitigation, Apex Barrier, shield-health, and hull-health rows. This aligns with zero runtime ability candidates:
those rows are normal combat/mitigation output, with passive research or always-on stat effects folded into the combat
math rather than represented as visible ability activations.
