# PLAY-WINDOW-PROFILE-TITLE — custom/profile title presentation

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; refactor starting point
`5c3637af61a3fb270db28f2610e95fe62666d5c6`. Frozen title workflow spans
`free_resize.cc:133–146`, `file.cc:77–91` and `titlewindows.h`. Current ownership
is `src/mod/features/window.cc` and File's selected path/label accessors.

| ID | Source obligation | Target contract |
| --- | --- | --- |
| T1 | Ordinary launches disable title attempts without reading or allocating the unchanged title. Custom naming activates title work independently of the display label. | Gate on `File::HasCustomNames()`, not nonempty `File::Profile()`. Ordinary launch leaves the native title untouched. |
| T2 | Source attempts at most once per second until successful. A window may not exist yet. | Restore steady-clock one-second retry cadence; a failed discovery/read/write keeps the request pending. |
| T3 | File::Title caches a composed title only after an actual nonempty native read. | Reject a zero/failed actual GetWindowTextW read even if the earlier reported length was positive. Cache the composed desired title before attempting its write, retry the same title after failure, and stop after success. |
| T4 | Custom prefixes use the selected config filename without its extension. | Preserve ordinary custom stems, spaces and Unicode. If the human label is empty despite custom routing, derive a prefix from the final selected config filename; do not silently skip the title. |
| T5 | Frozen admitted routing overwrote configPath with directory/config.toml, incidentally producing a generic config prefix. | Retain the explicit catalog intent: admitted profiles show their admitted immutable ID. Do not revert this deliberate presentation choice. No account identity is established by a title. |

The standard example remains `[alt] Star Trek Fleet Command` for `-ccm alt.toml`.
Directory-like custom operands retain the target's independently selected `.toml`
path. The fallback label follows that selected filename, rather than restoring
the frozen sequential replace_extension mutation defect. The direct fixture uses
an equivalent supplied UTF-8 conversion boundary and actual std::filesystem
operations to confirm `[.toml]` for that route. Separate File modes exercise the
production namespace selection.

Window discovery and installation remain with [PLAY-WINDOW](PLAY-WINDOW.md).
The title operation neither changes profile admission nor selects configuration,
stores, game processes or accounts. It does not relabel an already successful
window after a later native title/window replacement; that is outside the frozen
one-success workflow.

Local title-state, supplied File-context and controlled native-boundary checks
pass in the [cohort](../FEATURE-COHORT-20261102-WINDOW-CURSOR.md). Real window
creation/replacement, actual Win32 reads/writes, multiple clients and player-visible
titles remain unqualified. The title capability is Windows-only; Mcohall's testing
of an earlier Mac pre-release does not qualify these later local changes.
