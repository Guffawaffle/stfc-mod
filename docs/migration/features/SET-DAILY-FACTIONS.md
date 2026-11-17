# SET-DAILY-FACTIONS — Daily faction selection

Contract v1; cohort 20261026-01. Source scan: 2026-10-26. Source
`frozen/play-dev-20261026@1baa55fedf1758b19f843cc59707b8901a0b4998`;
target base `05ee3d7f66eec4d9cb39f8924d012a7ca1ec5952`.

Parent: PLAY-CLAIM. Entry: Mod Settings → Claims & Confirmations.
Windows/macOS. Upstream dev has TOML/gameplay consumers; play/play-dev have the
selector; target already has the selector with a different empty/all encoding.
See exact branch observations in the inventory.

## Source workflow

Producer: `settings/upstream_settings.cc:Lists` factionMask/factionText and
ChoiceMode::Multiple. Shared ValueSetting/ChoiceSetting, defaults/config parser,
`config_filters.cc:ParseFactionFilter`; consumer
`patches/parts/daily_faction_bulk_claim.cc`, including icon visibility, list filtering,
GetClaimableDailiesList and show/hide lifecycle. Target equivalents are
`settings/config_settings.cc` and typed `features/missions.cc`.

| Obligation | Source behavior | Target disposition / check |
| --- | --- | --- |
| F1 | Federation/Klingon/Romulan independently selected; mask 0–7. | Preserve existing native multiple-choice control; test all eight combinations. |
| F2 | Empty parsed list means native/all, shown as mask 7; all serializes blank. | Restore source semantics in UI reader/writer; no startup normalization rewrite. |
| F3 | All unchecked means explicit `none`, never empty. Unknown names rejected by strict adapter; contradictory none fails closed. | Reuse source parser semantics, preserve sentinel, test exact writer values and reopen parse. |
| F4 | Only main-faction dailies filtered; other daily rewards remain eligible. Icons follow selections. | Existing typed mission consumer has same empty/all and sentinel behavior; not rewritten. Native icon/claim trial pending. |
| F5 | Live requested selection uses registered writer, observed-write protection, reset/default metadata and reopen notice. | Same ValueSetting model and writer; test stale draft/default reset and unavailable owner. |

This is reconciliation, not another claim-system implementation. Source default
toggle-on behavior, its bounded attempts and lifecycle are supporting existing
consumer behavior, not an additional feature port in this cohort.

Validation/checkpoints: [cohort record](../FEATURE-COHORT-20261026-01.md).
Fixture success covers models/catalog/writer and parse round-trip, not actual
reward collection. Do not execute claims merely to qualify this settings adapter.
