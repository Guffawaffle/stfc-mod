# PLAY-OFFICER-SORT — Below Deck Ability sorting

Contract v1, scanned 2026-10-26. Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`;
target `play-dev-ripper@f4f64180543a9231234cd2b3d3f547dfa580476b`.
Windows/macOS; `patches.officersorthooks=true`, `ui.officer_sort=true`.
Entry: officer roster and officer assignment sorting menus.

Complete source `mods/src/patches/parts/officer_sort.cc` resolves comparers/fields,
discovers the native generic delegate class, roots temporary delegates/options,
appends one menu option and the roster sorter after native initialization, and
registers both hook outcomes. Config, Settings owner, native SortFunction,
ProcessingOption and OfficerSortGenerators are supporting boundaries.

| Obligation | Source behavior | Target mapping / check |
| --- | --- | --- |
| S1 | Native initialization runs first; disabled setting preserves native lists; no repeated duplicate key. | Existing typed original-first hooks and duplicate detection retained. |
| S2 | Roster and assignment use different default-direction comparers; unlocked first, BDA direction, optional index tie-break. | Compare every source wrapper with typed comparator chain; qualify optional comparers and ties. |
| S3 | Roster adds SortFunction to both sorter and option lists; assignment adds its option only. | Existing typed menu composition retained. |
| S4 | Required fields/classes/comparers must be usable before publishing constructed entries. | Source field preflight must survive adaptation to default-returning typed access. |
| S5 | Temporary delegate, SortFunction and option custody lasts through managed allocation and list insertion. | Preserve explicit source roots using existing GcHandle; do not infer this from a C++ raw local. |
| S6 | Shared generic delegate construction uses the verified constructor workaround; one transaction owns the two hooks. | Existing il2cpp::NewDelegate and HookSet retained, no parallel constructor/binding layer. |

Source optional-comparer loss can degrade ordering; it is not proof of unavailable
whole menu. Existing source and target choose unlocked grouping before BDA, while
the game's Descending comparer negates the combined result. Keep those semantics
explicit; do not silently redesign locked-officer ranking as part of migration.
Evidence and native trial limits: [cohort record](../FEATURE-COHORT-20261026-02.md).
