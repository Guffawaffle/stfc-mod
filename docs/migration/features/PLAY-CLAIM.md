# PLAY-CLAIM — gift drawer and faction dailies

Contract v1; frozen `1baa55fe`, target base `266a688c`.
Read complete `parts/open_bulk_claim_gifts.cc`, `parts/daily_faction_bulk_claim.cc`,
their bindings/configuration/Settings/registration and frame delivery. Target:
`features/shop.cc`, `features/missions.cc`, `game/inventory.h`, `game/missions.h`.
All inspected PLAY/play-dev/upstream baselines contain these consumers; exact
revision definitions and final qualification are in [cohort03](../FEATURE-COHORT-20261028-03.md).

| Obligation | Preserved behavior / narrow adapter | Verification |
| --- | --- | --- |
| C1 | Native show/update populates drawer; enabled context gates opening | Actual production lifecycle fixture |
| C2 | At most one attempt per landing, marked before native click | Nested native-update fixture |
| C3 | Hide/replacement cancels old episode; no persistent strong scene root | Weak revision owner and callback/custody fixtures; native pooling trial separate |
| C4 | Empty faction selection keeps all dailies; only excluded Fed/Rom/Klg rewards are removed | Actual filter fixture, native source list identity retained |
| C5 | Unknown/missing faction evidence is retained, never interpreted as excluded | Conservative source behavior retained |
| C6 | Three icons reflect selected factions; no reward/stat inference | Source/binding inspection; native appearance trial |
| C7 | Optional default goes through Toggle.OnPointerClick, once per session, bounded300 attempts | Actual pointer-handler fixture; later player choice retained |

The target inherits source selection behavior while correcting cancelled native
show/update episodes and unavailable toggle reads. The current configuration is
consulted live; persistent preference ownership remains with Settings/TOML.
No game reward action is invoked by fixtures. Main-faction filtering mutates the
native returned list as in the source, not a parallel claim collection. Filter
linkage gaps cannot establish which unclassified rewards would be claimed.

Installers retain the existing transactional hook sets; show/hide remain the
native lifecycle and the frame callback only services a requested toggle.
No global scanner, source deduplication, state migration or automatic purchase.
