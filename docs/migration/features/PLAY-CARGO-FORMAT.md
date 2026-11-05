# PLAY-CARGO-FORMAT — cargo abbreviation precision

Contract version: 1. Source-preserving implementation assessment, 2026-10-27.

## Inputs and scope

Source: frozen `play-dev` `1baa55fedf1758b19f843cc59707b8901a0b4998`, entire
`mods/src/patches/parts/cargo_format.cc`, its registry in
`mods/src/patches/patches.cc`, configuration and `settings/upstream_features.h`.
Target base: `play-dev-ripper` `a546b7c9f846aa22aa9a12b82367d8a2134fa5ff`.
Consumer: `src/mod/features/cargo.cc`; native access: `src/mod/game/ui.h`.
Central inventory owns exact branch-presence observations and final candidate.
The `[patches].cargoformathooks` installation gate is restart-only; live
`[ui].format_cargo_values` defaults true and `cargo_significant_decimals` defaults
2. Existing `cargo_format` alias, Settings writer and persistence remain unchanged.

## Workflow and obligations

| ID | Frozen source behavior | Target mapping / evidence |
| --- | --- | --- |
| O1 | Hook native `ColourTextLocalizer.SetLocalTextParameters` and always call original once with original arguments. | Existing transactional descriptor retained; exact void/bool/reference-array method contract replaces name-only lookup. Production-adapter fixture exercises original calls and signature refusal. |
| O2 | Only identifier `shared_x_of_y_x_coloured` receives changed precision; unrelated labels unaffected. | Existing full UTF-16/ASCII `IdentifierIs` retained without string construction. Null, unrelated and Unicode lookalike identifiers are fixture cases. |
| O3 | Live desired precision is clamped to 0–6 for display; configuration value is not rewritten. | Existing clamp reused. Fixture covers negative, large and live-changed values. |
| O4 | Set field before original rendering; leave field at desired precision because later `Localize` consumes it. Avoid an identical field write. | Restored previous-value comparison through typed `significantDecimals`. No temporary restore of this persistent label setting. |
| O5 | Missing required fields preserve native behavior, and disabled formatting does not mutate the field. | Typed field-availability adapter gates installation/operation; fixture disabled and missing-data paths retain original renderer. |

## Ownership and decisions

No field data or managed reference is retained between callbacks. The game owns
localizer construction, text formatting and label reuse. Generic native bindings
own field resolution; the feature owns only the cargo-identifier decision.
The source compared only the low byte of each UTF-16 character; the target's
existing full-character comparison intentionally does not reproduce that accidental
matching of non-ASCII lookalikes. Turning formatting off retains source behavior:
it stops new writes, without promising to restore an earlier precision on pooled
labels. No native arithmetic, analytical values, text hierarchy or settings UI was
changed. This feature is shared across Windows/macOS.

## Verification and remaining qualification

`tests/cargo_bindings_cases.h` compiles the actual production implementation in the
controlled IL2CPP runtime. It substitutes label storage and the native original
formatting body. Candidate-bound execution/results are recorded in the cohort;
this contract alone does not assert that the fixture ran. The shared Windows
fixture includes counter tests; macOS builds/runtime remain separate.

In-game: view cargo and unrelated numerical labels, adjust precision and toggle
formatting through existing Settings, reopen target cards, and reload an existing
setting. Verify native formatting still owns abbreviation/color and other labels
remain unaffected. Source comparison and a successful build are not visual parity.
