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

## Operator clarification and next integration boundary — 2026-10-26

Officer preset ordering is **not functioning in the current migration**. Guff
withdrew the proposed error explanation: it was speculation, not evidence. Do
not investigate officer sorting or attribute the separate contained
`GetComponent` exception to this failure. The established blocker is the missing
native preset adapter described above.

The next integration should preserve the complete workflow, including applying
the saved display order, modifier-click handling, canonical native preset saves,
replacement-row adoption and release. Shift+Edit moves up; Ctrl+Edit on Windows
or Command+Edit on macOS moves down. Recognized reorder clicks must consume the
native Edit action, including an end boundary; ordinary Edit remains native
renaming. Preserve the source's both/neither-modifier rule unless explicitly
changed. The source only consumes a move when its reorder handler succeeds;
the requested recognized-click consumption must be accounted for explicitly,
not represented as a qualified behavior of the absent adapter.

Keep three persistence boundaries distinct:

- **Feature settings:** `config_settings.cc` / `ValueSetting` call the registered
  `runtime_config::SaveSetting` path into the selected TOML file.
- **Local display order:** `mod_state::QueueOfficerOrder` persists
  `officer_preset_order` through the existing state worker. Admission is not a
  durable-save receipt. Integrating a caller and loading/applying its result are
  still part of the missing native workflow.
- **Actual preset contents:** the game's native save callback must run exactly
  once against canonical data and the corresponding visible receiver mapping.
  Neither a TOML write nor a local state receipt substitutes for that save.

Further tests and cycles remain on hold. No live configuration/store contents
were inspected or changed for this clarification.
