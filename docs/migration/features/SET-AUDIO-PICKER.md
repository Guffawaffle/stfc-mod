# SET-AUDIO-PICKER — Custom alert sound selection and preview

Contract v1; cohort 20261026-01. Source scan: 2026-10-26. Source
`frozen/play-dev-20261026@1baa55fedf1758b19f843cc59707b8901a0b4998`;
target base `05ee3d7f66eec4d9cb39f8924d012a7ca1ec5952`.

Parent: PLAY-AUDIO-CUES. Entry: Mod Settings → Audio Alerts → alert page.
Windows/macOS. Custom-file workflow absent upstream dev; present in play/play-dev;
target base supports prepared custom consumers and built-in/existing-custom choices
but lacks file selection and preview. Exact provenance is in the inventory.

## Source workflow

Complete producer: `settings/audio_alerts.cc/.h`, `persisted_setting.h`,
`audio_file_picker.h`, `audio_file_tasks.h`, Windows/macOS dialog providers.
Supporting consumers: `patches/notification_audio.cc`, `notification_audio_files.cc`,
`fleet_notification_settings.h`, `fleet_notification_types.h`, Config activation
and the runtime writer. `tests/audio_settings_test.cc` characterizes defaults,
prepared custom reuse, async completion and stale same-index reset.

| Obligation | Source behavior | Target disposition / check |
| --- | --- | --- |
| A1 | Built-ins, prepared custom cue reuse, custom file chooser and Preview for toast/incoming/fleet alert pages. | Preserve source alert state machine and page/metadata composition; adapt nested Config and existing host. |
| A2 | At most one pending file/dialog load; cancel/failure preserves old cue. Own paths/futures; no game/UI objects cross threads. | Preserve packaged-task model and shared single admission; loader uses existing bounded decoder. Test pending/cancel/failure and thread boundary. |
| A3 | File decode is off-thread; preview plays prepared data with no file I/O. | Existing LoadNotificationAudio/PlayNotificationSound reused, no second decoder/player. |
| A4 | Completed verified cue persists registered key; same custom index replacement invalidates old reset. | Preserve generation/source distinction, save status and exact source identity. Target's stronger ValueSetting contract requires truthful pending admission rather than pretending a cue is already applied. |
| A5 | Incoming/toast/fleet availability and fleet activation refresh follow their established owners. | Narrow exported readiness/refresh seam; no duplicated watcher or hooks. |
| A6 | Browsing registers metadata without config/file/game acquisition; shared frame polling only admitted for user intent. | Preserve target's pure catalog contract and lazy frame admission. |
| A7 | Pending operations must not overwrite a newer cue, reset, or unavailable owner. | Compare owned observed cue/source/generation before publication; record any strengthening of source behavior explicitly. |
| A8 | Coalescing/default/reset/search/save metadata and platform dialogs remain available. | Source setting definitions and existing preserving writer/maintenance owner; Windows COM and macOS Cocoa adapters. |

An external picker is process-level intent, as in source. Closing a page does not
retain a borrowed native view or by itself cancel the picker. Wrong/stale results
must not mutate replacement configuration. Source gaps discovered during port
must remain explicit rather than silently becoming migration equivalence claims.

Intentional safety adaptations: selecting a never-configured Custom option opens
the picker but does not claim that a cue has already changed. Explicit reset,
built-in selection or save retry revokes a pending result even if decoding finishes.
An observed cue generation prevents observed A→B→A replacement from reviving old
intent. This is not a global configuration/account epoch and cannot prove an
unobserved external replacement. Owner readiness combines the existing Toasts
installation and incoming-binding checks. Catalog construction remains inert;
frame polling is admitted only by explicit file-selection intent.

Validation/checkpoints: [cohort record](../FEATURE-COHORT-20261026-01.md).
Model/adapter fixtures cannot prove native dialogs, audible playback, device
availability or per-platform host behavior. macOS builds belong in CI, runtime
qualification with Mcohall. No CI work is part of this cohort.

## Deferred file-selector Clear action

Operator idea2026-10-28: loading-image and sound file selectors should expose a
clearly labeled **Clear** action. Whether it needs confirmation remains an open
UX question. This is deferred, not a change to existing picker behavior. Preserve
each consumer's fallback, save outcome and pending-selection cancellation rules
when designing it. Shared inventory follow-up: `SET-FILE-SELECTION-CLEAR`.
