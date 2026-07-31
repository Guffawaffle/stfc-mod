# Protobuf corpus refresh

The tracked schemas in `mods/src/prime/proto` are a curated, package-aggregated
view of protobuf types reachable in the STFC IL2CPP client. Refreshes therefore
use a two-stage workflow:

1. `scripts/Invoke-ProtobufRefresh.ps1` downloads a hash-pinned Protodec source
   revision, applies the checked-in STFC compatibility patch, and extracts
   candidate schemas directly from the installed `GameAssembly.dll` and
   `global-metadata.dat`.
2. `scripts/Compare-ProtobufCorpus.ps1` compares the candidates with the
   tracked corpus by top-level and nested protobuf type, field number, wire
   type/cardinality, enum value, and oneof membership. It writes a JSON report
   containing source versions and hashes.

Run from the repository root:

```powershell
.\scripts\Invoke-ProtobufRefresh.ps1
```

Protodec currently requires a .NET 10 SDK. Use `-GamePath`, `-UnityVersion`,
`-DotNetPath`, or `-OutputRoot` when auto-detection does not match the local
environment.

Candidate files and reports are written under `.cache/protobuf-refresh`; the
script never overwrites tracked schemas. Promote actionable additions and shape
changes after review, then run the comparator again. A clean promotion has zero
actionable changes. Pass `-FailOnActionableChanges` when using the refresh as a
validation gate.

`field-not-emitted`, `type-not-emitted`, and `type-incomplete` are retention
observations, not automatic removals. IL2CPP metadata can omit inherited or
unreachable generated members, so deleting tracked declarations based only on
those observations would be unsafe.

## 2026-07-30 refresh

Source client: Unity `6000.0.59f2`.

Promoted changes:

- 9 `ClientModifierType` values.
- 9 fields across `ApmFiltersStatus`, `EventMetadata`, `UserProfileSettings`,
  and nested `BuffSpec.Attributes`.
- 1 nested `EntityGroup.Type` value for the server `Tracker` model.
- 9 new schema declarations covering dynamic translations, tournament
  bundles, event-card metadata, event/news/leaderboard response holders, and
  the server `Tracker` model.

The post-promotion comparison covers 790 extracted and 803 tracked declarations
and reports zero actionable changes. Its 81 retained observations remain
documented in the generated JSON report for future audits.
