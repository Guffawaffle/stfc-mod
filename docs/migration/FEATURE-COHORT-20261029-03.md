# Sona-guided service migration and replay

Source frozen/play-dev-20261026 at1baa55fedf1758b19f843cc59707b8901a0b4998;
target play-dev-ripper based ondf56d989 with the prior navigation batch pending.
Historical PLAY cad98ef7990dd66167eb64ea5dda525c1fe2c296; inspected upstream/dev
e884efa7c6ff66d0776b4cd4d6b76ce193296be5. No source baseline advanced.

## Understanding inputs and decisions

This task consumed the actual scoped LexSona preservation-first_migration@revision1
manifest0.1.0, digest sha256:2afa0e97f6d511fdfafce975849ec31ad7370e21f7c4fdc2a050a92e3da49fe3.
Receipt and consumption observations are in
[the product-fit journal](LEXSONA-MIGRATION-PILOT-20261029.md). LexSona derived
guidance; the agent directly inspected, edited and tested source. No Runner
execution or automatic enforcement is claimed. The old skill was not invoked
for this task, but this same conversation previously read it: not a clean-room
comparison or proof that persona delivery caused the findings.

The chosen source categories are startup configuration and defensive buff
processing. Replays of the accepted Store refresh and preset lifecycle records
exercise different evidence categories without redoing their complete source
scans or claiming new in-game acceptance. Existing production-consumer fixtures
for those paths are included in the regression suite.

## Contracts and bounded changes

| Capability | Contract | Source-grounded outcome |
| --- | --- | --- |
| PLAY-GAME-CONFIG | [Endpoint overrides](features/PLAY-GAME-CONFIG.md) | Native-first load, independent empty overrides and fresh config reads preserved; temporary roots and replaced app-config guard added |
| PLAY-BUFF-GUARD | [Buff crash guard](features/PLAY-BUFF-GUARD.md) | Null/empty/valid lists unchanged; explicit unavailable count/item rejection instead of unknown becoming observed-empty |
| PLAY-OUT-OF-DOCK-POWER | [Station-only power](features/PLAY-OUT-OF-DOCK-POWER.md) | Existing live-toggle adaptation qualified without rewriting condition logic |

Before the corrections, real-consumer fixtures failed on the replaced Config
reference and on unobserved list count entering native code. Original logs are
retained. The endpoint fixture keeps the replaced C++ config alive to reproduce
stale semantics deterministically without relying on undefined behavior. Native
method bodies, metadata and Config::Get provider are controlled substitutes;
these are not actual server endpoint or gameplay tests.

The buff hardening is an explicit unavailable-data decision, not a claim that
the frozen raw _size accessor already had a qualified missing-member path.
No buff filtering, modifier classification, combat math, list mutation or
arbitrary element cap is introduced. The list stays rooted through inspection
and the native original call; element references are examined immediately.

## Parked work and qualification boundaries

The primary-action experiment remains parked. Its exact source was archived in
artifacts/sona-migration-fit-20261029/parked-primary-source.zip before removing
the experimental getter hook, dispatch override and trace changes from ordinary
source. Earlier recovery snapshots and notes remain. This is not a bug fix.
The useful prior preview-rebind guard remains. Dock synchronization remains
unchanged, including100ms cadence, pending its later redesign.

Evidence directory:
`D:/dev/stfc-workspace/artifacts/sona-migration-fit-20261029/`.
Before logs: endpoints-before.log and buffs-before.log. Final assertion-enabled
regression and native build receipts are recorded in the qualification file.
No endpoint overrides, buffs, stores or game configuration are changed to test
this work. Native startup, player behavior and macOS qualification remain
separate from controlled Windows tests. No complete-parity claim.
