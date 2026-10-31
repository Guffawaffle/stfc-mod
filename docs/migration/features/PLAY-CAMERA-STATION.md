# PLAY-CAMERA-STATION — station camera radius restoration

Source reconciliation, 2026-10-31. Frozen play-dev `1baa55fe` and historical play
`cad98ef7` remain unchanged. Target is `play-dev-ripper`, base `5c279fab`.

Read the complete frozen `interior_zoom.cc`, shared orbit hook/constraint bindings,
current `camera.cc` and upstream fix #349 (`f9a131ea677365c8927e3bf360b4e7c30749bbe9`).
Exterior rotation/input behavior is retained; this correction changes only interior
radius capture/restore identity.

| Obligation | Preserved behavior and correction |
| --- | --- |
| S1 | Existing `interiorzoomhooks` gate installs through the shared orbit constraint hook; no added detour. |
| S2 | In Starbase Interior, a finite `interior_zoom_scale > 1` multiplies the captured native range. Repeated updates never compound it. |
| S3 | The range belongs to `Constraint.FloatData`, which can survive replacement of its OrbitFrameProvider. Capture and restore by the shared radius object, not the provider. This imports the applicable upstream fix. |
| S4 | Outside the interior, or with native/disabled scale, restore the captured native min/max before the game's original constraint update. |
| S5 | Stored keys are comparison-only; retained values are plain floats. Never dereference a retired key. Separate radius objects retain separate ranges. |
| S6 | Call the native original exactly once, then retain the existing Haven constraint adapter. Exterior arrow rotation, system zoom and Haven geometry are not rewritten. |

The actual production callback fixture widens a range, replaces its provider while
sharing the radius, checks that expansion does not compound, restores on exterior
entry and restores on live scale disable. Native objects and section/config providers
are controlled substitutes. Actual station rendering, labels and repeated native
interior/exterior/Haven transitions require their own exact-artifact trial.

This is a bug fix outside the frozen source, not a new zoom feature or a settings change.
