# PLAY-OFFICER-PRESETS — Officer preset ordering and native save

Contract v1, scanned 2026-10-26. Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`;
target `play-dev-ripper@f4f64180543a9231234cd2b3d3f547dfa580476b`.
Windows/macOS; `patches.officerpresetreorderhooks=true`,
`ui.allow_officer_preset_reordering=false`. Entry: Officer Presets name-edit
button with Shift to move up, Ctrl (Windows) / Command (Mac) to move down.

Source `mods/src/patches/parts/officer_preset_reorder.cc` owns all five native
hooks, canonical validation, separate display order, save/rebind/release,
scroll preservation and state queue. Supporting `mod_state`, config, Settings
availability and native preset/scroller classes are part of the workflow.

| Obligation | Source behavior | Target status / qualification |
| --- | --- | --- |
| P1 | Preserve int64 SlotId including zero; canonical OrderId equals index; refuse ambiguous filled duplicate identities. | Existing `officer_preset_order.h` policy supports this; native acquisition absent. |
| P2 | Empty/all-null crew rows stay fixed, including duplicate pending SlotIds; IsOccupied is not authoritative. | Existing qualified observation/policy fixture supports this. |
| P3 | Reorder only display rows; keep canonical identities/crews unchanged and apply destination Presentation. | No qualified native adapter. Do not replace with a data-only swap. |
| P4 | Modifier name-edit moves one adjacent filled row, skips fixed rows and consumes boundaries; otherwise invoke native edit once. | Native input/binding absent; normal native behavior remains. |
| P5 | Native save runs once on canonical data and matching receivers; then observe any replacement row and reapply view safely. | Actual source/receiver/reentry/restore boundary unqualified; inactive experiment is not product behavior. |
| P6 | Restore native presentations/source on release or disable; never publish into another controller. | Requires rooted native ownership, generation and actual adapter tests. |
| P7 | Read persisted local order; append newly encountered IDs; queue order with original state key and truthful admission/durability. | Existing state worker reusable; no live mutation authorized merely for a test. |

This is the previously recorded missing native integration, not a newly invented
feature. Source preservation cannot justify reintroducing its unqualified raw
controller lifetime, reentrant rendering or original-save fallback hazards.
See [cohort evidence and options](../FEATURE-COHORT-20261026-02.md).
Pure fixtures and native compilation do not establish safe preset saving.
