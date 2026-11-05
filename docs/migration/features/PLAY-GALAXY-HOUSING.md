# PLAY-GALAXY-HOUSING — qualified housing-capable system markers

Contract v1, 2026-10-27. Frozen source:
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` on `play-dev-ripper`.
This preserves the existing native marker and its evidence meaning.

## Source, data and entry point

Read complete frozen `mods/src/patches/parts/galaxy_housing.{cc,h}`, its
`galaxy_labels.cc` bind/data/release/refresh callers, native UI helper ownership,
`assets/data/station_housing.json`, Settings registration and configuration.
The target uses `features/galaxy.cc`, `galaxy_housing_data.h`, the same embedded
dataset, typed native Image/Transform/LayoutElement bindings and the existing
GalaxyLabels capability. It adds no catalog download or account-state inference.

The named housing-marker capability/dataset is absent in inspected upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`. PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`, frozen play-dev and the target contain
the source, settings and retained dataset. Native home/station artwork by itself
does not imply the additional marker capability exists upstream.

`graphics.galaxy_station_housing` defaults false. Current Mod Settings already
registers the live control with `HousingAvailable` and `Refresh`; the old M8
missing-page statement is historical. Availability requires installed labels and
successfully parsed data. Housing data failure does not disable independent
native labels/overlays.

## Meaning and behavioral obligations

| ID | Required source behavior | Current mapping / qualification boundary |
| --- | --- | --- |
| H1 | Mark cataloged housing-capable systems, never available/free docking slots. Preserve exact positive IDs and dataset validity. | Existing schema/order/subset/Int64 parsing remains. `HousingData::Contains` distinguishes positive, covered-negative and unknown; pure dataset fixture covers malformed and exact-value cases. |
| H2 | Show only when enabled, native effective detail is Near, the same bound system is housing-positive, and its native name/row is eligible. | Existing `GalaxyHousing` admission preserved. Unknown/out-of-coverage systems are not invented positives. Real name/range visibility is native trial work. |
| H3 | Reuse the game's home sprite, reserve native row space and keep the decorative image from intercepting raycasts. | Existing 36×32 slot, 26×26 glyph, cyan tint, aspect and no-raycast treatment retained. Native Image/layout construction is not reproduced in the controlled lifecycle fixture. |
| H4 | Hide before updates and remove on pooled release/rebind/replacement, partial construction failure and disable. | Existing partial-marker readiness/custody retained; disable now explicitly reaches inactive pooled markers through Refresh/AfterZoom. Same-address replacement must first finish the old journal/asset cleanup. |
| H5 | Failure to acquire operation-local custody or confirm native destruction cannot silently drop the owned asset. | `GalaxyRemoveHousing` keeps the hidden marker until Destroy succeeds or destruction is positively observed. Failed disable cleanup also keeps a pending latch and retries on later existing Refresh/AfterZoom calls, even outside Galaxy. Production fixture tests failed Destroy, retained owner, independent later success and re-enable cancellation. Unknown Unity liveness is not disappearance. |
| H6 | Reentrant cleanup must not erase the newly bound record, and no static destructor should invoke IL2CPP after runtime shutdown. | Process-lifetime state and explicit game-thread disposal retained; raw record identity is rechecked after native cleanup. Lifecycle fixture covers replacement during cleanup and complete release of fixture roots. |

## Engineering decisions

The source's marker helper kept raw handles and caught native UI failures. The
current typed implementation already carries explicit incomplete-marker state
and strong asset custody. This pass preserves that safer boundary rather than
copying the source's release-on-failed-destruction behavior. It also repairs the
shared pooled-record paths that could discard an expired owner's pending cleanup
or mistake strong-handle allocation failure for object death.

No new art, marker semantics, offsets, persistence, generalized pool or layout
redesign is added. The dataset remains reusable game knowledge, separate from
account identity and transient docking availability. Asset rendering and shared
camera/LOD hooks retain their existing owners.

A failed disable edge is not considered completed merely because the setting is
now off. Its pending marker-cleanup latch survives until current tracked asset
custody is empty, independently of layout restoration. Re-enabling housing cancels
disabled-only destruction attempts; normal Data/bind/release ownership still
applies to the retained asset. No new frame hook, generalized task or retry owner
is introduced.

## Validation and native qualification

`tests/galaxy_housing_data_test.cc` covers data interpretation.
`tests/galaxy_presentation_bindings_cases.h` executes the actual marker cleanup,
pooled identity and Settings-disable helpers with counted native Destroy/layout
substitutes. It does not render the real home sprite or validate Unity layout.
The batch record attaches final builds/tests to the integrated source candidate.

Native trial: compare a known housing-positive, covered-negative and unknown
system; verify name/detail eligibility, native artwork, spacing and raycasts;
disable in Settings with labels inactive and return to Galaxy; release/rebind the
same pooled widgets and check for residual markers. Creation/setter failures,
Mac hook fit/builds and Mcohall's native trial remain distinct from Windows boot
or headless test success.

Implemented/automated qualification is not complete visual acceptance or
whole-product migration parity. AAI, travel/cargo, player stores, UI redesign,
system orbit and housing vacancy detection remain outside this feature pass.
