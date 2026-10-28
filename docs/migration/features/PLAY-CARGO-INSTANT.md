# PLAY-CARGO-INSTANT — native cargo counter snap

Contract version: 1. Source-preserving implementation assessment, 2026-10-27.

## Inputs and scope

Source: frozen `play-dev` `1baa55fedf1758b19f843cc59707b8901a0b4998`, complete
`mods/src/patches/parts/instant_cargo.cc`, registry, configuration and
`mods/src/settings/preview_settings.{cc,h}`. Target base:
`play-dev-ripper` `a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`.
Current implementation: `src/mod/features/cargo.cc`, game bindings in
`src/mod/game/ui.h` and `server.h`. Exact branch observations/candidate remain in
the central inventory.

Windows only in source and target. `[patches].instantcargocounterhooks` gates
installation; live `[ui].instant_cargo_counter` defaults true. The feature shows
the native cargo model total immediately instead of its count-up animation.
It computes no alternate cargo amount and does not change the game's model.

## Behavioral obligations

| ID | Frozen source behavior | Target mapping / qualification |
| --- | --- | --- |
| O1 | Detour the progress bar's own non-generic parameterless void `SetWidgetData`, never an inherited shared Widget method. | Typed exact method lookup plus declaring-class test; fixture inherited method refuses before a hook is committed. |
| O2 | Admit only resolved instance fields with expected managed label, IProgressData context, Boolean snap and string identifier. | Restored explicit typed field-availability gate; `m_context` uses bound `IProgressData`, rejecting unmanaged-pointer metadata before dereference. Tests include invalid snap kind and pointer sentinel. |
| O3 | Apply only to the exact cargo identifier and exact runtime `CargoProgressData` class. | Existing identifier/class equality retained; unrelated/null context and reused noncargo label keep native count-up. A subclass is not silently accepted. |
| O4 | Set snap true only around one native original call and restore the prior true or false value. | Restored source RAII scope; prior target restored only on normal return. Actual hook fixture checks both previous values and controlled unwinding. |
| O5 | Disabled/unqualified context calls original exactly once without changing native fields. | Existing live gate and native fallback retained; fixture verifies original counts and unsnapped paths. |
| O6 | Keep native formatting/color path and interpolation lifecycle after the call. | No model rewrite, extra renderer or persistent flag. Pooled reward/progress controls retain original flag after scope. Native visual qualification remains pending. |

## Ownership and adaptations

The native hook receiver remains live for the synchronous original call; the scope
does not outlive it or retain it across frames. Field adapters belong to game
bindings, installation to the existing centralized Feature/HookSet registry.
The native library owns exception containment; the RAII fixture's C++ unwind only
tests restoration semantics and is not a claim that a real managed exception
propagates as that test exception. No additional logger, settings store, platform
support or updater was added.

The port had removed explicit metadata admission and RAII. These are narrow
integration corrections. Unsupported metadata never becomes an observed false
prefab flag. Native hook extent must still be qualified for the exact Windows
client; compilation alone does not establish its prologue. The feature remains
excluded on macOS rather than enabling an unqualified hook there.

## Verification and native trial

The actual production hooks are included by `tests/cargo_bindings_cases.h` using
controlled fields and substituted original rendering. Modes cover canonical
behavior, native-pointer context, invalid snap metadata, inherited method and
format-signature refusal. Exact results and binary identities are recorded only
after coordinator execution in the cohort receipt.

In-game: compare the cargo counter with the option enabled/disabled; open and
reopen multiple target cards and unrelated reward/progress UI. Totals, colors and
ordinary noncargo interpolation must stay native. No repair/resource action is
required. macOS availability remains false as in the frozen source.
