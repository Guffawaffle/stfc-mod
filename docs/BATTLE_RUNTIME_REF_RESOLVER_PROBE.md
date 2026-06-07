# Battle Runtime Ref Resolver Probe

Status: experimental scout for `experiment/runtime-battle-ability-rows`.

This is a read-only diagnostic track. It does not promote runtime ability/effect candidates into CSV parity rows, and it
does not claim ability resolution without an explicit local catalog/domain match.

## Purpose

`battle.analytics` now emits exact string join keys and experimental runtime ability/effect candidates. The resolver
probe answers a narrower question:

> Do the local dumps currently available to this checkout resolve candidate refs such as `sourceRef`, `effectRef`,
> hull IDs, component IDs, or legacy triggered-effect refs into known data domains?

The probe output is evidence-first:

- exact ref string
- candidate domain
- matched record key/path
- matched display/localized name if available
- confidence label: `exact_id_match`, `hash_key_match`, `localized_name_match`, `domain_ambiguous`, or `unresolved`
- source file/path/provenance

## Local Surfaces Checked

Repo-local and sibling scout runs checked:

- `tools/il2cpp-dump/dump.cs`
- `tools/il2cpp-dump/script.json`
- `tools/il2cpp-dump/stringliteral.json`
- `.ax-priv/cache/dump-corpus.jsonl`
- `.ax-priv/cache/stfc.db`
- `.ax-priv/tools/Il2CppDumper/stringliteral.json`
- `D:\dev\stfc-mod-sidecar-static-modifier-catalog`
- `D:\dev\netniv\stfc-mod`

These surfaces expose useful metadata, but they are not the same thing as a complete Spocks/stfc.space/stfc.phd-style
gameplay catalog export. The IL2CPP and AX dumps expose code shape, classes, methods, fields, enum values, string
literals, and symbol cards. They do not currently expose full static rows for officers, officer abilities, buffs,
forbidden tech, ship abilities, hulls, weapons/components, resources, or localization tables in a way that resolves the
current runtime refs.

Native runtime code still has stronger in-game resolver seams:

- `SpecManager::GetHull`
- `SpecManager::SearchForSpec` for components
- `SpecManager::GetOfficerSpec`
- `SpecManager::GetBuffSpec`
- `SpecManager::GetForbiddenTechSpec`
- `ActivatedAbilityManager::GetActivatedAbilityLocaId`

The protobuf model also defines relevant static sync domains:

- `StaticSyncHullSpecsResponse`
- `StaticSyncOfficerSpecsResponse`
- `StaticSyncComponentSpecResponse`
- `StaticSyncOfficerAbilityBuffSpecResponse`
- `StaticSyncShipBonusBuffSpecResponse`
- `StaticSyncResearchSpecsResponse`
- `StaticSyncStarbaseBuffsSpecsResponse`
- `StaticSyncConsumableBuffsSpecsResponse`
- `StaticSyncActivatedAbilitySpecsResponse`
- `StaticSyncForbiddenTechSpecsResponse`
- `StaticSyncForbiddenTechBuffsSpecsResponse`

Those seams indicate where resolution probably needs to come from, but the current local file/dump scout did not find
catalog rows matching the representative runtime refs.

## Probe Command

Run the built-in fixture check:

```powershell
py tools\battle_ref_resolver_probe.py --self-test
```

Run against repo-local IL2CPP/AX dump surfaces with the current seed refs:

```powershell
py tools\battle_ref_resolver_probe.py --repo-root . --default-roots --demo-refs
```

Run against a real sidecar battle analytics JSON/JSONL file:

```powershell
py tools\battle_ref_resolver_probe.py `
  --repo-root . `
  --default-roots `
  --refs-from-json path\to\battle.analytics.json `
  --json
```

Add arbitrary local catalog/export roots when available:

```powershell
py tools\battle_ref_resolver_probe.py `
  --repo-root . `
  --default-roots `
  --root D:\path\to\spocks-or-static-catalog-export `
  --refs-from-json path\to\battle.analytics.json
```

The battle analytics input file is used only to extract refs and is excluded from catalog scanning so the probe cannot
resolve refs by finding them in the same battle event that supplied them.

## Current Scout Result

Command:

```powershell
py tools\battle_ref_resolver_probe.py `
  --repo-root . `
  --default-roots `
  --root D:\dev\stfc-mod-sidecar-static-modifier-catalog `
  --root D:\dev\netniv\stfc-mod `
  --demo-refs `
  --json
```

Result summary:

- `refCount = 19`
- `matchedRefCount = 0`
- `unresolvedRefCount = 19`
- `resolvableDomainsNow = []`
- `sourceEffectPairsJoinMeaningfully = false`

Unresolved representative refs:

- `4290764940`
- `1120204726`
- `3426564736`
- `182221633`
- `2974230331`
- `1426126747`
- `2813724537`
- `2241990218`
- `3308805436`
- `1761806598`
- `473132032`
- `405335503`
- `2573953069`
- `1280858269`
- `1720277001`
- `298753785`
- `3280907524`
- `4273474013`
- `2490276629`

## Interpretation

The current local metadata/dump surfaces are enough to prove that the seed refs are not trivially available as checked-in
JSON/JSONL/CSV rows, IL2CPP string literals, AX string literals, or enum values.

The tracked `.ax/` folder in this repo is now a public facade. The private dump
cache for this probe lives in `.ax-priv/` when present, and the probe also
checks `.ax/` as a fallback for older local layouts.

They are not enough to produce named ability rows for Sudo/stfc.phd. They are enough to demonstrate:

- exact string ref capture in `battle.analytics`
- deterministic display strings for damage/scalar values
- experimental candidate grouping without overclaiming
- a reproducible resolver scout showing which refs remain unresolved

The next useful slice is to add or capture a real static catalog source for these domains, likely by extending native
runtime/static catalog extraction or importing a representative Spocks/stfc.phd-style catalog file into the probe.
