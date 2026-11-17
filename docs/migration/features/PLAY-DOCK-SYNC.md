# PLAY-DOCK-SYNC — dormant management-to-dock synchronization

Source-reviewed, implementation held at current behavior by Guff's latest
direction. Exact baselines: [cohort](../FEATURE-COHORT-20261029-02.md).
Frozen `hotkeys.cc::UpdateManageShipDockSelection`, source FleetBar/FleetData/
FleetsManager/ShipManagement bindings, frame caller and Settings were inspected.
Target remains `features/dock.cc::ManagedDockFleet/DockSelectionFrame`.

The existing100ms throttle runs only the source's management synchronization.
Live `ui.dock_syncmanagement` gates the workflow; management Details, a visible
controller, a real ship, an available fleet service and matching dock mapping
are required. It uses native RequestSelect(index,false), verifies selection,
retries native refusal, and avoids repeatedly overriding manual selection after
acknowledgement. Unassigned/synthetic fleet rows must not imply another dock.

Production code is unchanged in this pass. The last-fleet-only cache can skip a
replaced bar; source-level concern, not qualified native failure. The local
ownership prototype failed its first synthetic setup and is unqualified; it is
saved for recovery, not counted as a passed test or promoted component.

Future direction: replace polling with qualified native selection/context/bar
events, following Guff's requested redesign. Validate all event producers before
assuming a setter covers them. Preserve exact IDs, native dock-index semantics,
disable/close/replacement behavior and native refusal. No timer expansion or
general-purpose event/task infrastructure is authorized by this note.
