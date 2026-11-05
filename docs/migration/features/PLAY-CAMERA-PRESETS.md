# PLAY-CAMERA-PRESETS — stored and session zoom presets

Contract v1, source scan 2026-10-27. Frozen play-dev:
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target base:
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` on `play-dev-ripper`.
Final qualification: [cohort 03](../FEATURE-COHORT-20261027-03.md).

## Source, entry points and branch evidence

Complete source: `mods/src/patches/parts/zoom.cc::StoreZoom` and Update;
MapKey/shortcut declarations, camera settings, configuration and registered
`PersistCurrentSetting` ownership. Current consumer is `features/camera.cc`,
with identities and conversions in `settings/basic_gameplay_settings.h` and
existing Settings/registered writer. F1–F5 select presets; Shift-F1–F5 store them;
Ctrl-= stores the default. Actual configured bindings and extended-tier rules
remain authoritative, including platform key resolution.

Upstream dev `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` has preset consumption
and `StoreZoom` in `mods/src/patches/parts/zoom.cc`, but its store has no save-key
or persistence submission: explicit stores there are session changes. PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296` and frozen play-dev have the keyed
persisting store. Thus consumer presence upstream is not equivalent persistence.
The refactor base above already has registered saves and session intents.

## Obligations

| ID | Source behavior | Target preservation / verification |
| --- | --- | --- |
| P1 | Store five presets/default as `(distance-minimum)/(maximum-minimum)*zoom` with six distinct canonical keys. | Existing Zoom identity table and writer reused; actual callback fixture drives all six keys and exact double submission. Shared pure writer tests cover actual preserving file behavior. |
| P2 | Store operations precede jumps and skip native camera Update for that successful store frame. | Existing order retained. An unavailable/invalid measurement no longer writes NaN or a guessed preset and permits native Update. |
| P3 | Jumps invert the same zoom-unit formula, preserving preset meanings across native range changes. | Qualified shared inverse conversion; native property setter and world-point command remain the effect owners. No alternate camera simulation. |
| P4 | Extended reset/min/max override a simultaneous preset jump; the preset still counts for use-presets-as-default. | Existing precedence retained, including the source's resulting-default rather than requested-preset interpretation. Callback fixture covers simultaneous preset/reset. |
| P5 | Explicit stores persist; automatic use-presets-as-default is session-only. | `SaveSetting` remains distinct from `RetainSessionIntent`; no disk write is submitted for the automatic default. The current observed zoom must be qualified after a successful native jump. |
| P6 | Text capture/native-hotkey/disabled mod paths do not store unintended values. | Existing Shortcut capture and Update focus gates remain; native-on-entry default is not a keyboard store. Saved vs accepted state remains the writer's separate outcome. |

## Decisions and qualification

The zero/nonfinite-range guard fixes a defect shared by frozen source and prior
target; it does not change configuration format, valid-value formulas or rounding.
No clamp hides a legitimate finite over-range observation. A failed distance
setter/world-point command cannot update session default as if the jump succeeded.
After a successful native distance setter, the scene, range and configured zoom
are checked again before publishing the acquired pointer/world data or issuing
the native zoom command. The setter's own change is not rolled back; a rebind
declines the remaining stale command. A deterministic setter-rebind fixture
covers that boundary separately from real in-game lifecycle qualification.
The existing default and five preset values remain unchanged by this source pass.
Preset destinations are resolved through a member pointer only after managed
measurement completes. Keyboard zoom likewise reacquires current configuration
after native calls, rather than retaining a reference into a potentially replaced
Config object. This is an ownership correction; controlled native measurement
tests do not claim to execute a real in-game Config replacement race.

The production callback fixture uses a counted save-submission sink, not a disk
writer. `tests/basic_gameplay_settings_test.cc` independently exercises registered
identities, exact-value preserving saves and session-only reload protection.
Neither establishes a successful save of player preferences on the current game.
Final executed results are source/binary bound in the cohort record.

Native trial: choose a disposable/operator-approved preset, store/invoke all five
and the default as convenient, verify reset/min/max precedence and use-presets-as-
default, then reopen/reload/relaunch to distinguish explicit durable stores from
the session-only automatic value. Do not overwrite preferences just to obtain a
passing test. Windows/macOS and file-save receipts remain separate qualifications.
