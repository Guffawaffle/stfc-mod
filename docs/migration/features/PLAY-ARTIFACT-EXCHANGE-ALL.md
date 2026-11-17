# PLAY-ARTIFACT-EXCHANGE-ALL — artifact bulk conversion visibility

Contract v1; complete source `parts/artifact_exchange.cc`, checked component/
GameObject metadata, Settings availability, alias and patch registration. Exact
refs: [cohort](../FEATURE-COHORT-20261029-01.md). Present in all three inspected
source baselines; target `features/shop.cc` uses the existing `game/inventory.h`
binding and `ArtifactExchange` transaction (`artifactexchangehooks`).

| Obligation | Preserved behavior | Qualification |
| --- | --- | --- |
| A1 | Native OnDidBindCanvasContext runs first, exactly once | Actual consumer bind/receiver fixture |
| A2 | When enabled, hide only `_convertAllButton` GameObject | Dedicated-field fixture; individual/unrelated objects unchanged |
| A3 | Missing button/object or disabled setting leaves native presentation | Missing-object and disable/reopen fixtures |
| A4 | Reopening/rebinding refreshes native button visibility | Same controller/native rebind fixture |
| A5 | Default=false; legacy hide_artifact_exchange_all alias retained | Existing Config parser/Settings persistence regressions |

The component only changes visibility after native binding. It never invokes a
conversion, changes conversion prices, blocks individual exchanges or overwrites
the entry button. Native bind remains responsible for populating/restoring the
actual controller. Typed binding failures remain contained and cannot establish
a successful hide. Static signature checks, fixture behavior, compiled platforms
and actual native UI remain separate qualification claims.
