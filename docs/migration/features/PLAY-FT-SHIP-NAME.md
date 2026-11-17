# PLAY-FT-SHIP-NAME — retained upstream feature provenance

Provenance assessment v1, 2026-11-02. This catalog identifier names the existing
equipped-ship row below a Forbidden Tech item's large-panel header. It is an
already-present upstream-derived feature in the typed target, not a missing
frozen PLAY behavior to restore in this continuation. No product code, setting,
default or native hook is added, removed or changed by this assessment.

| Inspected local revision | Observation |
| --- | --- |
| upstream/dev e884efa7c6ff66d0776b4cd4d6b76ce193296be5 | `mods/src/patches/parts/forbidden_tech_ship_name.cc` contains EquippedShipName, UpdateRow, SetWidgetData/ClearWidgetData hooks and InstallForbiddenTechShipNameHooks. Config/default/example contain ui.forbidden_tech_ship_name, default true. |
| origin/play cad98ef7990dd66167eb64ea5dda525c1fe2c296 | That file, setting key, ForbiddenTechInventoryItemWidget workflow and "Equipped on" row are absent. The existing ship_tech_indicators.cc is a different FT/CT tile-art and pin-badge owner. |
| origin/play-dev and frozen1baa55fedf1758b19f843cc59707b8901a0b4998 | Same absence of the equipped-ship-row owner/key/workflow. This is not evidence of a frozen migration gap. |
| target starting headf9e66f2428944c09f24ae338252de1eeb5dc1479 | Existing `src/mod/features/forbidden_tech_ship_name.cc` owns EquippedShipName, ShipNameRow, UpdateShipNameRow, SetWidgetData/ClearWidgetData callbacks and kForbiddenTechShipName. Its config/default/parser and descriptor already exist. |

Upstream commit `3c8ab85ed9013a542506ce905f56861642b7404a`, "Add name to forbidden
tech selections (#347)", introduces the old owner. It is an ancestor of both
the pinned upstream revision and the typed target. The later typed rewrites and
source move retain the feature; `b72e1199` separately keeps its managed handles
out of process static teardown. These are existing source history, not changes
introduced by the present frozen-behavior reconciliation. All observations use
retained local refs; no remote freshness claim is made.

The original inventory row attributed the workflow to PLAY's
`mods/src/patches/parts/ship_tech_indicators.cc` and a generic ShipName symbol.
That attribution is incorrect. The retained historical identifier and current
Implemented, unverified classification do not make it a frozen-source consumer.
Record the actual upstream provenance without removing the existing typed
feature or treating it as newly approved upstream functionality.

The target's equipped-ship lookup uses the tech slot's OwnerId, supporting ships
without a dock, and the panel row retains its own clone/layout cleanup. Those
existing implementations are identified here, not newly qualified in full.
The existing managed-owner exit fixture covers cache retirement/process lifetime;
it does not qualify ship resolution, pooled UI layout, visible text or native
hook fit. Complete feature behavior, actual native UI and Mac acceptance remain
separate from this provenance correction. New upstream features and AAI stay
deferred.
